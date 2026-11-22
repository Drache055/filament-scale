#include <unity.h>

#include <cstring>

#include "nfc/cbor.h"
#include "nfc/ndef.h"
#include "nfc/openprinttag.h"

// Beispiel-Tag aus der OpenPrintTag-Spezifikation (tests/encode_decode/01_data.bin, github.com/prusa3d/OpenPrintTag,
// MIT-Lizenz, Copyright 2025 Prusa Research a.s.): NFC-V-Speicherabbild, CC (4 Byte), dann NDEF-TLV mit
// URI-Record und OpenPrintTag-Record. Prusament PLA Prusa Galaxy Black.
static const uint8_t SAMPLE_01[] = {
0xE1,0x40,0x27,0x01,0x03,0xFF,0x01,0x2F,0x91,0x01,0x17,0x55,0x04,0x33,0x64,0x74,0x61,0x67,0x2E,0x6F,0x72,0x67,
0x2F,0x73,0x2F,0x33,0x33,0x34,0x63,0x35,0x34,0x66,0x30,0x38,0x38,0x52,0x1C,0xF5,0x61,0x70,0x70,0x6C,0x69,0x63,
0x61,0x74,0x69,0x6F,0x6E,0x2F,0x76,0x6E,0x64,0x2E,0x6F,0x70,0x65,0x6E,0x70,0x72,0x69,0x6E,0x74,0x74,0x61,0x67,
0xA1,0x02,0x18,0xD2,0xBF,0x04,0x1B,0x00,0x00,0x07,0xD0,0xFC,0xAB,0x45,0xF9,0x05,0x6A,0x33,0x33,0x34,0x63,0x35,
0x34,0x66,0x30,0x38,0x38,0x08,0x00,0x09,0x00,0x0A,0x76,0x50,0x4C,0x41,0x20,0x50,0x72,0x75,0x73,0x61,0x20,0x47,
0x61,0x6C,0x61,0x78,0x79,0x20,0x42,0x6C,0x61,0x63,0x6B,0x0B,0x69,0x50,0x72,0x75,0x73,0x61,0x6D,0x65,0x6E,0x74,
0x0E,0x1A,0x68,0xD3,0xC7,0xD7,0x10,0x19,0x03,0xE8,0x11,0x19,0x03,0xF4,0x12,0x19,0x01,0x18,0x13,0x44,0x3D,0x3E,
0x3D,0xFF,0x18,0x1C,0x9F,0x17,0xFF,0x18,0x1D,0xF9,0x3C,0xF6,0x18,0x22,0x18,0xCD,0x18,0x23,0x18,0xE1,0x18,0x24,
0x18,0xAA,0x18,0x25,0x18,0x28,0x18,0x26,0x18,0x3C,0x18,0x27,0x12,0x18,0x28,0x18,0x28,0x18,0x29,0x14,0x18,0x2A,
0x18,0x40,0x18,0x2B,0x18,0xC8,0x18,0x2C,0x18,0x64,0x18,0x2D,0x18,0x34,0x18,0x38,0x9F,0x00,0x01,0xFF,0x18,0x3B,
0x83,0x18,0x32,0xFA,0x41,0x34,0xCC,0xCD,0xFA,0x43,0x01,0x4C,0xCD,0x18,0x3C,0x69,0x32,0x37,0x30,0x20,0x33,0x30,
0x20,0x32,0x30,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xA0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0xFE
};

void setUp() {}
void tearDown() {}

// Ab Byte 4 (nach dem CC) ist der Aufbau derselbe wie auf einem NTAG ab Page 4
static bool samplePayload(NdefMime &m) {
  NdefLocation loc;
  if (ndefLocate(SAMPLE_01 + 4, sizeof(SAMPLE_01) - 4, loc) != NdefLocate::Found) {
    return false;
  }
  return ndefFindMimeRecord(SAMPLE_01 + 4 + loc.recordOffset, loc.recordLength, OPT_MIME_TYPE, m);
}

