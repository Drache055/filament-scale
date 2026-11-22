#include "spoolman.h"
#include "util/log.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

#include "config.h"
#include "secrets.h"
#include "storage/catalog.h"
#include "storage/devmode.h"
#include "storage/queue.h"

#ifndef SPOOLMAN_URL
#define SPOOLMAN_URL "http://spoolman.local:7912"
#endif

namespace {

enum class JobType : uint8_t { Lookup, Update, LinkTag, List };

struct Job {
  JobType type;
  int id;
  char uid[21];
  float remaining;
  bool hasLocation;
  char location[65];
  bool hasOwner;
  char owner[24];
};

QueueHandle_t jobs = nullptr;
SemaphoreHandle_t mutex = nullptr;
String baseUrl;
SpoolmanStatus status;
uint32_t lastCheckMs = 0;
volatile bool checkRequested = true;
SpoolmanResult lookupResult;
SpoolmanResult writeResult;
SpoolListResult listResult;
CatalogSyncResult syncResult;
uint32_t syncDoneMs = 0;
uint32_t lastSyncMs = 0;
volatile bool syncRequested = false;

struct Http {
  int code = 0;
  String body;
};

Http request(const char *method, const String &path, const String &payload = "") {
  Http r;
  if (WiFi.status() != WL_CONNECTED) {
    r.code = -1;
    return r;
  }
  HTTPClient http;
  WiFiClient client;
  http.setConnectTimeout(SPOOLMAN_TIMEOUT_MS);
  http.setTimeout(SPOOLMAN_TIMEOUT_MS);
  if (!http.begin(client, baseUrl + path)) {
    r.code = -2;
    return r;
  }
  http.addHeader("Content-Type", "application/json");
  r.code = http.sendRequest(method, payload);
  if (r.code > 0) {
    r.body = http.getString();
  }
  http.end();
  return r;
}

// Fehlertext aus {"message": "..."} oder HTTP-Code
void errorText(const Http &r, char *out, size_t cap) {
  if (r.code == -1) {
    strlcpy(out, "kein WLAN", cap);
  } else if (r.code <= 0) {
    snprintf(out, cap, "Spoolman nicht erreichbar (%d)", r.code);
  } else {
    JsonDocument doc;
    if (!deserializeJson(doc, r.body) && doc["message"].is<const char *>()) {
      snprintf(out, cap, "HTTP %d: %s", r.code, doc["message"].as<const char *>());
    } else if (!deserializeJson(doc, r.body) && doc["detail"].is<const char *>()) {
      snprintf(out, cap, "HTTP %d: %s", r.code, doc["detail"].as<const char *>());
    } else {
      snprintf(out, cap, "HTTP %d", r.code);
    }
  }
}

float numberOr(JsonVariantConst v, float fallback) { return v.is<float>() ? v.as<float>() : fallback; }

// Extra-Werte sind JSON-kodierte Strings: "\"Fabian\"" -> Fabian
void decodeExtraString(JsonVariantConst v, char *out, size_t cap) {
  out[0] = '\0';
  const char *raw = v | "";
  if (!raw[0]) {
    return;
  }
  JsonDocument d;
  if (!deserializeJson(d, raw) && d.is<const char *>()) {
    strlcpy(out, d.as<const char *>(), cap);
  }
}

void parseSpool(JsonObjectConst o, SpoolInfo &s) {
  s = SpoolInfo();
  s.id = o["id"] | -1;
  s.remaining = numberOr(o["remaining_weight"], -1);
  s.initialWeight = numberOr(o["initial_weight"], -1);
  strlcpy(s.location, o["location"] | "", sizeof(s.location));
  s.archived = o["archived"] | false;
  JsonObjectConst f = o["filament"];
  strlcpy(s.name, f["name"] | "", sizeof(s.name));
  strlcpy(s.material, f["material"] | "", sizeof(s.material));
  strlcpy(s.colorHex, f["color_hex"] | "", sizeof(s.colorHex));
  s.density = numberOr(f["density"], 0);
  s.diameter = numberOr(f["diameter"], 0);
  s.filamentWeight = numberOr(f["weight"], -1);
  JsonObjectConst v = f["vendor"];
  strlcpy(s.vendor, v["name"] | "", sizeof(s.vendor));
  // Leerspulengewicht: Spule, dann Filament, dann Hersteller (CLAUDE.md 6.1)
  s.emptySpool = numberOr(o["spool_weight"], -1);
  if (s.emptySpool < 0) {
    s.emptySpool = numberOr(f["spool_weight"], -1);
  }
  if (s.emptySpool < 0) {
    s.emptySpool = numberOr(v["empty_spool_weight"], -1);
  }
  decodeExtraString(o["extra"][SPOOLMAN_OWNER_FIELD_KEY], s.owner, sizeof(s.owner));
}

void publish(SpoolmanResult &target, const SpoolmanResult &value) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  uint32_t seq = target.seq + 1;
  target = value;
  target.seq = seq;
  xSemaphoreGive(mutex);
}

