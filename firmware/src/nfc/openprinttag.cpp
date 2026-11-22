#include "openprinttag.h"

#include <ArduinoJson.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <strings.h>

#include "cbor.h"
#include "ndef.h"

namespace {

// Schlüssel laut data/*.yaml der Spezifikation (github.com/prusa3d/OpenPrintTag)
enum MetaKey { META_MAIN_OFFSET = 0, META_MAIN_SIZE = 1, META_AUX_OFFSET = 2, META_AUX_SIZE = 3 };
enum MainKey {
  MAIN_MATERIAL_CLASS = 8,
  MAIN_MATERIAL_TYPE = 9,
  MAIN_MATERIAL_NAME = 10,
  MAIN_BRAND_NAME = 11,
  MAIN_NOMINAL_WEIGHT = 16,
  MAIN_ACTUAL_WEIGHT = 17,
  MAIN_EMPTY_CONTAINER_WEIGHT = 18,
  MAIN_PRIMARY_COLOR = 19,
  MAIN_MIN_PRINT_TEMP = 34,
  MAIN_MAX_PRINT_TEMP = 35,
};
enum AuxKey { AUX_CONSUMED_WEIGHT = 0 };

constexpr size_t AUX_REGION_SIZE = 32;  // Empfehlung der Spezifikation
constexpr size_t MAX_SECTION = 512;     // jeder Abschnitt höchstens 512 Byte

struct MaterialType {
  int key;
  const char *abbr;
};
// material_type_enum.yaml, Stand 2026-10-09
const MaterialType kMaterialTypes[] = {
    {0, "PLA"},   {1, "PETG"},  {2, "TPU"},   {3, "ABS"},   {4, "ASA"},  {5, "PC"},    {6, "PCTG"},  {7, "PP"},
    {8, "PA6"},   {9, "PA11"},  {10, "PA12"}, {42, "PA612"}, {11, "PA66"}, {12, "CPE"}, {13, "TPE"}, {14, "HIPS"},
    {15, "PHA"},  {16, "PET"},  {17, "PEI"},  {18, "PBT"},  {19, "PVB"}, {20, "PVA"},  {21, "PEKK"}, {22, "PEEK"},
    {23, "BVOH"}, {24, "TPC"},  {25, "PPS"},  {26, "PPSU"}, {27, "PVC"}, {28, "PEBA"}, {29, "PVDF"}, {30, "PPA"},
    {31, "PCL"},  {32, "PES"},  {33, "PMMA"}, {34, "POM"},  {35, "PPE"}, {36, "PS"},   {37, "PSU"},  {38, "TPI"},
    {39, "SBS"},  {40, "OBC"},  {41, "EVA"},
};

// Kopiert höchstens cap-1 Byte, ohne ein UTF-8-Zeichen zu zerschneiden.
void copyUtf8(char *dst, size_t cap, const uint8_t *src, size_t n) {
  size_t k = n < cap - 1 ? n : cap - 1;
  if (k < n) {
    while (k > 0 && (src[k] & 0xC0) == 0x80) {
      k--;
    }
  }
  memcpy(dst, src, k);
  dst[k] = '\0';
}

bool readNumber(const uint8_t *buf, size_t len, size_t pos, double &out) {
  CborItem it;
  return cborHead(buf, len, pos, it) && cborNumber(it, out) && std::isfinite(out);
}

bool readUInt(const uint8_t *buf, size_t len, size_t pos, int &out) {
  CborItem it;
  if (!cborHead(buf, len, pos, it) || it.type != CborType::UInt || it.value > 65535) {
    return false;
  }
  out = (int)it.value;
  return true;
}

void readText(const uint8_t *buf, size_t len, size_t pos, char *dst, size_t cap) {
  CborItem it;
  if (cborHead(buf, len, pos, it) && it.type == CborType::Text && !it.indefinite) {
    copyUtf8(dst, cap, it.data, (size_t)it.value);
  }
}

struct Meta {
  int64_t mainOffset = -1, mainSize = -1, auxOffset = -1, auxSize = -1;
};

void onMeta(void *ctx, int64_t key, const uint8_t *buf, size_t len, size_t pos) {
  Meta &m = *static_cast<Meta *>(ctx);
  CborItem it;
  if (!cborHead(buf, len, pos, it) || it.type != CborType::UInt || it.value > 0xFFFF) {
    return;
  }
  int64_t v = (int64_t)it.value;
  switch (key) {
    case META_MAIN_OFFSET: m.mainOffset = v; break;
    case META_MAIN_SIZE: m.mainSize = v; break;
    case META_AUX_OFFSET: m.auxOffset = v; break;
    case META_AUX_SIZE: m.auxSize = v; break;
    default: break;
  }
}

void onMain(void *ctx, int64_t key, const uint8_t *buf, size_t len, size_t pos) {
  OpenPrintTag &t = *static_cast<OpenPrintTag *>(ctx);
  double d;
  switch (key) {
    case MAIN_MATERIAL_CLASS: readUInt(buf, len, pos, t.materialClass); break;
    case MAIN_MATERIAL_TYPE: readUInt(buf, len, pos, t.materialType); break;
    case MAIN_MATERIAL_NAME: readText(buf, len, pos, t.materialName, sizeof(t.materialName)); break;
    case MAIN_BRAND_NAME: readText(buf, len, pos, t.brand, sizeof(t.brand)); break;
    case MAIN_PRIMARY_COLOR: {
      CborItem it;
      if (cborHead(buf, len, pos, it) && it.type == CborType::Bytes && !it.indefinite &&
          (it.value == 3 || it.value == 4)) {
        memcpy(t.color, it.data, 3);
        t.hasColor = true;
      }
      break;
    }
    case MAIN_MIN_PRINT_TEMP:
      if (readNumber(buf, len, pos, d) && d > 0 && d < 1000) t.minTemp = (int)lround(d);
      break;
    case MAIN_MAX_PRINT_TEMP:
      if (readNumber(buf, len, pos, d) && d > 0 && d < 1000) t.maxTemp = (int)lround(d);
      break;
    case MAIN_NOMINAL_WEIGHT:
      if (readNumber(buf, len, pos, d) && d >= 0) t.nominalWeight = (float)d;
      break;
    case MAIN_ACTUAL_WEIGHT:
      if (readNumber(buf, len, pos, d) && d >= 0) t.actualWeight = (float)d;
      break;
    case MAIN_EMPTY_CONTAINER_WEIGHT:
      if (readNumber(buf, len, pos, d) && d >= 0) t.emptyContainerWeight = (float)d;
      break;
    default: break;  // unbekannte Schlüssel werden übersprungen
  }
}

void onAux(void *ctx, int64_t key, const uint8_t *buf, size_t len, size_t pos) {
  OpenPrintTag &t = *static_cast<OpenPrintTag *>(ctx);
  double d;
  if (key == AUX_CONSUMED_WEIGHT && readNumber(buf, len, pos, d)) {
    t.consumedWeight = (float)d;
  }
}

// Bytes vor der Payload im TLV, wie ndefBuildMimeTlv sie anlegt
size_t tlvPayloadOffset(size_t typeLen, size_t payloadLen) {
  bool sr = payloadLen < 256;
  size_t recHeader = 1 + 1 + (sr ? 1 : 4) + typeLen;
  size_t recLen = recHeader + payloadLen;
  return (recLen < 255 ? 2 : 4) + recHeader;
}

size_t encodedUIntLen(uint64_t v) { return v < 24 ? 1 : v <= 0xFF ? 2 : v <= 0xFFFF ? 3 : 5; }

size_t metaLen(size_t auxOffset) { return 1 + 1 + encodedUIntLen(auxOffset) + 1 + encodedUIntLen(AUX_REGION_SIZE); }

void writeWeight(CborWriter &w, int key, float grams) {
  if (grams >= 0) {
    w.uint((uint64_t)key);
    w.uint((uint64_t)lroundf(grams));
  }
}

}  // namespace

