#include "session.h"
#include "util/log.h"

#include <freertos/semphr.h>

#include "config.h"
#include "i18n/i18n.h"
#include "net/spoolman.h"
#include "nfc/nfc.h"
#include "storage/admin.h"

namespace {

SemaphoreHandle_t mutex = nullptr;
bool active = false;
User current;
uint32_t lastActivity = 0;

char noticeText[64] = "";
int noticeLevel = 0;
uint32_t noticeUntil = 0;

void setNotice(const char *text, int level) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  strlcpy(noticeText, text, sizeof(noticeText));
  noticeLevel = level;
  noticeUntil = millis() + 3500;
  xSemaphoreGive(mutex);
}

// Sprache des Benutzers anwenden (0 = Gerätesprache)
void applyUserLanguage(const User &u) {
  if (u.lang == 1) {
    i18nApply(Lang::DE);
  } else if (u.lang == 2) {
    i18nApply(Lang::EN);
  } else {
    i18nRestoreDefault();
  }
}

void handleTag(const NfcState &n) {
  if (n.content.kind == TagKind::Admin) {
    if (adminTagMatches(n.content.admin.token)) {
      adminSessionStart();
      setNotice(tr(Str::AdminLoggedIn), 0);
    } else {
      setNotice(tr(Str::AdminTagUnknown), 1);
      logPrintln("[ADMIN] Unbekannter oder ungültiger Admin-Tag");
    }
  } else if (n.content.kind == TagKind::User) {
    User u;
    if (n.content.user.id >= 0 && usersFind((uint16_t)n.content.user.id, u)) {
      xSemaphoreTake(mutex, portMAX_DELAY);
      active = true;
      current = u;
      lastActivity = millis();
      xSemaphoreGive(mutex);
      applyUserLanguage(u);  // vor der Meldung, damit sie schon in der Sprache des Benutzers erscheint
      char text[64];
      snprintf(text, sizeof(text), "%s: %s", tr(Str::UserLoggedIn), u.name);
      setNotice(text, 0);
      logPrintf("[USER] Angemeldet: %s (ID %u)\n", u.name, u.id);
    } else {
      setNotice(tr(Str::UserUnknown), 1);
      logPrintf("[USER] Unbekannter Benutzer-Tag (ID %d, Name '%s')\n", n.content.user.id, n.content.user.name);
    }
  } else {
    // Spulen-Tags (und Tags ohne bekanntes Format, z. B. eine Spule mit fremdem Tag) in Spoolman nachschlagen
    if (n.content.kind == TagKind::Spool || n.content.kind == TagKind::Unknown) {
      spoolmanRequestLookup(n.uid, n.content.kind == TagKind::Spool ? n.content.spool.spoolmanId : -1);
    }
  }
  if (n.content.kind == TagKind::Spool || n.content.kind == TagKind::Box) {
    // Spulen- und Box-Scans halten die Sitzung am Leben
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (active) {
      lastActivity = millis();
    }
    xSemaphoreGive(mutex);
  }
}

void sessionTask(void *) {
  uint32_t handledSeq = nfcGet().seq;
  for (;;) {
    NfcState n = nfcGet();
    // Erst auswerten, wenn der Inhalt dieses Tags gelesen ist
    if (n.tagPresent && n.seq != handledSeq && n.readSeq == n.seq) {
      handledSeq = n.seq;
      handleTag(n);
    }

    xSemaphoreTake(mutex, portMAX_DELAY);
    bool expire = active && millis() - lastActivity > USER_TIMEOUT_MS;
    uint16_t id = current.id;
    bool wasActive = active;
    xSemaphoreGive(mutex);
    if (expire) {
      logPrintln("[USER] Abgemeldet (Zeitüberschreitung)");
      sessionLogout();
    } else if (wasActive) {
      User tmp;
      if (!usersFind(id, tmp)) {  // Benutzer wurde gelöscht
        logPrintln("[USER] Abgemeldet (Benutzer gelöscht)");
        sessionLogout();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

}  // namespace

void sessionBegin() {
  mutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(sessionTask, "session", 4096, nullptr, 1, nullptr, 0);
}

bool sessionCurrentUser(User &out) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  bool a = active;
  if (a) {
    out = current;
  }
  xSemaphoreGive(mutex);
  return a;
}

bool sessionUserMayTare() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  bool v = active && current.canTare;
  xSemaphoreGive(mutex);
  return v;
}

void sessionLogout() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  active = false;
  xSemaphoreGive(mutex);
  i18nRestoreDefault();
}

void sessionUserChanged(uint16_t id) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  bool a = active && current.id == id;
  xSemaphoreGive(mutex);
  User u;
  if (a && usersFind(id, u)) {
    xSemaphoreTake(mutex, portMAX_DELAY);
    current = u;
    xSemaphoreGive(mutex);
    applyUserLanguage(u);
  }
}

bool sessionSetMyLanguage(Lang lang) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  bool a = active;
  User u = current;
  xSemaphoreGive(mutex);
  if (!a) {
    return false;
  }
  u.lang = lang == Lang::EN ? 2 : 1;
  if (!usersSetLang(u.id, u.lang)) {
    return false;
  }
  xSemaphoreTake(mutex, portMAX_DELAY);
  current.lang = u.lang;
  xSemaphoreGive(mutex);
  i18nApply(lang);
  return true;
}

bool sessionNotice(char *text, size_t cap, int *level) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  bool show = noticeText[0] && (int32_t)(noticeUntil - millis()) > 0;
  if (show) {
    strlcpy(text, noticeText, cap);
    *level = noticeLevel;
  }
  xSemaphoreGive(mutex);
  return show;
}

void sessionPostNotice(const char *text, int level) { setNotice(text, level); }