void test_find_record_after_uri_record() {
  NdefMime m;
  TEST_ASSERT_TRUE(samplePayload(m));
  TEST_ASSERT_EQUAL_STRING(OPT_MIME_TYPE, m.type);
  TEST_ASSERT_EQUAL(245, m.payloadLength);
  // Der erste Record ist ein URI-Record, kein MIME: ndefParseMimeRecord allein findet nichts
  NdefLocation loc;
  ndefLocate(SAMPLE_01 + 4, sizeof(SAMPLE_01) - 4, loc);
  NdefMime first;
  TEST_ASSERT_FALSE(ndefParseMimeRecord(SAMPLE_01 + 4 + loc.recordOffset, loc.recordLength, first));
  NdefMime json;
  TEST_ASSERT_FALSE(ndefFindMimeRecord(SAMPLE_01 + 4 + loc.recordOffset, loc.recordLength, "application/json", json));
}

void test_parse_prusa_sample() {
  NdefMime m;
  TEST_ASSERT_TRUE(samplePayload(m));
  OpenPrintTag t;
  TEST_ASSERT_TRUE(optParse(m.payload, m.payloadLength, t));
  TEST_ASSERT_EQUAL(0, t.materialClass);  // FFF
  TEST_ASSERT_EQUAL(0, t.materialType);   // PLA
  TEST_ASSERT_EQUAL_STRING("PLA Prusa Galaxy Black", t.materialName);
  TEST_ASSERT_EQUAL_STRING("Prusament", t.brand);
  TEST_ASSERT_TRUE(t.hasColor);
  TEST_ASSERT_EQUAL_HEX8(0x3D, t.color[0]);
  TEST_ASSERT_EQUAL_HEX8(0x3E, t.color[1]);
  TEST_ASSERT_EQUAL_HEX8(0x3D, t.color[2]);
  TEST_ASSERT_EQUAL(205, t.minTemp);
  TEST_ASSERT_EQUAL(225, t.maxTemp);
  TEST_ASSERT_EQUAL_FLOAT(1000, t.nominalWeight);
  TEST_ASSERT_EQUAL_FLOAT(1012, t.actualWeight);
  TEST_ASSERT_EQUAL_FLOAT(280, t.emptyContainerWeight);
  TEST_ASSERT_TRUE(t.hasAux);
  TEST_ASSERT_EQUAL_FLOAT(-1, t.consumedWeight);

  SpoolTag s;
  optToSpool(t, s);
  TEST_ASSERT_EQUAL_STRING("PLA", s.type);
  TEST_ASSERT_EQUAL_STRING("Prusament", s.brand);
  TEST_ASSERT_EQUAL_STRING("3D3E3D", s.colorHex);
  TEST_ASSERT_EQUAL(205, s.minTemp);
  TEST_ASSERT_EQUAL(-1, s.spoolmanId);
}

