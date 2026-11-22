#include <Arduino.h>
#include "util/log.h"
#include <lvgl.h>

#include "config.h"
#include "display/display.h"
#include "app/session.h"
#include "i18n/i18n.h"
#include "storage/admin.h"
#include "net/wifi.h"
#include "net/spoolman.h"
#include "nfc/nfc.h"
#include "storage/catalog.h"
#include "storage/devicepw.h"
#include "scale/scale.h"
#include "screens.h"
#include "theme.h"

static void onBackHome(lv_event_t *) { uiShowHome(); }
static void onBackMenu(lv_event_t *) { screenMenuOpen(); }
static void onBackSettings(lv_event_t *) { screenSettingsOpen(); }
static void onSettings(lv_event_t *) { uiAdminGate(screenSettingsOpen); }
static void onWifi(lv_event_t *) { screenWifiOpen(); }
static void onDisplay(lv_event_t *) { screenDisplayOpen(); }
static void onLanguage(lv_event_t *) { screenLanguageOpen(); }
static void onCalibration(lv_event_t *) { screenCalibrationOpen(); }
static void onNfc(lv_event_t *) { uiAdminGate(screenNfcOpen); }
static void onMyLanguage(lv_event_t *) { screenMyLanguageOpen(); }
static void onAdminLogout(lv_event_t *) {
  adminSessionEnd();
  screenMenuOpen();
}
static void onTouchCal(lv_event_t *) { screenTouchCalOpen(); }
static void onBoxWrite(lv_event_t *) { screenBoxWriteOpen(); }
static void onFilamentWrite(lv_event_t *) { screenFilamentTypeOpen(); }
static void onAssign(lv_event_t *) { screenAssignSpoolOpen(); }

// Kopfzeile: Zurück-Pfeil links, Titel rechts daneben
static void addHeader(lv_obj_t *scr, const char *title, lv_event_cb_t backCb) {
  lv_obj_t *row = lv_obj_create(scr);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, LV_PCT(100), themeButtonHeight());
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(row, themePadding(), 0);

  lv_obj_t *btn = uiMakeButton(row, LV_SYMBOL_LEFT, backCb);
  lv_obj_set_flex_grow(btn, 0);
  lv_obj_set_width(btn, themeButtonHeight());
  lv_obj_set_ext_click_area(btn, 8);

  lv_obj_t *label = lv_label_create(row);
  lv_obj_add_style(label, &theme().body, 0);
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  lv_obj_set_flex_grow(label, 1);
  lv_label_set_text(label, title);
}

static lv_obj_t *addCard(lv_obj_t *scr) {
  lv_obj_t *card = lv_obj_create(scr);
  lv_obj_remove_style_all(card);
  lv_obj_add_style(card, &theme().card, 0);
  lv_obj_set_width(card, LV_PCT(100));
  lv_obj_set_height(card, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_row(card, themePadding(), 0);
  return card;
}

static lv_obj_t *addLabel(lv_obj_t *parent, const lv_style_t *style, bool wrap = false) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_add_style(l, style, 0);
  if (wrap) {
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, LV_PCT(100));
  }
  return l;
}

static lv_obj_t *addButtonRow(lv_obj_t *scr) {
  lv_obj_t *row = lv_obj_create(scr);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, LV_PCT(100), themeButtonHeight());
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(row, themePadding(), 0);
  return row;
}

// Timer, der mit dem Screen gelöscht wird
static void timerScreenDeleted(lv_event_t *e) { lv_timer_delete(static_cast<lv_timer_t *>(lv_event_get_user_data(e))); }

static lv_timer_t *addScreenTimer(lv_obj_t *scr, lv_timer_cb_t cb, uint32_t periodMs, void *userData) {
  lv_timer_t *t = lv_timer_create(cb, periodMs, userData);
  lv_obj_add_event_cb(scr, timerScreenDeleted, LV_EVENT_DELETE, t);
  return t;
}

// ---- Menü / Einstellungen ----
bool uiAdminGate(void (*onSuccess)()) {
  if (adminAllowed()) {
    onSuccess();
    return true;
  }
  screenAdminLoginOpen(onSuccess);
  return false;
}

void screenMenuOpen() {
  lv_obj_t *scr = uiNewScreen();
  addHeader(scr, tr(Str::Menu), onBackHome);
  User user;
  if (sessionCurrentUser(user)) {
    uiMakeButton(scr, tr(Str::MyLanguage), onMyLanguage, true);
  }
  uiMakeButton(scr, tr(Str::Settings), onSettings, true);
  uiMakeButton(scr, tr(Str::Nfc), onNfc, true);
  if (adminLockEnabled() && adminSessionActive()) {
    uiMakeButton(scr, tr(Str::AdminLogout), onAdminLogout, true);
  }
  uiNavigate(scr);
}

// ---- Admin-Anmeldung: Geräte-PIN mit Ziffernblock, oder Admin-Tag auflegen ----
static void (*loginOnSuccess)() = nullptr;
static lv_obj_t *loginTa = nullptr;
static lv_obj_t *loginMsg = nullptr;
static uint8_t loginFails = 0;
static uint32_t loginBlockedUntil = 0;

static void loginProceed() {
  void (*cb)() = loginOnSuccess;
  loginOnSuccess = nullptr;
  if (cb) {
    cb();
  }
}

static void loginSubmit() {
  if ((int32_t)(loginBlockedUntil - millis()) > 0) {
    lv_label_set_text(loginMsg, tr(Str::TooManyTries));
    lv_obj_set_style_text_color(loginMsg, themeColors().error, 0);
    lv_textarea_set_text(loginTa, "");
    return;
  }
  if (adminCheckDevicePin(lv_textarea_get_text(loginTa))) {
    loginFails = 0;
    lv_textarea_set_text(loginTa, "");
    adminSessionStart();
    loginProceed();
    return;
  }
  lv_textarea_set_text(loginTa, "");
  if (++loginFails >= 5) {
    loginFails = 0;
    loginBlockedUntil = millis() + 30000;  // 5 Fehlversuche: 30 s Pause
    lv_label_set_text(loginMsg, tr(Str::TooManyTries));
  } else {
    lv_label_set_text(loginMsg, tr(Str::WrongPassword));
  }
  lv_obj_set_style_text_color(loginMsg, themeColors().error, 0);
}

