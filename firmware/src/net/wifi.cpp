#include "wifi.h"

#include <ArduinoJson.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <WiFi.h>

#include "config.h"
#include "secrets.h"
#include "storage/devicepw.h"
#include "util/log.h"

namespace {

bool apActive = false;
DNSServer dns;
volatile bool hasCredentials = false;
uint32_t lastConnectedMs = 0;   // letzter Zeitpunkt, an dem verbunden war (bzw. Start)
uint32_t connectedSinceMs = 0;  // seit wann ohne Unterbrechung verbunden
bool wasConnected = false;

void onWifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      logPrintf("[NET] Mit AP verbunden, Kanal %u, RSSI %d dBm\n", info.wifi_sta_connected.channel, WiFi.RSSI());
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      logPrintf("[NET] IP %s\n", WiFi.localIP().toString().c_str());
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      logPrintf("[NET] Getrennt, Grund %u\n", info.wifi_sta_disconnected.reason);
      break;
    default:
      break;
  }
}

// Liest die gespeicherten Zugangsdaten. Beim ersten Start ohne gespeicherte Daten werden die aus secrets.h übernommen
// (einmalig, damit ein bereits laufendes Gerät beim Update nicht offline geht). Platzhalter aus der Vorlage zählen nicht.
void loadCredentials(String &ssid, String &pass) {
  Preferences prefs;
  prefs.begin("wifi", false);
  ssid = prefs.getString("ssid", "");
  pass = prefs.getString("pass", "");
  bool migrated = prefs.getBool("mig", false);
  if (ssid.length() == 0 && !migrated) {
    String s(WIFI_SSID);
    if (s.length() > 0 && s != "dein-wlan") {
      ssid = s;
      pass = WIFI_PASSWORD;
      prefs.putString("ssid", ssid);
      prefs.putString("pass", pass);
      logPrintf("[NET] Zugangsdaten für '%s' aus secrets.h übernommen\n", ssid.c_str());
    }
    prefs.putBool("mig", true);
  }
  prefs.end();
}

}  // namespace

void wifiBegin() {
  WiFi.onEvent(onWifiEvent);
  WiFi.setHostname(OTA_HOSTNAME);
  WiFi.setAutoReconnect(true);
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org", "time.nist.gov");  // Zeitstempel für die Warteschlange
  lastConnectedMs = millis();

  String ssid, pass;
  loadCredentials(ssid, pass);
  hasCredentials = ssid.length() > 0;
  if (!hasCredentials) {
    logPrintln("[NET] Keine WLAN-Zugangsdaten gespeichert, starte Hotspot");
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    wifiStartAp();
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // Energiesparmodus aus (erst nach mode(), sonst wirkungslos)
  WiFi.begin(ssid.c_str(), pass.c_str());
  logPrintf("[NET] Verbinde mit WLAN '%s' ...\n", ssid.c_str());
}

void wifiLoop() {
  if (apActive) {
    dns.processNextRequest();  // Captive Portal: alle Namen auf das Gerät auflösen
  }
  bool connected = WiFi.status() == WL_CONNECTED;
  uint32_t now = millis();
  if (connected) {
    lastConnectedMs = now;
    if (!wasConnected) {
      connectedSinceMs = now;
    }
  }
  wasConnected = connected;

  if (hasCredentials && !connected && !apActive && now - lastConnectedMs > WIFI_AP_FALLBACK_MS) {
    logPrintln("[NET] Keine Verbindung, starte Einrichtungs-WLAN");
    wifiStartAp();
  } else if (hasCredentials && connected && apActive && now - connectedSinceMs > WIFI_AP_STABLE_MS && WiFi.softAPgetStationNum() == 0) {
    logPrintln("[NET] Verbunden, beende Einrichtungs-WLAN");
    wifiStopAp();
  }
}

bool wifiConnected() { return WiFi.status() == WL_CONNECTED; }
String wifiSsid() { return WiFi.SSID(); }
String wifiIp() { return WiFi.localIP().toString(); }
int wifiRssi() { return WiFi.RSSI(); }

bool wifiHasCredentials() { return hasCredentials; }

String wifiSavedSsid() {
  Preferences prefs;
  prefs.begin("wifi", false);
  String ssid = prefs.getString("ssid", "");
  prefs.end();
  return ssid;
}

bool wifiSaveCredentials(const char *ssid, const char *password, String &error) {
  String s(ssid ? ssid : ""), p(password ? password : "");
  s.trim();
  if (s.length() == 0 || s.length() > 32) {
    error = "SSID muss 1 bis 32 Zeichen lang sein";
    return false;
  }
  if (p.length() != 0 && (p.length() < 8 || p.length() > 63)) {
    error = "Passwort muss leer (offenes WLAN) oder 8 bis 63 Zeichen lang sein";
    return false;
  }
  Preferences prefs;
  prefs.begin("wifi", false);
  prefs.putString("ssid", s);
  prefs.putString("pass", p);
  prefs.putBool("mig", true);
  prefs.end();
  logPrintf("[NET] WLAN-Zugangsdaten für '%s' gespeichert\n", s.c_str());
  return true;
}

bool wifiProvision(const char *ssid, const char *password, uint32_t timeoutMs, String &error) {
  String s(ssid ? ssid : "");
  s.trim();
  logPrintf("[NET] USB-Einrichtung: verbinde mit '%s' ...\n", s.c_str());
  WiFi.disconnect(false, false);
  WiFi.begin(s.c_str(), password ? password : "");
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    delay(200);
  }
  if (WiFi.status() != WL_CONNECTED) {
    error = "Keine Verbindung mit diesem WLAN";
    logPrintf("[NET] USB-Einrichtung: keine Verbindung mit '%s'\n", s.c_str());
    WiFi.disconnect(false, false);
    String oldSsid, oldPass;
    loadCredentials(oldSsid, oldPass);
    if (oldSsid.length()) {
      WiFi.begin(oldSsid.c_str(), oldPass.c_str());
    }
    return false;
  }
  if (!wifiSaveCredentials(s.c_str(), password, error)) {
    return false;
  }
  lastConnectedMs = millis();
  hasCredentials = true;  // der Hotspot endet wie gewohnt, sobald die Verbindung eine Minute steht
  return true;
}

