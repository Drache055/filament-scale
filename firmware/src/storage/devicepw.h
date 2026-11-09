#pragma once
#include <Arduino.h>

// Geräte-Passwort: schützt das Einrichtungs-WLAN (Hotspot) und das WLAN-Update (OTA).
//
// - Steht in secrets.h ein eigenes OTA_PASSWORD (mindestens 8 Zeichen, nicht der Platzhalter), gilt dieses.
//   So bleibt das Verhalten für Entwickler-Builds unverändert.
// - Sonst erzeugt die Waage beim ersten Start ein zufälliges Passwort (10 Zeichen), speichert es im NVS und zeigt es
//   auf dem Display an (Home im Hotspot-Betrieb, Menü → Einstellungen → WiFi). Das ist der Weg für fertige Images,
//   die ohne eigene secrets.h gebaut werden (zum Beispiel der Web-Flasher).
void devicePwBegin();  // einmal in setup(), vor dem net-Task

const String &devicePassword();   // immer mindestens 8 Zeichen
bool devicePasswordGenerated();   // true: vom Gerät erzeugt, false: aus secrets.h
