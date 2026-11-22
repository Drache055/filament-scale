#include "ui.h"
#include "util/log.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <lvgl.h>

#include "bus/i2c_bus.h"
#include "app/session.h"
#include "display/display.h"
#include "i18n/i18n.h"
#include "net/spoolman.h"
#include "net/wifi.h"
#include "nfc/nfc.h"
#include "scale/scale.h"
#include "storage/admin.h"
#include "storage/devicepw.h"
#include "screens.h"
#include "theme.h"

static lv_obj_t *wifiLabel = nullptr;
static lv_obj_t *weightLabel = nullptr;
static lv_obj_t *statusLabel = nullptr;
static lv_obj_t *userLabel = nullptr;
static lv_obj_t *queueLabel = nullptr;

static lv_obj_t *homeScreen = nullptr;
static uint32_t seenLanguageGeneration = 0;
static lv_obj_t *hintLabel = nullptr;
static lv_obj_t *taraLabel = nullptr;
static lv_obj_t *menuLabel = nullptr;

lv_obj_t *uiMakeButton(lv_obj_t *parent, const char *text, lv_event_cb_t cb, bool fullWidth, void *userData) {
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_remove_style_all(btn);
  lv_obj_add_style(btn, &theme().button, 0);
  lv_obj_add_style(btn, &theme().buttonPressed, LV_STATE_PRESSED);
  if (fullWidth) {
    lv_obj_set_width(btn, LV_PCT(100));
  } else {
    lv_obj_set_flex_grow(btn, 1);
  }
  lv_obj_t *label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_center(label);
  if (cb) {
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, userData);
  }
  return btn;
}

lv_obj_t *uiNewScreen() {
  lv_obj_t *scr = lv_obj_create(nullptr);
  lv_obj_remove_style_all(scr);
  lv_obj_add_style(scr, &theme().screen, 0);
  lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(scr, themePadding(), 0);
  return scr;
}

void uiNavigate(lv_obj_t *scr) {
  // Unterscreens werden beim Verlassen gelöscht, der Home-Screen bleibt bestehen.
  bool deleteOld = lv_screen_active() != homeScreen;
  lv_screen_load_anim(scr, LV_SCREEN_LOAD_ANIM_NONE, 0, 0, deleteOld);
}

void uiShowHome() {
  weighFlowAbort();  // der Ablauf liegt auf eigenen Screens, die hier verlassen werden
  uiNavigate(homeScreen);
}

static void onMenu(lv_event_t *) { screenMenuOpen(); }

void uiRetranslateHome() {
  lv_label_set_text(taraLabel, tr(Str::Tare));
  lv_label_set_text(menuLabel, tr(Str::Menu));
}

