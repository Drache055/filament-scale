#include "webserver.h"
#include "util/log.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_ota_ops.h>

#include "app/session.h"
#include "config.h"
#include "display/display.h"
#include "i18n/i18n.h"
#include "util/log.h"
#include "net/fwupdate.h"
#include "net/spoolman.h"
#include "nfc/nfc.h"
#include "scale/scale.h"
#include "secrets.h"
#include "net/websession.h"
#include "storage/admin.h"
#include "storage/catalog.h"
#include "storage/devmode.h"
#include "storage/devicepw.h"
#include "storage/users.h"
#include "web_page.h"
#include "wifi.h"

static WebServer server(80);
static bool started = false;
static uint32_t rebootAt = 0;

// ---- Anmeldung und Rollen ----
// Admin: Benutzer "admin" mit dem Admin-Passwort (Login-Seite, Cookie "sid"; Basic-Auth für Skripte).
// User: Name + PIN (nur wiegen, NFC lesen, WLAN-Status, Tara mit Recht, eigene Sprache und PIN). Generic: wie User ohne Tara.
// Ist die Admin-Sperre aus, darf jeder alles.
static uint8_t authFailures = 0;
static uint32_t authBlockedUntil = 0;

static bool authBlocked() { return (int32_t)(authBlockedUntil - millis()) > 0; }

static void authFailed() {
  if (++authFailures >= 5) {
    authFailures = 0;
    authBlockedUntil = millis() + 30000;
    logPrintln("[WEB] 5 Fehlversuche bei der Anmeldung, 30 s gesperrt");
  }
}

struct WebWho {
  WebRole role = WebRole::None;
  uint16_t userId = 0;
  String token;  // Sitzungs-Token, leer bei Basic-Auth/ohne Sitzung
};

static String cookieToken() {
  String c = server.header("Cookie");
  int i = c.indexOf("sid=");
  if (i < 0) {
    return "";
  }
  int e = c.indexOf(';', i);
  String t = c.substring(i + 4, e < 0 ? c.length() : e);
  t.trim();
  return t;
}

static WebWho currentWho() {
  WebWho w;
  if (!adminLockEnabled()) {
    w.role = WebRole::Admin;
    return w;
  }
  // Wer im geschützten Einrichtungs-WLAN ist, kennt dessen Passwort (Gerätepasswort, mindestens 8 Zeichen);
  // ein offener Hotspot ist dagegen kein Ausweis
  if (devicePassword().length() >= 8 && wifiApActive() && server.client().localIP() == WiFi.softAPIP()) {
    w.role = WebRole::Admin;
    return w;
  }
  String token = cookieToken();
  WebSession ss;
  if (token.length() && webSessionGet(token, ss)) {
    w.role = ss.role;
    w.userId = ss.userId;
    w.token = token;
    return w;
  }
  if (server.hasHeader("Authorization") && !authBlocked()) {  // Basic-Auth für Skripte (curl)
    bool ok = server.authenticate([](HTTPAuthMethod mode, String username, String params[]) -> String * {
      if (mode == BASIC_AUTH && username.equalsConstantTime("admin") && adminCheckPassword(params[0].c_str())) {
        return new String(params[0]);
      }
      return nullptr;
    });
    if (ok) {
      authFailures = 0;
      w.role = WebRole::Admin;
    } else {
      authFailed();
    }
  }
  return w;
}

static bool mayTare(const WebWho &w) {
  if (w.role == WebRole::Admin) {
    return true;
  }
  User u;
  return w.role == WebRole::User && usersFind(w.userId, u) && u.canTare;
}

static bool mayEditCatalog(const WebWho &w) {
  if (w.role == WebRole::Admin) {
    return true;
  }
  User u;
  return w.role == WebRole::User && usersFind(w.userId, u) && u.canCatalog;
}

// Mindestens die Rolle min, sonst 401 (nicht angemeldet) bzw. 403 (Rolle reicht nicht)
static bool requireRole(WebRole min, WebWho *out = nullptr) {
  WebWho w = currentWho();
  if (w.role == WebRole::None) {
    if (authBlocked()) {
      server.send(429, "text/plain", "Zu viele Fehlversuche, bitte kurz warten");
    } else {
      server.send(401, "text/plain", "Anmeldung nötig");
    }
    return false;
  }
  if ((int)w.role < (int)min) {
    server.send(403, "text/plain", "Dafür sind Admin-Rechte nötig");
    return false;
  }
  if (out) {
    *out = w;
  }
  return true;
}

static bool requireAdmin() { return requireRole(WebRole::Admin); }

// Filament-Katalog (Typen, Hersteller, Farben): Admin oder Benutzer mit dem Recht "Filament"
static bool requireCatalogEditor() {
  WebWho w;
  if (!requireRole(WebRole::Generic, &w)) {
    return false;
  }
  if (!mayEditCatalog(w)) {
    server.send(403, "text/plain", "Kein Recht für den Filament-Katalog");
    return false;
  }
  return true;
}

static void sendJson(const JsonDocument &doc) {
  String out;
  serializeJson(doc, out);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", out);
}

