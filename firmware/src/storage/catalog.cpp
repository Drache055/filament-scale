#include "catalog.h"
#include "util/log.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <freertos/semphr.h>

namespace {

SemaphoreHandle_t mutex = nullptr;
Catalog current;

constexpr size_t MAX_JSON_BYTES = 3500;  // NVS-Strings sind auf ca. 4000 Byte begrenzt

void setDefaultColors(Catalog &c) {
  const struct {
    const char *name;
    const char *hex;
  } colors[] = {{"Weiß", "FFFFFF"}, {"Schwarz", "000000"}, {"Grau", "808080"},  {"Rot", "FF0000"},
                {"Orange", "FF7F00"}, {"Gelb", "FFD000"},    {"Grün", "00A000"}, {"Blau", "0000FF"}};
  c.colorCount = 0;
  for (auto &col : colors) {
    strlcpy(c.colors[c.colorCount].name, col.name, CATALOG_NAME_LEN);
    strlcpy(c.colors[c.colorCount].hex, col.hex, sizeof(c.colors[0].hex));
    c.colorCount++;
  }
}

void setDefaults(Catalog &c) {
  c = Catalog();
  const struct {
    const char *name;
    int minT, maxT;
  } mats[] = {{"PLA", 190, 220}, {"ABS", 230, 260}, {"PETG", 220, 250}};
  for (auto &m : mats) {
    strlcpy(c.materials[c.materialCount].name, m.name, CATALOG_NAME_LEN);
    c.materials[c.materialCount].minTemp = m.minT;
    c.materials[c.materialCount].maxTemp = m.maxT;
    c.materialCount++;
  }
  for (const char *maker : {"Elegoo", "Generic"}) {
    strlcpy(c.makers[c.makerCount++], maker, CATALOG_MAKER_LEN);
  }
  setDefaultColors(c);
}

String trimmed(const char *s) {
  String t(s ? s : "");
  t.trim();
  return t;
}

// Prüft und übernimmt JSON in c. Leere Listen sind nicht erlaubt.
bool parse(const char *json, Catalog &c, String &error) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    error = String("ungültiges JSON: ") + err.c_str();
    return false;
  }
  JsonArrayConst mats = doc["materials"];
  JsonArrayConst makers = doc["manufacturers"];
  if (mats.isNull() || makers.isNull()) {
    error = "materials und manufacturers fehlen";
    return false;
  }
  if (mats.size() == 0 || makers.size() == 0) {
    error = "mindestens ein Typ und ein Hersteller nötig";
    return false;
  }
  if (mats.size() > CATALOG_MAX_ENTRIES || makers.size() > CATALOG_MAX_ENTRIES) {
    error = String("maximal ") + CATALOG_MAX_ENTRIES + " Einträge je Liste";
    return false;
  }
  Catalog next;
  for (JsonObjectConst m : mats) {
    String name = trimmed(m["name"] | "");
    int minT = m["min_temp"] | -1;
    int maxT = m["max_temp"] | -1;
    if (name.length() == 0 || name.length() >= CATALOG_NAME_LEN) {
      error = String("Typname leer oder länger als ") + (CATALOG_NAME_LEN - 1) + " Zeichen";
      return false;
    }
    if (minT < 0 || maxT < 0 || minT > 500 || maxT > 500 || minT > maxT) {
      error = "Temperaturen ungültig bei " + name + " (0 bis 500, min <= max)";
      return false;
    }
    CatalogMaterial &dst = next.materials[next.materialCount++];
    strlcpy(dst.name, name.c_str(), CATALOG_NAME_LEN);
    dst.minTemp = minT;
    dst.maxTemp = maxT;
  }
  for (JsonVariantConst v : makers) {
    String name = trimmed(v | "");
    if (name.length() == 0 || name.length() >= CATALOG_MAKER_LEN) {
      error = String("Herstellername leer oder länger als ") + (CATALOG_MAKER_LEN - 1) + " Zeichen";
      return false;
    }
    strlcpy(next.makers[next.makerCount++], name.c_str(), CATALOG_MAKER_LEN);
  }
  JsonArrayConst colors = doc["colors"];
  if (colors.isNull()) {  // ältere Kataloge ohne Farben
    setDefaultColors(next);
  } else {
    if (colors.size() == 0 || colors.size() > CATALOG_MAX_ENTRIES) {
      error = String("Farben: 1 bis ") + CATALOG_MAX_ENTRIES + " Einträge";
      return false;
    }
    for (JsonObjectConst col : colors) {
      String name = trimmed(col["name"] | "");
      String hex = trimmed(col["hex"] | "");
      if (hex.startsWith("#")) {
        hex.remove(0, 1);
      }
      hex.toUpperCase();
      if (name.length() == 0 || name.length() >= CATALOG_NAME_LEN) {
        error = String("Farbname leer oder länger als ") + (CATALOG_NAME_LEN - 1) + " Zeichen";
        return false;
      }
      bool valid = hex.length() == 6;
      for (size_t i = 0; valid && i < hex.length(); i++) {
        valid = isxdigit((unsigned char)hex[i]);
      }
      if (!valid) {
        error = "Farbe " + name + ": Hex-Wert muss 6 Stellen haben (z. B. FF7F00)";
        return false;
      }
      CatalogColor &dst = next.colors[next.colorCount++];
      strlcpy(dst.name, name.c_str(), CATALOG_NAME_LEN);
      strlcpy(dst.hex, hex.c_str(), sizeof(dst.hex));
    }
  }
  c = next;
  return true;
}

