#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Einrichtung per USB (CLAUDE.md 12.1): Die Einrichtungsmaske der Webseite (Web Serial) schickt WLAN und Admin-Passwort
// in Paketen im Improv-Serial-Rahmen: "IMPROV", Version 1, Typ, Länge, Daten, Prüfsumme (Summe aller Bytes davor, 8 Bit).
// Nur eigene Befehle (ab 0x80). Die Improv-Standardbefehle beantwortet die Waage absichtlich nicht, damit der Dialog von
// ESP Web Tools nicht zusätzlich nach dem WLAN fragt. Reine Logik ohne Hardwarezugriff, nativ getestet.

namespace usbsetup {

constexpr uint8_t kVersion = 1;

enum Type : uint8_t { TypeState = 0x01, TypeError = 0x02, TypeRpc = 0x03, TypeResult = 0x04 };

enum State : uint8_t { StateReady = 0x02, StateProvisioning = 0x03, StateProvisioned = 0x04 };

enum Error : uint8_t {
  ErrNone = 0x00,
  ErrInvalidPacket = 0x01,
  ErrUnknownCommand = 0x02,
  ErrConnect = 0x03,     // WLAN-Verbindung gescheitert
  ErrBadSsid = 0x80,     // SSID leer oder länger als 32 Zeichen
  ErrBadWifiPw = 0x81,   // WLAN-Passwort nicht leer und nicht 8 bis 63 Zeichen
  ErrBadAdminPw = 0x82,  // Admin-Passwort ungültig oder noch das Standardpasswort
  ErrUnknown = 0xFF,
};

enum Command : uint8_t {
  CmdHello = 0x80,  // Antwort: Ergebnis mit [Name, Firmware, Chip, gespeicherte SSID, "1"/"0" Standardpasswort aktiv]
  CmdSetup = 0x81,  // Daten: [SSID, WLAN-Passwort, Admin-Passwort]; Antwort: Zustand + Ergebnis mit [URL]
  CmdScan = 0x82,   // Antwort: je Netz ein Ergebnis [SSID, RSSI, "YES"/"NO" geschützt], zum Schluss ein leeres
};

struct Packet {
  uint8_t type = 0;
  std::vector<uint8_t> data;
};

// Byteweiser Empfänger. Alles, was nicht zu einem Paket gehört, geht an passthrough (serielle Textbefehle).
class Parser {
 public:
  enum Result { None, Got, BadChecksum };
  Result feed(uint8_t b, Packet &out, std::string &passthrough);

 private:
  std::vector<uint8_t> buf_;
};

// RPC-Daten: [Befehl, Länge, Nutzdaten]; Nutzdaten = Folge von [Länge, Bytes]
bool parseRpc(const std::vector<uint8_t> &data, uint8_t &command, std::vector<std::string> &strings);

std::vector<uint8_t> buildPacket(uint8_t type, const std::vector<uint8_t> &data);
std::vector<uint8_t> buildState(uint8_t state);
std::vector<uint8_t> buildError(uint8_t error);
std::vector<uint8_t> buildResult(uint8_t command, const std::vector<std::string> &strings);
std::vector<uint8_t> buildRpc(uint8_t command, const std::vector<std::string> &strings);  // Gegenstück, für Tests

// Prüfungen wie in wifiSaveCredentials bzw. adminSetPassword; das Standardpasswort ist hier nicht erlaubt.
uint8_t validateSetup(const std::string &ssid, const std::string &wifiPw, const std::string &adminPw,
                      const char *defaultAdminPw, size_t adminMin, size_t adminMax);

}  // namespace usbsetup
