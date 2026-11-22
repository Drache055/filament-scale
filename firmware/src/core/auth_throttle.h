#pragma once
#include <stdint.h>

// Fehlversuch-Sperre für die Web-Anmeldung, getrennt pro Client-IP (IPv4 als uint32_t).
// Nach AUTH_FAILS_PER_LOCK Fehlversuchen ist die IP gesperrt: zuerst AUTH_LOCK_BASE_MS,
// jede weitere Sperre doppelt so lang, höchstens AUTH_LOCK_MAX_MS. Eine erfolgreiche Anmeldung
// löscht den Eintrag; nach AUTH_IDLE_FORGET_MS ohne Fehlversuch fängt die IP wieder von vorn an.
// Die Tabelle ist begrenzt: Ist sie voll, wird der am längsten ruhige Eintrag ersetzt
// (bevorzugt einer, der gerade nicht gesperrt ist). Zeiten in ms (millis(), Überlauf erlaubt).

constexpr int AUTH_THROTTLE_SLOTS = 16;
constexpr uint8_t AUTH_FAILS_PER_LOCK = 5;
constexpr uint32_t AUTH_LOCK_BASE_MS = 30000;            // 30 s
constexpr uint32_t AUTH_LOCK_MAX_MS = 15UL * 60 * 1000;  // 15 min
constexpr uint32_t AUTH_IDLE_FORGET_MS = 60UL * 60 * 1000;  // 1 h

struct AuthThrottleEntry {
  uint32_t ip = 0;
  uint32_t lastFail = 0;
  uint32_t blockedUntil = 0;
  uint8_t fails = 0;
  uint8_t locks = 0;  // Anzahl bisheriger Sperren (bestimmt die nächste Dauer)
  bool used = false;
};

struct AuthThrottle {
  AuthThrottleEntry slots[AUTH_THROTTLE_SLOTS];
};

// Restliche Sperrzeit in ms für diese IP, 0 = nicht gesperrt
uint32_t authThrottleBlockedMs(const AuthThrottle &t, uint32_t ip, uint32_t now);
// Fehlversuch zählen. Rückgabe: Dauer der dadurch neu ausgelösten Sperre in ms, sonst 0
uint32_t authThrottleFail(AuthThrottle &t, uint32_t ip, uint32_t now);
// Erfolgreiche Anmeldung: Eintrag der IP löschen
void authThrottleSuccess(AuthThrottle &t, uint32_t ip);
// Dauer der n-ten Sperre (n ab 1)
uint32_t authThrottleLockMs(uint8_t n);
