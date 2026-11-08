#pragma once
#include "app/session.h"
#include "net/spoolman.h"
#include "nfc/nfc.h"
#include "scale/scale.h"
#include "storage/catalog.h"

// Vorgetäuschter Gerätezustand für die Screenshots. Die Stubs in stubs.cpp lesen daraus.
struct MockState {
  ScaleReading scale;
  NfcState nfc;
  NfcWrite nfcWrite = NfcWrite::Idle;
  SpoolmanResult lookup;
  SpoolmanResult write;
  SpoolmanStatus spoolman;
  SpoolListResult list;
  bool wifiUp = true;
  bool userLoggedIn = false;
  bool adminLoggedIn = false;
  bool lockEnabled = true;
  int queued = 0;
  uint8_t vcoms = 0x24;
  Lang lang = Lang::DE;
  uint32_t langGen = 0;
  uint32_t now = 100000;
};
extern MockState mock;
