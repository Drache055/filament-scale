#pragma once
#include <Arduino.h>

#include "core/update_core.h"

// Online-Update (CLAUDE.md 12.6): fragt die neueste GitHub-Release des gewählten Kanals ab und flasht deren OTA-Image
// über HTTPS in den freien App-Slot. Eigener Task; die Webseite stößt an und fragt den Zustand ab.
// Schlägt etwas fehl, bleibt die laufende Firmware aktiv.

enum class FwUpdState : uint8_t { Idle, Checking, Ready, Downloading, Done, Error };

struct FwUpdStatus {
  FwUpdState state = FwUpdState::Idle;
  bool checked = false;     // mindestens eine erfolgreiche Prüfung
  bool available = false;   // neuere Version im gewählten Kanal vorhanden
  bool downgrade = false;   // neueste Version des Kanals ist älter als die laufende (z. B. Wechsel Test -> Stable)
  bool noRelease = false;   // keine passende Release im Kanal
  FwChannel channel = FwChannel::Stable;        // gewählter Kanal
  FwChannel latestChannel = FwChannel::Stable;  // Kanal der gefundenen Version
  char latest[32] = "";
  char pageUrl[320] = "";
  uint32_t checkedSecondsAgo = 0;
  uint32_t done = 0;        // geschriebene Bytes beim Download
  uint32_t total = 0;
  char error[96] = "";
};

void fwUpdateBegin();
const char *fwVersion();  // eingebettete Version, z. B. "v1.2" oder "dev"
FwUpdStatus fwUpdateStatus();
String fwUpdateNotes(bool *cut = nullptr);  // Release Notes der zuletzt gefundenen Version (Markdown), cut: gekürzt
bool fwUpdateCheckNow();  // false, wenn gerade ein Download läuft
bool fwUpdateSetChannel(FwChannel ch);  // speichert den Kanal (NVS) und sucht neu; false während eines Downloads
bool fwUpdateInstall(String &error);  // startet den Download der gefundenen Version (auch Rückschritt nach Kanalwechsel)