void test_build_roundtrip_on_ntag() {
  SpoolTag s;
  strcpy(s.type, "PETG");
  strcpy(s.brand, "Elegoo");
  strcpy(s.colorHex, "FF5733");
  s.minTemp = 220;
  s.maxTemp = 250;
  OpenPrintTag t;
  optFromSpool(s, t);
  TEST_ASSERT_EQUAL(1, t.materialType);

  uint8_t buf[900];
  size_t n = optBuildTlv(t, 496, buf, sizeof buf);  // NTAG215
  TEST_ASSERT_GREATER_THAN(0, n);
  TEST_ASSERT_EQUAL(0, n % 4);

  NdefLocation loc;
  TEST_ASSERT_EQUAL((int)NdefLocate::Found, (int)ndefLocate(buf, n, loc));
  NdefMime m;
  TEST_ASSERT_TRUE(ndefFindMimeRecord(buf + loc.recordOffset, loc.recordLength, OPT_MIME_TYPE, m));
  OpenPrintTag back;
  TEST_ASSERT_TRUE(optParse(m.payload, m.payloadLength, back));
  TEST_ASSERT_EQUAL(0, back.materialClass);
  TEST_ASSERT_EQUAL(1, back.materialType);
  TEST_ASSERT_EQUAL_STRING("PETG", back.materialName);
  TEST_ASSERT_EQUAL_STRING("Elegoo", back.brand);
  TEST_ASSERT_EQUAL(220, back.minTemp);
  TEST_ASSERT_EQUAL(250, back.maxTemp);
  TEST_ASSERT_TRUE(back.hasAux);

  // Aux-Bereich: Offset aus dem Meta-Bereich, auf 4-Byte-Grenze des Tags, beginnt mit leerer Map (BF FF)
  CborItem map, key, val;
  TEST_ASSERT_TRUE(cborHead(m.payload, m.payloadLength, 0, map));
  TEST_ASSERT_EQUAL((int)CborType::Map, (int)map.type);
  TEST_ASSERT_TRUE(cborHead(m.payload, m.payloadLength, map.headerEnd, key));
  TEST_ASSERT_EQUAL(2, (int)key.value);  // aux_region_offset
  TEST_ASSERT_TRUE(cborHead(m.payload, m.payloadLength, key.headerEnd, val));
  size_t aux = (size_t)val.value;
  size_t absolute = (size_t)(m.payload - buf) + aux;
  TEST_ASSERT_EQUAL(0, absolute % 4);
  TEST_ASSERT_EQUAL_HEX8(0xBF, m.payload[aux]);
  TEST_ASSERT_EQUAL_HEX8(0xFF, m.payload[aux + 1]);
  TEST_ASSERT_EQUAL(aux + 32, m.payloadLength);

  SpoolTag s2;
  optToSpool(back, s2);
  TEST_ASSERT_EQUAL_STRING("PETG", s2.type);
  TEST_ASSERT_EQUAL_STRING("FF5733", s2.colorHex);
}

void test_unknown_type_uses_material_name() {
  SpoolTag s;
  strcpy(s.type, "PLA Silk");
  strcpy(s.brand, "Generic");
  OpenPrintTag t;
  optFromSpool(s, t);
  TEST_ASSERT_EQUAL(-1, t.materialType);
  TEST_ASSERT_FALSE(t.hasColor);  // keine Farbe gesetzt
  uint8_t buf[900];
  size_t n = optBuildTlv(t, 872, buf, sizeof buf);
  TEST_ASSERT_GREATER_THAN(0, n);
  NdefLocation loc;
  ndefLocate(buf, n, loc);
  NdefMime m;
  TEST_ASSERT_TRUE(ndefFindMimeRecord(buf + loc.recordOffset, loc.recordLength, OPT_MIME_TYPE, m));
  OpenPrintTag back;
  TEST_ASSERT_TRUE(optParse(m.payload, m.payloadLength, back));
  SpoolTag s2;
  optToSpool(back, s2);
  TEST_ASSERT_EQUAL_STRING("PLA Silk", s2.type);
  TEST_ASSERT_EQUAL_STRING("", s2.colorHex);
}

void test_too_small_capacity() {
  OpenPrintTag t;
  strcpy(t.materialName, "PLA");
  uint8_t buf[900];
  TEST_ASSERT_EQUAL(0, optBuildTlv(t, 48, buf, sizeof buf));
}

void test_unknown_keys_floats_and_indefinite_maps() {
  // Meta {} (Haupt folgt direkt), Haupt als offene Map mit unbekanntem Schlüssel 27 (double), Schlüssel 65535
  // (Text), Array, Tag, Temperatur als Half-Float (205.0 = 0x5A68) und Gewicht als Float32 (750.5)
  const uint8_t p[] = {0xA0, 0xBF, 0x18, 0x1B, 0xFB, 0x40, 0x14, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x19, 0xFF, 0xFF,
                       0x64, 't',  'e',  's',  't',  0x18, 0x1C, 0x82, 0x01, 0x02, 0x18, 0x1D, 0xC1, 0x1A, 0x00, 0x00,
                       0x00, 0x01, 0x18, 0x22, 0xF9, 0x5A, 0x68, 0x10, 0xFA, 0x44, 0x3B, 0xA0, 0x00, 0x08, 0x00, 0xFF};
  OpenPrintTag t;
  TEST_ASSERT_TRUE(optParse(p, sizeof p, t));
  TEST_ASSERT_EQUAL(0, t.materialClass);
  TEST_ASSERT_EQUAL(205, t.minTemp);
  TEST_ASSERT_EQUAL_FLOAT(750.5f, t.nominalWeight);
  TEST_ASSERT_FALSE(t.hasAux);
}

