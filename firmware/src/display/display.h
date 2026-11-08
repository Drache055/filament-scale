#pragma once
#include <cstdint>

// LovyanGFX + LVGL-Anbindung (Flush- und Touch-Callback). Nur aus dem ui-Task aufrufen.
bool displayBegin();
void displaySetBacklight(uint8_t percent);

// VCOMS-Register des ST7789 (0x00..0x3F). Wirkt sofort; mit save=true zusätzlich in NVS gespeichert.
constexpr uint8_t DISPLAY_VCOMS_DEFAULT = 0x24;  // 36, per Sichtprüfung gewählt
constexpr uint8_t DISPLAY_VCOMS_MAX = 0x3F;
uint8_t displayGetVcoms();
void displaySetVcoms(uint8_t value, bool save);

// Touch-Kalibrierung: Anzeigekoordinate = a * Rohwert + b, je Achse. Die Rohwerte decken die Glasfläche nicht ganz ab.
uint32_t displayTouchReleaseCount();                  // zählt mit jedem losgelassenen Tipp hoch
void displayTouchLastRaw(uint16_t &x, uint16_t &y);   // Rohwert des letzten Tipps
void displayTouchSetCalibration(float ax, float bx, float ay, float by, bool save);
void displayTouchRequestReset();                      // threadsicher aus anderen Tasks, wirkt im ui-Task
bool displayTouchCalibrated();

// Bildschirmschoner: nach dimMinutes ohne Aktivität auf DIM_LEVEL dimmen, nach offMinutes ausschalten (0 = nie).
// Aktivität: Berührung (der erste Tipp weckt nur), displayWake() aus anderen Modulen.
void displayWake();
void displayPowerTick();  // regelmäßig im ui-Task aufrufen
void displaySetScreensaver(uint8_t dimMinutes, uint8_t offMinutes, bool save);
void displayGetScreensaver(uint8_t &dimMinutes, uint8_t &offMinutes);
bool displayAsleep();     // gedimmt oder aus