bool optParse(const uint8_t *payload, size_t len, OpenPrintTag &out) {
  out = OpenPrintTag();
  Meta meta;
  size_t metaEnd = cborForEachInMap(payload, len, 0, onMeta, &meta);
  if (metaEnd == 0) {
    return false;
  }
  size_t mainStart = meta.mainOffset >= 0 ? (size_t)meta.mainOffset : metaEnd;
  size_t mainEnd = len;
  if (meta.mainSize >= 0) {
    mainEnd = mainStart + (size_t)meta.mainSize;
  } else if (meta.auxOffset > (int64_t)mainStart) {
    mainEnd = (size_t)meta.auxOffset;
  }
  if (mainEnd > len) {
    mainEnd = len;
  }
  if (mainStart >= mainEnd || cborForEachInMap(payload, mainEnd, mainStart, onMain, &out) == 0) {
    out = OpenPrintTag();
    return false;
  }

  if (meta.auxOffset >= 0 && (size_t)meta.auxOffset < len) {
    size_t auxStart = (size_t)meta.auxOffset;
    size_t auxEnd = len;
    if (meta.auxSize >= 0) {
      auxEnd = auxStart + (size_t)meta.auxSize;
    } else if (mainStart > auxStart) {
      auxEnd = mainStart;
    }
    if (auxEnd > len) {
      auxEnd = len;
    }
    // Der Aux-Bereich darf beliebigen Inhalt haben (jeder darf ihn beschreiben), ein Fehler dort ist kein Fehler des Tags
    OpenPrintTag aux;
    if (auxStart < auxEnd && cborForEachInMap(payload, auxEnd, auxStart, onAux, &aux) != 0) {
      out.hasAux = true;
      out.consumedWeight = aux.consumedWeight;
    }
  }
  return true;
}

