#include "bootcheck.h"

#include <Arduino.h>
#include <esp_ota_ops.h>

#include "core/boot_check_core.h"
#include "util/log.h"
#include "wifi.h"

#ifdef CONFIG_APP_ROLLBACK_ENABLE
// Schwache Funktion des Arduino-Cores (esp32-hal-misc.c): true = die App bestätigt sich selbst
extern "C" bool verifyRollbackLater() { return true; }
#endif

namespace {
volatile bool pending = false;
volatile bool setupDone = false;
char rolledBack[32] = "";

void markValid(const char *why) {
#ifdef CONFIG_APP_ROLLBACK_ENABLE
  esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
  logPrintf("[BOOT] Neue Firmware bestätigt (%s)%s\n", why, err == ESP_OK ? "" : ", Fehler beim Speichern");
#endif
  pending = false;
}
}  // namespace

void bootCheckBegin() {
  const esp_partition_t *bad = esp_ota_get_last_invalid_partition();
  esp_app_desc_t desc;
  if (bad && esp_ota_get_partition_description(bad, &desc) == ESP_OK) {
    strlcpy(rolledBack, desc.version, sizeof(rolledBack));
    logPrintf("[BOOT] Zuletzt verworfene Firmware: %s (%s)\n", rolledBack, bad->label);
  }
#ifdef CONFIG_APP_ROLLBACK_ENABLE
  esp_ota_img_states_t state;
  if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &state) == ESP_OK &&
      state == ESP_OTA_IMG_PENDING_VERIFY) {
    pending = true;
    logPrintf("[BOOT] Neue Firmware, wird geprüft (%lu s Laufzeit, WLAN)\n", BOOT_CHECK_MIN_MS / 1000);
  }
#else
  logPrintln("[BOOT] Rollback im Bootloader nicht aktiv");
#endif
}

void bootCheckSetupDone() { setupDone = true; }

void bootCheckLoop() {
  if (!pending) {
    return;
  }
  BootCheckInput in;
  in.uptimeMs = millis();
  in.setupDone = setupDone;
  in.wifiConfigured = wifiHasCredentials();
  in.wifiConnected = wifiConnected();
  switch (bootCheckDecide(in)) {
    case BootVerdict::Wait:
      break;
    case BootVerdict::Valid:
      markValid(in.wifiConnected ? "WLAN verbunden" : "Hotspot");
      break;
    case BootVerdict::Invalid:
      logPrintf("[BOOT] Neue Firmware fehlerhaft (%s), Rückkehr zur vorigen\n", setupDone ? "kein WLAN" : "Start hängt");
      delay(200);
      esp_ota_mark_app_invalid_rollback_and_reboot();
      // Nur bei Fehler zurück (keine vorige Firmware vorhanden): weiterlaufen statt endlos neu zu starten
      markValid("keine vorige Firmware");
      break;
  }
}

void bootCheckConfirm() {
  if (pending) {
    markValid("Neustart über die Webseite");
  }
}

bool bootCheckPending() { return pending; }

const char *bootCheckRolledBack() { return rolledBack; }