// Statusdaten je nach Rolle: Generic/User sehen Gewicht, NFC, WLAN-Status und ihren Namen, der Admin zusätzlich die Gerätedaten
static void handleStatus() {
  WebWho w;
  if (!requireRole(WebRole::Generic, &w)) {
    return;
  }
  bool admin = w.role == WebRole::Admin;
  ScaleReading r = scaleGet();
  JsonDocument doc;
  doc["valid"] = r.valid;
  doc["calibrated"] = r.calibrated;
  doc["stable"] = r.stable;
  doc["overload"] = r.overload;
  doc["weight"] = r.grams;
  doc["sensor"] = sensorTypeName(scaleSensorActive().type);
  NfcState n = nfcGet();
  JsonObject nfc = doc["nfc"].to<JsonObject>();
  nfc["module"] = n.moduleReady;
  nfc["present"] = n.tagPresent;
  nfc["uid"] = n.uid;
  nfc["kind"] = n.content.kind == TagKind::Spool  ? "spool"
                : n.content.kind == TagKind::Box   ? "box"
                : n.content.kind == TagKind::User  ? "user"
                : n.content.kind == TagKind::Admin ? "admin"
                                                   : "unknown";
  nfc["json"] = n.content.kind == TagKind::Admin ? "" : n.json;  // der Admin-Tag enthält ein Geheimnis
  User current;
  doc["user"] = sessionCurrentUser(current) ? current.name : "";
  doc["role"] = w.role == WebRole::Admin ? "admin" : w.role == WebRole::User ? "user" : "generic";
  doc["tare"] = mayTare(w);
  doc["catalog"] = mayEditCatalog(w);
  doc["simple"] = devModeSimple();  // Betriebsart "Einfache Waage"
  // Sprache der Seite: eigene Sprache des Benutzers, sonst die Gerätesprache
  Lang lang = i18nDefault();
  User me;
  if (w.role == WebRole::User && usersFind(w.userId, me) && me.lang) {
    lang = me.lang == 2 ? Lang::EN : Lang::DE;
  }
  doc["lang"] = lang == Lang::EN ? "en" : "de";
  doc["wifi"] = wifiConnected();
  doc["ssid"] = wifiSsid();
  if (admin) {
    doc["raw_net"] = r.rawNet;
    doc["ip"] = wifiIp();
    doc["rssi"] = wifiRssi();
    doc["uptime_s"] = millis() / 1000;
    doc["heap_free"] = ESP.getFreeHeap();
    doc["psram_free"] = ESP.getFreePsram();
    doc["slot"] = esp_ota_get_running_partition()->label;
    doc["admin_lock"] = adminLockEnabled();
    doc["fw_version"] = fwVersion();
    FwUpdStatus fu = fwUpdateStatus();
    doc["fw_update"] = fu.available ? fu.latest : "";
  }
  sendJson(doc);
}

// ---- Anmeldung der Webseite ----
static void sendMe(const WebWho &w) {
  JsonDocument doc;
  doc["role"] = w.role == WebRole::Admin ? "admin" : w.role == WebRole::User ? "user" : w.role == WebRole::Generic ? "generic" : "none";
  doc["lock"] = adminLockEnabled();
  if (w.role == WebRole::None) {
    doc["generic"] = usersGenericEnabled();
    doc["generic_pin"] = usersGenericEnabled() && usersHasPin(0);
  } else {
    User u;
    doc["name"] = w.role == WebRole::Admin ? "admin" : w.role == WebRole::Generic ? "generic" : (usersFind(w.userId, u) ? u.name : "");
    doc["tare"] = mayTare(w);
    doc["catalog"] = mayEditCatalog(w);
    doc["lang"] = (w.role == WebRole::User && usersFind(w.userId, u)) ? (u.lang == 1 ? "de" : u.lang == 2 ? "en" : "") : "";
  }
  sendJson(doc);
}

static void handleMe() {
  sendMe(currentWho());
}

static void handleLogin() {
  if (authBlocked()) {
    server.send(429, "text/plain", "Zu viele Fehlversuche, bitte kurz warten");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "text/plain", "ungültiges JSON");
    return;
  }
  String name(doc["name"] | "");
  String secret(doc["secret"] | "");
  name.trim();
  WebRole role = WebRole::None;
  uint16_t userId = 0;
  if (name.equalsIgnoreCase("admin")) {
    if (adminCheckPassword(secret.c_str())) {
      role = WebRole::Admin;
    }
  } else if (name.equalsIgnoreCase("generic")) {
    if (usersGenericEnabled() && (!usersHasPin(0) || usersCheckPin(0, secret.c_str()))) {
      role = WebRole::Generic;
    }
  } else {
    User u;
    if (usersFindByName(name.c_str(), u) && usersHasPin(u.id) && usersCheckPin(u.id, secret.c_str())) {
      role = WebRole::User;
      userId = u.id;
    }
  }
  if (role == WebRole::None) {
    authFailed();
    server.send(401, "text/plain", "Name oder PIN falsch");
    return;
  }
  authFailures = 0;
  String token = webSessionCreate(role, userId);
  server.sendHeader("Set-Cookie", "sid=" + token + "; Path=/; HttpOnly; SameSite=Strict; Max-Age=" + String(WEB_SESSION_MS / 1000));
  WebWho w;
  w.role = role;
  w.userId = userId;
  w.token = token;
  logPrintf("[WEB] Angemeldet: %s\n", role == WebRole::Admin ? "admin" : role == WebRole::Generic ? "generic" : name.c_str());
  sendMe(w);
}

static void handleLogout() {
  String token = cookieToken();
  if (token.length()) {
    webSessionDestroy(token);
  }
  server.sendHeader("Set-Cookie", "sid=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
  server.send(200, "text/plain", "ok");
}

// Eigene Sprache (nur Benutzer mit PIN). lang = de, en oder leer (Gerätesprache)
static void handleMeLang() {
  WebWho w;
  if (!requireRole(WebRole::User, &w)) {
    return;
  }
  if (w.role != WebRole::User) {
    server.send(403, "text/plain", "Nur für Benutzer");
    return;
  }
  String lang = server.arg("lang");
  uint8_t value = lang == "de" ? 1 : lang == "en" ? 2 : 0;
  if (lang.length() && value == 0) {
    server.send(400, "text/plain", "lang muss de, en oder leer sein");
    return;
  }
  usersSetLang(w.userId, value);
  sessionUserChanged(w.userId);
  server.send(200, "text/plain", "ok");
}

// Eigene PIN ändern (alte PIN zur Bestätigung)
static void handleMePin() {
  WebWho w;
  if (!requireRole(WebRole::User, &w)) {
    return;
  }
  if (w.role != WebRole::User) {
    server.send(403, "text/plain", "Nur für Benutzer");
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "text/plain", "ungültiges JSON");
    return;
  }
  if (authBlocked()) {
    server.send(429, "text/plain", "Zu viele Fehlversuche, bitte kurz warten");
    return;
  }
  if (!usersCheckPin(w.userId, doc["old"] | "")) {
    authFailed();
    server.send(403, "text/plain", "Alte PIN falsch");
    return;
  }
  String pin(doc["pin"] | "");
  if (pin.length() == 0) {
    server.send(400, "text/plain", "PIN: 4 bis 6 Ziffern");
    return;
  }
  String error;
  if (!usersSetPin(w.userId, pin.c_str(), error)) {
    server.send(400, "text/plain", error);
    return;
  }
  server.send(200, "text/plain", "ok");
}