static void onNumpad(lv_event_t *e) {
  lv_obj_t *m = static_cast<lv_obj_t *>(lv_event_get_target(e));
  uint32_t id = lv_buttonmatrix_get_selected_button(m);
  const char *txt = lv_buttonmatrix_get_button_text(m, id);
  if (!txt) {
    return;
  }
  lv_label_set_text(loginMsg, "");
  if (txt[0] >= '0' && txt[0] <= '9') {
    lv_textarea_add_char(loginTa, txt[0]);
  } else if (strcmp(txt, "<") == 0) {
    lv_textarea_delete_char(loginTa);
  } else {
    loginSubmit();  // Häkchen
  }
}

// Ein aufgelegter Admin-Tag meldet über die Sitzung an, hier nur weitermachen
static void loginWatch(lv_timer_t *) {
  if (adminAllowed()) {
    loginProceed();
  }
}

void screenAdminLoginOpen(void (*onSuccess)()) {
  loginOnSuccess = onSuccess;
  lv_obj_t *scr = uiNewScreen();
  addHeader(scr, tr(Str::AdminLogin), onBackHome);

    loginTa = lv_textarea_create(scr);
  lv_obj_add_style(loginTa, &theme().body, 0);
  // Eingabefeld dunkel mit hellem Text und Rand, sonst sind die Sternchen kaum zu sehen
  lv_obj_set_style_bg_color(loginTa, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(loginTa, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(loginTa, lv_color_white(), 0);
  lv_obj_set_style_border_width(loginTa, 2, 0);
  lv_obj_set_style_border_color(loginTa, themeColors().text, 0);
  lv_textarea_set_one_line(loginTa, true);
  lv_textarea_set_password_mode(loginTa, true);
  lv_textarea_set_max_length(loginTa, ADMIN_PIN_MAX);
  lv_textarea_set_placeholder_text(loginTa, tr(Str::Password));
  lv_obj_set_width(loginTa, LV_PCT(100));
  lv_obj_remove_flag(loginTa, LV_OBJ_FLAG_CLICK_FOCUSABLE);  // Eingabe nur über den Ziffernblock

  loginMsg = addLabel(scr, &theme().dim, true);
  lv_label_set_text(loginMsg, "");

  static const char *numpadMap[] = {"1", "2", "3", "\n", "4", "5", "6", "\n", "7", "8", "9", "\n", "<", "0", LV_SYMBOL_OK, ""};
  lv_obj_t *pad = lv_buttonmatrix_create(scr);
  lv_obj_remove_style_all(pad);
  lv_obj_add_style(pad, &theme().button, LV_PART_ITEMS);
  lv_obj_add_style(pad, &theme().buttonPressed, LV_PART_ITEMS | LV_STATE_PRESSED);
  lv_obj_set_style_pad_row(pad, themePadding(), 0);
  lv_obj_set_style_pad_column(pad, themePadding(), 0);
  lv_buttonmatrix_set_map(pad, numpadMap);
  lv_obj_set_width(pad, LV_PCT(100));
  lv_obj_set_flex_grow(pad, 1);
  lv_obj_add_event_cb(pad, onNumpad, LV_EVENT_VALUE_CHANGED, nullptr);

  addScreenTimer(scr, loginWatch, 300, nullptr);
  uiNavigate(scr);
}

void screenSettingsOpen() {
  lv_obj_t *scr = uiNewScreen();
  addHeader(scr, tr(Str::Settings), onBackMenu);
  uiMakeButton(scr, tr(Str::Calibration), onCalibration, true);
  uiMakeButton(scr, tr(Str::WiFi), onWifi, true);
  uiMakeButton(scr, tr(Str::Display), onDisplay, true);
  uiMakeButton(scr, tr(Str::Language), onLanguage, true);
  uiMakeButton(scr, tr(Str::TouchCal), onTouchCal, true);
  uiNavigate(scr);
}

// ---- WiFi-Info ----
struct WifiInfoUi {
  lv_obj_t *status;
  lv_obj_t *ssid;
  lv_obj_t *ip;
  lv_obj_t *rssi;
  lv_obj_t *quality;
  lv_obj_t *spoolman;
  lv_obj_t *ap;
  lv_obj_t *devpw;
};

static void spoolmanRefresh(WifiInfoUi *ui);

static void wifiRefresh(lv_timer_t *t) {
  auto *ui = static_cast<WifiInfoUi *>(lv_timer_get_user_data(t));
  spoolmanRefresh(ui);
  lv_label_set_text_fmt(ui->devpw, tr(Str::DevicePassword), devicePassword().c_str());
  if (wifiApActive()) {
    lv_label_set_text_fmt(ui->ap, "%s: %s (%s)", tr(Str::SetupWifi), wifiApSsid().c_str(), wifiApIp().c_str());
  } else {
    lv_label_set_text(ui->ap, "");
  }
  if (!wifiConnected()) {
    lv_label_set_text(ui->status, tr(Str::NotConnected));
    lv_obj_set_style_text_color(ui->status, themeColors().error, 0);
    lv_label_set_text_fmt(ui->ssid, "%s: -", tr(Str::Ssid));
    lv_label_set_text_fmt(ui->ip, "%s: -", tr(Str::Ip));
    lv_label_set_text_fmt(ui->rssi, "%s: -", tr(Str::Signal));
    lv_label_set_text(ui->quality, "");
    return;
  }
  int rssi = wifiRssi();
  const char *q;
  lv_color_t col;
  if (rssi >= -67) {
    q = tr(Str::QualityGood);
    col = themeColors().ok;
  } else if (rssi >= -80) {
    q = tr(Str::QualityMedium);
    col = themeColors().warn;
  } else {
    q = tr(Str::QualityWeak);
    col = themeColors().error;
  }
  lv_label_set_text(ui->status, tr(Str::Connected));
  lv_obj_set_style_text_color(ui->status, themeColors().ok, 0);
  lv_label_set_text_fmt(ui->ssid, "%s: %s", tr(Str::Ssid), wifiSsid().c_str());
  lv_label_set_text_fmt(ui->ip, "%s: %s", tr(Str::Ip), wifiIp().c_str());
  lv_label_set_text_fmt(ui->rssi, "%s: %d dBm", tr(Str::Signal), rssi);
  lv_label_set_text_fmt(ui->quality, "%s: %s", tr(Str::Quality), q);
  lv_obj_set_style_text_color(ui->quality, col, 0);
}

static void spoolmanRefresh(WifiInfoUi *ui) {
  SpoolmanStatus st = spoolmanStatus();
  if (!st.everChecked) {
    lv_label_set_text_fmt(ui->spoolman, "%s: ...", tr(Str::Spoolman));
    lv_obj_set_style_text_color(ui->spoolman, themeColors().dim, 0);
  } else if (st.reachable) {
    lv_label_set_text_fmt(ui->spoolman, "%s: %s (v%s)", tr(Str::Spoolman), tr(Str::SpoolmanReachable), st.version);
    lv_obj_set_style_text_color(ui->spoolman, themeColors().ok, 0);
  } else {
    lv_label_set_text_fmt(ui->spoolman, "%s: %s", tr(Str::Spoolman), tr(Str::SpoolmanOffline));
    lv_obj_set_style_text_color(ui->spoolman, themeColors().error, 0);
  }
}

static void wifiScreenDeleted(lv_event_t *e) { delete static_cast<WifiInfoUi *>(lv_event_get_user_data(e)); }

void screenWifiOpen() {
  lv_obj_t *scr = uiNewScreen();
  addHeader(scr, tr(Str::WiFi), onBackSettings);
  lv_obj_t *card = addCard(scr);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  auto *ui = new WifiInfoUi();
  ui->status = addLabel(card, &theme().title, true);
  ui->ssid = addLabel(card, &theme().body, true);
  ui->ip = addLabel(card, &theme().body, true);
  ui->rssi = addLabel(card, &theme().body, true);
  ui->quality = addLabel(card, &theme().body, true);
  ui->spoolman = addLabel(card, &theme().body, true);
  ui->ap = addLabel(card, &theme().dim, true);
  ui->devpw = addLabel(card, &theme().dim, true);

  lv_timer_t *t = addScreenTimer(scr, wifiRefresh, 1000, ui);
  lv_obj_add_event_cb(scr, wifiScreenDeleted, LV_EVENT_DELETE, ui);
  wifiRefresh(t);
  uiNavigate(scr);
}

// ---- Display (VCOMS einstellen) ----
static lv_obj_t *vcomsLabel = nullptr;
static lv_obj_t *vcomsHint = nullptr;

static void vcomsRefresh(bool saved) {
  uint8_t v = displayGetVcoms();
  lv_label_set_text_fmt(vcomsLabel, "0x%02X (%u)", v, v);
  lv_label_set_text(vcomsHint, saved ? tr(Str::Saved) : tr(Str::NotSaved));
  lv_obj_set_style_text_color(vcomsHint, saved ? themeColors().ok : themeColors().warn, 0);
}

static void onVcomsStep(lv_event_t *e) {
  int delta = (int)(intptr_t)lv_event_get_user_data(e);
  int v = constrain((int)displayGetVcoms() + delta, 0, (int)DISPLAY_VCOMS_MAX);
  displaySetVcoms((uint8_t)v, false);
  vcomsRefresh(false);
}

static void onVcomsSave(lv_event_t *) {
  displaySetVcoms(displayGetVcoms(), true);
  vcomsRefresh(true);
}

static void onVcomsReset(lv_event_t *) {
  displaySetVcoms(DISPLAY_VCOMS_DEFAULT, false);
  vcomsRefresh(false);
}

void screenDisplayOpen() {
  lv_obj_t *scr = uiNewScreen();
  addHeader(scr, tr(Str::Display), onBackSettings);

  lv_obj_t *card = addCard(scr);
  lv_obj_t *name = addLabel(card, &theme().dim);
  lv_label_set_text(name, tr(Str::VcomsTitle));
  vcomsLabel = addLabel(card, &theme().title);
  vcomsHint = addLabel(card, &theme().dim);

  lv_obj_t *steps = addButtonRow(scr);
  uiMakeButton(steps, "-5", onVcomsStep, false, (void *)(intptr_t)-5);
  uiMakeButton(steps, "-1", onVcomsStep, false, (void *)(intptr_t)-1);
  uiMakeButton(steps, "+1", onVcomsStep, false, (void *)(intptr_t)1);
  uiMakeButton(steps, "+5", onVcomsStep, false, (void *)(intptr_t)5);

  lv_obj_t *actions = addButtonRow(scr);
  uiMakeButton(actions, tr(Str::Save), onVcomsSave);
  uiMakeButton(actions, tr(Str::Default), onVcomsReset);
  vcomsRefresh(true);
  uiNavigate(scr);
}

// ---- Sprache ----
static void onPickLanguage(lv_event_t *e) {
  i18nSet((Lang)(intptr_t)lv_event_get_user_data(e));
  User user;
  if (sessionCurrentUser(user)) {
    sessionUserChanged(user.id);  // ein angemeldeter Benutzer behält seine eigene Sprache
  }
  uiLanguageHandled();
  uiRetranslateHome();
  screenLanguageOpen();  // Screen in der neuen Sprache neu aufbauen
}

void screenLanguageOpen() {
  lv_obj_t *scr = uiNewScreen();
  addHeader(scr, tr(Str::Language), onBackSettings);
  // Gerätesprache mit Häkchen
  const bool de = i18nDefault() == Lang::DE;
  uiMakeButton(scr, de ? LV_SYMBOL_OK " Deutsch" : "Deutsch", onPickLanguage, true, (void *)(intptr_t)Lang::DE);
  uiMakeButton(scr, !de ? LV_SYMBOL_OK " English" : "English", onPickLanguage, true, (void *)(intptr_t)Lang::EN);
  uiNavigate(scr);
}

// Eigene Sprache des angemeldeten Benutzers (wird beim Anmelden wieder geladen)
static void onPickMyLanguage(lv_event_t *e) {
  if (!sessionSetMyLanguage((Lang)(intptr_t)lv_event_get_user_data(e))) {
    screenMenuOpen();  // niemand mehr angemeldet
    return;
  }
  uiLanguageHandled();
  uiRetranslateHome();
  screenMyLanguageOpen();
}

void screenMyLanguageOpen() {
  User user;
  if (!sessionCurrentUser(user)) {
    screenMenuOpen();
    return;
  }
  lv_obj_t *scr = uiNewScreen();
  addHeader(scr, tr(Str::MyLanguage), onBackMenu);
  const bool de = i18nGet() == Lang::DE;
  uiMakeButton(scr, de ? LV_SYMBOL_OK " Deutsch" : "Deutsch", onPickMyLanguage, true, (void *)(intptr_t)Lang::DE);
  uiMakeButton(scr, !de ? LV_SYMBOL_OK " English" : "English", onPickMyLanguage, true, (void *)(intptr_t)Lang::EN);
  uiNavigate(scr);
}

// ---- Kalibrierung: Schritt 1 (Tara), Schritt 2 (Referenzgewicht) ----
static float calReference = 100.0f;
static lv_obj_t *calLive = nullptr;
static lv_obj_t *calRefLabel = nullptr;
static bool calHoldResult = false;  // Ergebnis der Kalibrierung stehen lassen
static void screenCalibrationStep2Open();

static void calRefreshLive(lv_timer_t *) {
  if (calHoldResult) {
    return;
  }
  ScaleReading r = scaleGet();
  if (!r.valid) {
    lv_label_set_text(calLive, tr(Str::NoSensor));
    lv_obj_set_style_text_color(calLive, themeColors().error, 0);
  } else {
    lv_label_set_text_fmt(calLive, "%s: %ld", tr(Str::Raw), r.rawNet);
    lv_obj_set_style_text_color(calLive, themeColors().dim, 0);
  }
}

static void onCalTare(lv_event_t *) {
  if (scaleTare()) {
    screenCalibrationStep2Open();
  }
}

void screenCalibrationOpen() {
  calHoldResult = false;
  lv_obj_t *scr = uiNewScreen();
  addHeader(scr, tr(Str::Calibration), onBackSettings);
  lv_obj_t *card = addCard(scr);
  lv_obj_t *hint = addLabel(card, &theme().body, true);
  lv_label_set_text(hint, tr(Str::CalEmptyHint));
  calLive = addLabel(card, &theme().dim, true);
  uiMakeButton(scr, tr(Str::TareContinue), onCalTare, true);
  addScreenTimer(scr, calRefreshLive, 300, nullptr);
  calRefreshLive(nullptr);
  uiNavigate(scr);
}

static void calRefreshRef() { lv_label_set_text_fmt(calRefLabel, "%ld g", lroundf(calReference)); }

static void onCalStep(lv_event_t *e) {
  calHoldResult = false;
  calReference = constrain(calReference + (int)(intptr_t)lv_event_get_user_data(e), 1.0f, 5000.0f);
  calRefreshRef();
}

static void onCalibrate(lv_event_t *) {
  bool ok = scaleCalibrate(calReference);
  calHoldResult = true;
  lv_label_set_text(calLive, ok ? tr(Str::CalOk) : tr(Str::CalFailed));
  lv_obj_set_style_text_color(calLive, ok ? themeColors().ok : themeColors().error, 0);
}

static void screenCalibrationStep2Open() {
  calHoldResult = false;
  lv_obj_t *scr = uiNewScreen();
  addHeader(scr, tr(Str::PlaceWeight), onBackSettings);
  lv_obj_t *card = addCard(scr);
  calRefLabel = addLabel(card, &theme().title);
  calLive = addLabel(card, &theme().dim, true);
  calRefreshRef();

  lv_obj_t *steps = addButtonRow(scr);
  uiMakeButton(steps, "-50", onCalStep, false, (void *)(intptr_t)-50);
  uiMakeButton(steps, "-10", onCalStep, false, (void *)(intptr_t)-10);
  uiMakeButton(steps, "+10", onCalStep, false, (void *)(intptr_t)10);
  uiMakeButton(steps, "+50", onCalStep, false, (void *)(intptr_t)50);
  uiMakeButton(scr, tr(Str::Calibrate), onCalibrate, true);

  addScreenTimer(scr, calRefreshLive, 300, nullptr);
  calRefreshLive(nullptr);
  uiNavigate(scr);
}

// ---- NFC: Anzeige des aufliegenden Tags, darunter Box / Filament ----
struct NfcUi {
  lv_obj_t *status;
  lv_obj_t *info;
  lv_obj_t *spool;
  lv_obj_t *assignBtn;  // nur sichtbar, wenn der aufliegende Tag in Spoolman unbekannt ist
};

static void nfcRefresh(lv_timer_t *t) {
  auto *ui = static_cast<NfcUi *>(lv_timer_get_user_data(t));
  NfcState s = nfcGet();
  {
    SpoolmanResult r = spoolmanLookupResult();
    bool unknown = s.tagPresent && (s.content.kind == TagKind::Spool || s.content.kind == TagKind::Unknown) &&
                   r.state == SpoolmanState::NotFound && strcmp(r.uid, s.uid) == 0;
    if (unknown) {
      lv_obj_remove_flag(ui->assignBtn, LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(ui->assignBtn, LV_OBJ_FLAG_HIDDEN);
    }
  }
  if (!s.moduleReady) {
    lv_label_set_text(ui->status, tr(Str::NfcNoModule));
    lv_obj_set_style_text_color(ui->status, themeColors().error, 0);
    lv_label_set_text(ui->info, "");
    lv_label_set_text(ui->spool, "");
    return;
  }
  if (!s.tagPresent) {
    lv_label_set_text(ui->status, tr(Str::NfcReady));
    lv_obj_set_style_text_color(ui->status, themeColors().ok, 0);
    lv_label_set_text(ui->info, "");
    lv_label_set_text(ui->spool, "");
    return;
  }
  lv_obj_set_style_text_color(ui->status, themeColors().text, 0);
  switch (s.content.kind) {
    case TagKind::Spool:
      lv_label_set_text_fmt(ui->status, "%s %s", s.content.spool.brand, s.content.spool.type);
      break;
    case TagKind::Box:
      lv_label_set_text(ui->status, s.content.box.name);
      break;
    case TagKind::User:
      lv_label_set_text_fmt(ui->status, "%s: %s", tr(Str::User), s.content.user.name);
      break;
    case TagKind::Admin:
      lv_label_set_text(ui->status, tr(Str::KindAdmin));
      break;
    default:
      lv_label_set_text(ui->status, tr(Str::KindUnknown));
      break;
  }
  lv_label_set_text_fmt(ui->info, "%s: %s%s%s", tr(Str::NfcTagUid), s.uid, s.locked ? "\n" : "",
                        s.locked ? tr(Str::NfcLocked) : "");

  // Spoolman-Ergebnis, nur wenn es zum aufliegenden Tag gehört
  SpoolmanResult r = spoolmanLookupResult();
  if ((s.content.kind == TagKind::Spool || s.content.kind == TagKind::Unknown) && strcmp(r.uid, s.uid) == 0) {
    switch (r.state) {
      case SpoolmanState::Found: {
        char weight[24] = "";
        if (r.spool.remaining >= 0) {
          snprintf(weight, sizeof(weight), ", %ld g", lroundf(r.spool.remaining));
        }
        lv_label_set_text_fmt(ui->spool, "%s #%d: %s %s%s%s%s%s", tr(Str::Spoolman), r.spool.id, r.spool.vendor,
                              r.spool.material, weight, r.spool.location[0] ? ", " : "", r.spool.location,
                              r.spool.owner[0] ? (String(" (") + r.spool.owner + ")").c_str() : "");
        lv_obj_set_style_text_color(ui->spool, themeColors().ok, 0);
        break;
      }
      case SpoolmanState::NotFound:
        lv_label_set_text(ui->spool, tr(Str::SpoolmanUnknown));
        lv_obj_set_style_text_color(ui->spool, themeColors().warn, 0);
        break;
      case SpoolmanState::Error:
        lv_label_set_text_fmt(ui->spool, "%s: %s", tr(Str::Spoolman), r.error);
        lv_obj_set_style_text_color(ui->spool, themeColors().error, 0);
        break;
      default:
        lv_label_set_text(ui->spool, "...");
        lv_obj_set_style_text_color(ui->spool, themeColors().dim, 0);
        break;
    }
  } else {
    lv_label_set_text(ui->spool, "");
  }
}

static void nfcScreenDeleted(lv_event_t *e) { delete static_cast<NfcUi *>(lv_event_get_user_data(e)); }

void screenNfcOpen() {
  lv_obj_t *scr = uiNewScreen();
  addHeader(scr, tr(Str::Nfc), onBackMenu);
  lv_obj_t *card = addCard(scr);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  auto *ui = new NfcUi();
  ui->status = addLabel(card, &theme().body, true);
  ui->info = addLabel(card, &theme().dim, true);
  ui->spool = addLabel(card, &theme().dim, true);
  uiMakeButton(scr, tr(Str::KindBox), onBoxWrite, true);
  uiMakeButton(scr, tr(Str::Filament), onFilamentWrite, true);
  ui->assignBtn = uiMakeButton(scr, tr(Str::AssignSpool), onAssign, true);
  lv_obj_add_flag(ui->assignBtn, LV_OBJ_FLAG_HIDDEN);

  lv_timer_t *t = addScreenTimer(scr, nfcRefresh, 300, ui);
  lv_obj_add_event_cb(scr, nfcScreenDeleted, LV_EVENT_DELETE, ui);
  nfcRefresh(t);
  uiNavigate(scr);
}

// ---- Gemeinsamer Statustext für Schreibvorgänge ----
static lv_obj_t *writeStatus = nullptr;

const char *uiNfcWriteText(NfcWrite st, lv_color_t *color) {
  const char *text = nullptr;
  lv_color_t col = themeColors().error;
  switch (st) {
    case NfcWrite::Writing: text = tr(Str::Writing); col = themeColors().warn; break;
    case NfcWrite::Ok: text = tr(Str::WriteOk); col = themeColors().ok; break;
    case NfcWrite::ErrLocked: text = tr(Str::WriteLocked); break;
    case NfcWrite::ErrTooBig: text = tr(Str::WriteTooBig); break;
    case NfcWrite::ErrNotNtag: text = tr(Str::WriteNotNtag); break;
    case NfcWrite::ErrFailed: text = tr(Str::WriteFailed); break;
    case NfcWrite::ErrTimeout: text = tr(Str::WriteTimeout); break;
    case NfcWrite::ErrOccupied: text = tr(Str::WriteOccupied); break;
    default: break;  // Idle und WaitingForTag: Text bleibt wie gesetzt
  }
  if (color) {
    *color = col;
  }
  return text;
}

static void writeStatusRefresh(lv_timer_t *) {
  lv_color_t col;
  const char *text = uiNfcWriteText(nfcWriteStatus(), &col);
  if (text) {
    lv_label_set_text(writeStatus, text);
    lv_obj_set_style_text_color(writeStatus, col, 0);
  }
}

static void startWrite(const char *json) {
  if (!nfcRequestWrite(json)) {
    return;
  }
  lv_label_set_text(writeStatus, tr(Str::PlaceTag));
  lv_obj_set_style_text_color(writeStatus, themeColors().warn, 0);
}

// ---- Box-Tag: Nummer wählen, Tag auflegen ----
static int boxNumber = 1;
static lv_obj_t *boxNumLabel = nullptr;

static void boxRefreshNumber() { lv_label_set_text_fmt(boxNumLabel, "BOX-%02d", boxNumber); }

static void onBoxStep(lv_event_t *e) {
  nfcCancelWrite();
  boxNumber = constrain(boxNumber + (int)(intptr_t)lv_event_get_user_data(e), 1, 99);
  boxRefreshNumber();
  lv_label_set_text(writeStatus, "");
}

static void onBoxWriteStart(lv_event_t *) {
  char json[128];
  if (tagBuildBoxNumber(boxNumber, json, sizeof(json)) != 0) {
    startWrite(json);
  }
}

static void onWriteBack(lv_event_t *) {
  nfcCancelWrite();
  screenNfcOpen();
}

void screenBoxWriteOpen() {
  lv_obj_t *scr = uiNewScreen();
  addHeader(scr, tr(Str::KindBox), onWriteBack);
  lv_obj_t *card = addCard(scr);
  lv_obj_t *name = addLabel(card, &theme().dim);
  lv_label_set_text(name, tr(Str::BoxNumber));
  boxNumLabel = addLabel(card, &theme().title);
  writeStatus = addLabel(card, &theme().body, true);
  lv_label_set_text(writeStatus, "");
  boxRefreshNumber();

  lv_obj_t *steps = addButtonRow(scr);
  uiMakeButton(steps, "-5", onBoxStep, false, (void *)(intptr_t)-5);
  uiMakeButton(steps, "-1", onBoxStep, false, (void *)(intptr_t)-1);
  uiMakeButton(steps, "+1", onBoxStep, false, (void *)(intptr_t)1);
  uiMakeButton(steps, "+5", onBoxStep, false, (void *)(intptr_t)5);
  uiMakeButton(scr, tr(Str::Write), onBoxWriteStart, true);

  addScreenTimer(scr, writeStatusRefresh, 300, nullptr);
  uiNavigate(scr);
}

// ---- Filament-Tag: Typ, dann Hersteller, dann schreiben ----
static Catalog catalog;          // Kopie für die Dauer der Auswahl
static int pickedMaterial = 0;
static int pickedMaker = 0;
static int pickedColor = 0;

static void onBackFilamentType(lv_event_t *) { screenFilamentTypeOpen(); }
static void onBackFilamentMaker(lv_event_t *) { screenFilamentMakerOpen(); }
static void onBackFilamentColor(lv_event_t *) { screenFilamentColorOpen(); }

// Farbfeld links im Button bzw. in der Karte
static lv_obj_t *addSwatch(lv_obj_t *parent, const char *hex, int size) {
  lv_obj_t *sw = lv_obj_create(parent);
  lv_obj_remove_style_all(sw);
  lv_obj_add_flag(sw, LV_OBJ_FLAG_FLOATING);
  lv_obj_remove_flag(sw, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(sw, size, size);
  lv_obj_set_style_radius(sw, size / 4, 0);
  lv_obj_set_style_bg_color(sw, lv_color_hex(strtoul(hex, nullptr, 16)), 0);
  lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(sw, 2, 0);
  lv_obj_set_style_border_color(sw, themeColors().dim, 0);
  return sw;
}

// Scrollbare Liste mit einem Button je Eintrag, füllt den Rest des Screens
static lv_obj_t *addList(lv_obj_t *scr) {
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *list = lv_obj_create(scr);
  lv_obj_remove_style_all(list);
  lv_obj_set_width(list, LV_PCT(100));
  lv_obj_set_flex_grow(list, 1);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, themePadding(), 0);
  lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(list, LV_DIR_VER);
  return list;
}

static void onPickMaterial(lv_event_t *e) {
  pickedMaterial = (int)(intptr_t)lv_event_get_user_data(e);
  screenFilamentMakerOpen();
}

static void onPickMaker(lv_event_t *e) {
  pickedMaker = (int)(intptr_t)lv_event_get_user_data(e);
  screenFilamentColorOpen();
}

static void onPickColor(lv_event_t *e) {
  pickedColor = (int)(intptr_t)lv_event_get_user_data(e);
  screenFilamentWriteOpen();
}

void screenFilamentTypeOpen() {
  catalogGet(catalog);
  lv_obj_t *scr = uiNewScreen();
  addHeader(scr, tr(Str::FilamentType), onWriteBack);
  lv_obj_t *list = addList(scr);
  for (int i = 0; i < catalog.materialCount; i++) {
    uiMakeButton(list, catalog.materials[i].name, onPickMaterial, true, (void *)(intptr_t)i);
  }
  uiNavigate(scr);
}

void screenFilamentMakerOpen() {
  lv_obj_t *scr = uiNewScreen();
  addHeader(scr, tr(Str::Manufacturer), onBackFilamentType);
  lv_obj_t *list = addList(scr);
  for (int i = 0; i < catalog.makerCount; i++) {
    uiMakeButton(list, catalog.makers[i], onPickMaker, true, (void *)(intptr_t)i);
  }
  uiNavigate(scr);
}

void screenFilamentColorOpen() {
  lv_obj_t *scr = uiNewScreen();
  addHeader(scr, tr(Str::Color), onBackFilamentMaker);
  lv_obj_t *list = addList(scr);
  for (int i = 0; i < catalog.colorCount; i++) {
    lv_obj_t *btn = uiMakeButton(list, catalog.colors[i].name, onPickColor, true, (void *)(intptr_t)i);
    lv_obj_t *sw = addSwatch(btn, catalog.colors[i].hex, 28);
    lv_obj_align(sw, LV_ALIGN_LEFT_MID, 10, 0);
  }
  uiNavigate(scr);
}

static void onFilamentWriteStart(lv_event_t *) {
  SpoolTag spool;
  const CatalogMaterial &m = catalog.materials[pickedMaterial];
  strlcpy(spool.type, m.name, sizeof(spool.type));
  strlcpy(spool.colorHex, catalog.colors[pickedColor].hex, sizeof(spool.colorHex));
  strlcpy(spool.brand, catalog.makers[pickedMaker], sizeof(spool.brand));
  spool.minTemp = m.minTemp;
  spool.maxTemp = m.maxTemp;
  char json[256];
  if (tagBuildSpool(spool, json, sizeof(json)) != 0) {
    startWrite(json);
  }
}

void screenFilamentWriteOpen() {
  lv_obj_t *scr = uiNewScreen();
  addHeader(scr, tr(Str::WriteFilamentTag), onBackFilamentColor);
  const CatalogMaterial &m = catalog.materials[pickedMaterial];
  lv_obj_t *card = addCard(scr);
  lv_obj_t *type = addLabel(card, &theme().title);
  lv_label_set_text(type, m.name);
  lv_obj_t *maker = addLabel(card, &theme().body);
  lv_label_set_text(maker, catalog.makers[pickedMaker]);
  lv_obj_t *temps = addLabel(card, &theme().dim);
  lv_label_set_text_fmt(temps, "%d-%d °C", m.minTemp, m.maxTemp);
  lv_obj_t *colorRow = addLabel(card, &theme().body);
  lv_label_set_text(colorRow, catalog.colors[pickedColor].name);
  lv_obj_t *sw = addSwatch(card, catalog.colors[pickedColor].hex, 30);
  lv_obj_align(sw, LV_ALIGN_TOP_RIGHT, 0, 0);
  writeStatus = addLabel(card, &theme().body, true);
  lv_label_set_text(writeStatus, "");
  uiMakeButton(scr, tr(Str::Write), onFilamentWriteStart, true);

  addScreenTimer(scr, writeStatusRefresh, 300, nullptr);
  uiNavigate(scr);
}

// ---- Touch kalibrieren: vier Fadenkreuze, je Achse eine lineare Abbildung ----
struct TouchCalUi {
  lv_obj_t *cross[4];
  lv_obj_t *label;
  uint32_t seenReleases;
  int step;
  uint16_t rawX[4], rawY[4];
  bool done;
};

static const int kTargetX[4] = {20, 219, 20, 219};
static const int kTargetY[4] = {20, 20, 299, 299};

static lv_obj_t *makeCrosshair(lv_obj_t *scr, int cx, int cy) {
  lv_obj_t *box = lv_obj_create(scr);
  lv_obj_remove_style_all(box);
  lv_obj_add_flag(box, LV_OBJ_FLAG_FLOATING);
  lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(box, 31, 31);
  lv_obj_set_pos(box, cx - 15, cy - 15);
  for (int i = 0; i < 2; i++) {
    lv_obj_t *bar = lv_obj_create(box);
    lv_obj_remove_style_all(bar);
    lv_obj_set_style_bg_color(bar, themeColors().warn, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_size(bar, i == 0 ? 31 : 3, i == 0 ? 3 : 31);
    lv_obj_center(bar);
  }
  lv_obj_add_flag(box, LV_OBJ_FLAG_HIDDEN);
  return box;
}

static void touchCalShowStep(TouchCalUi *ui) {
  for (int i = 0; i < 4; i++) {
    if (i == ui->step) {
      lv_obj_remove_flag(ui->cross[i], LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(ui->cross[i], LV_OBJ_FLAG_HIDDEN);
    }
  }
}

static void onTouchCalBack(lv_event_t *) { screenSettingsOpen(); }

static void touchCalFinish(TouchCalUi *ui) {
  // Je Achse: Mittel der beiden linken/rechten (bzw. oberen/unteren) Ziele
  float xl = (ui->rawX[0] + ui->rawX[2]) / 2.0f, xr = (ui->rawX[1] + ui->rawX[3]) / 2.0f;
  float yt = (ui->rawY[0] + ui->rawY[1]) / 2.0f, yb = (ui->rawY[2] + ui->rawY[3]) / 2.0f;
  if (fabsf(xr - xl) < 80 || fabsf(yb - yt) < 120) {  // unplausibel: Ziele nicht getroffen oder gleiche Stelle
    lv_label_set_text(ui->label, tr(Str::TouchFail));
    lv_obj_set_style_text_color(ui->label, themeColors().error, 0);
    ui->step = 0;
    touchCalShowStep(ui);
    return;
  }
  float ax = (kTargetX[1] - kTargetX[0]) / (xr - xl);
  float bx = kTargetX[0] - ax * xl;
  float ay = (kTargetY[2] - kTargetY[0]) / (yb - yt);
  float by = kTargetY[0] - ay * yt;
  displayTouchSetCalibration(ax, bx, ay, by, true);
  ui->done = true;
  for (int i = 0; i < 4; i++) {
    lv_obj_add_flag(ui->cross[i], LV_OBJ_FLAG_HIDDEN);
  }
  lv_label_set_text(ui->label, tr(Str::TouchDone));
  lv_obj_set_style_text_color(ui->label, themeColors().ok, 0);
  lv_obj_t *back = uiMakeButton(lv_obj_get_parent(ui->label), LV_SYMBOL_LEFT, onTouchCalBack);
  lv_obj_add_flag(back, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_size(back, 120, themeButtonHeight());
  lv_obj_align(back, LV_ALIGN_CENTER, 0, 60);
}

static void touchCalTick(lv_timer_t *t) {
  auto *ui = static_cast<TouchCalUi *>(lv_timer_get_user_data(t));
  if (ui->done) {
    return;
  }
  uint32_t count = displayTouchReleaseCount();
  if (count == ui->seenReleases) {
    return;
  }
  ui->seenReleases = count;
  displayTouchLastRaw(ui->rawX[ui->step], ui->rawY[ui->step]);
  logPrintf("[TOUCH] Ziel %d: roh %u/%u\n", ui->step, ui->rawX[ui->step], ui->rawY[ui->step]);
  if (++ui->step >= 4) {
    touchCalFinish(ui);
  } else {
    lv_label_set_text(ui->label, tr(Str::TouchTap));
    lv_obj_set_style_text_color(ui->label, themeColors().text, 0);
    touchCalShowStep(ui);
  }
}

static void touchCalDeleted(lv_event_t *e) { delete static_cast<TouchCalUi *>(lv_event_get_user_data(e)); }

void screenTouchCalOpen() {
  lv_obj_t *scr = uiNewScreen();
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  auto *ui = new TouchCalUi();
  ui->seenReleases = displayTouchReleaseCount();
  ui->step = 0;
  ui->done = false;
  for (int i = 0; i < 4; i++) {
    ui->cross[i] = makeCrosshair(scr, kTargetX[i], kTargetY[i]);
  }
  ui->label = lv_label_create(scr);
  lv_obj_add_style(ui->label, &theme().body, 0);
  lv_obj_add_flag(ui->label, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_width(ui->label, 180);
  lv_label_set_long_mode(ui->label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(ui->label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(ui->label, LV_ALIGN_CENTER, 0, 0);
  lv_label_set_text(ui->label, tr(Str::TouchTap));
  touchCalShowStep(ui);

  lv_timer_t *t = addScreenTimer(scr, touchCalTick, 50, ui);
  (void)t;
  lv_obj_add_event_cb(scr, touchCalDeleted, LV_EVENT_DELETE, ui);
  uiNavigate(scr);
}

// ---- Unbekannte Spule zuordnen: Spule aus Spoolman wählen, Tag verknüpfen, Tag mit spoolman_id beschreiben ----
enum class AssignStep : uint8_t { Loading, Pick, Linking, LookingUp, Writing, Done, Failed };

struct AssignCtx {
  AssignStep step = AssignStep::Loading;
  char uid[21] = "";
  TagContent tag;
  int pickedId = -1;
  uint32_t listSeq = 0, writeSeq = 0, lookupSeq = 0;
  lv_obj_t *status = nullptr;
  lv_obj_t *list = nullptr;
};
static AssignCtx assign;
static SpoolListResult assignList;

static void assignSay(const char *text, lv_color_t color) {
  lv_label_set_text(assign.status, text);
  lv_obj_set_style_text_color(assign.status, color, 0);
}

static void assignFail(const char *text) {
  assign.step = AssignStep::Failed;
  assignSay(text, themeColors().error);
}

static void onAssignBack(lv_event_t *) {
  nfcCancelWrite();
  uiShowHome();
}

static void onAssignPick(lv_event_t *e) {
  assign.pickedId = (int)(intptr_t)lv_event_get_user_data(e);
  lv_obj_add_flag(assign.list, LV_OBJ_FLAG_HIDDEN);
  assignSay(tr(Str::Linking), themeColors().warn);
  assign.writeSeq = spoolmanWriteResult().seq;
  spoolmanRequestLinkTag(assign.pickedId, assign.uid);
  assign.step = AssignStep::Linking;
}

// Tag-Inhalt: vorhandene OpenSpool-Daten behalten und spoolman_id ergänzen, sonst aus der Spoolman-Spule aufbauen
static bool buildAssignedJson(const SpoolInfo &info, char *json, size_t cap) {
  SpoolTag sp;
  if (assign.tag.kind == TagKind::Spool) {
    sp = assign.tag.spool;
  } else {
    strlcpy(sp.type, info.material[0] ? info.material : info.name, sizeof(sp.type));
    strlcpy(sp.colorHex, info.colorHex[0] ? info.colorHex : "FFFFFF", 7);  // 6 Stellen RRGGBB
    strlcpy(sp.brand, info.vendor, sizeof(sp.brand));
    sp.minTemp = 200;  // Standardwerte, falls die Sorte nicht im Katalog steht
    sp.maxTemp = 230;
    catalogGet(catalog);
    for (int i = 0; i < catalog.materialCount; i++) {
      if (strcasecmp(catalog.materials[i].name, sp.type) == 0) {
        sp.minTemp = catalog.materials[i].minTemp;
        sp.maxTemp = catalog.materials[i].maxTemp;
        break;
      }
    }
  }
  sp.spoolmanId = assign.pickedId;
  return tagBuildSpool(sp, json, cap) != 0;
}

static void assignBuildList() {
  for (int i = 0; i < assignList.count; i++) {
    char text[96];
    if (assignList.items[i].remaining >= 0) {
      snprintf(text, sizeof(text), "#%d %s, %ld g", assignList.items[i].id, assignList.items[i].label,
               lroundf(assignList.items[i].remaining));
    } else {
      snprintf(text, sizeof(text), "#%d %s", assignList.items[i].id, assignList.items[i].label);
    }
    lv_obj_t *btn = uiMakeButton(assign.list, text, onAssignPick, true, (void *)(intptr_t)assignList.items[i].id);
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(label, LV_PCT(92));
  }
}

static void assignTick(lv_timer_t *) {
  switch (assign.step) {
    case AssignStep::Loading: {
      spoolmanListResult(assignList);
      if (assignList.state == SpoolmanState::Busy || assignList.seq == assign.listSeq) {
        break;
      }
      if (assignList.state != SpoolmanState::Found) {
        assignFail(assignList.error);
      } else if (assignList.count == 0) {
        assignFail(tr(Str::NoSpools));
      } else {
        assignBuildList();
        lv_obj_remove_flag(assign.list, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_fmt(assign.status, "%s (%d)", tr(Str::AssignSpool), assignList.total);
        lv_obj_set_style_text_color(assign.status, themeColors().text, 0);
        assign.step = AssignStep::Pick;
      }
      break;
    }
    case AssignStep::Linking: {
      SpoolmanResult w = spoolmanWriteResult();
      if (w.state == SpoolmanState::Busy || w.seq == assign.writeSeq) {
        break;
      }
      if (w.state != SpoolmanState::Found) {
        assignFail(w.error);
        break;
      }
      assign.lookupSeq = spoolmanLookupResult().seq;
      spoolmanRequestLookup(assign.uid, assign.pickedId);
      assign.step = AssignStep::LookingUp;
      break;
    }
    case AssignStep::LookingUp: {
      SpoolmanResult r = spoolmanLookupResult();
      if (r.state == SpoolmanState::Busy || r.seq == assign.lookupSeq) {
        break;
      }
      char json[256];
      if (r.state != SpoolmanState::Found || !buildAssignedJson(r.spool, json, sizeof(json))) {
        assignFail(r.error[0] ? r.error : tr(Str::WriteFailed));
        break;
      }
      if (!nfcRequestWrite(json)) {
        assignFail(tr(Str::WriteFailed));
        break;
      }
      assignSay(tr(Str::PlaceTag), themeColors().warn);
      assign.step = AssignStep::Writing;
      break;
    }
    case AssignStep::Writing:
      switch (nfcWriteStatus()) {
        case NfcWrite::WaitingForTag: assignSay(tr(Str::PlaceTag), themeColors().warn); break;
        case NfcWrite::Writing: assignSay(tr(Str::Writing), themeColors().warn); break;
        case NfcWrite::Ok:
          assign.step = AssignStep::Done;
          assignSay(tr(Str::AssignDone), themeColors().ok);
          break;
        case NfcWrite::ErrLocked: assignFail(tr(Str::WriteLocked)); break;
        case NfcWrite::ErrTooBig: assignFail(tr(Str::WriteTooBig)); break;
        case NfcWrite::ErrNotNtag: assignFail(tr(Str::WriteNotNtag)); break;
        case NfcWrite::ErrTimeout: assignFail(tr(Str::WriteTimeout)); break;
        case NfcWrite::Idle: break;
        default: assignFail(tr(Str::WriteFailed)); break;
      }
      break;
    default:
      break;
  }
}

void screenAssignSpoolOpen() {
  NfcState n = nfcGet();
  assign = AssignCtx();
  strlcpy(assign.uid, n.uid, sizeof(assign.uid));
  assign.tag = n.content;

  lv_obj_t *scr = uiNewScreen();
  addHeader(scr, tr(Str::UnknownSpool), onAssignBack);
  assign.status = addLabel(scr, &theme().body, true);
  lv_label_set_text(assign.status, tr(Str::Loading));
  assign.list = addList(scr);
  lv_obj_add_flag(assign.list, LV_OBJ_FLAG_HIDDEN);

  spoolmanListResult(assignList);
  assign.listSeq = assignList.seq;
  spoolmanRequestList();
  addScreenTimer(scr, assignTick, 300, nullptr);
  uiNavigate(scr);
}
