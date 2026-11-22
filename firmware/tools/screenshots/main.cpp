// Rendert die echten UI-Screens (LVGL 9.5) am Rechner und schreibt sie als PPM nach <out>/NN_name.ppm.
// Aufruf: ./shots <out-Verzeichnis>. Die Umwandlung in PNG macht build.sh.
#include "mock.h"

#include <Arduino.h>
#include <lvgl.h>

// ui.cpp und screens_menu.cpp direkt einbinden, damit auch die statischen Funktionen (Home, Kalibrierung Schritt 2) erreichbar sind.
#include "../../src/ui/ui.cpp"
#include "../../src/ui/screens_menu.cpp"
#include "../../src/ui/screen_weigh.cpp"
#include "../../src/ui/screen_simple.cpp"

static uint16_t frame[240 * 320];
static const char *outDir = ".";
static int shotNo = 0;

static void flushCb(lv_display_t *disp, const lv_area_t *area, uint8_t *px) {
  int w = lv_area_get_width(area);
  const uint16_t *src = reinterpret_cast<const uint16_t *>(px);
  for (int y = area->y1; y <= area->y2; y++) {
    memcpy(&frame[y * 240 + area->x1], src, w * 2);
    src += w;
  }
  lv_display_flush_ready(disp);
}

static void settle() {
  for (int i = 0; i < 6; i++) {
    mock.now += 100;
    lv_timer_handler();
  }
  lv_refr_now(nullptr);
}

