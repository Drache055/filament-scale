#pragma once
#include <Arduino.h>

// Betriebsart der Waage (NVS, Namespace "devmode"), umschaltbar nur vom Admin auf der Webseite.
// - Smarte Waage (Standard): Spulen- und Box-Tags, Anmeldung per Benutzer-Tag, Spoolman.
// - Einfache Waage: nur "Filament wiegen" (Spule oder Box + Spule), keine Anmeldung, kein Spoolman, kein NFC nötig.
//   Benutzer-Tags melden nicht an, Spulen werden nicht nachgeschlagen, der Spoolman-Client ruht.
void devModeBegin();  // vor allen Tasks, die die Betriebsart abfragen

bool devModeSimple();
void devModeSetSimple(bool simple);

// Leerspulengewicht der einfachen Waage in g (zuletzt eingestellt, Start WEIGH_DEFAULT_EMPTY_G)
int devModeEmptySpoolG();
void devModeSetEmptySpoolG(int grams);
