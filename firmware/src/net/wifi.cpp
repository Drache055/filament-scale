#include "wifi.h"

#include <DNSServer.h>
#include <Preferences.h>
#include <WiFi.h>

#include <mutex>

#include "config.h"
#include "core/wifi_core.h"
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

// Scan und USB-Einrichtung: Anfragen kommen aus anderen Tasks (Webserver, loop), ausgeführt wird nur im net-Task
// (wifiLoop). Alles hier drunter nur mit jobsMutex.
std::mutex jobsMutex;
ScanJob scanJob;
ProvisionJob provJob;
String provSsid, provPass, provError;

void wipe(String &s) {
  for (size_t i = 0; i < s.length(); i++) s.setCharAt(i, 0);
  s = "";
}

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

void restoreOldConnection() {
  WiFi.disconnect(false, false);
  String oldSsid, oldPass;
  loadCredentials(oldSsid, oldPass);
  if (oldSsid.length()) {
    WiFi.begin(oldSsid.c_str(), oldPass.c_str());
  }
  wipe(oldPass);
}

// Verbindungsversuch der USB-Einrichtung. Gibt true zurück, solange er läuft (dann ruhen Scan und Hotspot-Automatik).
bool runProvision(uint32_t now) {
  std::lock_guard<std::mutex> lock(jobsMutex);
  if (provJob.wantsStart()) {
    if (scanJob.state() == ScanState::Running) {
      return true;  // laufenden Scan abwarten, sonst scheitert die Verbindung
    }
    logPrintf("[NET] USB-Einrichtung: verbinde mit '%s' ...\n", provSsid.c_str());
    WiFi.disconnect(false, false);
    WiFi.begin(provSsid.c_str(), provPass.c_str());
    provJob.started(now);
    return true;
  }
  if (provJob.state() != ProvState::Connecting) {
    return false;
  }
  ProvState st = provJob.step(now, WiFi.status() == WL_CONNECTED);
  if (st == ProvState::Connecting) {
    return true;
  }
  if (st == ProvState::Connected) {
    if (wifiSaveCredentials(provSsid.c_str(), provPass.c_str(), provError)) {
      lastConnectedMs = now;
      hasCredentials = true;  // der Hotspot endet wie gewohnt, sobald die Verbindung eine Minute steht
    } else {
      provJob.fail();
    }
  } else {
    provError = "Keine Verbindung mit diesem WLAN";
    logPrintf("[NET] USB-Einrichtung: keine Verbindung mit '%s'\n", provSsid.c_str());
    restoreOldConnection();
  }
  wipe(provPass);
  return false;
}

// allowStart = false: nur einen laufenden Scan zu Ende bringen (während der USB-Einrichtung)
void runScan(uint32_t now, bool allowStart) {
  std::lock_guard<std::mutex> lock(jobsMutex);
  if (scanJob.wantsStart()) {
    if (!allowStart) {
      return;
    }
    if (WiFi.scanNetworks(true, false, false, 300) == WIFI_SCAN_FAILED) {
      logPrintln("[NET] WLAN-Suche konnte nicht starten");
      scanJob.failed(now);
    } else {
      scanJob.started(now);
    }
    return;
  }
  if (scanJob.state() != ScanState::Running) {
    return;
  }
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING && !scanJob.timedOut(now)) {
    return;
  }
  if (n < 0) {
    logPrintln("[NET] WLAN-Suche fehlgeschlagen");
    WiFi.scanDelete();
    scanJob.failed(now);
    return;
  }
  std::vector<WifiNet> raw;
  raw.reserve(n);
  for (int i = 0; i < n; i++) {
    WifiNet w;
    w.ssid = WiFi.SSID(i).c_str();
    w.rssi = WiFi.RSSI(i);
    w.open = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
    raw.push_back(std::move(w));
  }
  WiFi.scanDelete();
  scanJob.finished(now, wifiMergeNetworks(raw));
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

  bool provisioning = runProvision(now);
  runScan(now, !provisioning);
  if (provisioning) {
    // während der USB-Einrichtung keinen Hotspot starten oder beenden (WiFi.mode würde den Versuch stören)
  } else if (hasCredentials && !connected && !apActive && now - lastConnectedMs > WIFI_AP_FALLBACK_MS) {
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
  {
    std::lock_guard<std::mutex> lock(jobsMutex);
    if (!provJob.request(timeoutMs)) {
      error = "Verbindungsversuch läuft bereits";
      return false;
    }
    provSsid = s;
    provPass = password ? password : "";
    provError = "";
  }
  // Der net-Task verbindet (wifiLoop), hier nur warten. Sicherheitsgrenze, falls der net-Task hängt.
  uint32_t start = millis();
  for (;;) {
    {
      std::lock_guard<std::mutex> lock(jobsMutex);
      ProvState st = provJob.state();
      if (st == ProvState::Connected) {
        return true;
      }
      if (st == ProvState::Failed) {
        error = provError;
        return false;
      }
    }
    if (millis() - start > timeoutMs + 10000) {
      error = "Keine Antwort vom WLAN-Task";
      return false;
    }
    delay(100);
  }
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

ScanState wifiScanRequest(std::vector<WifiNet> *nets) {
  std::lock_guard<std::mutex> lock(jobsMutex);
  ScanState st = scanJob.request(millis());
  if (st == ScanState::Done && nets) {
    *nets = scanJob.networks();
  }
  return st;
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
  const String &pw = devicePassword();  // immer mindestens 8 Zeichen, der Hotspot ist also immer geschützt
  bool ok = WiFi.softAP(WIFI_AP_SSID, pw.c_str());
  apActive = ok;
  if (ok) {
    dns.start(53, "*", WiFi.softAPIP());
  }
  logPrintf("[NET] Einrichtungs-WLAN '%s' %s, IP %s\n", WIFI_AP_SSID, ok ? "aktiv" : "konnte nicht starten",
            WiFi.softAPIP().toString().c_str());
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
