#include <Arduino.h>
#include "util/log.h"
#include <WiFi.h>
#include <esp_ota_ops.h>

#include "nfc/nfc.h"
#include "app/session.h"
#include "bus/i2c_bus.h"
#include "net/fwupdate.h"
#include "net/ota.h"
#include "net/spoolman.h"
#include "net/usbsetup.h"
#include "net/webserver.h"
#include "net/wifi.h"
#include "scale/scale.h"
#include <LittleFS.h>
#include "storage/admin.h"
#include "storage/devicepw.h"
#include "storage/devmode.h"
#include "storage/catalog.h"
#include "storage/drying.h"
#include "storage/queue.h"
#include "storage/users.h"
#include "ui/ui.h"

// WLAN und OTA laufen im net-Task auf Kern 0 (siehe CLAUDE.md 8.1).
static void netTask(void *) {
  wifiBegin();
  bool ipLogged = false;
  for (;;) {
    if (wifiConnected() && !ipLogged) {
      logPrintf("[NET] Verbunden, IP %s\n", WiFi.localIP().toString().c_str());
      ipLogged = true;
    } else if (!wifiConnected()) {
      ipLogged = false;
    }
    wifiLoop();
    otaLoop();
    webLoop();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void setup() {
  Serial.begin(115200);
  logBegin();
  delay(1000);
  logPrintln("[BOOT] Filamentwaage");
  logPrintf("[BOOT] Flash %u MB, PSRAM %u MB\n", ESP.getFlashChipSize() / (1024 * 1024),
                ESP.getPsramSize() / (1024 * 1024));
  logPrintf("[BOOT] Laufende Partition: %s\n", esp_ota_get_running_partition()->label);

  devicePwBegin();  // vor dem net-Task: Hotspot und OTA brauchen das Geräte-Passwort
  adminBegin();  // vor dem net-Task, die Webseite fragt die Sperre ab
  devModeBegin();  // vor dem net-Task und session/spoolman/ui: alle fragen die Betriebsart ab
  fwUpdateBegin();  // vor dem net-Task, die Webseite fragt den Update-Zustand ab
  xTaskCreatePinnedToCore(netTask, "net", 12288, nullptr, 1, nullptr, 0);
  if (!LittleFS.begin(true)) {
    logPrintln("[BOOT] LittleFS konnte nicht gemountet werden");
  }
  catalogBegin();
  dryingBegin();
  queueBegin();
  usersBegin();
  i2cBusBegin();  // vor allen Tasks, die den Bus nutzen (Touch, NFC)
  scaleBegin();
  spoolmanBegin();
  nfcBegin();
  sessionBegin();
  uiStart();
}

// Serielle Notbedienung (USB), z. B. wenn das WLAN nicht erreichbar ist:
//   wifi set <SSID>|<Passwort>   Zugangsdaten speichern und neu starten (Passwort darf fehlen)
//   wifi reset                   Zugangsdaten löschen und neu starten (Hotspot)
//   wifi status | admin reset (Standardpasswort, Sperre an, Admin-Tag ungültig) | reboot
static void handleSerialCommand(String line) {
  line.trim();
  String lower = line;
  lower.toLowerCase();
  if (lower.startsWith("wifi set ")) {
    String rest = line.substring(9);
    int bar = rest.indexOf('|');
    String ssid = bar < 0 ? rest : rest.substring(0, bar);
    String pass = bar < 0 ? "" : rest.substring(bar + 1);
    String error;
    if (wifiSaveCredentials(ssid.c_str(), pass.c_str(), error)) {
      logPrintln("[CMD] Gespeichert, Neustart");
      delay(300);
      ESP.restart();
    } else {
      logPrintf("[CMD] Fehler: %s\n", error.c_str());
    }
  } else if (lower == "wifi reset") {
    wifiResetCredentials();
    logPrintln("[CMD] Gelöscht, Neustart");
    delay(300);
    ESP.restart();
  } else if (lower == "wifi status") {
    logPrintf("[CMD] gespeichert '%s', verbunden %s, Hotspot %s\n", wifiSavedSsid().c_str(), wifiConnected() ? "ja" : "nein",
              wifiApActive() ? "an" : "aus");
  } else if (lower == "admin reset") {
    adminReset();
  } else if (lower == "reboot") {
    ESP.restart();
  } else if (line.length()) {
    logPrintln("[CMD] Befehle: wifi set <SSID>|<Passwort>, wifi reset, wifi status, admin reset, reboot");
  }
}

void loop() {
  static String line;
  while (Serial.available()) {
    String text;  // Pakete der USB-Einrichtung (Webseite) herausfiltern, der Rest sind Textbefehle
    usbSetupFeed((uint8_t)Serial.read(), text);
    for (size_t i = 0; i < text.length(); i++) {
      char c = text[i];
      if (c == '\n' || c == '\r') {
        if (line.length()) {
          handleSerialCommand(line);
        }
        line = "";
      } else if (line.length() < 200) {
        line += c;
      }
    }
  }
  vTaskDelay(pdMS_TO_TICKS(50));
}
