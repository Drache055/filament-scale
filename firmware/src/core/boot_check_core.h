#pragma once
#include <cstdint>

// Automatischer Rollback nach einem Update (CLAUDE.md 12.6). Reine Logik ohne Hardwarezugriff.
// Nach einem Update startet die neue Firmware im Zustand "ungeprüft" (ESP-IDF: ESP_OTA_IMG_PENDING_VERIFY).
// Sie gilt als gut, wenn setup() durchgelaufen ist, sie BOOT_CHECK_MIN_MS ohne Neustart läuft und das WLAN steht
// (ohne gespeicherte Zugangsdaten reicht der Hotspot). Steht das WLAN nach BOOT_CHECK_WIFI_MS nicht oder hängt setup(),
// wird sie verworfen und die vorige Firmware startet. Ein Absturz oder Neustart vor der Bestätigung führt der
// Bootloader von selbst auf die vorige Firmware zurück.

#define BOOT_CHECK_MIN_MS  60000UL   // so lange muss die neue Firmware ohne Neustart laufen
#define BOOT_CHECK_WIFI_MS 180000UL  // spätestens dann muss das WLAN stehen, sonst Rollback

enum class BootVerdict : uint8_t { Wait, Valid, Invalid };

struct BootCheckInput {
  uint32_t uptimeMs = 0;
  bool setupDone = false;       // setup() ist durchgelaufen
  bool wifiConfigured = false;  // WLAN-Zugangsdaten gespeichert
  bool wifiConnected = false;
};

BootVerdict bootCheckDecide(const BootCheckInput &in);