void setBusy(SpoolmanResult &target, const char *uid) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  target.state = SpoolmanState::Busy;
  strlcpy(target.uid, uid ? uid : "", sizeof(target.uid));
  xSemaphoreGive(mutex);
}

// ---- Aufträge ----
void doCheck() {
  Http r = request("GET", "/api/v1/info");
  SpoolmanStatus next;
  strlcpy(next.url, baseUrl.c_str(), sizeof(next.url));
  next.everChecked = true;
  next.httpCode = r.code;
  if (r.code == 200) {
    JsonDocument doc;
    if (!deserializeJson(doc, r.body)) {
      next.reachable = true;
      strlcpy(next.version, doc["version"] | "", sizeof(next.version));
    }
  }
  xSemaphoreTake(mutex, portMAX_DELAY);
  bool changed = status.reachable != next.reachable;
  status = next;
  lastCheckMs = millis();
  xSemaphoreGive(mutex);
  if (changed) {
    logPrintf("[SPOOLMAN] %s (%s)\n", next.reachable ? "erreichbar" : "nicht erreichbar", baseUrl.c_str());
  }
}

void doLookup(const Job &job) {
  SpoolmanResult res;
  strlcpy(res.uid, job.uid, sizeof(res.uid));

  Http r;
  bool done = false;
  if (job.id >= 0) {
    r = request("GET", String("/api/v1/spool/") + job.id);
    if (r.code == 200) {
      JsonDocument doc;
      if (!deserializeJson(doc, r.body)) {
        parseSpool(doc.as<JsonObjectConst>(), res.spool);
        res.state = SpoolmanState::Found;
        done = true;
      }
    } else if (r.code != 404) {
      res.state = SpoolmanState::Error;
      res.httpStatus = r.code;
      errorText(r, res.error, sizeof(res.error));
      done = true;
    }
  }
  if (!done && job.uid[0]) {
    r = request("GET", String("/api/v1/spool?allow_archived=true&limit=1&tag=") + job.uid);
    res.httpStatus = r.code;
    if (r.code == 200) {
      JsonDocument doc;
      if (deserializeJson(doc, r.body)) {
        res.state = SpoolmanState::Error;
        strlcpy(res.error, "Antwort nicht lesbar", sizeof(res.error));
      } else if (doc.as<JsonArrayConst>().size() == 0) {
        res.state = SpoolmanState::NotFound;
      } else {
        parseSpool(doc[0].as<JsonObjectConst>(), res.spool);
        res.state = SpoolmanState::Found;
      }
    } else {
      res.state = SpoolmanState::Error;
      errorText(r, res.error, sizeof(res.error));
    }
  } else if (!done) {
    res.state = SpoolmanState::NotFound;
  }
  logPrintf("[SPOOLMAN] Lookup %s -> %s\n", job.uid,
                res.state == SpoolmanState::Found      ? "gefunden"
                : res.state == SpoolmanState::NotFound ? "unbekannt"
                                                       : res.error);
  publish(lookupResult, res);
}

// Legt das Extra-Feld "owner" an, falls es fehlt. true = Feld ist vorhanden.
bool ensureOwnerField(char *err, size_t cap, int &code) {
  Http r = request("GET", "/api/v1/field/spool");
  code = r.code;
  if (r.code != 200) {
    errorText(r, err, cap);
    return false;
  }
  JsonDocument doc;
  if (!deserializeJson(doc, r.body)) {
    for (JsonObjectConst f : doc.as<JsonArrayConst>()) {
      if (strcmp(f["key"] | "", SPOOLMAN_OWNER_FIELD_KEY) == 0) {
        return true;
      }
    }
  }
  r = request("POST", String("/api/v1/field/spool/") + SPOOLMAN_OWNER_FIELD_KEY,
              "{\"name\":\"Owner\",\"field_type\":\"text\"}");
  code = r.code;
  if (r.code != 200) {
    errorText(r, err, cap);
    return false;
  }
  logPrintln("[SPOOLMAN] Extra-Feld 'owner' angelegt");
  return true;
}

