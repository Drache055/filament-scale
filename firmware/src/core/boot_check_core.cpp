#include "boot_check_core.h"

BootVerdict bootCheckDecide(const BootCheckInput &in) {
  if (in.uptimeMs < BOOT_CHECK_MIN_MS) {
    return BootVerdict::Wait;
  }
  if (!in.setupDone) {
    return in.uptimeMs >= BOOT_CHECK_WIFI_MS ? BootVerdict::Invalid : BootVerdict::Wait;
  }
  if (!in.wifiConfigured || in.wifiConnected) {
    return BootVerdict::Valid;  // ohne Zugangsdaten läuft der Einrichtungs-Hotspot, mehr lässt sich nicht prüfen
  }
  return in.uptimeMs >= BOOT_CHECK_WIFI_MS ? BootVerdict::Invalid : BootVerdict::Wait;
}
