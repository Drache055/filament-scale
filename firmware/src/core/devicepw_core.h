#pragma once
#include <cstddef>
#include <cstdint>

// Geräte-Passwörter (CLAUDE.md 13.2): Hotspot-Passwort und OTA-Passwort. Reine Logik ohne Hardwarezugriff, nativ getestet.

namespace devicepw {

constexpr size_t kLength = 10;    // Länge erzeugter Passwörter
constexpr size_t kMinLength = 8;  // WPA2 verlangt mindestens 8 Zeichen
constexpr const char *kPlaceholder = "bitte-aendern";  // Platzhalter aus secrets.h.example

// Ohne verwechselbare Zeichen (0/O, 1/I/L), damit sich das Passwort vom Display gut abtippen lässt
constexpr const char kAlphabet[] = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";

// Eigenes OTA_PASSWORD aus secrets.h: mindestens 8 Zeichen und nicht der Platzhalter
bool compiledUsable(const char *pw);

// Schreibt kLength zufällige Zeichen aus kAlphabet plus '\0' nach out (Größe mindestens kLength + 1).
// rnd liefert 32 Bit Zufall (am Gerät esp_random).
void generate(uint32_t (*rnd)(), char *out, size_t outSize);

// Hotspot-Passwort auf Home zeigen? Nur bei der Ersteinrichtung (keine WLAN-Daten gespeichert). Läuft der Hotspot als
// Rückfall nach einem WLAN-Ausfall, steht dort nur ein Hinweis: Wer am Gerät vorbeikommt, soll nicht ins Netz der Waage
// und damit an die Admin-Rechte der Weboberfläche kommen.
inline bool showOnHome(bool hasCredentials) { return !hasCredentials; }

}  // namespace devicepw
