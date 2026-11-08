#pragma once
#include <Arduino.h>
// Ohne Speicher: liefert immer den Standardwert.
class Preferences {
 public:
  bool begin(const char *, bool = false) { return true; }
  void end() {}
  uint8_t getUChar(const char *, uint8_t def = 0) { return def; }
  size_t putUChar(const char *, uint8_t) { return 1; }
};
