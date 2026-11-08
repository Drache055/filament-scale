#include "scale.h"
#include "util/log.h"

#include <Arduino.h>
#include <HX711.h>
#include <Preferences.h>
#include <freertos/semphr.h>

#include "config.h"
#include "pins.h"

namespace {

HX711 hx;
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

void saveCalibration() {
  Preferences prefs;
  prefs.begin("scale", false);
  prefs.putLong("offset", offset);
  prefs.putFloat("factor", factor);
  prefs.putBool("cal", calibrated);
  prefs.end();
}

void loadCalibration() {
  Preferences prefs;
  prefs.begin("scale", false);
  offset = prefs.getLong("offset", 0);
  factor = prefs.getFloat("factor", 1.0f);
  calibrated = prefs.getBool("cal", false) && factor != 0.0f;
  prefs.end();
  logPrintf("[SCALE] Kalibrierung: %s, Offset %ld, Faktor %.4f\n", calibrated ? "ja" : "nein", offset, factor);
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

// Ungültige Lesungen: Ohne HX711 liest die Bibliothek am freien DOUT-Pin lauter Nullen (0) oder lauter Einsen (-1); das
// gilt auch, wenn der HX711 durch eine zu lange SCK-Pause in den Stromsparmodus fällt. Die Sättigungswerte des
// 24-Bit-Wandlers sind ebenfalls keine Messung.
bool rawInvalid(long raw) {
  return raw == 0 || raw == -1 || raw >= 0x7FFFFF || raw <= -0x800000;
}

void scaleTask(void *) {
  hx.begin(PIN_HX711_DOUT, PIN_HX711_SCK);
  lastSampleMs = millis();
  uint32_t rejected = 0;
  uint32_t lastRejectLogMs = 0;
  for (;;) {
    if (hx.is_ready()) {
      long raw = hx.read();
      if (rawInvalid(raw)) {
        rejected++;
        if (rejected && millis() - lastRejectLogMs > 5000) {
          logPrintf("[SCALE] %u ungültige Lesungen verworfen (Rohwert %ld), HX711 angeschlossen?\n", (unsigned)rejected,
                    raw);
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
        xSemaphoreTake(mutex, portMAX_DELAY);
        shared.valid = false;
        xSemaphoreGive(mutex);
        count = 0;
        head = 0;
      }
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }
}

}  // namespace

void scaleBegin() {
  mutex = xSemaphoreCreateMutex();
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
