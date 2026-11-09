#include "devicepw.h"
#include "util/log.h"

#include <Preferences.h>
#include <esp_random.h>

#include "secrets.h"

namespace {

String password;
bool generated = false;

// Ohne verwechselbare Zeichen (0/O, 1/I/L), damit sich das Passwort vom Display gut abtippen lässt
const char kAlphabet[] = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";
constexpr size_t kLength = 10;
constexpr size_t kMinLength = 8;

bool compiledPasswordUsable() {
  const char *pw = OTA_PASSWORD;
  return strlen(pw) >= kMinLength && strcmp(pw, "bitte-aendern") != 0;
}

String randomPassword() {
  String out;
  const size_t n = sizeof(kAlphabet) - 1;
  for (size_t i = 0; i < kLength; i++) {
    // Rest-Verzerrung durch den Modulo ist bei 31 Zeichen und 32 Bit Zufall vernachlässigbar
    out += kAlphabet[esp_random() % n];
  }
  return out;
}

}  // namespace

void devicePwBegin() {
  if (compiledPasswordUsable()) {
    password = OTA_PASSWORD;
    generated = false;
    return;
  }
  Preferences prefs;
  prefs.begin("devpw", false);
  String stored = prefs.getString("pw", "");
  if (stored.length() < kMinLength) {
    stored = randomPassword();
    prefs.putString("pw", stored);
    logPrintln("[BOOT] Geräte-Passwort erzeugt (siehe Display)");
  }
  prefs.end();
  password = stored;
  generated = true;
}

const String &devicePassword() { return password; }

bool devicePasswordGenerated() { return generated; }
