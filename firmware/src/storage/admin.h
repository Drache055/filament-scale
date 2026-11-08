#pragma once
#include <Arduino.h>

// Admin-Zugang (NVS). Der Admin darf alle Einstellungen ändern, Benutzer anlegen und die Sperre abschalten.
// Ist die Sperre aus, darf jeder Einstellungen ändern (Gerät und Webseite). Das Passwort liegt nur als Salted-SHA-256-Hash im NVS.
// Anmeldung am Gerät: Passwort per Bildschirmtastatur oder Admin-NFC-Tag. Die Anmeldung endet nach ADMIN_TIMEOUT_MS.
void adminBegin();

bool adminLockEnabled();
void adminSetLock(bool on);

bool adminCheckPassword(const char *password);
bool adminSetPassword(const char *password, String &error);
bool adminIsDefaultPassword();  // noch das Auslieferungspasswort

// Geräte-PIN: Anmeldung am Gerät mit dem Ziffernblock (die Bildschirmtastatur ist auf dem kleinen Display nicht nutzbar).
// 4 bis 6 Ziffern, am Anfang ADMIN_DEFAULT_PIN. Das Passwort gilt nur noch für die Webseite.
bool adminCheckDevicePin(const char *pin);
bool adminSetDevicePin(const char *pin, String &error);
bool adminIsDefaultDevicePin();

// Admin-Tag: {"type":"admin","token":"..."}. Es gilt immer nur der zuletzt erzeugte Token.
String adminNewTagToken();  // erzeugt und speichert einen neuen Token (der alte Tag wird ungültig)
bool adminTagMatches(const char *token);
bool adminHasTag();

// Notfall (serieller Befehl "admin reset"): Standardpasswort, Sperre an, Admin-Tag ungültig
void adminReset();

// Anmeldung am Gerät
void adminSessionStart();
void adminSessionEnd();
bool adminSessionActive();  // false, sobald die Anmeldung abgelaufen ist
void adminSessionTouch();   // Bedienung verlängert die Anmeldung
bool adminAllowed();        // Sperre aus oder Anmeldung aktiv