enum class Outcome : uint8_t { Ok, Retry, Permanent };

// Netzwerkfehler und Serverfehler (5xx) lohnen einen späteren Versuch, Clientfehler (4xx) nicht
bool retryable(int code) { return code <= 0 || code >= 500; }

Outcome performUpdate(const Job &job, SpoolmanResult &res) {
  char err[96] = "";
  int code = 0;

  // Aktuelle Spule lesen: Extra-Felder müssen zusammengeführt werden, ein PATCH mit "extra" ersetzt sie alle
  JsonDocument patch;
  if (job.remaining >= 0) {
    patch["remaining_weight"] = job.remaining;
  }
  if (job.hasLocation) {
    patch["location"] = job.location;
  }
  if (job.hasOwner) {
    if (!ensureOwnerField(err, sizeof(err), code)) {
      res.state = SpoolmanState::Error;
      res.httpStatus = code;
      snprintf(res.error, sizeof(res.error), "Owner-Feld: %s", err);
      return retryable(code) ? Outcome::Retry : Outcome::Permanent;
    }
    Http cur = request("GET", String("/api/v1/spool/") + job.id);
    if (cur.code != 200) {
      res.state = SpoolmanState::Error;
      res.httpStatus = cur.code;
      errorText(cur, res.error, sizeof(res.error));
      return retryable(cur.code) ? Outcome::Retry : Outcome::Permanent;
    }
    JsonDocument curDoc;
    deserializeJson(curDoc, cur.body);
    JsonObject extra = patch["extra"].to<JsonObject>();
    for (JsonPairConst kv : curDoc["extra"].as<JsonObjectConst>()) {
      extra[kv.key()] = kv.value();
    }
    String encoded;  // JSON-kodierter String: "Fabian" mit Anführungszeichen
    JsonDocument s;
    s.set(job.owner);
    serializeJson(s, encoded);
    extra[SPOOLMAN_OWNER_FIELD_KEY] = encoded;
  }

  String body;
  serializeJson(patch, body);
  Http r = request("PATCH", String("/api/v1/spool/") + job.id, body);
  res.httpStatus = r.code;
  if (r.code == 200) {
    JsonDocument doc;
    if (!deserializeJson(doc, r.body)) {
      parseSpool(doc.as<JsonObjectConst>(), res.spool);
    }
    res.state = SpoolmanState::Found;
    return Outcome::Ok;
  }
  res.state = SpoolmanState::Error;
  errorText(r, res.error, sizeof(res.error));
  if (r.code == 400 && job.remaining >= 0) {  // häufigste Ursache: Filamentsorte ohne Füllgewicht
    strlcat(res.error, " (Fuellgewicht der Sorte gesetzt?)", sizeof(res.error));
  }
  return retryable(r.code) ? Outcome::Retry : Outcome::Permanent;
}

uint32_t nowEpoch() {
  time_t t = time(nullptr);
  return t > 1700000000 ? (uint32_t)t : 0;  // 0 = Uhr noch nicht gestellt
}

void doUpdate(const Job &job) {
  SpoolmanResult res;
  Outcome outcome = performUpdate(job, res);
  if (outcome == Outcome::Retry) {
    QueueEntry e;
    e.spoolId = job.id;
    e.remaining = job.remaining;
    e.hasLocation = job.hasLocation;
    strlcpy(e.location, job.location, sizeof(e.location));
    strlcpy(e.owner, job.hasOwner ? job.owner : "", sizeof(e.owner));
    e.timestamp = nowEpoch();
    if (queueAdd(e)) {
      res.queued = true;
      strlcat(res.error, " - in Warteschlange", sizeof(res.error));
    }
  }
  logPrintf("[SPOOLMAN] Update Spule %d -> %s\n", job.id,
            res.state == SpoolmanState::Found ? "ok" : res.error);
  publish(writeResult, res);
}

