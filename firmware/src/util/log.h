#pragma once
#include <Arduino.h>

// Ausgabe über Serial UND in einen Ringpuffer (für die Weboberfläche). Zusätzlich werden die Meldungen
// des ESP-IDF (Wi-Fi, NVS, ...) mitgeschrieben.
void logBegin();
void logPrintf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void logPrintln(const char *msg);

// Hängt alle Zeilen mit Folgenummer > sinceSeq an out an (Format "[ssss.s] Text\n"). Gibt die höchste Folgenummer zurück.
uint32_t logRead(uint32_t sinceSeq, String &out, size_t maxBytes = 12000);
