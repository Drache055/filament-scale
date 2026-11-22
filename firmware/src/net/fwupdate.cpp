#include "fwupdate.h"

#include <HTTPClient.h>
#include <Preferences.h>
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_ota_ops.h>
#include <freertos/semphr.h>
#include <mbedtls/sha256.h>

#include <algorithm>

#include "config.h"
#include "util/log.h"
#include "wifi.h"

#ifndef FW_VERSION
#define FW_VERSION "dev"
#endif

namespace {

SemaphoreHandle_t mutex = nullptr;
TaskHandle_t task = nullptr;
FwUpdStatus status;
FwRelease release;  // zuletzt gefundene Release (unter mutex)
uint32_t lastCheckMs = 0;
volatile bool checkRequested = false;
volatile bool installRequested = false;

template <typename F>
void locked(F f) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  f();
  xSemaphoreGive(mutex);
}

void setError(const char *msg) {
  logPrintf("[UPDATE] Fehler: %s\n", msg);
  locked([&]() {
    status.state = FwUpdState::Error;
    strlcpy(status.error, msg, sizeof(status.error));
  });
}

String userAgent() { return String("filament-scale/") + FW_VERSION; }

// Sucht die neueste Release im gewählten Kanal. auto_: automatische Prüfung (nur Log, kein Fehlerzustand auf der Webseite)
bool check(bool auto_) {
  FwChannel ch = FwChannel::Stable;
  locked([&]() {
    ch = status.channel;
    if (status.state != FwUpdState::Downloading) {
      status.state = FwUpdState::Checking;
      status.error[0] = 0;
    }
  });
  WiFiClientSecure client;
  client.useBuiltinCACertBundle();  // Root-Zertifikate aus dem Bundle des Arduino-Cores
  HTTPClient http;
  http.useHTTP10(true);  // keine Chunked-Übertragung
  http.setTimeout(FW_UPDATE_TIMEOUT_MS);
  http.setUserAgent(userAgent());
  // Liste statt /releases/latest, weil diese Pre-Releases auslässt; neueste zuerst
  String url = String("https://api.github.com/repos/") + FW_UPDATE_REPO + "/releases?per_page=" + FW_UPDATE_LIST;
  if (!http.begin(client, url)) {
    setError("GitHub-Adresse ungültig");
    return false;
  }
  http.addHeader("Accept", "application/vnd.github+json");
  http.addHeader("X-GitHub-Api-Version", "2022-11-28");
  int code = http.GET();
  if (code != 200) {
    char msg[96];
    if (code < 0) {
      snprintf(msg, sizeof(msg), "GitHub nicht erreichbar (%s)", HTTPClient::errorToString(code).c_str());
    } else if (code == 403 || code == 429) {
      snprintf(msg, sizeof(msg), "GitHub-Abfragelimit erreicht (HTTP %d), später erneut versuchen", code);
    } else {
      snprintf(msg, sizeof(msg), "GitHub antwortet mit HTTP %d", code);
    }
    http.end();
    if (auto_) {
      logPrintf("[UPDATE] Automatische Prüfung: %s\n", msg);
      locked([]() { status.state = status.available ? FwUpdState::Ready : FwUpdState::Idle; });
    } else {
      setError(msg);
    }
    return false;
  }
  FwRelease *r = new FwRelease();  // ca. 4 KB, nicht auf den Task-Stack
  const char *err = nullptr;
  bool found = false;
  // Direkt aus dem Stream parsen, die ganze Antwort (über 100 KB) wird nie als String gehalten
  bool ok = fwParseReleases(http.getStream(), ch, *r, found, err);
  http.end();
  if (!ok) {
    delete r;
    setError(err ? err : "Releases nicht lesbar");
    return false;
  }
  bool avail = found && fwUpdateAvailable(r->tag, FW_VERSION);
  bool down = found && fwIsDowngrade(r->tag, FW_VERSION);
  if (found) {
    logPrintf("[UPDATE] Kanal %s: neueste Version %s, installiert %s%s\n", fwChannelName(ch), r->tag, FW_VERSION,
              avail ? ", Update verfügbar" : down ? ", älter als installiert" : "");
  } else {
    logPrintf("[UPDATE] Kanal %s: keine passende Release auf GitHub\n", fwChannelName(ch));
  }
  locked([&]() {
    if (status.channel != ch) {
      return;  // Kanal wurde während der Abfrage gewechselt, die neue Prüfung folgt
    }
    release = *r;  // bei !found leer
    status.checked = true;
    status.noRelease = !found;
    status.available = avail;
    status.downgrade = down;
    status.latestChannel = r->channel;
    strlcpy(status.latest, r->tag, sizeof(status.latest));
    strlcpy(status.pageUrl, r->pageUrl, sizeof(status.pageUrl));
    if (status.state != FwUpdState::Downloading) {
      status.state = avail ? FwUpdState::Ready : FwUpdState::Idle;
    }
  });
  delete r;
  lastCheckMs = millis();
  return true;
}

