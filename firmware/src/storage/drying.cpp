#include "drying.h"

#include <Preferences.h>
#include <freertos/semphr.h>
#include <time.h>

#include "util/log.h"

namespace {
SemaphoreHandle_t mutex = nullptr;
DryingConfig current;
}  // namespace

void dryingBegin() {
  mutex = xSemaphoreCreateMutex();
  current = dryingDefaults();
  Preferences p;
  p.begin("drying", true);
  String json = p.getString("cfg", "");
  p.end();
  if (json.length()) {
    std::string err;
    if (!dryingConfigFromJson(json.c_str(), current, &err)) {
      logPrintf("[DRY] Gespeicherte Einstellungen ungültig (%s), Standard gilt\n", err.c_str());
      current = dryingDefaults();
    }
  }
}

DryingConfig dryingConfig() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  DryingConfig copy = current;
  xSemaphoreGive(mutex);
  return copy;
}

bool dryingSetConfig(const DryingConfig &cfg) {
  std::string json = dryingConfigToJson(cfg);
  Preferences p;
  p.begin("drying", false);
  bool ok = p.putString("cfg", json.c_str()) > 0;
  p.end();
  if (!ok) {
    return false;
  }
  xSemaphoreTake(mutex, portMAX_DELAY);
  current = cfg;
  xSemaphoreGive(mutex);
  return true;
}

static bool localDate(int &y, int &m, int &d) {
  time_t t = time(nullptr);
  if (t < 1700000000) {
    return false;  // Uhr noch nicht gestellt
  }
  struct tm tmv;
  localtime_r(&t, &tmv);
  y = tmv.tm_year + 1900;
  m = tmv.tm_mon + 1;
  d = tmv.tm_mday;
  return true;
}

long dryingToday() {
  int y, m, d;
  return localDate(y, m, d) ? dryingDayNumber(y, m, d) : -1;
}

bool dryingTodayText(char *out11) {
  long n = dryingToday();
  if (n < 0) {
    return false;
  }
  dryingFormatDate(n, out11);
  return true;
}

DryingStatus dryingStatusFor(const SpoolInfo &spool) {
  return dryingStatus(dryingConfig(), spool.material, spool.location[0] != '\0', spool.lastDried, dryingToday());
}
