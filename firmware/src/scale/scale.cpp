#include "scale.h"
#include "util/log.h"

#include <Arduino.h>
#include <Preferences.h>
#include <freertos/semphr.h>

#include "config.h"
#include "sensor.h"

namespace {

SensorConfig active;  // beim Start geladen, gilt bis zum Neustart
SensorConfig stored;  // gespeichert (Webseite), wirksam nach dem Neustart
LoadSensor *sensor = nullptr;
SemaphoreHandle_t mutex = nullptr;
ScaleReading shared;

long offset = 0;
float factor = 1.0f;
bool calibrated = false;

long window[SCALE_WINDOW];
int count = 0;
int head = 0;
uint32_t lastSampleMs = 0;
bool bootZeroDone = false;

// Kalibrierung pro Sensortyp: HX711 mit den alten Schlüsseln (offset, factor, cal), andere mit Anhang (offset_nau ...)
String calKey(const char *base) { return String(base) + sensorCalSuffix(active.type); }

void saveCalibration() {
  Preferences prefs;
  prefs.begin("scale", false);
  prefs.putLong(calKey("offset").c_str(), offset);
  prefs.putFloat(calKey("factor").c_str(), factor);
  prefs.putBool(calKey("cal").c_str(), calibrated);
  prefs.end();
}

void loadCalibration() {
  Preferences prefs;
  prefs.begin("scale", false);
  offset = prefs.getLong(calKey("offset").c_str(), 0);
  factor = prefs.getFloat(calKey("factor").c_str(), 1.0f);
  calibrated = prefs.getBool(calKey("cal").c_str(), false) && factor != 0.0f;
  prefs.end();
  logPrintf("[SCALE] Kalibrierung %s: %s, Offset %ld, Faktor %.4f\n", sensorTypeName(active.type),
            calibrated ? "ja" : "nein", offset, factor);
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
  long long sum = 0;
  long lo = window[0], hi = window[0];
  for (int i = 0; i < count; i++) {
    sum += window[i];
    lo = min(lo, window[i]);
    hi = max(hi, window[i]);
  }
  long mean = (long)(sum / count);
  bool full = count == SCALE_WINDOW;

  xSemaphoreTake(mutex, portMAX_DELAY);
  // Einmaliges Nullen nach dem Start, wenn die Waage leer und stabil ist (Drift seit der letzten Tara ausgleichen)
  if (!bootZeroDone && full && calibrated) {
    bootZeroDone = true;
    float g = (mean - offset) / factor;
    float range = fabsf((hi - lo) / factor);
    if (range <= SCALE_STABLE_G && fabsf(g) <= SCALE_EMPTY_G) {
      offset = mean;
      logPrintf("[SCALE] Autonullung, neuer Offset %ld\n", offset);
    }
  }
  shared.valid = true;
  shared.calibrated = calibrated;
  shared.rawMean = mean;
  shared.rawNet = mean - offset;
  shared.grams = calibrated ? (mean - offset) / factor : 0;
  shared.stable = full && calibrated && fabsf((hi - lo) / factor) <= SCALE_STABLE_G;
  shared.empty = shared.stable && fabsf(shared.grams) <= SCALE_EMPTY_G;
  shared.overload = calibrated && shared.grams > SCALE_OVERLOAD_G;
  xSemaphoreGive(mutex);
}

void markNoData() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  shared.valid = false;
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
    saveCalibration();
    logPrintf("[SCALE] Tara, Offset %ld\n", offset);
  }
  xSemaphoreGive(mutex);
  return ok;
}

bool scaleCalibrate(float knownGrams) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  long delta = shared.rawMean - offset;
  bool ok = shared.valid && knownGrams > 0 && labs(delta) >= SCALE_MIN_CAL_DELTA;
  if (ok) {
    factor = delta / knownGrams;
    calibrated = true;
    saveCalibration();
    logPrintf("[SCALE] Kalibriert: Delta %ld bei %.1f g, Faktor %.4f\n", delta, knownGrams, factor);
  }
  xSemaphoreGive(mutex);
  return ok;
}

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
