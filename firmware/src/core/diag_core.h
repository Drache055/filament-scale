#pragma once
#include <cstdint>

// Selbstdiagnose (CLAUDE.md 12.7): macht aus den Zuständen der Komponenten je einen Befund mit Stufe und Code.
// Reine Logik ohne Hardwarezugriff, nativ getestet. Der Code (z. B. "no_data") wird auf der Webseite in
// Klartext und Lösungstipp übersetzt. Sensor-unabhängig: nur "liefert Daten" zählt, nicht welcher Wandler.
enum class DiagComp : uint8_t { Sensor, Nfc, Wifi, Spoolman, Queue };
enum class DiagLevel : uint8_t { Ok, Info, Warn, Error };

struct DiagInput {
  uint32_t uptimeS = 0;
  bool simple = false;        // Einfache Waage: kein NFC, kein Spoolman
  bool sensorValid = false;   // Wägesensor liefert gültige Werte
  bool calibrated = false;
  bool overload = false;
  bool nfcReady = false;      // PN532 antwortet
  bool wifiConnected = false;
  bool wifiHasCreds = false;  // Zugangsdaten gespeichert
  int wifiRssi = 0;           // dBm
  bool smChecked = false;     // mindestens eine Prüfung gelaufen
  bool smReachable = false;
  int smHttp = 0;             // HTTP-Status der letzten Prüfung, 0 = keine Verbindung
  int queueCount = 0;
  uint32_t queueDropped = 0;
};

struct DiagItem {
  DiagComp comp;
  DiagLevel level;
  const char *code;
};

#define DIAG_MAX_ITEMS 5
#define DIAG_STARTUP_S 30  // so lange gelten fehlende Daten als "startet noch"

// Schreibt höchstens DIAG_MAX_ITEMS Befunde nach out, liefert die Anzahl.
int diagRun(const DiagInput &in, DiagItem *out);
const char *diagCompName(DiagComp c);    // "sensor", "nfc", "wifi", "spoolman", "queue"
const char *diagLevelName(DiagLevel l);  // "ok", "info", "warn", "error"