uint32_t lastQueueTry = 0;
volatile bool queueFlushRequested = false;

// Arbeitet die Warteschlange ab, solange Spoolman antwortet
void processQueue() {
  QueueEntry e;
  while (queuePeek(e)) {
    Job job = {};
    job.type = JobType::Update;
    job.id = e.spoolId;
    job.remaining = e.remaining;
    job.hasLocation = e.hasLocation;
    strlcpy(job.location, e.location, sizeof(job.location));
    if (e.owner[0]) {
      job.hasOwner = true;
      strlcpy(job.owner, e.owner, sizeof(job.owner));
    }
    SpoolmanResult res;
    Outcome o = performUpdate(job, res);
    if (o == Outcome::Ok) {
      queuePopFront();
      logPrintf("[QUEUE] Update für Spule %d nachgeholt\n", e.spoolId);
    } else if (o == Outcome::Permanent) {
      queuePopFront();  // lässt sich nie ausführen (z. B. Spule gelöscht), nicht ewig wiederholen
      queueCountDropped();
      logPrintf("[QUEUE] Update für Spule %d verworfen: %s\n", e.spoolId, res.error);
    } else {
      logPrintf("[QUEUE] Spoolman nicht erreichbar, später erneut (%s)\n", res.error);
      return;
    }
  }
}

void doList() {
  SpoolListResult res;
  res.state = SpoolmanState::Error;
  if (WiFi.status() != WL_CONNECTED) {
    strlcpy(res.error, "kein WLAN", sizeof(res.error));
  } else {
    HTTPClient http;
    WiFiClient client;
    http.setConnectTimeout(SPOOLMAN_TIMEOUT_MS);
    http.setTimeout(SPOOLMAN_TIMEOUT_MS);
    http.useHTTP10(true);  // keine Chunked-Antwort, damit der Stream direkt lesbar ist
    const char *keys[] = {"x-total-count"};
    http.collectHeaders(keys, 1);
    String url = baseUrl + "/api/v1/spool?limit=" + SPOOL_LIST_MAX;
    if (!http.begin(client, url)) {
      strlcpy(res.error, "Spoolman nicht erreichbar", sizeof(res.error));
    } else {
      int code = http.GET();
      if (code == 200) {
        // Nur die benötigten Felder parsen, die Antwort ist sonst sehr groß
        JsonDocument filter;
        filter[0]["id"] = true;
        filter[0]["remaining_weight"] = true;
        filter[0]["filament"]["material"] = true;
        filter[0]["filament"]["name"] = true;
        filter[0]["filament"]["vendor"]["name"] = true;
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
        if (err) {
          snprintf(res.error, sizeof(res.error), "Antwort nicht lesbar (%s)", err.c_str());
        } else {
          res.state = SpoolmanState::Found;
          res.total = http.header("x-total-count").toInt();
          for (JsonObjectConst o : doc.as<JsonArrayConst>()) {
            if (res.count >= SPOOL_LIST_MAX) {
              break;
            }
            SpoolListItem &it = res.items[res.count++];
            it.id = o["id"] | -1;
            it.remaining = o["remaining_weight"].is<float>() ? o["remaining_weight"].as<float>() : -1;
            const char *vendor = o["filament"]["vendor"]["name"] | "";
            const char *material = o["filament"]["material"] | "";
            const char *name = o["filament"]["name"] | "";
            snprintf(it.label, sizeof(it.label), "%s %s", vendor, material[0] ? material : name);
          }
          if (res.total < res.count) {
            res.total = res.count;
          }
        }
      } else {
        Http fake;
        fake.code = code;
        errorText(fake, res.error, sizeof(res.error));
      }
      http.end();
    }
  }
  logPrintf("[SPOOLMAN] Liste: %s (%d von %d)\n", res.state == SpoolmanState::Found ? "ok" : res.error, res.count, res.total);
  xSemaphoreTake(mutex, portMAX_DELAY);
  uint32_t seq = listResult.seq + 1;
  listResult = res;
  listResult.seq = seq;
  xSemaphoreGive(mutex);
}

