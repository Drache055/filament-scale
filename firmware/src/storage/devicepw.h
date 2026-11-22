#pragma once
#include <Arduino.h>

// Geräte-Passwörter (CLAUDE.md 13.2), zwei getrennte:
//
// - Hotspot-Passwort: schützt das Einrichtungs-WLAN. Wird beim ersten Start zufällig erzeugt (10 Zeichen, NVS) und
//   auf Home nur bei der Ersteinrichtung angezeigt (keine WLAN-Daten gespeichert). Sonst nur für den Admin: am Gerät unter
//   Menü → Einstellungen → WiFi, im Web unter WLAN. Wer im Hotspot ist, gilt in der Weboberfläche als Admin.
// - OTA-Passwort: schützt das Flashen per WLAN (ArduinoOTA). Steht in secrets.h ein eigenes OTA_PASSWORD (mindestens
//   8 Zeichen, nicht der Platzhalter), gilt dieses. Sonst eigenes Zufallspasswort im NVS, nie auf dem Display, nur für
//   den Admin im Web unter WLAN.
//
// Beide lassen sich im Web neu erzeugen (das OTA-Passwort nur, wenn es nicht aus secrets.h kommt).
void devicePwBegin();  // einmal in setup(), vor dem net-Task

String hotspotPassword();  // immer mindestens 8 Zeichen
String otaPassword();      // immer mindestens 8 Zeichen
bool otaPasswordFixed();   // true: aus secrets.h, lässt sich nicht neu erzeugen

// Neues Zufallspasswort erzeugen und speichern. Nur aus dem net-Task aufrufen. false: OTA-Passwort aus secrets.h.
// Ein laufender Hotspot bzw. ArduinoOTA übernimmt das neue Passwort nicht von selbst (siehe webserver.cpp, ota.h).
void hotspotPasswordRegenerate();
bool otaPasswordRegenerate();
