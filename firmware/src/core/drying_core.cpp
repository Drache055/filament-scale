#include "drying_core.h"

#include <ArduinoJson.h>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace {
bool sameName(const char *a, const char *b) {
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
      return false;
    }
    a++;
    b++;
  }
  return *a == *b;
}
}  // namespace

DryingConfig dryingDefaults() {
  DryingConfig c;
  // Annahme: übliche Richtwerte für offene Lagerung, nicht am Gerät oder mit Messungen geprüft.
  c.materials = {{"PLA", 90}, {"PETG", 30}, {"ABS", 60}, {"ASA", 60}, {"TPU", 30}, {"PA", 14}, {"PC", 14}, {"PVA", 7}};
  return c;
}

int dryingBaseDays(const DryingConfig &cfg, const char *material) {
  for (const DryingMaterial &m : cfg.materials) {
    if (material && sameName(m.name.c_str(), material)) {
      return m.days;
    }
  }
  return cfg.defaultDays;
}

int dryingEffectiveDays(const DryingConfig &cfg, const char *material, bool inBox) {
  if (!cfg.enabled) {
    return 0;
  }
  long days = dryingBaseDays(cfg, material);
  if (days <= 0) {
    return 0;
  }
  if (inBox && cfg.boxFactor > 1) {
    days *= cfg.boxFactor;
  }
  return days > DRYING_MAX_DAYS ? DRYING_MAX_DAYS : (int)days;
}

// Algorithmus "days_from_civil" (Howard Hinnant)
long dryingDayNumber(int year, int month, int day) {
  year -= month <= 2;
  long era = (year >= 0 ? year : year - 399) / 400;
  long yoe = year - era * 400;
  long doy = (153L * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

bool dryingParseDate(const char *text, long &dayNumber) {
  if (!text || strlen(text) != 10 || text[4] != '-' || text[7] != '-') {
    return false;
  }
  for (int i = 0; i < 10; i++) {
    if (i != 4 && i != 7 && !isdigit((unsigned char)text[i])) {
      return false;
    }
  }
  int y = atoi(text), m = atoi(text + 5), d = atoi(text + 8);
  if (y < 2000 || y > 2200 || m < 1 || m > 12 || d < 1 || d > 31) {
    return false;
  }
  long n = dryingDayNumber(y, m, d);
  char back[11];
  dryingFormatDate(n, back);  // 31. Februar o. ä. fällt durch den Rückweg
  if (strcmp(back, text) != 0) {
    return false;
  }
  dayNumber = n;
  return true;
}

void dryingFormatDate(long dayNumber, char *out) {
  long z = dayNumber + 719468;
  long era = (z >= 0 ? z : z - 146096) / 146097;
  long doe = z - era * 146097;
  long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  long y = yoe + era * 400;
  long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  long mp = (5 * doy + 2) / 153;
  long d = doy - (153 * mp + 2) / 5 + 1;
  long m = mp < 10 ? mp + 3 : mp - 9;
  int yy = (int)(y + (m <= 2));
  if (yy < 0 || yy > 9999) {
    yy = 0;  // außerhalb des sinnvollen Bereichs
  }
  snprintf(out, 11, "%04d-%02d-%02d", yy, (int)m, (int)d);
}

DryingStatus dryingStatus(const DryingConfig &cfg, const char *material, bool inBox, const char *lastDried, long today) {
  DryingStatus s;
  s.intervalDays = dryingEffectiveDays(cfg, material, inBox);
  if (s.intervalDays <= 0 || today < 0) {
    return s;  // Off
  }
  long last;
  if (!lastDried || !dryingParseDate(lastDried, last)) {
    s.state = DryingState::NoRecord;
    return s;
  }
  s.daysLeft = (int)(last + s.intervalDays - today);
  s.state = s.daysLeft <= 0 ? DryingState::Due : DryingState::Ok;
  return s;
}

std::string dryingConfigToJson(const DryingConfig &cfg) {
  JsonDocument doc;
  doc["enabled"] = cfg.enabled;
  doc["box_factor"] = cfg.boxFactor;
  doc["default_days"] = cfg.defaultDays;
  JsonObject mats = doc["materials"].to<JsonObject>();
  for (const DryingMaterial &m : cfg.materials) {
    mats[m.name] = m.days;
  }
  std::string out;
  serializeJson(doc, out);
  return out;
}

bool dryingConfigFromJson(const std::string &json, DryingConfig &cfg, std::string *error) {
  auto fail = [&](const char *msg) {
    if (error) {
      *error = msg;
    }
    return false;
  };
  JsonDocument doc;
  if (deserializeJson(doc, json) || !doc.is<JsonObject>()) {
    return fail("ungültiges JSON");
  }
  DryingConfig n;
  n.enabled = doc["enabled"] | true;
  n.boxFactor = doc["box_factor"] | 3;
  n.defaultDays = doc["default_days"] | 0;
  if (n.boxFactor < 1 || n.boxFactor > DRYING_MAX_FACTOR) {
    return fail("Box-Faktor muss 1 bis 10 sein");
  }
  if (n.defaultDays < 0 || n.defaultDays > DRYING_MAX_DAYS) {
    return fail("Intervall muss 0 bis 3650 Tage sein");
  }
  for (JsonPairConst kv : doc["materials"].as<JsonObjectConst>()) {
    std::string name = kv.key().c_str();
    if (name.empty() || name.size() > DRYING_MAX_NAME) {
      return fail("Materialname leer oder zu lang");
    }
    if (!kv.value().is<int>() || kv.value().as<int>() < 0 || kv.value().as<int>() > DRYING_MAX_DAYS) {
      return fail("Intervall muss 0 bis 3650 Tage sein");
    }
    if (n.materials.size() >= DRYING_MAX_MATERIALS) {
      return fail("zu viele Materialien");
    }
    n.materials.push_back({name, kv.value().as<int>()});
  }
  cfg = n;
  return true;
}