// GET mit eigener Behandlung der Weiterleitungen (GitHub leitet Assets auf *.githubusercontent.com um);
// jedes Ziel muss https und ein GitHub-Host sein. Liefert den HTTP-Code der letzten Antwort.
int openDownload(HTTPClient &http, WiFiClientSecure &client, String url) {
  for (int hop = 0; hop < 6; hop++) {
    if (!fwAllowedUrl(url.c_str())) {
      return -1000;
    }
    http.end();
    client.stop();
    http.useHTTP10(true);
    http.setTimeout(FW_UPDATE_TIMEOUT_MS);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.setUserAgent(userAgent());
    if (!http.begin(client, url)) {
      return -1001;
    }
    int code = http.GET();
    if (code == 301 || code == 302 || code == 303 || code == 307 || code == 308) {
      url = http.getLocation();
      continue;
    }
    return code;
  }
  return -1002;
}

void install() {
  struct {
    char tag[FW_TAG_LEN];
    char otaUrl[FW_URL_LEN];
    uint32_t otaSize;
    char sha256[65];
  } r;  // nur das Nötige, die Release Notes bleiben vom Task-Stack fern
  locked([&]() {
    strlcpy(r.tag, release.tag, sizeof(r.tag));
    strlcpy(r.otaUrl, release.otaUrl, sizeof(r.otaUrl));
    r.otaSize = release.otaSize;
    strlcpy(r.sha256, release.otaSha256, sizeof(r.sha256));
  });
  if (!r.otaUrl[0]) {
    setError("Keine Version gefunden, erst nach Updates suchen");
    return;
  }
  if (!r.sha256[0]) {
    setError("Release hat keine Prüfsumme (SHA-256), Installation aus Sicherheitsgründen abgelehnt");
    return;
  }
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *next = esp_ota_get_next_update_partition(nullptr);
  if (!next || next == running) {
    setError("Partitionstabelle hat keinen zweiten App-Slot");
    return;
  }
  logPrintf("[UPDATE] Lade %s nach %s\n", r.tag, next->label);
  locked([&]() {
    status.state = FwUpdState::Downloading;
    status.done = 0;
    status.total = r.otaSize;
    status.error[0] = 0;
  });

  WiFiClientSecure client;
  client.useBuiltinCACertBundle();
  HTTPClient http;
  int code = openDownload(http, client, r.otaUrl);
  if (code != 200) {
    char msg[96];
    if (code == -1000) {
      snprintf(msg, sizeof(msg), "Download-Adresse ist kein GitHub-Server");
    } else if (code < 0) {
      snprintf(msg, sizeof(msg), "Download fehlgeschlagen (%s)", code <= -1000 ? "Weiterleitung" : HTTPClient::errorToString(code).c_str());
    } else {
      snprintf(msg, sizeof(msg), "Download fehlgeschlagen (HTTP %d)", code);
    }
    http.end();
    setError(msg);
    return;
  }
  int len = http.getSize();
  if (len <= 0) {
    http.end();
    setError("Server meldet keine Dateigröße");
    return;
  }
  if ((uint32_t)len > next->size) {
    http.end();
    setError("Image ist größer als der App-Slot");
    return;
  }
  if (r.otaSize && (uint32_t)len != r.otaSize) {
    http.end();
    setError("Dateigröße passt nicht zur Release");
    return;
  }
  if (!Update.begin(len, U_FLASH)) {
    char msg[96];
    snprintf(msg, sizeof(msg), "Update nicht startbar: %s", Update.errorString());
    http.end();
    setError(msg);
    return;
  }
  locked([&]() { status.total = len; });

  NetworkClient *stream = http.getStreamPtr();
  const size_t BUF = 4096;
  uint8_t *buf = (uint8_t *)malloc(BUF);
  uint32_t written = 0, lastData = millis();
  uint8_t lastPct = 255;
  const char *fail = nullptr;
  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);  // 0 = SHA-256
  if (!buf) {
    fail = "Kein Speicher frei";
  }
  while (!fail && written < (uint32_t)len) {
    size_t avail = stream->available();
    if (!avail) {
      if (!stream->connected() && !stream->available()) {
        fail = "Verbindung abgebrochen";
      } else if (millis() - lastData > FW_UPDATE_TIMEOUT_MS) {
        fail = "Zeitüberschreitung beim Download";
      }
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }
    size_t want = std::min(std::min(avail, BUF), (size_t)(len - written));
    int got = stream->read(buf, want);
    if (got <= 0) {
      continue;
    }
    lastData = millis();
    if (Update.write(buf, got) != (size_t)got) {
      fail = Update.errorString();
      break;
    }
    mbedtls_sha256_update(&sha, buf, got);
    written += got;
    locked([&]() { status.done = written; });
    uint8_t pct = (uint64_t)written * 100 / len;
    if (pct != lastPct && pct % 10 == 0) {
      lastPct = pct;
      logPrintf("[UPDATE] %u%%\n", pct);
    }
  }
  free(buf);
  http.end();
  uint8_t digest[32];
  mbedtls_sha256_finish(&sha, digest);
  mbedtls_sha256_free(&sha);
  // Prüfsumme vor Update.end(), das erst den neuen Slot aktiviert; bei Abweichung bleibt die alte Firmware aktiv
  if (!fail) {
    char hex[65];
    fwHashToHex(digest, hex);
    if (strcmp(hex, r.sha256) != 0) {
      logPrintf("[UPDATE] SHA-256 erwartet %s, berechnet %s\n", r.sha256, hex);
      fail = "Prüfsumme (SHA-256) stimmt nicht mit der Release überein";
    }
  }
  // end() prüft das Image (Magic, Prüfsumme) und setzt erst dann den neuen Slot als Boot-Partition
  if (!fail && !Update.end()) {
    fail = Update.errorString();
  }
  if (fail) {
    Update.abort();
    char msg[96];
    snprintf(msg, sizeof(msg), "Update abgebrochen: %s. Die bisherige Firmware bleibt aktiv.", fail);
    setError(msg);
    return;
  }
  logPrintf("[UPDATE] %s installiert, Neustart\n", r.tag);
  locked([]() { status.state = FwUpdState::Done; });
  vTaskDelay(pdMS_TO_TICKS(2500));  // die Webseite soll "Fertig" noch abholen können
  ESP.restart();
}