size_t optBuildTlv(const OpenPrintTag &tag, size_t capacityBytes, uint8_t *out, size_t outCap) {
  // Hauptbereich
  uint8_t main[MAX_SECTION];
  CborWriter w(main, sizeof(main));
  size_t pairs = 1 + (tag.materialType >= 0) + (tag.materialName[0] != 0) + (tag.brand[0] != 0) + tag.hasColor +
                 (tag.minTemp > 0) + (tag.maxTemp > 0) + (tag.nominalWeight >= 0) + (tag.actualWeight >= 0) +
                 (tag.emptyContainerWeight >= 0);
  w.mapHeader(pairs);
  w.uint(MAIN_MATERIAL_CLASS);
  w.uint(tag.materialClass >= 0 ? (uint64_t)tag.materialClass : 0);  // Pflichtfeld, Standard FFF
  if (tag.materialType >= 0) {
    w.uint(MAIN_MATERIAL_TYPE);
    w.uint((uint64_t)tag.materialType);
  }
  if (tag.materialName[0]) {
    w.uint(MAIN_MATERIAL_NAME);
    w.text(tag.materialName);
  }
  if (tag.brand[0]) {
    w.uint(MAIN_BRAND_NAME);
    w.text(tag.brand);
  }
  writeWeight(w, MAIN_NOMINAL_WEIGHT, tag.nominalWeight);
  writeWeight(w, MAIN_ACTUAL_WEIGHT, tag.actualWeight);
  writeWeight(w, MAIN_EMPTY_CONTAINER_WEIGHT, tag.emptyContainerWeight);
  if (tag.hasColor) {
    w.uint(MAIN_PRIMARY_COLOR);
    w.bytes(tag.color, 3);
  }
  if (tag.minTemp > 0) {
    w.uint(MAIN_MIN_PRINT_TEMP);
    w.uint((uint64_t)tag.minTemp);
  }
  if (tag.maxTemp > 0) {
    w.uint(MAIN_MAX_PRINT_TEMP);
    w.uint((uint64_t)tag.maxTemp);
  }
  if (!w.ok) {
    return 0;
  }
  size_t mainLen = w.pos;
  size_t typeLen = strlen(OPT_MIME_TYPE);

  // Layout: Meta | Haupt | Füllbytes | Aux (32 Byte). Der Aux-Bereich liegt auf einer 4-Byte-Grenze des Tags,
  // gerechnet ab Page 4 (selbst auf einer Grenze). Die Kopflänge hängt von der Payload-Länge ab, daher iterieren.
  size_t auxOffset = 0, payloadLen = 0, guess = 0;
  for (int round = 0; round < 4; round++) {
    size_t head = tlvPayloadOffset(typeLen, guess);
    size_t m = metaLen(0);
    for (int i = 0; i < 4; i++) {
      size_t end = head + m + mainLen;
      auxOffset = ((end + 3) & ~(size_t)3) - head;
      size_t m2 = metaLen(auxOffset);
      if (m2 == m) {
        break;
      }
      m = m2;
    }
    payloadLen = auxOffset + AUX_REGION_SIZE;
    if (tlvPayloadOffset(typeLen, payloadLen) == head) {
      break;
    }
    guess = payloadLen;
  }

  uint8_t payload[MAX_SECTION + 64];
  if (payloadLen > sizeof(payload)) {
    return 0;
  }
  memset(payload, 0, payloadLen);
  CborWriter mw(payload, auxOffset);
  mw.mapHeader(2);
  mw.uint(META_AUX_OFFSET);
  mw.uint(auxOffset);
  mw.uint(META_AUX_SIZE);
  mw.uint(AUX_REGION_SIZE);
  if (!mw.ok || mw.pos + mainLen > auxOffset) {
    return 0;
  }
  memcpy(payload + mw.pos, main, mainLen);
  payload[auxOffset] = 0xBF;  // leere Map mit offener Länge, wie die Spezifikation empfiehlt
  payload[auxOffset + 1] = 0xFF;
  return ndefBuildMimeTlv(OPT_MIME_TYPE, payload, payloadLen, capacityBytes, out, outCap);
}

