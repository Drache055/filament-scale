#include "ota.h"
#include "util/log.h"

#include <Arduino.h>
#include <ArduinoOTA.h>

#include "secrets.h"
#include "storage/devicepw.h"
#include "wifi.h"

static bool otaStarted = false;

static void otaStart() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(otaPassword().c_str());

  ArduinoOTA.onStart([]() {
    logPrintf("[OTA] Start (%s)\n",
                  ArduinoOTA.getCommand() == U_FLASH ? "Firmware" : "Dateisystem");
  });
  ArduinoOTA.onEnd([]() { logPrintln("[OTA] Fertig, Neustart"); });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    static uint8_t lastPct = 255;
    uint8_t pct = done * 100 / total;
    if (pct != lastPct && pct % 10 == 0) {
      lastPct = pct;
      logPrintf("[OTA] %u%%\n", pct);
    }
  });
  ArduinoOTA.onError([](ota_error_t err) { logPrintf("[OTA] Fehler %u\n", err); });

  ArduinoOTA.begin();
  logPrintf("[OTA] Bereit als '%s'\n", OTA_HOSTNAME);
}

void otaPasswordChanged() {
  // ArduinoOTA übernimmt ein neues Passwort nur im Leerlauf, also nicht während eines laufenden Uploads
  if (otaStarted) {
    ArduinoOTA.setPassword(otaPassword().c_str());
  }
}

void otaLoop() {
  if (!wifiConnected()) {
    return;
  }
  if (!otaStarted) {
    otaStart();
    otaStarted = true;
  }
  ArduinoOTA.handle();
}