static void handleTare() {
  WebWho w;
  if (!requireRole(WebRole::Generic, &w)) {
    return;
  }
  if (!mayTare(w)) {
    server.send(403, "text/plain", "Kein Tara-Recht");
    return;
  }
  if (scaleTare()) {
    server.send(200, "text/plain", "ok");
  } else {
    server.send(409, "text/plain", "Wägesensor liefert keine Daten");
  }
}

static void handleCalibrate() {
  if (!requireAdmin()) {
    return;
  }
  float grams = server.arg("grams").toFloat();
  if (grams < 1.0f || grams > 5000.0f) {
    server.send(400, "text/plain", "Referenzgewicht muss zwischen 1 und 5000 g liegen");
    return;
  }
  if (scaleCalibrate(grams)) {
    server.send(200, "text/plain", "ok");
  } else {
    server.send(409, "text/plain", "Signal zu klein oder Wägesensor ohne Daten (erst Tara, dann Gewicht auflegen)");
  }
}

// ---- Wägesensor: Typ und Pins (Admin). Wirksam nach dem Neustart, Kalibrierung gilt pro Sensortyp. ----
static void sensorToJson(JsonObject o, const SensorConfig &c) {
  o["type"] = sensorTypeId(c.type);
  JsonArray pins = o["pins"].to<JsonArray>();
  for (int i = 0; i < SENSOR_PIN_COUNT; i++) {
    pins.add(c.pin[i]);
  }
}

static void handleSensorGet() {
  if (!requireAdmin()) {
    return;
  }
  JsonDocument doc;
  sensorToJson(doc["active"].to<JsonObject>(), scaleSensorActive());
  sensorToJson(doc["stored"].to<JsonObject>(), scaleSensorStored());
  doc["restart"] = scaleSensorActive() != scaleSensorStored();
  JsonArray free = doc["free"].to<JsonArray>();
  int n = 0;
  const int *p = sensorFreePins(n);
  for (int i = 0; i < n; i++) {
    free.add(p[i]);
  }
  JsonObject defs = doc["defaults"].to<JsonObject>();
  for (int t = 0; t < SENSOR_TYPE_COUNT; t++) {
    SensorConfig d = sensorDefault((SensorType)t);
    JsonArray a = defs[sensorTypeId(d.type)].to<JsonArray>();
    for (int i = 0; i < SENSOR_PIN_COUNT; i++) {
      a.add(d.pin[i]);
    }
  }
  String out;
  serializeJson(doc, out);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", out);
}

// Body: {"type":"nau7802","pins":[47,48,-1],"reboot":true}
static void handleSensorSet() {
  if (!requireAdmin()) {
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "text/plain", "Ungültiges JSON");
    return;
  }
  SensorConfig c;
  if (!sensorTypeParse(doc["type"] | "", c.type)) {
    server.send(400, "text/plain", "Unbekannter Sensortyp");
    return;
  }
  JsonArray pins = doc["pins"].as<JsonArray>();
  if (pins.size() != SENSOR_PIN_COUNT) {
    server.send(400, "text/plain", "pins: drei Einträge erwartet (-1 = nicht belegt)");
    return;
  }
  for (int i = 0; i < SENSOR_PIN_COUNT; i++) {
    int v = pins[i] | -1;
    c.pin[i] = (v < -1 || v > 48) ? 127 : (int8_t)v;
  }
  const char *err = nullptr;
  if (!scaleSensorSave(c, err)) {
    server.send(400, "text/plain", err ? err : "Ungültige Sensorwahl");
    return;
  }
  server.send(200, "text/plain", "ok");
  if (doc["reboot"] | false) {
    rebootAt = millis() + 600;
  }
}

static void handleThemeGet() {
  if (!requireAdmin()) {
    return;
  }
  File f = LittleFS.open(THEME_FILE, "r");
  if (!f) {
    server.send(404, "text/plain", "keine theme.json vorhanden");
    return;
  }
  server.sendHeader("Cache-Control", "no-store");
  server.streamFile(f, "application/json");
  f.close();
}

static void handleThemePost() {
  if (!requireAdmin()) {
    return;
  }
  String body = server.arg("plain");
  if (body.length() == 0 || body.length() > 4096) {
    server.send(400, "text/plain", "leer oder zu groß (max. 4 KB)");
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err || !doc.is<JsonObject>()) {
    server.send(400, "text/plain", String("ungültiges JSON: ") + err.c_str());
    return;
  }
  File f = LittleFS.open(THEME_FILE, "w");
  if (!f) {
    server.send(500, "text/plain", "Schreiben fehlgeschlagen");
    return;
  }
  f.print(body);
  f.close();
  logPrintln("[WEB] theme.json gespeichert, Neustart");
  server.send(200, "text/plain", "ok");
  rebootAt = millis() + 600;  // Antwort erst ausliefern, dann neu starten (das Theme wird beim Start geladen)
}

