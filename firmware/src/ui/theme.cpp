#include "theme.h"
#include "util/log.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#include "config.h"
#include "fonts/fonts.h"

namespace {

struct Palette {
  uint32_t bg = 0x10131A, surface = 0x1B2030, primary = 0x3B82F6, primaryText = 0xFFFFFF;
  uint32_t text = 0xF1F5F9, textDim = 0x8892A6, ok = 0x22C55E, warn = 0xF59E0B, error = 0xEF4444;
};

struct Params {
  Palette c;
  int radius = 12, padding = 10, buttonHeight = 52;
  int fSmall = 14, fNormal = 20, fLarge = 28, fWeight = 48;
};

ThemeStyles styles;
ThemeColors colors;
int buttonHeight = 52;
int padding = 10;
bool initialized = false;

const lv_font_t *fontFor(int size) {
  switch (size) {
    case 14: return &lv_font_app_14;
    case 20: return &lv_font_app_20;
    case 28: return &lv_font_app_28;
    case 48: return &lv_font_app_48;
    default:
      logPrintf("[UI] Schriftgröße %d nicht verfügbar (14/20/28/48), nehme 20\n", size);
      return &lv_font_app_20;
  }
}

uint32_t parseColor(JsonVariantConst v, uint32_t fallback) {
  const char *s = v | "";
  if (s[0] == '#') {
    s++;
  }
  if (strlen(s) != 6) {
    return fallback;
  }
  char *end = nullptr;
  uint32_t val = strtoul(s, &end, 16);
  return (*end == '\0') ? val : fallback;
}

void readFile(Params &p) {
  File f = LittleFS.open(THEME_FILE, "r");
  if (!f) {
    logPrintln("[UI] Keine theme.json, nutze Defaults");
    return;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) {
    logPrintf("[UI] theme.json ungültig (%s), nutze Defaults\n", err.c_str());
    return;
  }
  JsonObjectConst c = doc["colors"];
  p.c.bg = parseColor(c["bg"], p.c.bg);
  p.c.surface = parseColor(c["surface"], p.c.surface);
  p.c.primary = parseColor(c["primary"], p.c.primary);
  p.c.primaryText = parseColor(c["primary_text"], p.c.primaryText);
  p.c.text = parseColor(c["text"], p.c.text);
  p.c.textDim = parseColor(c["text_dim"], p.c.textDim);
  p.c.ok = parseColor(c["ok"], p.c.ok);
  p.c.warn = parseColor(c["warn"], p.c.warn);
  p.c.error = parseColor(c["error"], p.c.error);
  p.radius = doc["radius"] | p.radius;
  p.padding = doc["padding"] | p.padding;
  p.buttonHeight = max(48, (int)(doc["button_height"] | p.buttonHeight));  // Mindesthöhe laut CLAUDE.md 8.3
  JsonObjectConst fo = doc["fonts"];
  p.fSmall = fo["small"] | p.fSmall;
  p.fNormal = fo["normal"] | p.fNormal;
  p.fLarge = fo["large"] | p.fLarge;
  p.fWeight = fo["weight"] | p.fWeight;
  logPrintln("[UI] theme.json geladen");
}

}  // namespace

void themeLoad() {
  Params p;
  readFile(p);

  if (initialized) {  // Styles zurücksetzen, falls erneut geladen wird
    lv_style_reset(&styles.screen);
    lv_style_reset(&styles.card);
    lv_style_reset(&styles.button);
    lv_style_reset(&styles.buttonPressed);
    lv_style_reset(&styles.title);
    lv_style_reset(&styles.body);
    lv_style_reset(&styles.dim);
    lv_style_reset(&styles.weight);
  }
  initialized = true;

  lv_style_init(&styles.screen);
  lv_style_set_bg_color(&styles.screen, lv_color_hex(p.c.bg));
  lv_style_set_bg_opa(&styles.screen, LV_OPA_COVER);
  lv_style_set_text_color(&styles.screen, lv_color_hex(p.c.text));
  lv_style_set_text_font(&styles.screen, fontFor(p.fNormal));
  lv_style_set_pad_all(&styles.screen, p.padding);

  lv_style_init(&styles.card);
  lv_style_set_bg_color(&styles.card, lv_color_hex(p.c.surface));
  lv_style_set_bg_opa(&styles.card, LV_OPA_COVER);
  lv_style_set_radius(&styles.card, p.radius);
  lv_style_set_border_width(&styles.card, 0);
  lv_style_set_pad_all(&styles.card, p.padding);

  lv_style_init(&styles.button);
  lv_style_set_bg_color(&styles.button, lv_color_hex(p.c.primary));
  lv_style_set_bg_opa(&styles.button, LV_OPA_COVER);
  lv_style_set_text_color(&styles.button, lv_color_hex(p.c.primaryText));
  lv_style_set_text_font(&styles.button, fontFor(p.fNormal));
  lv_style_set_radius(&styles.button, p.radius);
  lv_style_set_border_width(&styles.button, 0);
  lv_style_set_shadow_width(&styles.button, 0);
  lv_style_set_height(&styles.button, p.buttonHeight);

  lv_style_init(&styles.buttonPressed);
  lv_style_set_bg_color(&styles.buttonPressed, lv_color_darken(lv_color_hex(p.c.primary), 40));

  lv_style_init(&styles.title);
  lv_style_set_text_font(&styles.title, fontFor(p.fLarge));
  lv_style_set_text_color(&styles.title, lv_color_hex(p.c.text));

  lv_style_init(&styles.body);
  lv_style_set_text_font(&styles.body, fontFor(p.fNormal));
  lv_style_set_text_color(&styles.body, lv_color_hex(p.c.text));

  lv_style_init(&styles.dim);
  lv_style_set_text_font(&styles.dim, fontFor(p.fSmall));
  lv_style_set_text_color(&styles.dim, lv_color_hex(p.c.textDim));

  lv_style_init(&styles.weight);
  lv_style_set_text_font(&styles.weight, fontFor(p.fWeight));
  lv_style_set_text_color(&styles.weight, lv_color_hex(p.c.text));

  colors = {lv_color_hex(p.c.ok), lv_color_hex(p.c.warn), lv_color_hex(p.c.error), lv_color_hex(p.c.textDim), lv_color_hex(p.c.text)};
  buttonHeight = p.buttonHeight;
  padding = p.padding;
}

const ThemeStyles &theme() { return styles; }
const ThemeColors &themeColors() { return colors; }
int themeButtonHeight() { return buttonHeight; }
int themePadding() { return padding; }
