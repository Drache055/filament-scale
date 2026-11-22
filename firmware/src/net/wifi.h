#pragma once
#include <Arduino.h>

// WLAN-Verbindung. Zugangsdaten liegen im NVS (Weboberfläche oder serielle Konsole). Beim allerersten Start nach einer
// Firmware ohne diese Funktion werden WIFI_SSID/WIFI_PASSWORD aus secrets.h einmalig übernommen.
// Sind keine Zugangsdaten gespeichert, startet sofort ein Hotspot (WIFI_AP_SSID, Passwort = Hotspot-Passwort aus
// storage/devicepw.h, immer geschützt) mit Captive Portal: Die Einrichtungsseite öffnet sich beim Verbinden von selbst, sonst unter
// http://192.168.4.1. Gelingt die Verbindung mit gespeicherten Daten nicht (WIFI_AP_FALLBACK_MS), startet der Hotspot ebenfalls.
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
// Einrichtung per USB: verbindet sofort (ohne Neustart) und speichert die Daten nur bei Erfolg. Blockiert bis timeoutMs.
// Scheitert die Verbindung, bleibt alles wie vorher (alte Zugangsdaten bzw. Hotspot).
bool wifiProvision(const char *ssid, const char *password, uint32_t timeoutMs, String &error);
void wifiResetCredentials();          // Zugangsdaten löschen, nach dem Neustart startet der Hotspot
String wifiScanJson();                // blockiert einige Sekunden: [{"ssid","rssi","open"}]

// Einrichtungs-WLAN
bool wifiApActive();
String wifiApSsid();
String wifiApIp();
void wifiStartAp();
void wifiStopAp();
void wifiApApplyPassword();  // laufenden Hotspot auf das aktuelle Hotspot-Passwort umstellen 