static void handleCatalogGet() {
  if (!requireCatalogEditor()) {
    return;
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", catalogJson());
}

static void handleCatalogPost() {
  if (!requireCatalogEditor()) {
    return;
  }
  String body = server.arg("plain");
  if (body.length() == 0 || body.length() > 4096) {
    server.send(400, "text/plain", "leer oder zu groß (max. 4 KB)");
    return;
  }
  String error;
  if (catalogSet(body.c_str(), error)) {
    server.send(200, "text/plain", "ok");
  } else {
    server.send(400, "text/plain", error);
  }
}

static void handleTouchReset() {
  if (!requireAdmin()) {
    return;
  }
  displayTouchRequestReset();
  server.send(200, "text/plain", "ok");
}

// ---- Benutzer ----
static void handleUsersGet() {
  if (!requireAdmin()) {
    return;
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", usersJson());
}

static void handleUsersAdd() {
  if (!requireAdmin()) {
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "text/plain", "ungültiges JSON");
    return;
  }
  String error;
  String pin(doc["pin"] | "");
  uint16_t newId = 0;
  if (!usersAdd(doc["name"] | "", error, &newId)) {
    server.send(400, "text/plain", error);
    return;
  }
  if (pin.length() && !usersSetPin(newId, pin.c_str(), error)) {
    usersRemove(newId);
    server.send(400, "text/plain", error);
    return;
  }
  server.send(200, "application/json", usersJson());
}

static void handleUsersDelete() {
  if (!requireAdmin()) {
    return;
  }
  uint16_t removeId = (uint16_t)server.arg("id").toInt();
  if (usersRemove(removeId)) {
    webSessionsDropUser(removeId);
    server.send(200, "application/json", usersJson());
  } else {
    server.send(404, "text/plain", "Benutzer nicht gefunden");
  }
}

// Eigene Sprache eines Benutzers (lang = de, en oder leer für die Gerätesprache)
static void handleUsersLang() {
  if (!requireAdmin()) {
    return;
  }
  uint16_t id = (uint16_t)server.arg("id").toInt();
  String lang = server.arg("lang");
  uint8_t value = lang == "de" ? 1 : lang == "en" ? 2 : 0;
  if (lang.length() && value == 0) {
    server.send(400, "text/plain", "lang muss de, en oder leer sein");
    return;
  }
  if (!usersSetLang(id, value)) {
    server.send(404, "text/plain", "Benutzer nicht gefunden");
    return;
  }
  sessionUserChanged(id);
  server.send(200, "application/json", usersJson());
}

// Recht "Filament" (Katalog pflegen) eines Benutzers (allow = 1 oder 0)
static void handleUsersCatalog() {
  if (!requireAdmin()) {
    return;
  }
  if (!usersSetCatalog((uint16_t)server.arg("id").toInt(), server.arg("allow") == "1")) {
    server.send(404, "text/plain", "Benutzer nicht gefunden");
    return;
  }
  server.send(200, "application/json", usersJson());
}

// Katalogabgleich mit Spoolman anstoßen (POST) bzw. Ergebnis abholen (GET)
static void handleCatalogSyncRequest() {
  if (!requireCatalogEditor()) {
    return;
  }
  spoolmanRequestCatalogSync();
  server.send(200, "text/plain", "ok");
}

static void handleCatalogSyncGet() {
  if (!requireCatalogEditor()) {
    return;
  }
  CatalogSyncResult r = spoolmanCatalogSyncResult();
  JsonDocument doc;
  doc["state"] = r.state == SpoolmanState::Found ? "ok" : r.state == SpoolmanState::Busy ? "busy" : r.state == SpoolmanState::Error ? "error" : "idle";
  doc["vendors_read"] = r.vendorsRead;
  doc["materials_read"] = r.materialsRead;
  doc["added_makers"] = r.addedMakers;
  doc["added_materials"] = r.addedMaterials;
  doc["skipped"] = r.skipped;
  doc["seconds_ago"] = r.secondsAgo;
  doc["error"] = r.error;
  sendJson(doc);
}

// Web-PIN eines Benutzers setzen oder entfernen (id 0 = Generic-Zugang, pin leer = entfernen)
static void handleUsersPin() {
  if (!requireAdmin()) {
    return;
  }
  uint16_t id = (uint16_t)server.arg("id").toInt();
  String error;
  if (!usersSetPin(id, server.arg("pin").c_str(), error)) {
    server.send(error == "Benutzer nicht gefunden" ? 404 : 400, "text/plain", error);
    return;
  }
  webSessionsDropUser(id);  // wer mit der alten PIN angemeldet war, muss sich neu anmelden
  server.send(200, "application/json", usersJson());
}

// Generic-Zugang (nur wiegen und NFC lesen) ein- oder ausschalten
static void handleGenericSet() {
  if (!requireAdmin()) {
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) || !doc["enabled"].is<bool>()) {
    server.send(400, "text/plain", "enabled (true/false) fehlt");
    return;
  }
  usersSetGenericEnabled(doc["enabled"].as<bool>());
  server.send(200, "text/plain", "ok");
}

// Tara-Recht eines Benutzers (allow = 1 oder 0)
static void handleUsersTare() {
  if (!requireAdmin()) {
    return;
  }
  uint16_t id = (uint16_t)server.arg("id").toInt();
  if (!usersSetTare(id, server.arg("allow") == "1")) {
    server.send(404, "text/plain", "Benutzer nicht gefunden");
    return;
  }
  sessionUserChanged(id);
  server.send(200, "application/json", usersJson());
}

// ---- Admin ----
static void sendAdminState() {
  JsonDocument doc;
  doc["lock"] = adminLockEnabled();
  doc["default_password"] = adminIsDefaultPassword();
  doc["default_pin"] = adminIsDefaultDevicePin();
  doc["has_tag"] = adminHasTag();
  doc["device_session"] = adminSessionActive();
  doc["generic"] = usersGenericEnabled();
  doc["generic_pin"] = usersHasPin(0);
  sendJson(doc);
}

