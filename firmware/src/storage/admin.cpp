#include "admin.h"
#include "util/log.h"

#include <Preferences.h>
#include <freertos/semphr.h>

#include "config.h"
#include "util/secret.h"

namespace {

SemaphoreHandle_t mutex = nullptr;
bool lockOn = true;
bool defaultPassword = true;
String salt;   // 32 Hex-Zeichen
String hash;   // 64 Hex-Zeichen
String token;  // 32 Hex-Zeichen, leer = kein Admin-Tag
String pinSalt;  // Geräte-PIN: Salt (32 Hex) und Hash (64 Hex)
String pinHash;
bool defaultPin = true;

bool sessionOn = false;
uint32_t lastTouch = 0;

// Mutex gehalten
void storePassword(const char *password, bool isDefault) {
  salt = secretRandomHex(16);
  hash = secretHash(salt, password);
  defaultPassword = isDefault;
  Preferences prefs;
  prefs.begin("admin", false);
  prefs.putString("salt", salt);
  prefs.putString("hash", hash);
  prefs.putBool("isdef", isDefault);
  prefs.end();
}

// Mutex gehalten
void storeDevicePin(const char *pin, bool isDefault) {
  pinSalt = secretRandomHex(16);
  pinHash = secretHash(pinSalt, pin);
  defaultPin = isDefault;
  Preferences prefs;
  prefs.begin("admin", false);
  prefs.putString("psalt", pinSalt);
  prefs.putString("phash", pinHash);
  prefs.putBool("pdef", isDefault);
  prefs.end();
}

void saveLock() {
  Preferences prefs;
  prefs.begin("admin", false);
  prefs.putBool("lock", lockOn);
  prefs.end();
}

void saveToken() {
  Preferences prefs;
  prefs.begin("admin", false);
  prefs.putString("token", token);
  prefs.end();
}

}  // namespace

void adminBegin() {
  mutex = xSemaphoreCreateMutex();
  Preferences prefs;
  prefs.begin("admin", false);
  lockOn = prefs.getBool("lock", true);
  salt = prefs.getString("salt", "");
  hash = prefs.getString("hash", "");
  defaultPassword = prefs.getBool("isdef", true);
  token = prefs.getString("token", "");
  pinSalt = prefs.getString("psalt", "");
  pinHash = prefs.getString("phash", "");
  defaultPin = prefs.getBool("pdef", true);
  prefs.end();
  if (salt.length() != 32 || hash.length() != 64) {
    storePassword(ADMIN_DEFAULT_PASSWORD, true);
    logPrintln("[ADMIN] Standardpasswort gesetzt");
  }
  if (pinSalt.length() != 32 || pinHash.length() != 64) {
    storeDevicePin(ADMIN_DEFAULT_PIN, true);
    logPrintln("[ADMIN] Standard-Geräte-PIN gesetzt");
  }
  logPrintf("[ADMIN] Sperre %s%s%s\n", lockOn ? "an" : "AUS", defaultPassword ? ", Standardpasswort" : "",
            defaultPin ? ", Standard-PIN" : "");
}

bool adminLockEnabled() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  bool v = lockOn;
  xSemaphoreGive(mutex);
  return v;
}

void adminSetLock(bool on) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  lockOn = on;
  saveLock();
  xSemaphoreGive(mutex);
  logPrintf("[ADMIN] Sperre %s\n", on ? "an" : "AUS");
}

bool adminCheckPassword(const char *password) {
  if (!password) {
    return false;
  }
  xSemaphoreTake(mutex, portMAX_DELAY);
  bool ok = secretEqual(secretHash(salt, password), hash);
  xSemaphoreGive(mutex);
  return ok;
}