static void shot(const char *name) {
  settle();
  char path[512];
  snprintf(path, sizeof(path), "%s/%02d_%s.ppm", outDir, ++shotNo, name);
  FILE *f = fopen(path, "wb");
  fprintf(f, "P6\n240 320\n255\n");
  for (int i = 0; i < 240 * 320; i++) {
    uint16_t c = frame[i];
    uint8_t rgb[3] = {(uint8_t)(((c >> 11) & 31) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63), (uint8_t)((c & 31) * 255 / 31)};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
  printf("%s\n", path);
}

static void setWeight(float g, bool stable = true) {
  mock.scale = ScaleReading();
  mock.scale.valid = mock.scale.calibrated = true;
  mock.scale.stable = stable;
  mock.scale.empty = stable && g <= 5 && g >= -5;
  mock.scale.grams = g;
  mock.scale.rawNet = (long)(g * 420);
}

// Zeit vorspulen, damit die Timer des Wiegeablaufs laufen
static void advance(uint32_t ms) {
  for (uint32_t t = 0; t < ms; t += 200) {
    mock.now += 200;
    lv_timer_handler();
  }
}

static void clickRowButton(int rowIndex, int btnIndex) {
  lv_obj_t *row = lv_obj_get_child(lv_screen_active(), rowIndex);
  lv_obj_send_event(lv_obj_get_child(row, btnIndex), LV_EVENT_CLICKED, nullptr);
}

// Button im Card-Bereich (Auswahl "Filament wiegen": Kind 1 und 2 der Card)
static void clickCardButton(int childIndex) {
  lv_obj_t *card = lv_obj_get_child(lv_screen_active(), 1);
  lv_obj_send_event(lv_obj_get_child(card, childIndex), LV_EVENT_CLICKED, nullptr);
}

static void setBoxTag(bool present) {
  mock.nfc = NfcState();
  mock.nfc.moduleReady = true;
  if (present) {
    mock.nfc.tagPresent = true;
    strlcpy(mock.nfc.uid, "04BB22CC33DD44", sizeof(mock.nfc.uid));
    mock.nfc.seq = mock.nfc.readSeq = 2;
    mock.nfc.hasNdef = true;
    mock.nfc.content.kind = TagKind::Box;
    strlcpy(mock.nfc.content.box.id, "BOX-03", 16);
    strlcpy(mock.nfc.content.box.name, "Box 03", 32);
  }
}

static void setSpoolTag() {
  mock.nfc = NfcState();
  mock.nfc.moduleReady = mock.nfc.tagPresent = true;
  strlcpy(mock.nfc.uid, "04A1B2C3D4E5F6", sizeof(mock.nfc.uid));
  mock.nfc.capacity = 872;
  mock.nfc.seq = mock.nfc.readSeq = 1;
  mock.nfc.hasNdef = true;
  mock.nfc.content.kind = TagKind::Spool;
  strlcpy(mock.nfc.content.spool.brand, "Elegoo", 32);
  strlcpy(mock.nfc.content.spool.type, "PLA", 16);
  mock.lookup = SpoolmanResult();
  mock.lookup.state = SpoolmanState::Found;
  strlcpy(mock.lookup.uid, mock.nfc.uid, sizeof(mock.lookup.uid));
  mock.lookup.spool.id = 42;
  strlcpy(mock.lookup.spool.vendor, "Elegoo", 32);
  strlcpy(mock.lookup.spool.material, "PLA", 24);
  strlcpy(mock.lookup.spool.location, "Box 03", 65);
  strlcpy(mock.lookup.spool.owner, "Fabian", 24);
  mock.lookup.spool.remaining = 732;
}

static Lang baseLang = Lang::DE;

int main(int argc, char **argv) {
  if (argc > 1) outDir = argv[1];

  lv_init();
  lv_tick_set_cb(millis);
  lv_display_t *disp = lv_display_create(240, 320);
  static uint16_t buf[240 * 320];
  lv_display_set_buffers(disp, buf, nullptr, sizeof(buf), LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(disp, flushCb);

  mock.spoolman.everChecked = mock.spoolman.reachable = true;
  strlcpy(mock.spoolman.version, "0.27.0", sizeof(mock.spoolman.version));
  mock.nfc.moduleReady = true;
  setWeight(0);

  i18nBegin();
  const char *lg = getenv("SHOT_LANG");
  baseLang = (lg && !strcmp(lg, "en")) ? Lang::EN : Lang::DE;
  i18nSet(baseLang);
  themeLoad();
  buildHome();
  lv_timer_create(refreshTimerCb, 200, nullptr);

  // --- Home ---
  setWeight(0);
  shot("home_leer");
  setWeight(1284);
  mock.userLoggedIn = true;
  shot("home_gewicht_benutzer");
  mock.userLoggedIn = false;
  mock.queued = 2;
  setWeight(4620);
  mock.scale.overload = true;
  shot("home_ueberlast_warteschlange");
  mock.queued = 0;
  mock.scale = ScaleReading();
  mock.scale.valid = true;
  shot("home_nicht_kalibriert");
  mock.scale = ScaleReading();
  shot("home_kein_sensor");
  setWeight(0);

  // --- Menü / Einstellungen ---
  mock.adminLoggedIn = false;
  screenMenuOpen();
  shot("menue");
  mock.userLoggedIn = true;
  screenMenuOpen();
  shot("menue_benutzer");
  mock.userLoggedIn = false;
  screenMyLanguageOpen();
  mock.userLoggedIn = true;
  screenMyLanguageOpen();
  shot("meine_sprache");
  mock.userLoggedIn = false;

  screenAdminLoginOpen([] {});
  shot("admin_anmeldung");

  mock.adminLoggedIn = true;
  screenMenuOpen();
  shot("menue_admin");
  screenSettingsOpen();
  shot("einstellungen");
  screenWifiOpen();
  shot("wlan");
  mock.wifiUp = false;
  mock.spoolman.reachable = false;
  screenWifiOpen();
  shot("wlan_getrennt");
  mock.wifiUp = true;
  mock.spoolman.reachable = true;
  screenDisplayOpen();
  shot("display");
  screenLanguageOpen();
  shot("sprache");
  screenTouchCalOpen();
  shot("touch_kalibrierung");

  setWeight(0);
  mock.scale.rawNet = 312;
  screenCalibrationOpen();
  shot("kalibrierung_1_leer");
  setWeight(500);
  screenCalibrationStep2Open();
  shot("kalibrierung_2_gewicht");

  // --- NFC ---
  mock.nfc = NfcState();
  mock.nfc.moduleReady = true;
  screenNfcOpen();
  shot("nfc_bereit");
  setSpoolTag();
  screenNfcOpen();
  shot("nfc_spule_erkannt");
  mock.lookup.state = SpoolmanState::NotFound;
  screenNfcOpen();
  shot("nfc_spule_unbekannt");

  screenBoxWriteOpen();
  boxNumber = 3;
  boxRefreshNumber();
  shot("box_tag_schreiben");
  mock.nfcWrite = NfcWrite::Ok;
  shot("box_tag_ok");
  mock.nfcWrite = NfcWrite::Idle;

  screenFilamentTypeOpen();
  shot("filament_typ");
  screenFilamentMakerOpen();
  shot("filament_hersteller");
  screenFilamentColorOpen();
  shot("filament_farbe");
  pickedMaterial = 2;
  pickedMaker = 0;
  pickedColor = 3;
  screenFilamentWriteOpen();
  shot("filament_schreiben");

  // --- Unbekannte Spule zuordnen ---
  mock.list = SpoolListResult();
  mock.list.state = SpoolmanState::Found;
  mock.list.seq = 1;
  const char *labels[] = {"Elegoo PLA", "Elegoo PETG", "Generic ABS", "Elegoo PLA", "Prusament PETG"};
  float rem[] = {732, 410, 980, 55, 300};
  mock.list.count = 5;
  mock.list.total = 5;
  for (int i = 0; i < 5; i++) {
    mock.list.items[i].id = 40 + i;
    strlcpy(mock.list.items[i].label, labels[i], 48);
    mock.list.items[i].remaining = rem[i];
  }
  screenAssignSpoolOpen();
  mock.list.seq++;  // Ergebnis trifft nach dem Öffnen ein
  shot("spule_zuordnen");
  mock.nfc = NfcState();
  mock.nfc.moduleReady = true;
  mock.lookup = SpoolmanResult();

  // --- Englisch ---
  i18nSet(Lang::EN);
  uiRetranslateHome();
  uiShowHome();
  setWeight(1284);
  shot("en_home");
  screenMenuOpen();
  shot("en_menu");

  // --- Werbeclip (deutsch): Tara mit 3 g Drift, Box, Spule "Blaues PLA" ---
  i18nSet(baseLang);
  uiRetranslateHome();
  uiShowHome();
  mock.adminLoggedIn = false;
  mock.userLoggedIn = true;
  setWeight(3);
  shot("clip_home_drift_3g");
  setWeight(0);
  shot("clip_home_nach_tara");
  setWeight(412);
  shot("clip_home_box_412g");
  setWeight(1254);
  shot("clip_home_box_mit_spule_1254g");
  setWeight(412);
  shot("clip_home_spule_entnommen");
  setWeight(842);
  shot("clip_home_spule_842g");
  setWeight(0);
  mock.userLoggedIn = false;
  setSpoolTag();
  strlcpy(mock.nfc.content.spool.colorHex, "1E64FF", sizeof(mock.nfc.content.spool.colorHex));
  mock.lookup.spool.remaining = 592;
  strlcpy(mock.lookup.spool.owner, "Fabian", 24);
  mock.adminLoggedIn = true;
  screenNfcOpen();
  shot("clip_nfc_blaues_pla");

  // --- Wiegeablauf (Differenzwägung): Blaues PLA, Box 03 ---
  mock.adminLoggedIn = false;
  mock.userLoggedIn = true;
  uiShowHome();
  SpoolInfo sp;
  sp.id = 42;
  strlcpy(sp.vendor, "Elegoo", sizeof(sp.vendor));
  strlcpy(sp.material, "PLA", sizeof(sp.material));
  strlcpy(sp.colorHex, "1E64FF", sizeof(sp.colorHex));
  sp.remaining = 592;
  sp.emptySpool = 250;
  sp.initialWeight = 1000;
  setWeight(0);
  setBoxTag(false);
  weighFlowStart(sp, "04A1B2C3D4E5F6", "Fabian");
  advance(400);
  shot("ablauf_1_spule_erkannt");
  advance(2400);
  shot("ablauf_2_box_auflegen_leer");
  setWeight(412, false);
  setBoxTag(true);
  advance(400);
  shot("ablauf_3_box_erkannt");
  setWeight(412);
  advance(400);
  setWeight(412, false);
  setBoxTag(true);
  advance(400);
  shot("ablauf_4_spule_einstellen_leer");
  setWeight(1254, false);
  advance(400);
  shot("ablauf_5_spule_einstellen_842g");
  setWeight(1254);
  advance(400);
  shot("ablauf_6_bestaetigen");
  clickRowButton(2, 0);
  advance(400);
  shot("ablauf_7_speichern");
  mock.write.seq = 1;
  mock.write.state = SpoolmanState::Found;
  advance(400);
  shot("ablauf_8_gespeichert");
  setWeight(0);
  setBoxTag(false);
  advance(400);
  shot("ablauf_9_home_nach_ende");
    // --- Werbeclip: Zählschritte (hinten angehängt, damit 01–46 ihre Nummern behalten) ---
  // Box: Live-Gewicht zählt mit Zittern von 0 auf 412 g, Box-Tag noch nicht gelesen.
  setWeight(0);
  setBoxTag(false);
  weighFlowStart(sp, "04A1B2C3D4E5F6", "Fabian");
  advance(400);
  advance(2400);
  {
    const int boxSteps[] = {37, 118, 236, 349, 405, 419, 409, 414, 411, 412};
    char name[48];
    for (int i = 0; i < 10; i++) {
      setWeight(boxSteps[i], false);
      snprintf(name, sizeof(name), "zaehl_box_%02d_%dg", i + 1, boxSteps[i]);
      shot(name);
    }
  }
  // Weiter bis "Spule in die Box stellen" (wie im Ablauf oben).
  setWeight(412, false);
  setBoxTag(true);
  advance(400);
  setWeight(412);
  advance(400);
  setWeight(412, false);
  setBoxTag(true);
  advance(400);
  // Spule: Anzeige relativ zur Box zählt von 0 auf 842 g.
  {
    const int spoolSteps[] = {61, 198, 377, 560, 712, 808, 851, 836, 845, 841, 842};
    char name[48];
    for (int i = 0; i < 11; i++) {
      setWeight(412 + spoolSteps[i], false);
      snprintf(name, sizeof(name), "zaehl_spule_%02d_%dg", i + 1, spoolSteps[i]);
      shot(name);
    }
  }
  // --- Hotspot mit Geräte-Passwort (am Ende, damit die Nummern der älteren Bilder gleich bleiben) ---
  uiShowHome();
  mock.apActive = true;
  mock.wifiUp = false;
  setWeight(0);
  shot("home_hotspot");
  mock.apActive = false;
  mock.wifiUp = true;

  // --- Neue Box anlegen: Box mit leerem Tag (hinten angehängt, Nummern der älteren Bilder bleiben) ---
  setWeight(0);
  setBoxTag(false);
  weighFlowStart(sp, "04A1B2C3D4E5F6", "Fabian");
  advance(2800);
  setWeight(412);
  mock.nfc.moduleReady = mock.nfc.tagPresent = true;
  strlcpy(mock.nfc.uid, "04EE11FF22AA33", sizeof(mock.nfc.uid));
  mock.nfc.capacity = 872;
  mock.nfc.seq = mock.nfc.readSeq = 3;
  advance(400);
  shot("neue_box_1_leerer_tag");
  clickRowButton(2, 3);  // +5
  clickRowButton(2, 2);  // +1
  clickRowButton(3, 0);  // Schreiben
  mock.nfcWrite = NfcWrite::Ok;
  advance(400);
  shot("neue_box_2_geschrieben");
  mock.nfcWrite = NfcWrite::Idle;
  setBoxTag(true);
  strlcpy(mock.nfc.content.box.id, "BOX-07", 16);
  strlcpy(mock.nfc.content.box.name, "Box 07", 32);
  advance(400);
  advance(400);
  shot("neue_box_3_spule_einstellen");
  uiShowHome();

  // --- Einfache Waage (hinten angehängt, Nummern der älteren Bilder bleiben) ---
  mock.simpleMode = true;
  mock.userLoggedIn = false;
  mock.adminLoggedIn = false;
  setBoxTag(false);
  setWeight(0);
  uiShowHome();
  shot("einfach_1_home");
  screenSimpleWeighOpen();
  shot("einfach_2_auswahl");
  clickCardButton(1);  // Nur Spule
  setWeight(1092);
  advance(400);
  shot("einfach_3_nur_spule_1092g");
  setWeight(0);
  advance(400);  // Spule abgenommen: zurück zu Home
  screenSimpleWeighOpen();
  clickCardButton(2);  // Box + Spule
  setWeight(412, false);
  advance(400);
  shot("einfach_4_box_auflegen");
  setWeight(412);
  advance(400);
  shot("einfach_5_box_stabil");
  clickRowButton(2, 0);  // Box tarieren
  advance(400);
  setWeight(1254);
  advance(400);
  shot("einfach_6_box_und_spule_1254g");
  clickRowButton(3, 0);  // Fertig
  advance(400);
  setWeight(300);
  screenSimpleWeighOpen();
  clickCardButton(1);
  advance(400);
  shot("einfach_7_waage_leeren");
  uiShowHome();
  setWeight(0);

  // Smarte Waage, angemeldeter Benutzer mit "Nur wiegen"
  mock.simpleMode = false;
  mock.userLoggedIn = true;
  mock.weighOnly = true;
  uiShowHome();
  shot("home_nur_wiegen");
  mock.weighOnly = false;
  uiShowHome();

  return 0;
}