static void handleAdminGet() {
  if (!requireAdmin()) {
    return;
  }
  sendAdminState();
}

static void handleAdminPassword() {
  WebWho w;
  if (!requireRole(WebRole::Admin, &w)) {
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "text/plain", "ungültiges JSON");
    return;
  }
  String error;
  if (adminSetPassword(doc["password"] | "", error)) {
    webSessionsDropAdminExcept(w.token);  // andere Admin-Anmeldungen enden
    server.send(200, "text/plain", "ok");
  } else {
    server.send(400, "text/plain", error);
  }
}

// Geräte-PIN (Ziffernblock am Gerät)
static void handleAdminPin() {
  if (!requireAdmin()) {
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "text/plain", "ungültiges JSON");
    return;
  }
  String error;
  if (adminSetDevicePin(doc["pin"] | "", error)) {
    server.send(200, "text/plain", "ok");
  } else {
    server.send(400, "text/plain", error);
  }
}

static void handleAdminLock() {
  if (!requireAdmin()) {
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) || !doc["enabled"].is<bool>()) {
    server.send(400, "text/plain", "enabled (true/false) fehlt");
    return;
  }
  adminSetLock(doc["enabled"].as<bool>());
  sendAdminState();
}

// Löst das Schreiben eines Admin-Tags am Gerät aus. Ein neuer Tag macht den bisherigen ungültig.
static void handleAdminTag() {
  if (!requireAdmin()) {
    return;
  }
  char json[128];
  String token = adminNewTagToken();
  if (tagBuildAdmin(token.c_str(), json, sizeof(json)) == 0) {
    server.send(500, "text/plain", "Tag-Inhalt zu groß");
    return;
  }
  if (!nfcRequestWrite(json, NfcWriteOrigin::Web)) {
    server.send(409, "text/plain", "Es läuft bereits ein Schreibvorgang");
    return;
  }
  logPrintln("[WEB] Admin-Tag angefordert");
  server.send(200, "text/plain", "ok");
}

// Löst das Schreiben eines Benutzer-Tags am Gerät aus. Geschrieben wird auf den nächsten neu aufgelegten Tag.
static void handleUsersWrite() {
  if (!requireAdmin()) {
    return;
  }
  User u;
  if (!usersFind((uint16_t)server.arg("id").toInt(), u)) {
    server.send(404, "text/plain", "Benutzer nicht gefunden");
    return;
  }
  char json[128];
  if (tagBuildUser(u.id, u.name, json, sizeof(json)) == 0) {
    server.send(500, "text/plain", "Tag-Inhalt zu groß");
    return;
  }
  if (!nfcRequestWrite(json, NfcWriteOrigin::Web)) {
    server.send(409, "text/plain", "Es läuft bereits ein Schreibvorgang");
    return;
  }
  logPrintf("[WEB] Benutzer-Tag für '%s' angefordert\n", u.name);
  server.send(200, "text/plain", "ok");
}

static const char *writeStateName(NfcWrite st) {
  switch (st) {
    case NfcWrite::WaitingForTag: return "waiting";
    case NfcWrite::Writing: return "writing";
    case NfcWrite::Ok: return "ok";
    case NfcWrite::ErrLocked: return "locked";
    case NfcWrite::ErrTooBig: return "too_big";
    case NfcWrite::ErrNotNtag: return "not_ntag";
    case NfcWrite::ErrFailed: return "failed";
    case NfcWrite::ErrTimeout: return "timeout";
    case NfcWrite::ErrOccupied: return "occupied";
    default: return "idle";
  }
}

static void handleNfcWriteStatus() {
  if (!requireAdmin()) {
    return;
  }
  JsonDocument doc;
  doc["state"] = writeStateName(nfcWriteStatus());
  doc["origin"] = nfcWriteOrigin() == NfcWriteOrigin::Web ? "web" : "device";
  doc["module"] = nfcGet().moduleReady;
  sendJson(doc);
}

// ---- Spoolman ----
static void handleSpoolmanGet() {
  if (!requireAdmin()) {
    return;
  }
  SpoolmanStatus st = spoolmanStatus();
  SpoolmanResult r = spoolmanLookupResult();
  JsonDocument doc;
  doc["url"] = st.url;
  doc["reachable"] = st.reachable;
  doc["checked"] = st.everChecked;
  doc["version"] = st.version;
  doc["checked_s"] = st.checkedSecondsAgo;
  JsonObject q = doc["queue"].to<JsonObject>();
  q["count"] = spoolmanQueueCount();
  q["dropped"] = spoolmanQueueDropped();
  JsonObject lk = doc["lookup"].to<JsonObject>();
  const char *state = r.state == SpoolmanState::Found      ? "found"
                      : r.state == SpoolmanState::NotFound ? "not_found"
                      : r.state == SpoolmanState::Error    ? "error"
                      : r.state == SpoolmanState::Busy     ? "busy"
                                                           : "idle";
  lk["state"] = state;
  lk["uid"] = r.uid;
  lk["error"] = r.error;
  if (r.state == SpoolmanState::Found) {
    JsonObject sp = lk["spool"].to<JsonObject>();
    sp["id"] = r.spool.id;
    sp["vendor"] = r.spool.vendor;
    sp["material"] = r.spool.material;
    sp["name"] = r.spool.name;
    sp["color_hex"] = r.spool.colorHex;
    sp["location"] = r.spool.location;
    sp["owner"] = r.spool.owner;
    sp["remaining"] = r.spool.remaining;
    sp["empty_spool"] = r.spool.emptySpool;
    sp["filament_weight"] = r.spool.filamentWeight;
  }
  SpoolmanResult w = spoolmanWriteResult();
  JsonObject wr = doc["write"].to<JsonObject>();
  wr["state"] = w.state == SpoolmanState::Found   ? "ok"
                : w.state == SpoolmanState::Error ? "error"
                : w.state == SpoolmanState::Busy  ? "busy"
                                                  : "idle";
  wr["seq"] = w.seq;
  wr["http"] = w.httpStatus;
  wr["error"] = w.error;
  wr["queued"] = w.queued;
  if (w.state == SpoolmanState::Found && w.spool.id >= 0) {
    wr["id"] = w.spool.id;
    wr["remaining"] = w.spool.remaining;
    wr["location"] = w.spool.location;
    wr["owner"] = w.spool.owner;
  }
  sendJson(doc);
}