void updateTask(void *) {
  uint32_t connectedSince = 0;
  bool autoDue = true;
  uint32_t nextAutoMs = 0;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(1000));
    if (!wifiConnected()) {
      connectedSince = 0;
      continue;
    }
    if (!connectedSince) {
      connectedSince = millis();
    }
    if (installRequested) {
      installRequested = false;
      checkRequested = false;
      install();
      continue;
    }
    if (checkRequested) {
      checkRequested = false;
      check(false);
      autoDue = false;
      nextAutoMs = millis() + FW_UPDATE_INTERVAL_MS;
      continue;
    }
    // Automatisch: kurz nach dem Verbinden, dann täglich
    if (autoDue && millis() - connectedSince > FW_UPDATE_FIRST_MS) {
      autoDue = false;
      nextAutoMs = millis() + (check(true) ? FW_UPDATE_INTERVAL_MS : FW_UPDATE_RETRY_MS);
    } else if (!autoDue && (int32_t)(millis() - nextAutoMs) > 0) {
      autoDue = true;
      connectedSince = millis() - FW_UPDATE_FIRST_MS - 1;
    }
  }
}

}  // namespace

void fwUpdateBegin() {
  mutex = xSemaphoreCreateMutex();
  Preferences prefs;
  prefs.begin("fwupd", true);
  uint8_t ch = prefs.getUChar("ch", 0);
  prefs.end();
  status.channel = ch <= (uint8_t)FwChannel::Test ? (FwChannel)ch : FwChannel::Stable;
  logPrintf("[UPDATE] Firmware-Version %s, Kanal %s\n", FW_VERSION, fwChannelName(status.channel));
  // TLS (mbedTLS) braucht Stack; Kern 0 wie die übrigen Netz-Tasks
  xTaskCreatePinnedToCore(updateTask, "fwupdate", 12288, nullptr, 1, &task, 0);
}