// GET mit Filter, direkt aus dem Stream gelesen (HTTP/1.0, keine Chunked-Antwort)
bool fetchFiltered(const String &path, JsonDocument &filter, JsonDocument &doc, char *err, size_t cap) {
  HTTPClient http;
  WiFiClient client;
  http.setConnectTimeout(SPOOLMAN_TIMEOUT_MS);
  http.setTimeout(SPOOLMAN_TIMEOUT_MS);
  http.useHTTP10(true);
  if (!http.begin(client, baseUrl + path)) {
    strlcpy(err, "Spoolman nicht erreichbar", cap);
    return false;
  }
  int code = http.GET();
  bool ok = false;
  if (code == 200) {
    DeserializationError e = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    if (e) {
      snprintf(err, cap, "Antwort nicht lesbar (%s)", e.c_str());
    } else {
      ok = true;
    }
  } else {
    Http fake;
    fake.code = code;
    errorText(fake, err, cap);
  }
  http.end();
  return ok;
}

// Übliche Düsentemperaturen, wenn Spoolman für ein Material keine Temperatur kennt
void defaultTemps(const String &material, int &lo, int &hi) {
  String m = material;
  m.toUpperCase();
  struct {
    const char *name;
    int lo, hi;
  } known[] = {{"PLA", 190, 220}, {"PETG", 220, 250}, {"ABS", 230, 260}, {"ASA", 240, 260},
               {"TPU", 210, 230}, {"PA", 250, 280},   {"PC", 260, 300}};
  for (auto &k : known) {
    if (m == k.name) {
      lo = k.lo;
      hi = k.hi;
      return;
    }
  }
  lo = 190;
  hi = 230;
}

void doCatalogSync() {
  constexpr int MAX_VENDORS = 100, MAX_MATERIALS = 60;
  CatalogSyncResult res;
  res.state = SpoolmanState::Error;
  if (WiFi.status() != WL_CONNECTED) {
    strlcpy(res.error, "kein WLAN", sizeof(res.error));
  } else {
    String makers[MAX_VENDORS];
    int makerCount = 0;
    static CatalogMaterial mats[MAX_MATERIALS];
    static int tempLo[MAX_MATERIALS], tempHi[MAX_MATERIALS];
    int matCount = 0;
    bool ok = true;
    {
      JsonDocument filter, doc;
      filter[0]["name"] = true;
      ok = fetchFiltered(String("/api/v1/vendor?limit=") + MAX_VENDORS, filter, doc, res.error, sizeof(res.error));
      if (ok) {
        for (JsonObjectConst o : doc.as<JsonArrayConst>()) {
          const char *name = o["name"] | "";
          if (name[0] && makerCount < MAX_VENDORS) {
            makers[makerCount++] = name;
          }
        }
      }
    }
    if (ok) {
      JsonDocument filter, doc;
      filter[0]["material"] = true;
      filter[0]["settings_extruder_temp"] = true;
      ok = fetchFiltered("/api/v1/filament?limit=200", filter, doc, res.error, sizeof(res.error));
      if (ok) {
        for (JsonObjectConst o : doc.as<JsonArrayConst>()) {
          String material(o["material"] | "");
          material.trim();
          if (material.length() == 0) {
            continue;
          }
          if (material.length() >= CATALOG_NAME_LEN) {  // passt nicht in den Katalog (und nicht auf den Tag)
            res.skipped++;
            continue;
          }
          int idx = -1;
          for (int i = 0; i < matCount; i++) {
            if (material.equalsIgnoreCase(mats[i].name)) {
              idx = i;
            }
          }
          if (idx < 0) {
            if (matCount >= MAX_MATERIALS) {
              res.skipped++;
              continue;
            }
            idx = matCount++;
            strlcpy(mats[idx].name, material.c_str(), sizeof(mats[idx].name));
            tempLo[idx] = tempHi[idx] = 0;
          }
          int t = o["settings_extruder_temp"] | 0;
          if (t > 0 && t <= 500) {
            tempLo[idx] = tempLo[idx] ? min(tempLo[idx], t) : t;
            tempHi[idx] = max(tempHi[idx], t);
          }
        }
        for (int i = 0; i < matCount; i++) {
          if (tempLo[i] > 0) {
            // Gibt es nur einen Wert, ergibt er einen Bereich von +/- 10 °C (Annahme, im Katalog änderbar)
            mats[i].minTemp = tempLo[i] == tempHi[i] ? max(0, tempLo[i] - 10) : tempLo[i];
            mats[i].maxTemp = tempLo[i] == tempHi[i] ? tempHi[i] + 10 : tempHi[i];
          } else {
            defaultTemps(String(mats[i].name), mats[i].minTemp, mats[i].maxTemp);
          }
        }
      }
    }
    if (ok) {
      CatalogMergeResult m = catalogMerge(makers, makerCount, mats, matCount);
      res.vendorsRead = makerCount;
      res.materialsRead = matCount;
      res.addedMakers = m.addedMakers;
      res.addedMaterials = m.addedMaterials;
      res.skipped += m.skipped;
      if (m.saved) {
        res.state = SpoolmanState::Found;
      } else {
        strlcpy(res.error, m.error.c_str(), sizeof(res.error));
      }
    }
  }
  if (res.state == SpoolmanState::Found) {
    if (res.addedMakers || res.addedMaterials || res.skipped) {
      logPrintf("[SPOOLMAN] Katalogabgleich: +%d Hersteller, +%d Typen (gelesen: %d/%d, nicht übernommen: %d)\n",
                res.addedMakers, res.addedMaterials, res.vendorsRead, res.materialsRead, res.skipped);
    }
  } else {
    logPrintf("[SPOOLMAN] Katalogabgleich fehlgeschlagen: %s\n", res.error);
  }
  xSemaphoreTake(mutex, portMAX_DELAY);
  uint32_t seq = syncResult.seq + 1;
  syncResult = res;
  syncResult.seq = seq;
  syncDoneMs = millis();
  xSemaphoreGive(mutex);
}