void wifiResetCredentials() {
  Preferences prefs;
  prefs.begin("wifi", false);
  prefs.remove("ssid");
  prefs.remove("pass");
  prefs.putBool("mig", true);  // secrets.h nicht erneut übernehmen
  prefs.end();
  logPrintln("[NET] WLAN-Zugangsdaten gelöscht, nach dem Neustart startet der Hotspot");
}

String wifiScanJson() {
  int n = WiFi.scanNetworks(false, false, false, 300);
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < n && i < 30; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) {
      continue;  // verstecktes Netz
    }
    bool dup = false;
    for (JsonObject o : arr) {
      if (ssid == o["ssid"].as<const char *>()) {
        if (WiFi.RSSI(i) > o["rssi"].as<int>()) {
          o["rssi"] = WiFi.RSSI(i);  // stärkstes Signal je Name
        }
        dup = true;
      }
    }
    if (!dup) {
      JsonObject o = arr.add<JsonObject>();
      o["ssid"] = ssid;
      o["rssi"] = WiFi.RSSI(i);
      o["open"] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
    }
  }
  WiFi.scanDelete();
  String out;
  serializeJson(doc, out);
  return out;
}

bool wifiApActive() { return apActive; }
String wifiApSsid() { return WIFI_AP_SSID; }
String wifiApIp() { return WiFi.softAPIP().toString(); }

void wifiStartAp() {
  if (apActive) {
    return;
  }
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  String pw = hotspotPassword();  // immer mindestens 8 Zeichen, der Hotspot ist also immer geschützt
  bool ok = WiFi.softAP(WIFI_AP_SSID, pw.c_str());
  apActive = ok;
  if (ok) {
    dns.start(53, "*", WiFi.softAPIP());
  }
  logPrintf("[NET] Einrichtungs-WLAN '%s' %s, IP %s\n", WIFI_AP_SSID, ok ? "aktiv" : "konnte nicht starten",
            WiFi.softAPIP().toString().c_str());
}

void wifiApApplyPassword() {
  if (!apActive) {
    return;
  }
  // softAP() auf einem laufenden Hotspot übernimmt die neue Konfiguration, ohne die Betriebsart (und damit die
  // Verbindung zum Heim-WLAN) anzufassen
  String pw = hotspotPassword();
  bool ok = WiFi.softAP(WIFI_AP_SSID, pw.c_str());
  logPrintf("[NET] Hotspot mit neuem Passwort %s\n", ok ? "aktiv" : "konnte nicht starten");
}

void wifiStopAp() {
  if (!apActive) {
    return;
  }
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  apActive = false;
}
