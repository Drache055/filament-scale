#pragma once
#include <Arduino.h>

#include <vector>

#include "core/wifi_core.h"

// WLAN-Verbindung. Zugangsdaten liegen im NVS (Weboberfläche oder serielle Konsole). Beim allerersten Start nach einer
// Firmware ohne diese Funktion werden WIFI_SSID/WIFI_PASSWORD aus secrets.h einmalig übernommen.
// Sind keine Zugangsdaten gespeichert, startet sofort ein Hotspot (WIFI_AP_SSID, Passwort = Hotspot-Passwort aus
// storage/devicepw.h, immer geschützt) mit Captive Portal: Die Einrichtungsseite öffnet sich beim Verbinden von selbst, sonst unter
// http://192.168.4.1. Gelingt die Verbindung mit gespeicherten Daten nicht (WIFI_AP_FALLBACK_MS), startet der Hotspot ebenfalls.
// Mit gespeicherten Daten versucht der net-Task ohne Ende neu zu verbinden (ReconnectPolicy, wachsende Pausen bis 60 s).
void wifiBegin();
void wifiLoop();  // regelmäßig aus dem net-Task aufrufen
bool wifiConnected();

String wifiSsid();
String wifiIp();
int wifiRssi();  // dBm, nur sinnvoll wenn verbunden

// Zugangsdaten
bool wifiHasCredentials();
String wifiSavedSsid();               // "" = keine gespeichert (Hotspot)
bool wifiSaveCredentials(const char *ssid, const char *password, String &error);
// Einrichtung per USB: speichert die Zugangsdaten sofort und verbindet ohne Neustart. Blockiert den Aufrufer bis
// timeoutMs; die Verbindung baut der net-Task auf (wifiLoop), nicht der Aufrufer. Nicht aus dem net-Task aufrufen.
// Rejected = nichts gespeichert (error), Connected = verbunden, Saved = gespeichert, aber noch nicht verbunden (fail = Grund).
// Die Waage versucht es danach weiter, bis es klappt (Wiederverbindung, CLAUDE.md 12.1).
enum class ProvResult : uint8_t { Rejected, Connected, Saved };
ProvResult wifiProvision(const char *ssid, const char *password, uint32_t timeoutMs, String &error, WifiFail &fail);
void wifiResetCredentials();          // Zugangsdaten löschen, nach dem Neustart startet der Hotspot
// Netzwerksuche, blockiert nicht. Startet bei Bedarf einen Scan im net-Task, den sich alle Anfragen teilen.
// Pending/Running = später erneut fragen, Done = Liste in *nets (zusammengefasst, max. 30), Failed = Suche gescheitert.
ScanState wifiScanRequest(std::vector<WifiNet> *nets);

// Einrichtungs-WLAN
bool wifiApActive();
String wifiApSsid();
String wifiApIp();
void wifiStartAp();
void wifiStopAp();
void wifiApApplyPassword();  // laufenden Hotspot auf das aktuelle Hotspot-Passwort umstellen 
