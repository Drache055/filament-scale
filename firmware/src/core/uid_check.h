#pragma once

// Prüft eine Tag-UID: nur Hex-Zeichen (0-9, A-F, a-f), gerade Anzahl, 4 bis 10 Byte (8 bis 20 Zeichen).
// Schützt vor Zeichen, die in JSON-Body oder URL von Spoolman-Anfragen landen würden.
bool isValidTagUid(const char *uid);