static void handleSpoolmanUrl() {
  if (!requireAdmin()) {
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "text/plain", "ungültiges JSON");
    return;
  }
  String error;
  if (spoolmanSetUrl(doc["url"] | "", error)) {
    server.send(200, "text/plain", "ok");
  } else {
    server.send(400, "text/plain", error);
  }
}

// Debug/Komfort: Spule nach UID und/oder Spoolman-ID nachschlagen (ohne NFC-Tag), Ergebnis über GET /api/spoolman
static void handleSpoolmanLookup() {
  if (!requireAdmin()) {
    return;
  }
  String uid = server.arg("uid");
  uid.toUpperCase();
  int id = server.hasArg("id") ? server.arg("id").toInt() : -1;
  if (uid.length() > 20 || (uid.length() == 0 && id < 0)) {
    server.send(400, "text/plain", "uid (Hex, max. 20 Zeichen) oder id angeben");
    return;
  }
  spoolmanRequestLookup(uid.c_str(), id);
  server.send(200, "text/plain", "ok");
}

// Debug: Spule aktualisieren (remaining, location, owner optional), Ergebnis über GET /api/spoolman ("write")
static void handleSpoolmanUpdate() {
  if (!requireAdmin()) {
    return;
  }
  int id = server.arg("id").toInt();
  if (id <= 0) {
    server.send(400, "text/plain", "id fehlt");
    return;
  }
  float remaining = server.hasArg("remaining") ? server.arg("remaining").toFloat() : -1;
  String location = server.arg("location");
  String owner = server.arg("owner");
  spoolmanRequestUpdate(id, remaining, server.hasArg("location") ? location.c_str() : nullptr,
                        owner.length() ? owner.c_str() : nullptr);
  server.send(200, "text/plain", "ok");
}

// Debug: Tag (UID) mit einer Spule verknüpfen
static void handleSpoolmanLink() {
  if (!requireAdmin()) {
    return;
  }
  int id = server.arg("id").toInt();
  String uid = server.arg("uid");
  uid.toUpperCase();
  if (id <= 0 || uid.length() == 0 || uid.length() > 20) {
    server.send(400, "text/plain", "id und uid (Hex, max. 20 Zeichen) angeben");
    return;
  }
  spoolmanRequestLinkTag(id, uid.c_str());
  server.send(200, "text/plain", "ok");
}

static void handleQueueFlush() {
  if (!requireAdmin()) {
    return;
  }
  spoolmanQueueFlushNow();
  server.send(200, "text/plain", "ok");
}

static void handleQueueClear() {
  if (!requireAdmin()) {
    return;
  }
  spoolmanQueueClear();
  server.send(200, "text/plain", "ok");
}

// Debug: Spulenliste anfordern (POST) bzw. abholen (GET)
static void handleSpoolListRequest() {
  if (!requireAdmin()) {
    return;
  }
  spoolmanRequestList();
  server.send(200, "text/plain", "ok");
}

static void handleSpoolListGet() {
  if (!requireAdmin()) {
    return;
  }
  static SpoolListResult r;  // groß, nicht auf den Stack
  spoolmanListResult(r);
  JsonDocument doc;
  doc["state"] = r.state == SpoolmanState::Found ? "ok" : r.state == SpoolmanState::Busy ? "busy" : r.state == SpoolmanState::Error ? "error" : "idle";
  doc["total"] = r.total;
  doc["error"] = r.error;
  JsonArray items = doc["items"].to<JsonArray>();
  for (int i = 0; i < r.count; i++) {
    JsonObject o = items.add<JsonObject>();
    o["id"] = r.items[i].id;
    o["label"] = r.items[i].label;
    o["remaining"] = r.items[i].remaining;
  }
  sendJson(doc);
}

static void handleSpoolmanCheck() {
  if (!requireAdmin()) {
    return;
  }
  spoolmanCheckNow();
  server.send(200, "text/plain", "ok");
}

// ---- Sprache des Geräts ----
// Betriebsart: {"simple": true} = Einfache Waage, false = Smarte Waage. Nur der Admin.
static void handleModeSet() {
  if (!requireAdmin()) {
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) || !doc["simple"].is<bool>()) {
    server.send(400, "text/plain", "simple (true/false) fehlt");
    return;
  }
  devModeSetSimple(doc["simple"].as<bool>());
  server.send(200, "text/plain", "ok");
}

// Rolle "Nur wiegen" eines Benutzers (allow = 1 oder 0)
static void handleUsersWeighOnly() {
  if (!requireAdmin()) {
    return;
  }
  uint16_t id = (uint16_t)server.arg("id").toInt();
  if (!usersSetWeighOnly(id, server.arg("allow") == "1")) {
    server.send(404, "text/plain", "Benutzer nicht gefunden");
    return;
  }
  sessionUserChanged(id);
  server.send(200, "application/json", usersJson());
}

static void handleLanguageSet() {
  if (!requireAdmin()) {
    return;
  }
  String lang = server.arg("lang");
  if (lang == "de") {
    i18nSet(Lang::DE);
  } else if (lang == "en") {
    i18nSet(Lang::EN);
  } else {
    server.send(400, "text/plain", "lang muss de oder en sein");
    return;
  }
  server.send(200, "text/plain", "ok");
}