const char *fwVersion() { return FW_VERSION; }

FwUpdStatus fwUpdateStatus() {
  FwUpdStatus copy;
  locked([&]() {
    copy = status;
    copy.checkedSecondsAgo = status.checked ? (millis() - lastCheckMs) / 1000 : 0;
  });
  return copy;
}

String fwUpdateNotes(bool *cut) {
  String s;
  locked([&]() {
    s = release.notes;
    if (cut) {
      *cut = release.notesCut;
    }
  });
  return s;
}

bool fwUpdateCheckNow() {
  FwUpdState st = fwUpdateStatus().state;
  if (st == FwUpdState::Downloading || st == FwUpdState::Done) {
    return false;
  }
  checkRequested = true;
  xTaskNotifyGive(task);
  return true;
}

bool fwUpdateSetChannel(FwChannel ch) {
  bool busy = false;
  locked([&]() {
    busy = status.state == FwUpdState::Downloading || status.state == FwUpdState::Done || installRequested;
    if (busy || status.channel == ch) {
      return;
    }
    status.channel = ch;
    // Ergebnis des alten Kanals verwerfen, damit nichts Falsches installiert wird
    release = FwRelease();
    status.checked = false;
    status.available = false;
    status.downgrade = false;
    status.noRelease = false;
    status.latest[0] = 0;
    status.pageUrl[0] = 0;
    status.error[0] = 0;
    status.state = FwUpdState::Idle;
  });
  if (busy) {
    return false;
  }
  Preferences prefs;
  prefs.begin("fwupd", false);
  prefs.putUChar("ch", (uint8_t)ch);
  prefs.end();
  logPrintf("[UPDATE] Kanal %s gewählt\n", fwChannelName(ch));
  if (wifiConnected()) {
    fwUpdateCheckNow();
  }
  return true;
}

bool fwUpdateInstall(String &error) {
  FwUpdStatus st = fwUpdateStatus();
  if (st.state == FwUpdState::Downloading || st.state == FwUpdState::Done || installRequested) {
    error = "Update läuft bereits";
    return false;
  }
  if (!st.checked || !st.latest[0]) {
    error = "Erst nach Updates suchen";
    return false;
  }
  if (!st.available && !st.downgrade) {
    error = "Diese Version ist bereits installiert";
    return false;
  }
  if (!wifiConnected()) {
    error = "Kein WLAN (im Einrichtungs-Hotspot ist kein Update möglich)";
    return false;
  }
  installRequested = true;
  xTaskNotifyGive(task);
  return true;
}