const char *optMaterialTypeName(int key) {
  for (const auto &m : kMaterialTypes) {
    if (m.key == key) {
      return m.abbr;
    }
  }
  return nullptr;
}

int optMaterialTypeKey(const char *abbreviation) {
  for (const auto &m : kMaterialTypes) {
    if (strcasecmp(m.abbr, abbreviation) == 0) {
      return m.key;
    }
  }
  return -1;
}

void optToSpool(const OpenPrintTag &tag, SpoolTag &spool) {
  spool = SpoolTag();
  const char *abbr = optMaterialTypeName(tag.materialType);
  const char *type = abbr ? abbr : tag.materialName;
  copyUtf8(spool.type, sizeof(spool.type), (const uint8_t *)type, strlen(type));
  copyUtf8(spool.brand, sizeof(spool.brand), (const uint8_t *)tag.brand, strlen(tag.brand));
  if (tag.hasColor) {
    snprintf(spool.colorHex, sizeof(spool.colorHex), "%02X%02X%02X", tag.color[0], tag.color[1], tag.color[2]);
  }
  spool.minTemp = tag.minTemp;
  spool.maxTemp = tag.maxTemp;
}

void optFromSpool(const SpoolTag &spool, OpenPrintTag &tag) {
  tag = OpenPrintTag();
  tag.materialClass = 0;  // FFF
  tag.materialType = optMaterialTypeKey(spool.type);
  copyUtf8(tag.materialName, sizeof(tag.materialName), (const uint8_t *)spool.type, strlen(spool.type));
  copyUtf8(tag.brand, sizeof(tag.brand), (const uint8_t *)spool.brand, strlen(spool.brand));
  unsigned r, g, b;
  if (strlen(spool.colorHex) == 6 && sscanf(spool.colorHex, "%2x%2x%2x", &r, &g, &b) == 3) {
    tag.color[0] = (uint8_t)r;
    tag.color[1] = (uint8_t)g;
    tag.color[2] = (uint8_t)b;
    tag.hasColor = true;
  }
  tag.minTemp = spool.minTemp;
  tag.maxTemp = spool.maxTemp;
}

size_t optDescribeJson(const OpenPrintTag &tag, char *out, size_t cap) {
  JsonDocument doc;
  doc["format"] = "openprinttag";
  const char *abbr = optMaterialTypeName(tag.materialType);
  if (abbr) {
    doc["type"] = abbr;
  }
  if (tag.materialName[0]) {
    doc["name"] = tag.materialName;
  }
  if (tag.brand[0]) {
    doc["brand"] = tag.brand;
  }
  if (tag.hasColor) {
    char hex[8];
    snprintf(hex, sizeof(hex), "%02X%02X%02X", tag.color[0], tag.color[1], tag.color[2]);
    doc["color_hex"] = hex;
  }
  if (tag.minTemp > 0) doc["min_temp"] = tag.minTemp;
  if (tag.maxTemp > 0) doc["max_temp"] = tag.maxTemp;
  float net = tag.actualWeight >= 0 ? tag.actualWeight : tag.nominalWeight;
  if (net >= 0) doc["weight"] = net;
  if (tag.emptyContainerWeight >= 0) doc["spool_weight"] = tag.emptyContainerWeight;
  if (tag.consumedWeight >= 0) doc["consumed"] = tag.consumedWeight;
  size_t need = measureJson(doc);
  if (need + 1 > cap) {
    return 0;
  }
  return serializeJson(doc, out, cap);
}