// ---- WLAN ----
static void handleWifiGet() {
  if (!requireAdmin()) {
    return;
  }
  JsonDocument doc;
  doc["ssid"] = wifiSsid();
  doc["saved"] = wifiSavedSsid();
  doc["source"] = wifiHasCredentials() ? "nvs" : "none";
  doc["connected"] = wifiConnected();
  doc["ap_active"] = wifiApActive();
  doc["ap_ssid"] = wifiApSsid();
  doc["ap_ip"] = wifiApIp();
  sendJson(doc);
}

// Blockiert einige Sekunden (Scan)
static void handleWifiScan() {
  if (!requireAdmin()) {
    return;
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", wifiScanJson());
}

static void handleWifiSave() {
  if (!requireAdmin()) {
    return;
  }
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "text/plain", "ungültiges JSON");
    return;
  }
  String error;
  if (!wifiSaveCredentials(doc["ssid"] | "", doc["password"] | "", error)) {
    server.send(400, "text/plain", error);
    return;
  }
  server.send(200, "text/plain", "ok");
  rebootAt = millis() + 800;  // neu starten, damit die neuen Zugangsdaten verwendet werden
}

static void handleWifiReset() {
  if (!requireAdmin()) {
    return;
  }
  wifiResetCredentials();
  server.send(200, "text/plain", "ok");
  rebootAt = millis() + 800;
}

static void handleWifiAp() {
  if (!requireAdmin()) {
    return;
  }
  if (server.arg("on") == "1") {
    wifiStartAp();
  } else {
    wifiStopAp();
  }
  server.send(200, "text/plain", "ok");
}

// ---- Bildschirmschoner ----
static void handleScreensaverGet() {
  if (!requireAdmin()) {
    return;
  }
  uint8_t dim, off;
  displayGetScreensaver(dim, off);
  JsonDocument doc;
  doc["dim_min"] = dim;
  doc["off_min"] = off;
  sendJson(doc);
}

static void handleScreensaverSet() {
  if (!requireAdmin()) {
    return;
  }
  int dim = server.arg("dim").toInt();
  int off = server.arg("off").toInt();
  if (dim < 0 || dim > 240 || off < 0 || off > 240 || (dim > 0 && off > 0 && off <= dim)) {
    server.send(400, "text/plain", "dim und off in Minuten (0 = nie, höchstens 240), off muss größer als dim sein");
    return;
  }
  displaySetScreensaver((uint8_t)dim, (uint8_t)off, true);
  server.send(200, "text/plain", "ok");
}

// ---- Log ----
// GET /api/log?since=N liefert alle Zeilen mit Folgenummer > N als Text, die höchste Nummer steht im Header X-Log-Seq
static void handleLog() {
  if (!requireAdmin()) {
    return;
  }
  uint32_t since = (uint32_t)server.arg("since").toInt();
  String out;
  uint32_t last = logRead(since, out);
  server.sendHeader("Cache-Control", "no-store");
  server.sendHeader("X-Log-Seq", String(last));
  server.send(200, "text/plain; charset=utf-8", out);
}

static void handleReboot() {
  if (!requireAdmin()) {
    return;
  }
  server.send(200, "text/plain", "ok");
  rebootAt = millis() + 600;
}

// ---- Online-Update (GitHub-Releases) ----
static void handleUpdateGet() {
  if (!requireAdmin()) {
    return;
  }
  FwUpdStatus st = fwUpdateStatus();
  static const char *STATES[] = {"idle", "checking", "ready", "downloading", "done", "error"};
  JsonDocument doc;
  doc["version"] = fwVersion();
  doc["state"] = STATES[(int)st.state];
  doc["checked"] = st.checked;
  doc["checked_s"] = st.checkedSecondsAgo;
  doc["available"] = st.available;
  doc["downgrade"] = st.downgrade;
  doc["channel"] = fwChannelName(st.channel);
  doc["latest_channel"] = fwChannelName(st.latestChannel);
  doc["no_release"] = st.noRelease;
  doc["latest"] = st.latest;
  doc["page_url"] = st.pageUrl;
  doc["done"] = st.done;
  doc["total"] = st.total;
  doc["error"] = st.error;
  if (server.hasArg("notes")) {
    bool cut = false;
    doc["notes"] = fwUpdateNotes(&cut);
    doc["notes_cut"] = cut;
  }
  sendJson(doc);
}

static void handleUpdateCheck() {
  if (!requireAdmin()) {
    return;
  }
  if (!wifiConnected()) {
    server.send(409, "text/plain", "Kein WLAN (im Einrichtungs-Hotspot ist keine Prüfung möglich)");
    return;
  }
  if (!fwUpdateCheckNow()) {
    server.send(409, "text/plain", "Update läuft bereits");
    return;
  }
  server.send(200, "text/plain", "ok");
}

// POST /api/update/channel?ch=stable|rc|test
static void handleUpdateChannel() {
  if (!requireAdmin()) {
    return;
  }
  FwChannel ch;
  if (!fwChannelFromName(server.arg("ch").c_str(), ch)) {
    server.send(400, "text/plain", "Kanal muss stable, rc oder test sein");
    return;
  }
  if (!fwUpdateSetChannel(ch)) {
    server.send(409, "text/plain", "Update läuft gerade");
    return;
  }
  server.send(200, "text/plain", "ok");
}

static void handleUpdateInstall() {
  if (!requireAdmin()) {
    return;
  }
  String error;
  if (!fwUpdateInstall(error)) {
    server.send(409, "text/plain", error);
    return;
  }
  server.send(200, "text/plain", "ok");
}

