#include "scale.h"
#include "util/log.h"

#include <Arduino.h>
#include <Preferences.h>
#include <cstring>
#include <freertos/semphr.h>

#include <atomic>

#include "config.h"
#include "core/calib_core.h"
#include "core/scale_filter.h"
#include "sensor.h"

namespace {

SensorConfig active;  // beim Start geladen, gilt bis zum Neustart
SensorConfig stored;  // gespeichert (Webseite), wirksam nach dem Neustart
LoadSensor *sensor = nullptr;
SemaphoreHandle_t mutex = nullptr;
ScaleReading shared;

long offset = 0;
float factor = 1.0f;     // Rohwert pro Gramm (Ausgleichsgerade), für Stabilität und alte Firmware = cal.factor
bool calibrated = false;
CalModel cal;            // Kalibrierpunkte (Mehrpunkt-Kalibrierung, core/calib_core)
static_assert(sizeof(CalPoint) == 8, "NVS-Format der Kalibrierpunkte");

long window[SCALE_WINDOW];
int count = 0;
int head = 0;
uint32_t lastSampleMs = 0;

ZeroConfig zeroConfig() {
  ZeroConfig c;
  c.settleMs = SCALE_SETTLE_MS;
  c.bootG = SCALE_EMPTY_G;
  c.bootWindowMs = SCALE_BOOT_ZERO_MS;
  c.trackG = SCALE_ZERO_TRACK_G;
  c.trackHoldMs = SCALE_ZERO_TRACK_HOLD_MS;
  return c;
}
ZeroTracker zeroTracker(zeroConfig());
std::atomic<bool> uiIdle{true};  // von der UI gesetzt (scaleSetIdle)

// Kalibrierung pro Sensortyp: HX711 mit den alten Schlüsseln (offset, factor, cal), andere mit Anhang (offset_nau ...)
String calKey(const char *base) { return String(base) + sensorCalSuffix(active.type); }

void saveCalibration() {
  Preferences prefs;
  prefs.begin("scale", false);
  prefs.putLong(calKey("offset").c_str(), offset);
  prefs.putFloat(calKey("factor").c_str(), factor);
  prefs.putBool(calKey("cal").c_str(), calibrated);
  // Punktliste: Anzahl, Rohwerte, Gewichte. Ältere Firmware kennt den Schlüssel nicht und nutzt den Faktor.
  uint8_t blob[1 + CAL_MAX_POINTS * 8] = {};
  blob[0] = (uint8_t)cal.count;
  memcpy(blob + 1, cal.pt, sizeof(cal.pt));
  prefs.putBytes(calKey("pts").c_str(), blob, 1 + cal.count * sizeof(CalPoint));
  prefs.end();
}

void loadCalibration() {
  Preferences prefs;
  prefs.begin("scale", false);
  offset = prefs.getLong(calKey("offset").c_str(), 0);
  factor = prefs.getFloat(calKey("factor").c_str(), 1.0f);
  calibrated = prefs.getBool(calKey("cal").c_str(), false) && factor != 0.0f;
  // Punktliste (Mehrpunkt). Fehlt sie (alte Einpunkt-Kalibrierung) oder ist sie ungültig, gilt nur der Faktor.
  cal = CalModel();
  cal.factor = factor;
  uint8_t blob[1 + CAL_MAX_POINTS * 8] = {};
  size_t len = prefs.getBytes(calKey("pts").c_str(), blob, sizeof(blob));
  if (len >= 1 && blob[0] <= CAL_MAX_POINTS && len == 1 + blob[0] * sizeof(CalPoint)) {
    CalModel m;
    m.count = blob[0];
    memcpy(m.pt, blob + 1, m.count * sizeof(CalPoint));
    calUpdateFactor(m);
    if (m.count > 0 && calValid(m)) {
      cal = m;
      factor = cal.factor;
    }
  }
  prefs.end();
  logPrintf("[SCALE] Kalibrierung %s: %s, Offset %ld, Faktor %.4f, %d Punkt(e)\n", sensorTypeName(active.type),
            calibrated ? "ja" : "nein", offset, factor, cal.count);
}

// Sensorwahl (NVS-Namensraum "sensor"). Ungültige oder fehlende Einträge ergeben den HX711 an GPIO17/18.
SensorConfig loadSensorConfig() {
  Preferences prefs;
  prefs.begin("sensor", true);
  SensorConfig c;
  c.type = (SensorType)prefs.getUChar("type", (uint8_t)SensorType::HX711);
  SensorConfig def = sensorDefault((int)c.type < SENSOR_TYPE_COUNT ? c.type : SensorType::HX711);
  for (int i = 0; i < SENSOR_PIN_COUNT; i++) {
    char key[4] = {'p', (char)('0' + i), 0, 0};
    c.pin[i] = prefs.getChar(key, def.pin[i]);
  }
  prefs.end();
  const char *err = nullptr;
  if (!sensorValidate(c, err)) {
    logPrintf("[SCALE] Gespeicherte Sensorwahl ungültig (%s), nutze HX711\n", err);
    c = sensorDefault(SensorType::HX711);
  }
  return c;
}

void update() {
  WindowStats w = scaleWindowStats(window, count, SCALE_TRIM);
  bool full = count == SCALE_WINDOW;

  xSemaphoreTake(mutex, portMAX_DELAY);
  bool stable = full && calibrated && fabsf((w.hi - w.lo) / factor) <= SCALE_STABLE_G;
  if (calibrated) {
    // Autonullung nach dem Start und Nullpunkt-Nachführung im Leerlauf (CLAUDE.md 6.2). Nur im RAM, Tara speichert.
    ZeroInput zi;
    zi.now = millis();
    zi.stable = stable;
    zi.grams = calToGrams(cal, w.mean - offset);
    zi.idle = uiIdle.load();
    ZeroAction act = zeroTracker.step(zi);
    if (act != ZeroAction::None) {
      if (act == ZeroAction::BootZero) {
        logPrintf("[SCALE] Autonullung (%.1f g), neuer Offset %ld\n", zi.grams, w.mean);
      }
      offset = w.mean;
    }
  }
  shared.valid = true;
  shared.calibrated = calibrated;
  shared.rawMean = w.mean;
  shared.rawNet = w.mean - offset;
  shared.grams = calibrated ? calToGrams(cal, w.mean - offset) : 0;
  shared.stable = stable;
  shared.empty = shared.stable && fabsf(shared.grams) <= SCALE_EMPTY_G;
  shared.overload = calibrated && shared.grams > SCALE_OVERLOAD_G;
  xSemaphoreGive(mutex);
}

void markNoData() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  shared.valid = false;
  zeroTracker.dataLost();
  xSemaphoreGive(mutex);
  count = 0;
  head = 0;
}

