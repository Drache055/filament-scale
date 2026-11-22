#include "devicepw.h"
#include "util/log.h"

#include <Preferences.h>
#include <esp_random.h>

#include "core/devicepw_core.h"
#include "secrets.h"

namespace {

// Die UI liest im ui-Task, geschrieben wird im net-Task: feste Puffer, Zugriff unter Spinlock
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
char apPw[devicepw::kLength + 1] = "";
char otaPw[64] = "";
bool otaFixed = false;

const char *kNamespace = "devpw";
const char *kKeyAp = "pw";    // Schlüssel aus der Zeit, als es nur ein Geräte-Passwort gab: Hotspot-Passwort bleibt
const char *kKeyOta = "ota";

uint32_t rnd() { return esp_random(); }

void setPw(char *dst, size_t size, const char *src) {
  portENTER_CRITICAL(&mux);
  strlcpy(dst, src, size);
  portEXIT_CRITICAL(&mux);
}

String getPw(const char *src, size_t size) {
  char copy[64];
  portENTER_CRITICAL(&mux);
  strlcpy(copy, src, size < sizeof(copy) ? size : sizeof(copy));
  portEXIT_CRITICAL(&mux);
  return String(copy);
}

// Gespeichertes Passwort laden oder neu erzeugen
void loadOrCreate(Preferences &prefs, const char *key, char *dst, size_t size, const char *what) {
  String stored = prefs.getString(key, "");
  if (stored.length() < devicepw::kMinLength || stored.length() >= size) {
    char fresh[devicepw::kLength + 1];
    devicepw::generate(rnd, fresh, sizeof(fresh));
    prefs.putString(key, fresh);
    stored = fresh;
    logPrintf("[BOOT] %s erzeugt\n", what);
  }
  setPw(dst, size, stored.c_str());
}

void regenerate(const char *key, char *dst, size_t size) {
  char fresh[devicepw::kLength + 1];
  devicepw::generate(rnd, fresh, sizeof(fresh));
  Preferences prefs;
  prefs.begin(kNamespace, false);
  prefs.putString(key, fresh);
  prefs.end();
  setPw(dst, size, fresh);
}

}  // namespace

void devicePwBegin() {
  Preferences prefs;
  prefs.begin(kNamespace, false);
  loadOrCreate(prefs, kKeyAp, apPw, sizeof(apPw), "Hotspot-Passwort");
  otaFixed = devicepw::compiledUsable(OTA_PASSWORD);
  if (otaFixed) {
    setPw(otaPw, sizeof(otaPw), OTA_PASSWORD);
  } else {
    loadOrCreate(prefs, kKeyOta, otaPw, sizeof(otaPw), "OTA-Passwort");
  }
  prefs.end();
}

String hotspotPassword() { return getPw(apPw, sizeof(apPw)); }

String otaPassword() { return getPw(otaPw, sizeof(otaPw)); }

bool otaPasswordFixed() { return otaFixed; }

void hotspotPasswordRegenerate() {
  regenerate(kKeyAp, apPw, sizeof(apPw));
  logPrintln("[NET] Hotspot-Passwort neu erzeugt");
}

bool otaPasswordRegenerate() {
  if (otaFixed) {
    return false;
  }
  regenerate(kKeyOta, otaPw, sizeof(otaPw));
  logPrintln("[OTA] OTA-Passwort neu erzeugt");
  return true;
}
