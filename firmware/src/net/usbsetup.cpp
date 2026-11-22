#include "usbsetup.h"

#include <vector>

#include "config.h"
#include "core/usb_setup_core.h"
#include "net/websession.h"
#include "net/wifi.h"
#include "storage/admin.h"
#include "util/log.h"

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

using namespace usbsetup;

namespace {

constexpr uint32_t kConnectTimeoutMs = 20000;  // die Webseite wartet 40 s auf die Antwort
constexpr uint32_t kScanWaitMs = 15000;        // Scan dauert einige Sekunden, die Webseite wartet 20 s

Parser parser;

void send(const std::vector<uint8_t> &p) {
  Serial.write(p.data(), p.size());  // ein Aufruf, damit keine Logzeile in das Paket gerät
  Serial.flush();
}

// Passwörter im Speicher überschreiben, bevor er freigegeben wird
void wipe(std::string &s) {
  for (char &c : s) c = 0;
  s.clear();
}
void wipe(std::vector<uint8_t> &v) {
  for (uint8_t &c : v) c = 0;
  v.clear();
}

void handleHello() {
  send(buildResult(CmdHello, {"Filament Scale", FW_VERSION, "ESP32-S3", wifiSavedSsid().c_str(),
                              adminIsDefaultPassword() ? "1" : "0"}));
}

// Der Scan läuft im net-Task (net/wifi), hier nur warten. Bei Fehler oder Zeitüberschreitung kommt eine leere Liste.
void handleScan() {
  std::vector<WifiNet> nets;
  uint32_t start = millis();
  ScanState st;
  while ((st = wifiScanRequest(&nets)) == ScanState::Pending || st == ScanState::Running) {
    if (millis() - start > kScanWaitMs) {
      break;
    }
    delay(100);
  }
  if (st != ScanState::Done) {
    logPrintln("[SETUP] Netzwerksuche ohne Ergebnis");
  }
  for (const WifiNet &n : nets) {
    send(buildResult(CmdScan, {n.ssid.c_str(), String(n.rssi).c_str(), n.open ? "NO" : "YES"}));
  }
  send(buildResult(CmdScan, {}));
}

void handleSetup(std::vector<std::string> &s) {
  if (s.size() != 3) {
    send(buildError(ErrInvalidPacket));
    return;
  }
  uint8_t err = validateSetup(s[0], s[1], s[2], ADMIN_DEFAULT_PASSWORD, ADMIN_PASSWORD_MIN, ADMIN_PASSWORD_MAX);
  if (err != ErrNone) {
    logPrintf("[SETUP] USB-Einrichtung abgelehnt (Fehler 0x%02X)\n", err);
    send(buildError(err));
    return;
  }
  send(buildState(StateProvisioning));
  String error;
  if (!wifiProvision(s[0].c_str(), s[1].c_str(), kConnectTimeoutMs, error)) {
    send(buildError(ErrConnect));
    send(buildState(StateReady));
    return;
  }
  // Admin-Passwort erst nach erfolgreicher WLAN-Verbindung, damit ein Fehlversuch nichts halb einrichtet
  if (!adminSetPassword(s[2].c_str(), error)) {
    send(buildError(ErrBadAdminPw));
    send(buildState(StateReady));
    return;
  }
  webSessionsDropAdmin();  // Anmeldungen mit dem alten Admin-Passwort enden
  logPrintln("[SETUP] USB-Einrichtung abgeschlossen");
  send(buildState(StateProvisioned));
  send(buildResult(CmdSetup, {(String("http://") + wifiIp() + "/").c_str()}));
}

}  // namespace

void usbSetupFeed(uint8_t b, String &text) {
  Packet pkt;
  std::string pass;
  Parser::Result r = parser.feed(b, pkt, pass);
  for (char c : pass) {
    text += c;
  }
  if (r == Parser::BadChecksum) {
    send(buildError(ErrInvalidPacket));
    return;
  }
  if (r != Parser::Got) {
    return;
  }
  uint8_t cmd = 0;
  std::vector<std::string> strings;
  if (pkt.type != TypeRpc || !parseRpc(pkt.data, cmd, strings)) {
    wipe(pkt.data);
    send(buildError(ErrInvalidPacket));
    return;
  }
  wipe(pkt.data);
  switch (cmd) {
    case CmdHello:
      handleHello();
      break;
    case CmdScan:
      handleScan();
      break;
    case CmdSetup:
      handleSetup(strings);
      break;
    default:
      break;  // Improv-Standardbefehle (0x01 bis 0x04) bewusst unbeantwortet, siehe core/usb_setup_core.h
  }
  for (std::string &s : strings) wipe(s);
}