bool adminSetPassword(const char *password, String &error) {
  size_t len = password ? strlen(password) : 0;
  if (len < ADMIN_PASSWORD_MIN || len > ADMIN_PASSWORD_MAX) {
    error = String("Passwort: ") + ADMIN_PASSWORD_MIN + " bis " + ADMIN_PASSWORD_MAX + " Zeichen";
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    if ((uint8_t)password[i] < 0x20 || password[i] == ':') {
      error = "Passwort enthält ein unzulässiges Zeichen (Steuerzeichen oder ':')";
      return false;
    }
  }
  xSemaphoreTake(mutex, portMAX_DELAY);
  storePassword(password, strcmp(password, ADMIN_DEFAULT_PASSWORD) == 0);
  xSemaphoreGive(mutex);
  logPrintln("[ADMIN] Passwort geändert");
  return true;
}

bool adminCheckDevicePin(const char *pin) {
  if (!pin) {
    return false;
  }
  xSemaphoreTake(mutex, portMAX_DELAY);
  bool ok = secretEqual(secretHash(pinSalt, pin), pinHash);
  xSemaphoreGive(mutex);
  return ok;
}

bool adminSetDevicePin(const char *pin, String &error) {
  size_t len = pin ? strlen(pin) : 0;
  if (len < ADMIN_PIN_MIN || len > ADMIN_PIN_MAX) {
    error = String("Geräte-PIN: ") + ADMIN_PIN_MIN + " bis " + ADMIN_PIN_MAX + " Ziffern";
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    if (pin[i] < '0' || pin[i] > '9') {
      error = "Geräte-PIN: nur Ziffern";
      return false;
    }
  }
  xSemaphoreTake(mutex, portMAX_DELAY);
  storeDevicePin(pin, strcmp(pin, ADMIN_DEFAULT_PIN) == 0);
  xSemaphoreGive(mutex);
  logPrintln("[ADMIN] Geräte-PIN geändert");
  return true;
}

bool adminIsDefaultDevicePin() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  bool v = defaultPin;
  xSemaphoreGive(mutex);
  return v;
}

bool adminIsDefaultPassword() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  bool v = defaultPassword;
  xSemaphoreGive(mutex);
  return v;
}

String adminNewTagToken() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  token = secretRandomHex(16);
  saveToken();
  String out = token;
  xSemaphoreGive(mutex);
  return out;
}

bool adminTagMatches(const char *candidate) {
  if (!candidate || !candidate[0]) {
    return false;
  }
  xSemaphoreTake(mutex, portMAX_DELAY);
  bool ok = token.length() > 0 && secretEqual(token, String(candidate));
  xSemaphoreGive(mutex);
  return ok;
}

bool adminHasTag() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  bool v = token.length() > 0;
  xSemaphoreGive(mutex);
  return v;
}

void adminReset() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  storePassword(ADMIN_DEFAULT_PASSWORD, true);
  storeDevicePin(ADMIN_DEFAULT_PIN, true);
  lockOn = true;
  saveLock();
  token = "";
  saveToken();
  sessionOn = false;
  xSemaphoreGive(mutex);
  logPrintln("[ADMIN] Zurückgesetzt: Standardpasswort und Standard-PIN, Sperre an, Admin-Tag ungültig");
}

void adminSessionStart() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  sessionOn = true;
  lastTouch = millis();
  xSemaphoreGive(mutex);
  logPrintln("[ADMIN] Am Gerät angemeldet");
}

void adminSessionEnd() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  bool was = sessionOn;
  sessionOn = false;
  xSemaphoreGive(mutex);
  if (was) {
    logPrintln("[ADMIN] Am Gerät abgemeldet");
  }
}

bool adminSessionActive() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  if (sessionOn && millis() - lastTouch > ADMIN_TIMEOUT_MS) {
    sessionOn = false;
    logPrintln("[ADMIN] Anmeldung abgelaufen");
  }
  bool v = sessionOn;
  xSemaphoreGive(mutex);
  return v;
}

void adminSessionTouch() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  if (sessionOn) {
    lastTouch = millis();
  }
  xSemaphoreGive(mutex);
}

bool adminAllowed() { return !adminLockEnabled() || adminSessionActive(); }