void test_garbage_is_rejected() {
  const uint8_t empty[] = {0x00};
  OpenPrintTag t;
  TEST_ASSERT_FALSE(optParse(empty, 0, t));
  const uint8_t notMap[] = {0x01, 0x02};
  TEST_ASSERT_FALSE(optParse(notMap, sizeof notMap, t));
  const uint8_t truncated[] = {0xA0, 0xA2, 0x08, 0x00, 0x0A, 0x65, 'a'};  // Text kürzer als angegeben
  TEST_ASSERT_FALSE(optParse(truncated, sizeof truncated, t));
  const uint8_t deep[] = {0xA0, 0xA1, 0x01, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x00};
  TEST_ASSERT_FALSE(optParse(deep, sizeof deep, t));  // zu tief verschachtelt
}

void test_aux_garbage_is_ignored() {
  // Meta {2: 4}, Haupt {8: 0} an Offset 2, Aux an Offset 4 mit Müll (0xFF = Break außerhalb einer Map)
  const uint8_t p[] = {0xA1, 0x02, 0x04, 0x00, 0xFF, 0xFF};
  // Haupt würde hier bei Offset 3 (nach Meta) beginnen und bis 4 gehen: 0x00 ist keine Map -> ungültig
  OpenPrintTag t;
  TEST_ASSERT_FALSE(optParse(p, sizeof p, t));
  const uint8_t ok[] = {0xA1, 0x02, 0x06, 0xA1, 0x08, 0x00, 0xFF, 0xFF};
  TEST_ASSERT_TRUE(optParse(ok, sizeof ok, t));
  TEST_ASSERT_FALSE(t.hasAux);
}

void test_utf8_truncation() {
  OpenPrintTag t;
  // SpoolTag.type hat Platz für 15 Byte, das "ü" läge auf Byte 14 und 15 und wird nicht halbiert
  strcpy(t.materialName, "ABCDEFGHIJKLMN\xC3\xBC");
  SpoolTag s;
  optToSpool(t, s);
  TEST_ASSERT_EQUAL_STRING("ABCDEFGHIJKLMN", s.type);
}

void test_describe_json() {
  NdefMime m;
  samplePayload(m);
  OpenPrintTag t;
  optParse(m.payload, m.payloadLength, t);
  char json[256];
  TEST_ASSERT_GREATER_THAN(0, optDescribeJson(t, json, sizeof json));
  TEST_ASSERT_NOT_NULL(strstr(json, "\"format\":\"openprinttag\""));
  TEST_ASSERT_NOT_NULL(strstr(json, "\"weight\":1012"));
}

void test_cbor_writer_ints() {
  uint8_t b[16];
  CborWriter w(b, sizeof b);
  w.uint(23);
  w.uint(24);
  w.uint(256);
  w.sint(-1);
  w.sint(-25);
  TEST_ASSERT_TRUE(w.ok);
  const uint8_t expect[] = {0x17, 0x18, 0x18, 0x19, 0x01, 0x00, 0x20, 0x38, 0x18};
  TEST_ASSERT_EQUAL(sizeof expect, w.pos);
  TEST_ASSERT_EQUAL_MEMORY(expect, b, sizeof expect);
  CborWriter small(b, 1);
  small.uint(300);
  TEST_ASSERT_FALSE(small.ok);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_find_record_after_uri_record);
  RUN_TEST(test_parse_prusa_sample);
  RUN_TEST(test_build_roundtrip_on_ntag);
  RUN_TEST(test_unknown_type_uses_material_name);
  RUN_TEST(test_too_small_capacity);
  RUN_TEST(test_unknown_keys_floats_and_indefinite_maps);
  RUN_TEST(test_garbage_is_rejected);
  RUN_TEST(test_aux_garbage_is_ignored);
  RUN_TEST(test_utf8_truncation);
  RUN_TEST(test_describe_json);
  RUN_TEST(test_cbor_writer_ints);
  return UNITY_END();
}
