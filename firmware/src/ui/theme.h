#pragma once
#include <lvgl.h>

// Theme aus /theme.json (LittleFS). Fehlt die Datei oder ist sie ungültig, gelten die eingebauten Defaults.
// Änderungen am Aussehen: data/theme.json anpassen und per "pio run -e waage-ota -t uploadfs" aufspielen.
struct ThemeStyles {
  lv_style_t screen;
  lv_style_t card;
  lv_style_t button;
  lv_style_t buttonPressed;
  lv_style_t title;     // Schriftgröße "large"
  lv_style_t body;      // Schriftgröße "normal"
  lv_style_t dim;       // Schriftgröße "small", gedämpft
  lv_style_t weight;    // Live-Gewicht
};

struct ThemeColors {
  lv_color_t ok, warn, error, dim, text;
};

void themeLoad();  // nur im ui-Task aufrufen, vor dem Bau der Screens
const ThemeStyles &theme();
const ThemeColors &themeColors();
int themeButtonHeight();
int themePadding();
