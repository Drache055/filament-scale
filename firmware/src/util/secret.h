#pragma once
#include <Arduino.h>

// Hilfen für Passwörter und PINs: Zufall, Salted-SHA-256, Vergleich ohne frühen Abbruch.
String secretRandomHex(size_t bytes);                         // Großbuchstaben-Hex, 2 Zeichen je Byte
String secretHash(const String &salt, const char *secret);    // SHA-256 über salt + secret, 64 Hex-Zeichen (klein)
bool secretEqual(const String &a, const String &b);           // konstante Laufzeit bei gleicher Länge