static void buildHome() {
  lv_obj_t *scr = uiNewScreen();
  homeScreen = scr;

  // Statusleiste: links WLAN, rechts der angemeldete Benutzer (Tippen = abmelden)
  lv_obj_t *bar = lv_obj_create(scr);
  lv_obj_remove_style_all(bar);
  lv_obj_set_width(bar, LV_PCT(100));
  lv_obj_set_height(bar, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  wifiLabel = lv_label_create(bar);
  lv_obj_add_style(wifiLabel, &theme().dim, 0);
  lv_label_set_text(wifiLabel, "WLAN: ...");

  userLabel = lv_label_create(bar);
  lv_obj_add_style(userLabel, &theme().dim, 0);
  lv_label_set_long_mode(userLabel, LV_LABEL_LONG_DOT);
  lv_obj_set_flex_grow(userLabel, 1);
  lv_obj_set_style_text_align(userLabel, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_add_flag(userLabel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(userLabel, 12);
  lv_obj_add_event_cb(userLabel, [](lv_event_t *) {
    sessionLogout();
    adminSessionEnd();
  }, LV_EVENT_CLICKED, nullptr);
  lv_label_set_text(userLabel, "");

  // Live-Gewicht (Platzhalter bis das scale-Modul steht)
  lv_obj_t *card = lv_obj_create(scr);
  lv_obj_remove_style_all(card);
  lv_obj_add_style(card, &theme().card, 0);
  lv_obj_set_width(card, LV_PCT(100));
  lv_obj_set_flex_grow(card, 1);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  weightLabel = lv_label_create(card);
  lv_obj_add_style(weightLabel, &theme().weight, 0);
  lv_label_set_text(weightLabel, "0 g");

  hintLabel = lv_label_create(card);
  lv_obj_add_style(hintLabel, &theme().body, 0);
  lv_obj_set_width(hintLabel, LV_PCT(100));  // lange Hinweise (Hotspot mit Passwort) umbrechen statt abzuschneiden
  lv_label_set_long_mode(hintLabel, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(hintLabel, LV_TEXT_ALIGN_CENTER, 0);

  statusLabel = lv_label_create(card);
  lv_obj_add_style(statusLabel, &theme().dim, 0);
  lv_label_set_text(statusLabel, "");

  queueLabel = lv_label_create(card);
  lv_obj_add_style(queueLabel, &theme().dim, 0);
  lv_label_set_text(queueLabel, "");

  // Buttons
  lv_obj_t *row = lv_obj_create(scr);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, LV_PCT(100), themeButtonHeight());
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, themePadding(), 0);
  taraLabel = lv_obj_get_child(uiMakeButton(row, "", [](lv_event_t *) { uiOnTare(); }), 0);
  menuLabel = lv_obj_get_child(uiMakeButton(row, "", onMenu), 0);
  uiRetranslateHome();

  lv_screen_load(scr);
}

// Tara: mit Admin-Sperre nur für den angemeldeten Admin oder einen Benutzer mit Tara-Recht
static void tareAfterLogin() {
  scaleTare();
  uiShowHome();
}

void uiOnTare() {
  if (adminAllowed() || sessionUserMayTare()) {
    scaleTare();
  } else {
    screenAdminLoginOpen(tareAfterLogin);
  }
}

void uiLanguageHandled() { seenLanguageGeneration = i18nGeneration(); }

// Hinweiszeile unter dem Gewicht: kurze Meldungen (Anmeldung, Web-Schreibauftrag), sonst "Spule scannen"
static void updateHint() {
  static char shown[80] = "";
  static uint32_t webResultSince = 0;
  char text[80];
  lv_color_t color = themeColors().text;

  char notice[64];
  int level = 0;
  NfcWrite ws = nfcWriteStatus();
  bool webWrite = nfcWriteOrigin() == NfcWriteOrigin::Web && ws != NfcWrite::Idle;

  if (sessionNotice(notice, sizeof(notice), &level)) {
    strlcpy(text, notice, sizeof(text));
    color = level ? themeColors().error : themeColors().ok;
  } else if (wifiApActive() && !wifiConnected()) {
    char line1[48], line2[40];
    snprintf(line1, sizeof(line1), tr(Str::HotspotHint), wifiApSsid().c_str());
    snprintf(line2, sizeof(line2), tr(Str::HotspotPassword), devicePassword().c_str());
    snprintf(text, sizeof(text), "%s\n%s", line1, line2);
    color = themeColors().warn;
  } else if (webWrite) {
    switch (ws) {
      case NfcWrite::WaitingForTag: strlcpy(text, tr(Str::PlaceTag), sizeof(text)); color = themeColors().warn; break;
      case NfcWrite::Writing: strlcpy(text, tr(Str::Writing), sizeof(text)); color = themeColors().warn; break;
      case NfcWrite::Ok: strlcpy(text, tr(Str::WriteOk), sizeof(text)); color = themeColors().ok; break;
      case NfcWrite::ErrLocked: strlcpy(text, tr(Str::WriteLocked), sizeof(text)); color = themeColors().error; break;
      case NfcWrite::ErrTooBig: strlcpy(text, tr(Str::WriteTooBig), sizeof(text)); color = themeColors().error; break;
      case NfcWrite::ErrNotNtag: strlcpy(text, tr(Str::WriteNotNtag), sizeof(text)); color = themeColors().error; break;
      case NfcWrite::ErrTimeout: strlcpy(text, tr(Str::WriteTimeout), sizeof(text)); color = themeColors().error; break;
      case NfcWrite::ErrOccupied: strlcpy(text, tr(Str::WriteOccupied), sizeof(text)); color = themeColors().error; break;
      default: strlcpy(text, tr(Str::WriteFailed), sizeof(text)); color = themeColors().error; break;
    }
    // Ergebnis einige Sekunden stehen lassen, dann quittieren (die Webseite fragt den Status in kurzen Abständen ab)
    bool final = ws != NfcWrite::WaitingForTag && ws != NfcWrite::Writing;
    if (final) {
      if (webResultSince == 0) {
        webResultSince = millis();
      } else if (millis() - webResultSince > 5000) {
        nfcAcknowledgeWrite();
        webResultSince = 0;
      }
    } else {
      webResultSince = 0;
    }
  } else {
    strlcpy(text, tr(Str::ScanSpool), sizeof(text));
  }

  if (strcmp(text, shown) != 0) {
    strlcpy(shown, text, sizeof(shown));
    lv_label_set_text(hintLabel, text);
  }
  lv_obj_set_style_text_color(hintLabel, color, 0);
}

static void refreshTimerCb(lv_timer_t *) {
  lv_label_set_text(wifiLabel, wifiConnected() ? tr(Str::WifiConnectedShort) : tr(Str::WifiDisconnectedShort));

  // Bildschirmschoner: Aktivität außer Berührung (Tag, Gewicht, Meldungen, Schreibaufträge) weckt den Bildschirm
  {
    static uint32_t lastNfcSeq = 0;
    static float lastGrams = 0;
    NfcState nfcNow = nfcGet();
    ScaleReading scaleNow = scaleGet();
    char noticeText[8];
    int noticeLevel;
    bool woke = nfcNow.seq != lastNfcSeq || nfcWriteStatus() != NfcWrite::Idle ||
                sessionNotice(noticeText, sizeof(noticeText), &noticeLevel) ||
                (scaleNow.calibrated && fabsf(scaleNow.grams - lastGrams) > 3.0f);
    lastNfcSeq = nfcNow.seq;
    lastGrams = scaleNow.grams;
    if (woke) {
      displayWake();
    }
    displayPowerTick();
  }

  // Sprache wurde extern (Weboberfläche) geändert: Home neu beschriften und zurück zu Home,
  // damit alle weiteren Screens beim Öffnen in der neuen Sprache entstehen
  if (i18nGeneration() != seenLanguageGeneration) {
    seenLanguageGeneration = i18nGeneration();
    uiRetranslateHome();
    if (lv_screen_active() != homeScreen && !weighFlowActive()) {
      uiShowHome();
    }
  }

  static uint32_t lastQueueCheck = 0;
  if (millis() - lastQueueCheck > 2000) {  // LittleFS nicht im 200-ms-Takt lesen
    lastQueueCheck = millis();
    int pending = spoolmanQueueCount();
    if (pending > 0) {
      lv_label_set_text_fmt(queueLabel, tr(Str::QueuePending), pending);
      lv_obj_set_style_text_color(queueLabel, themeColors().warn, 0);
    } else {
      lv_label_set_text(queueLabel, "");
    }
  }

  // Unbekannter Spulen-Tag (in Spoolman nicht gefunden): Zuordnen-Screen anbieten, einmal je Tag-Erkennung
  static uint32_t assignHandledSeq = 0;
  if (lv_screen_active() == homeScreen && nfcWriteStatus() == NfcWrite::Idle) {
    NfcState n = nfcGet();
    if (n.tagPresent && n.readSeq == n.seq && n.seq != assignHandledSeq &&
        (n.content.kind == TagKind::Spool || n.content.kind == TagKind::Unknown)) {
      SpoolmanResult r = spoolmanLookupResult();
      if (r.state == SpoolmanState::NotFound && strcmp(r.uid, n.uid) == 0) {
        assignHandledSeq = n.seq;
        screenAssignSpoolOpen();
      }
    }
  }

  // Bekannte Spule aufgelegt: Wiegeablauf starten (einmal je Tag-Erkennung)
  static uint32_t flowHandledSeq = 0;
  if (lv_screen_active() == homeScreen && !weighFlowActive() && nfcWriteStatus() == NfcWrite::Idle) {
    NfcState n = nfcGet();
    if (n.tagPresent && n.readSeq == n.seq && n.seq != flowHandledSeq &&
        (n.content.kind == TagKind::Spool || n.content.kind == TagKind::Unknown)) {
      SpoolmanResult r = spoolmanLookupResult();
      if (r.state == SpoolmanState::Found && strcmp(r.uid, n.uid) == 0 && r.spool.id >= 0) {
        flowHandledSeq = n.seq;
        User u;
        weighFlowStart(r.spool, n.uid, sessionCurrentUser(u) ? u.name : "");
      }
    }
  }

  // Admin-Anmeldung: Bedienung verlängert sie, Ablauf wirft aus den Einstellungen zurück zu Home
  {
    static bool wasAdmin = false;
    bool admin = adminLockEnabled() && adminSessionActive();
    if (admin && lv_display_get_inactive_time(nullptr) < 1000) {
      adminSessionTouch();
    }
    if (wasAdmin && !admin && lv_screen_active() != homeScreen && !weighFlowActive()) {
      uiShowHome();
    }
    wasAdmin = admin;
  }

  User user;
  bool hasUser = sessionCurrentUser(user);
  bool isAdmin = adminLockEnabled() && adminSessionActive();
  if (hasUser || isAdmin) {
    char who[48];
    if (isAdmin && hasUser) {
      snprintf(who, sizeof(who), "Admin / %s", user.name);
    } else if (isAdmin) {
      strlcpy(who, "Admin", sizeof(who));
    } else {
      strlcpy(who, user.name, sizeof(who));
    }
    lv_label_set_text(userLabel, who);
    lv_obj_set_style_text_color(userLabel, isAdmin ? themeColors().warn : themeColors().ok, 0);
  } else {
    lv_label_set_text(userLabel, "");
  }
  updateHint();

  ScaleReading r = scaleGet();
  const char *status = "";
  lv_color_t statusColor = themeColors().ok;
  if (!r.valid) {
    lv_label_set_text(weightLabel, "-- g");
    status = tr(Str::NoSensor);
    statusColor = themeColors().error;
  } else if (!r.calibrated) {
    lv_label_set_text(weightLabel, "-- g");
    status = tr(Str::NotCalibrated);
    statusColor = themeColors().warn;
  } else {
    long g = lroundf(r.grams);
    lv_label_set_text_fmt(weightLabel, "%ld g", g == 0 ? 0L : g);
    if (r.overload) {
      status = tr(Str::Overload);
      statusColor = themeColors().error;
    } else if (r.stable) {
      status = tr(Str::Stable);
    }
  }
  lv_label_set_text(statusLabel, status);
  lv_obj_set_style_text_color(statusLabel, statusColor, 0);
}

static void uiTask(void *) {
  i18nBegin();
  seenLanguageGeneration = i18nGeneration();
  if (!displayBegin()) {
    vTaskDelete(nullptr);
  }
  themeLoad();
  buildHome();
  lv_timer_create(refreshTimerCb, 200, nullptr);

  for (;;) {
    uint32_t wait = lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(wait < 5 ? 5 : (wait > 30 ? 30 : wait)));
  }
}

void uiStart() {
  xTaskCreatePinnedToCore(uiTask, "ui", 16384, nullptr, 2, nullptr, 1);
}