// Captive Portal: Im Hotspot leiten die Prüf-URLs der Betriebssysteme (und jeder fremde Host) auf die Einrichtungsseite
static bool captiveRedirect() {
  if (!wifiApActive()) {
    return false;
  }
  String portal = String("http://") + wifiApIp();
  if (server.hostHeader() == wifiApIp()) {
    return false;  // schon auf der Seite
  }
  server.sendHeader("Location", portal + "/#wifi", true);
  server.send(302, "text/plain", "");
  return true;
}

static void handleCaptive() {
  if (!captiveRedirect()) {
    server.send(404, "text/plain", "nicht gefunden");
  }
}

static void webStart() {
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html; charset=utf-8", WEB_PAGE); });
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/tare", HTTP_POST, handleTare);
  server.on("/api/calibrate", HTTP_POST, handleCalibrate);
  server.on("/api/sensor", HTTP_GET, handleSensorGet);
  server.on("/api/sensor", HTTP_POST, handleSensorSet);
  server.on("/api/theme", HTTP_GET, handleThemeGet);
  server.on("/api/theme", HTTP_POST, handleThemePost);
  server.on("/api/catalog", HTTP_GET, handleCatalogGet);
  server.on("/api/catalog", HTTP_POST, handleCatalogPost);
  server.on("/api/touch/reset", HTTP_POST, handleTouchReset);
  server.on("/api/users", HTTP_GET, handleUsersGet);
  server.on("/api/users", HTTP_POST, handleUsersAdd);
  server.on("/api/users/delete", HTTP_POST, handleUsersDelete);
  server.on("/api/users/write", HTTP_POST, handleUsersWrite);
  server.on("/api/users/lang", HTTP_POST, handleUsersLang);
  server.on("/api/users/tare", HTTP_POST, handleUsersTare);
  server.on("/api/users/pin", HTTP_POST, handleUsersPin);
  server.on("/api/users/catalog", HTTP_POST, handleUsersCatalog);
  server.on("/api/catalog/sync", HTTP_POST, handleCatalogSyncRequest);
  server.on("/api/catalog/sync", HTTP_GET, handleCatalogSyncGet);
  server.on("/api/generic", HTTP_POST, handleGenericSet);
  server.on("/api/me", HTTP_GET, handleMe);
  server.on("/api/login", HTTP_POST, handleLogin);
  server.on("/api/logout", HTTP_POST, handleLogout);
  server.on("/api/me/lang", HTTP_POST, handleMeLang);
  server.on("/api/me/pin", HTTP_POST, handleMePin);
  server.on("/api/admin", HTTP_GET, handleAdminGet);
  server.on("/api/admin/password", HTTP_POST, handleAdminPassword);
  server.on("/api/admin/lock", HTTP_POST, handleAdminLock);
  server.on("/api/admin/pin", HTTP_POST, handleAdminPin);
  server.on("/api/admin/tag", HTTP_POST, handleAdminTag);
  server.on("/api/nfc/write", HTTP_GET, handleNfcWriteStatus);
  server.on("/api/spoolman", HTTP_GET, handleSpoolmanGet);
  server.on("/api/spoolman", HTTP_POST, handleSpoolmanUrl);
  server.on("/api/spoolman/check", HTTP_POST, handleSpoolmanCheck);
  server.on("/api/spoolman/list", HTTP_POST, handleSpoolListRequest);
  server.on("/api/spoolman/list", HTTP_GET, handleSpoolListGet);
  server.on("/api/spoolman/queue/flush", HTTP_POST, handleQueueFlush);
  server.on("/api/spoolman/queue/clear", HTTP_POST, handleQueueClear);
  server.on("/api/spoolman/lookup", HTTP_POST, handleSpoolmanLookup);
  server.on("/api/spoolman/update", HTTP_POST, handleSpoolmanUpdate);
  server.on("/api/spoolman/link", HTTP_POST, handleSpoolmanLink);
  server.on("/api/language", HTTP_POST, handleLanguageSet);
  server.on("/api/mode", HTTP_POST, handleModeSet);
  server.on("/api/users/weigh", HTTP_POST, handleUsersWeighOnly);
  server.on("/api/wifi", HTTP_GET, handleWifiGet);
  server.on("/api/wifi", HTTP_POST, handleWifiSave);
  server.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
  server.on("/api/wifi/reset", HTTP_POST, handleWifiReset);
  server.on("/api/wifi/ap", HTTP_POST, handleWifiAp);
  server.on("/api/screensaver", HTTP_GET, handleScreensaverGet);
  server.on("/api/screensaver", HTTP_POST, handleScreensaverSet);
  server.on("/api/log", HTTP_GET, handleLog);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.on("/api/update", HTTP_GET, handleUpdateGet);
  server.on("/api/update/check", HTTP_POST, handleUpdateCheck);
  server.on("/api/update/install", HTTP_POST, handleUpdateInstall);
  server.on("/api/update/channel", HTTP_POST, handleUpdateChannel);
  // Prüf-URLs von Android, iOS/macOS, Windows und Firefox
  for (const char *path : {"/generate_204", "/gen_204", "/hotspot-detect.html", "/library/test/success.html",
                           "/connecttest.txt", "/ncsi.txt", "/fwlink", "/redirect", "/canonical.html", "/success.txt"}) {
    server.on(path, HTTP_GET, handleCaptive);
  }
  server.onNotFound([]() {
    if (!captiveRedirect()) {
      server.send(404, "text/plain", "nicht gefunden");
    }
  });
  static const char *collected[] = {"Cookie"};
  server.collectHeaders(collected, 1);
  server.begin();
  logPrintf("[WEB] Server läuft auf http://%s/\n", wifiConnected() ? wifiIp().c_str() : wifiApIp().c_str());
}

void webLoop() {
  if (!wifiConnected() && !wifiApActive()) {
    return;
  }
  if (!started) {
    webStart();
    started = true;
  }
  server.handleClient();
  if (rebootAt && millis() > rebootAt) {
    ESP.restart();
  }
}