void doLinkTag(const Job &job) {
  SpoolmanResult res;
  strlcpy(res.uid, job.uid, sizeof(res.uid));
  String body = String("{\"uid\":\"") + job.uid + "\",\"format\":\"ntag\"}";
  Http r = request("POST", String("/api/v1/spool/") + job.id + "/tag", body);
  res.httpStatus = r.code;
  if (r.code == 200 || r.code == 201) {
    res.state = SpoolmanState::Found;
  } else {
    res.state = SpoolmanState::Error;
    errorText(r, res.error, sizeof(res.error));  // 409: Tag gehört schon zu einer anderen Spule
  }
  logPrintf("[SPOOLMAN] Tag %s -> Spule %d: %s\n", job.uid, job.id,
                res.state == SpoolmanState::Found ? "verknüpft" : res.error);
  publish(writeResult, res);
}

void spoolmanTask(void *) {
  for (;;) {
    Job job;
    if (xQueueReceive(jobs, &job, pdMS_TO_TICKS(500)) == pdTRUE) {
      switch (job.type) {
        case JobType::Lookup: doLookup(job); break;
        case JobType::Update: doUpdate(job); break;
        case JobType::LinkTag: doLinkTag(job); break;
        case JobType::List: doList(); break;
      }
      continue;
    }
    if (devModeSimple()) {
      continue;  // Einfache Waage: keine Erreichbarkeitsprüfung, kein Abgleich, Warteschlange ruht
    }
    if (checkRequested || millis() - lastCheckMs > SPOOLMAN_CHECK_MS) {
      checkRequested = false;
      if (WiFi.status() == WL_CONNECTED) {
        doCheck();
      }
    }
    if ((syncRequested || lastSyncMs == 0 || millis() - lastSyncMs > CATALOG_SYNC_MS) && WiFi.status() == WL_CONNECTED &&
        status.reachable) {
      syncRequested = false;
      lastSyncMs = millis();
      doCatalogSync();
    }
    if ((queueFlushRequested || millis() - lastQueueTry > QUEUE_RETRY_MS) && WiFi.status() == WL_CONNECTED &&
        status.reachable && queueCount() > 0) {
      queueFlushRequested = false;
      lastQueueTry = millis();
      processQueue();
    }
  }
}

bool enqueue(const Job &job, SpoolmanResult &target, const char *uid) {
  setBusy(target, uid);
  if (xQueueSend(jobs, &job, 0) != pdTRUE) {
    SpoolmanResult res;
    res.state = SpoolmanState::Error;
    strlcpy(res.error, "Warteschlange voll", sizeof(res.error));
    publish(target, res);
    return false;
  }
  return true;
}

}  // namespace