// Ungültige Lesungen (sensorRawInvalid): Ohne HX711/ADS1232 liest man am freien DOUT-Pin lauter Nullen (0) oder lauter
// Einsen (-1); das gilt auch, wenn der HX711 durch eine zu lange SCK-Pause in den Stromsparmodus fällt. Die
// Sättigungswerte des 24-Bit-Wandlers sind bei allen Sensoren keine Messung.
void scaleTask(void *) {
  const char *name = sensorTypeName(active.type);
  sensor = sensorCreate(active);
  bool started = sensor->begin();
  uint32_t lastBeginMs = millis();
  if (!started) {
    logPrintf("[SCALE] %s nicht gefunden, neuer Versuch alle 2 s\n", name);
  }
  lastSampleMs = millis();
  uint32_t rejected = 0;
  uint32_t lastRejectLogMs = 0;
  for (;;) {
    if (!started) {
      if (millis() - lastBeginMs > 2000) {
        lastBeginMs = millis();
        started = sensor->begin();
        if (started) {
          logPrintf("[SCALE] %s gefunden\n", name);
          lastSampleMs = millis();
        }
      }
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    if (sensor->ready()) {
      long raw = 0;
      bool ok = sensor->read(raw);
      if (!ok || sensorRawInvalid(active.type, raw)) {
        rejected++;
        if (rejected && millis() - lastRejectLogMs > 5000) {
          logPrintf("[SCALE] %u ungültige Lesungen verworfen (Rohwert %ld), %s angeschlossen?\n", (unsigned)rejected,
                    raw, name);
          rejected = 0;
          lastRejectLogMs = millis();
        }
        vTaskDelay(pdMS_TO_TICKS(5));
        continue;
      }
      lastSampleMs = millis();
      window[head] = raw;
      head = (head + 1) % SCALE_WINDOW;
      if (count < SCALE_WINDOW) {
        count++;
      }
      update();
    } else {
      if (millis() - lastSampleMs > SCALE_NO_DATA_MS) {
        markNoData();
        // Ein I²C-Sensor kann nach einem Spannungseinbruch seine Register verloren haben: neu initialisieren
        if (active.type == SensorType::NAU7802) {
          started = false;
          lastBeginMs = millis();
        }
      }
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }
}

}  // namespace

void scaleBegin() {
  mutex = xSemaphoreCreateMutex();
  active = loadSensorConfig();
  stored = active;
  logPrintf("[SCALE] Sensor %s, Pins %d/%d/%d\n", sensorTypeName(active.type), active.pin[0], active.pin[1],
            active.pin[2]);
  loadCalibration();
  xTaskCreatePinnedToCore(scaleTask, "scale", 4096, nullptr, 3, nullptr, 1);
}

ScaleReading scaleGet() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  ScaleReading copy = shared;
  xSemaphoreGive(mutex);
  return copy;
}