String toJson(const Catalog &c) {
  JsonDocument doc;
  JsonArray mats = doc["materials"].to<JsonArray>();
  for (int i = 0; i < c.materialCount; i++) {
    JsonObject o = mats.add<JsonObject>();
    o["name"] = c.materials[i].name;
    o["min_temp"] = c.materials[i].minTemp;
    o["max_temp"] = c.materials[i].maxTemp;
  }
  JsonArray makers = doc["manufacturers"].to<JsonArray>();
  for (int i = 0; i < c.makerCount; i++) {
    makers.add(c.makers[i]);
  }
  JsonArray colors = doc["colors"].to<JsonArray>();
  for (int i = 0; i < c.colorCount; i++) {
    JsonObject o = colors.add<JsonObject>();
    o["name"] = c.colors[i].name;
    o["hex"] = c.colors[i].hex;
  }
  String out;
  serializeJson(doc, out);
  return out;
}

}  // namespace

void catalogBegin() {
  mutex = xSemaphoreCreateMutex();
  setDefaults(current);
  Preferences prefs;
  prefs.begin("catalog", false);
  String stored = prefs.getString("json", "");
  prefs.end();
  if (stored.length()) {
    String error;
    Catalog loaded;
    if (parse(stored.c_str(), loaded, error)) {
      current = loaded;
    } else {
      logPrintf("[CATALOG] Gespeicherter Katalog ungültig (%s), nutze Standard\n", error.c_str());
    }
  }
  logPrintf("[CATALOG] %d Typen, %d Hersteller, %d Farben\n", current.materialCount, current.makerCount, current.colorCount);
}

void catalogGet(Catalog &out) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  out = current;
  xSemaphoreGive(mutex);
}

String catalogJson() {
  Catalog c;
  catalogGet(c);
  return toJson(c);
}

bool catalogSet(const char *json, String &error) {
  Catalog next;
  if (!parse(json, next, error)) {
    return false;
  }
  String normalized = toJson(next);
  if (normalized.length() > MAX_JSON_BYTES) {
    error = "Katalog zu groß";
    return false;
  }
  Preferences prefs;
  prefs.begin("catalog", false);
  size_t written = prefs.putString("json", normalized);
  prefs.end();
  if (written == 0) {
    error = "Speichern fehlgeschlagen";
    return false;
  }
  xSemaphoreTake(mutex, portMAX_DELAY);
  current = next;
  xSemaphoreGive(mutex);
  return true;
}

CatalogMergeResult catalogMerge(const String *makers, int makerCount, const CatalogMaterial *materials, int materialCount) {
  CatalogMergeResult res;
  Catalog *next = new Catalog();  // 2 KB: nicht auf den Stack (läuft im Spoolman-Task)
  catalogGet(*next);
  for (int i = 0; i < makerCount; i++) {
    String name = trimmed(makers[i].c_str());
    if (name.length() == 0 || name.length() >= CATALOG_MAKER_LEN) {
      res.skipped++;
      continue;
    }
    bool exists = false;
    for (int j = 0; j < next->makerCount; j++) {
      exists = exists || name.equalsIgnoreCase(next->makers[j]);
    }
    if (exists) {
      continue;
    }
    if (next->makerCount >= CATALOG_MAX_ENTRIES) {
      res.skipped++;
      continue;
    }
    strlcpy(next->makers[next->makerCount++], name.c_str(), CATALOG_MAKER_LEN);
    res.addedMakers++;
  }
  for (int i = 0; i < materialCount; i++) {
    String name = trimmed(materials[i].name);
    if (name.length() == 0 || name.length() >= CATALOG_NAME_LEN || materials[i].minTemp < 0 || materials[i].maxTemp > 500 ||
        materials[i].minTemp > materials[i].maxTemp) {
      res.skipped++;
      continue;
    }
    bool exists = false;
    for (int j = 0; j < next->materialCount; j++) {
      exists = exists || name.equalsIgnoreCase(next->materials[j].name);
    }
    if (exists) {
      continue;
    }
    if (next->materialCount >= CATALOG_MAX_ENTRIES) {
      res.skipped++;
      continue;
    }
    CatalogMaterial &dst = next->materials[next->materialCount++];
    strlcpy(dst.name, name.c_str(), CATALOG_NAME_LEN);
    dst.minTemp = materials[i].minTemp;
    dst.maxTemp = materials[i].maxTemp;
    res.addedMaterials++;
  }
  if (res.addedMakers || res.addedMaterials) {
    String error;
    if (!catalogSet(toJson(*next).c_str(), error)) {
      res.saved = false;
      res.error = error;
    }
  }
  delete next;
  return res;
}