void spoolmanBegin() {
  mutex = xSemaphoreCreateMutex();
  jobs = xQueueCreate(4, sizeof(Job));
  Preferences prefs;
  prefs.begin("spoolman", false);
  baseUrl = prefs.getString("url", SPOOLMAN_URL);
  prefs.end();
  logPrintf("[SPOOLMAN] URL %s\n", baseUrl.c_str());
  xTaskCreatePinnedToCore(spoolmanTask, "spoolman", 16384, nullptr, 1, nullptr, 0);
}

String spoolmanUrl() { return baseUrl; }

bool spoolmanSetUrl(const char *url, String &error) {
  String u(url ? url : "");
  u.trim();
  while (u.endsWith("/")) {
    u.remove(u.length() - 1);
  }
  if (!(u.startsWith("http://")) || u.length() < 11 || u.length() >= sizeof(status.url)) {
    error = "URL muss mit http:// beginnen (HTTPS wird nicht unterstützt) und darf nicht zu lang sein";
    return false;
  }
  Preferences prefs;
  prefs.begin("spoolman", false);
  prefs.putString("url", u);
  prefs.end();
  baseUrl = u;
  spoolmanCheckNow();
  return true;
}

void spoolmanCheckNow() { checkRequested = true; }

SpoolmanStatus spoolmanStatus() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  SpoolmanStatus copy = status;
  copy.checkedSecondsAgo = status.everChecked ? (millis() - lastCheckMs) / 1000 : 0;
  xSemaphoreGive(mutex);
  strlcpy(copy.url, baseUrl.c_str(), sizeof(copy.url));
  return copy;
}

void spoolmanRequestLookup(const char *uid, int spoolmanId) {
  Job job = {};
  job.type = JobType::Lookup;
  job.id = spoolmanId;
  strlcpy(job.uid, uid ? uid : "", sizeof(job.uid));
  enqueue(job, lookupResult, job.uid);
}

SpoolmanResult spoolmanLookupResult() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  SpoolmanResult copy = lookupResult;
  xSemaphoreGive(mutex);
  return copy;
}

void spoolmanRequestUpdate(int spoolId, float remaining, const char *location, const char *owner) {
  Job job = {};
  job.type = JobType::Update;
  job.id = spoolId;
  job.remaining = remaining;
  if (location) {
    job.hasLocation = true;
    strlcpy(job.location, location, sizeof(job.location));
  }
  if (owner && owner[0]) {
    job.hasOwner = true;
    strlcpy(job.owner, owner, sizeof(job.owner));
  }
  enqueue(job, writeResult, "");
}

void spoolmanRequestLinkTag(int spoolId, const char *uid) {
  Job job = {};
  job.type = JobType::LinkTag;
  job.id = spoolId;
  strlcpy(job.uid, uid ? uid : "", sizeof(job.uid));
  enqueue(job, writeResult, job.uid);
}

SpoolmanResult spoolmanWriteResult() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  SpoolmanResult copy = writeResult;
  xSemaphoreGive(mutex);
  return copy;
}

int spoolmanQueueCount() { return queueCount(); }
uint32_t spoolmanQueueDropped() { return queueDropped(); }

void spoolmanQueueFlushNow() {
  queueFlushRequested = true;
  checkRequested = true;
}

void spoolmanQueueClear() { queueClear(); }

void spoolmanRequestList() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  listResult.state = SpoolmanState::Busy;
  xSemaphoreGive(mutex);
  Job job = {};
  job.type = JobType::List;
  if (xQueueSend(jobs, &job, 0) != pdTRUE) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    listResult.state = SpoolmanState::Error;
    strlcpy(listResult.error, "Warteschlange voll", sizeof(listResult.error));
    listResult.seq++;
    xSemaphoreGive(mutex);
  }
}

void spoolmanListResult(SpoolListResult &out) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  out = listResult;
  xSemaphoreGive(mutex);
}

void spoolmanRequestCatalogSync() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  syncResult.state = SpoolmanState::Busy;
  xSemaphoreGive(mutex);
  syncRequested = true;
}

CatalogSyncResult spoolmanCatalogSyncResult() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  CatalogSyncResult out = syncResult;
  out.secondsAgo = syncDoneMs ? (millis() - syncDoneMs) / 1000 : 0;
  xSemaphoreGive(mutex);
  return out;
}