bool scaleTare() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  bool ok = shared.valid;
  if (ok) {
    offset = shared.rawMean;
    shared.rawNet = 0;
    shared.grams = 0;
    zeroTracker.manualZero();
    saveCalibration();
    logPrintf("[SCALE] Tara, Offset %ld\n", offset);
  }
  xSemaphoreGive(mutex);
  return ok;
}

bool scaleCalibrate(float knownGrams) { return scaleCalibratePoint(knownGrams, false) == CalError::None; }

CalError scaleCalibratePoint(float knownGrams, bool add) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  long delta = shared.rawMean - offset;
  CalError err = CalError::SmallSignal;
  if (shared.valid) {
    // Ohne gültige Kalibrierung gibt es nichts zu ergänzen: dann wie eine Einpunkt-Kalibrierung
    bool extend = add && calibrated;
    if (extend && cal.count == 0) {
      // Alte Einpunkt-Kalibrierung (nur Faktor): Ergänzen ist ohne den Punkt nicht möglich, neu beginnen
      extend = false;
    }
    err = extend ? calAddPoint(cal, (int32_t)delta, knownGrams, SCALE_MIN_CAL_DELTA)
                 : calSetSingle(cal, (int32_t)delta, knownGrams, SCALE_MIN_CAL_DELTA);
  }
  if (err == CalError::None) {
    factor = cal.factor;
    calibrated = true;
    saveCalibration();
    logPrintf("[SCALE] Kalibriert: Delta %ld bei %.1f g, %d Punkt(e), Faktor %.4f\n", delta, knownGrams, cal.count,
              factor);
  }
  xSemaphoreGive(mutex);
  return err;
}

ScaleCalibration scaleCalibration() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  ScaleCalibration c;
  c.calibrated = calibrated;
  c.model = cal;
  xSemaphoreGive(mutex);
  return c;
}

void scaleSetIdle(bool isIdle) { uiIdle.store(isIdle); }

SensorConfig scaleSensorActive() { return active; }

SensorConfig scaleSensorStored() { return stored; }

bool scaleSensorSave(const SensorConfig &cfg, const char *&error) {
  if (!sensorValidate(cfg, error)) {
    return false;
  }
  Preferences prefs;
  prefs.begin("sensor", false);
  prefs.putUChar("type", (uint8_t)cfg.type);
  for (int i = 0; i < SENSOR_PIN_COUNT; i++) {
    char key[4] = {'p', (char)('0' + i), 0, 0};
    prefs.putChar(key, cfg.pin[i]);
  }
  prefs.end();
  stored = cfg;
  logPrintf("[SCALE] Sensorwahl gespeichert: %s, Pins %d/%d/%d (nach Neustart)\n", sensorTypeName(cfg.type), cfg.pin[0],
            cfg.pin[1], cfg.pin[2]);
  return true;
}
