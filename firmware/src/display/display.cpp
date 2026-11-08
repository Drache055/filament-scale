#include "display.h"
#include "util/log.h"

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <Preferences.h>
#include <Wire.h>
#include <lvgl.h>

#include "bus/i2c_bus.h"
#include "config.h"
#include "pins.h"

// ---- LovyanGFX: ST7789 (SPI), Hochformat 240x320 ----
class Lcd : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 panel;
  lgfx::Bus_SPI bus;
  lgfx::Light_PWM light;

 public:
  Lcd() {
    {
      auto cfg = bus.config();
      cfg.spi_host = SPI3_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = PIN_LCD_SCLK;
      cfg.pin_mosi = PIN_LCD_MOSI;
      cfg.pin_miso = PIN_LCD_MISO;
      cfg.pin_dc = PIN_LCD_DC;
      bus.config(cfg);
      panel.setBus(&bus);
    }
    {
      auto cfg = panel.config();
      cfg.pin_cs = PIN_LCD_CS;
      cfg.pin_rst = PIN_LCD_RST;
      cfg.pin_busy = -1;
      cfg.panel_width = 240;
      cfg.panel_height = 320;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.readable = false;
      cfg.invert = true;  // IPS
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      panel.config(cfg);
    }
    {
      auto cfg = light.config();
      cfg.pin_bl = PIN_LCD_BL;
      cfg.invert = false;
      cfg.freq = 5000;
      cfg.pwm_channel = 7;
      light.config(cfg);
      panel.setLight(&light);
    }
    setPanel(&panel);
  }
};

static Lcd lcd;

// Panel-Feineinstellung aus dem Waveshare-Demo (Arduino_GFX). Die LovyanGFX-Defaults (u. a. VCOMS 0x28)
// passen nicht zu diesem Panel und führten zu Geisterbildern auf dunklen Flächen.
static void lcdCmd(uint8_t cmd, std::initializer_list<uint8_t> data) {
  lcd.writeCommand(cmd);
  for (uint8_t b : data) {
    lcd.writeData(b);
  }
}

static uint8_t vcoms = DISPLAY_VCOMS_DEFAULT;

static void lcdTunePanel() {
  lcd.startWrite();
  lcdCmd(0xB2, {0x0C, 0x0C, 0x00, 0x33, 0x33});  // Porch
  lcdCmd(0xB7, {0x35});                          // Gate control
  lcdCmd(0xBB, {vcoms});                         // VCOMS (Demo 0x19, LovyanGFX 0x28)
  lcdCmd(0xC0, {0x0C});                          // LCM control (0x2C aus dem Demo vertauscht Rot/Blau)
  lcdCmd(0xC2, {0x01});
  lcdCmd(0xC3, {0x12});                          // VRHS
  lcdCmd(0xC4, {0x20});
  lcdCmd(0xC6, {0x0F});                          // Framerate 60 Hz
  lcdCmd(0xD0, {0xA4, 0xA1});
  lcdCmd(0xE0, {0xF0, 0x09, 0x13, 0x12, 0x12, 0x2B, 0x3C, 0x44, 0x4B, 0x1B, 0x18, 0x17, 0x1D, 0x21});
  lcdCmd(0xE1, {0xF0, 0x09, 0x13, 0x0C, 0x0D, 0x27, 0x3B, 0x44, 0x4D, 0x0B, 0x17, 0x17, 0x1D, 0x21});
  lcd.endWrite();
}

// ---- LVGL ----
static lv_display_t *lvDisplay = nullptr;
static lv_indev_t *lvTouch = nullptr;
static uint8_t *drawBuf1 = nullptr;
static uint8_t *drawBuf2 = nullptr;

static void flushCb(lv_display_t *disp, const lv_area_t *area, uint8_t *px) {
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);
  lcd.startWrite();
  lcd.pushImage(area->x1, area->y1, w, h, reinterpret_cast<lgfx::rgb565_t *>(px));
  lcd.endWrite();
  lv_display_flush_ready(disp);
}

// CST816: ab Register 0x02 [Anzahl, XH, XL, YH, YL]. Im Hochformat entsprechen die Rohwerte den Displaykoordinaten.
static bool touchRead(uint16_t &x, uint16_t &y) {
  uint8_t buf[5] = {0};
  if (!i2cBusLock()) {
    return false;
  }
  Wire.beginTransmission(TOUCH_I2C_ADDR);
  Wire.write(0x02);
  bool ok = Wire.endTransmission(true) == 0 && Wire.requestFrom((uint8_t)TOUCH_I2C_ADDR, (uint8_t)5) == 5;
  if (ok) {
    for (auto &b : buf) {
      b = Wire.read();
    }
  }
  i2cBusUnlock();
  if (!ok || buf[0] == 0) {
    return false;
  }
  x = ((buf[1] & 0x0F) << 8) | buf[2];
  y = ((buf[3] & 0x0F) << 8) | buf[4];
  return true;
}

// Kalibrierung (Identität, solange nichts gespeichert ist)
static float calAx = 1, calBx = 0, calAy = 1, calBy = 0;
static bool touchCalibrated = false;
static volatile bool touchResetRequested = false;
static uint32_t releaseCount = 0;
static uint16_t lastRawX = 0, lastRawY = 0;

static void touchLoadCalibration() {
  Preferences prefs;
  prefs.begin("touch", false);
  if (prefs.getBool("cal", false)) {
    calAx = prefs.getFloat("ax", 1);
    calBx = prefs.getFloat("bx", 0);
    calAy = prefs.getFloat("ay", 1);
    calBy = prefs.getFloat("by", 0);
    touchCalibrated = true;
  }
  prefs.end();
  logPrintf("[DISPLAY] Touch-Kalibrierung: %s (x: %.3f %.1f, y: %.3f %.1f)\n", touchCalibrated ? "ja" : "nein", calAx,
                calBx, calAy, calBy);
}

// ---- Bildschirmschoner ----
static constexpr uint8_t BRIGHT_FULL = 255;
static constexpr uint8_t BRIGHT_DIM = 25;
static volatile uint32_t lastActivityMs = 0;
static uint8_t ssDimMin = 5, ssOffMin = 15;
static uint8_t brightness = BRIGHT_FULL;
static bool swallowTouch = false;  // Tipp, der nur den Bildschirm geweckt hat, darf nichts auslösen

static void touchCb(lv_indev_t *, lv_indev_data_t *data) {
  static bool wasPressed = false;
  if (touchResetRequested) {
    touchResetRequested = false;
    calAx = calAy = 1;
    calBx = calBy = 0;
    touchCalibrated = false;
    Preferences prefs;
    prefs.begin("touch", false);
    prefs.clear();
    prefs.end();
    logPrintln("[DISPLAY] Touch-Kalibrierung zurückgesetzt");
  }
  uint16_t x, y;
  bool touched = touchRead(x, y);
  if (touched) {
    if (brightness != BRIGHT_FULL) {
      swallowTouch = true;  // erster Tipp weckt nur
    }
    lastActivityMs = millis();
  } else {
    swallowTouch = false;
  }
  if (touched && swallowTouch) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }
  if (touched) {
    lastRawX = x;
    lastRawY = y;
    data->point.x = constrain((int)lroundf(calAx * x + calBx), 0, 239);
    data->point.y = constrain((int)lroundf(calAy * y + calBy), 0, 319);
    data->state = LV_INDEV_STATE_PRESSED;
    wasPressed = true;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
    if (wasPressed) {
      releaseCount++;
    }
    wasPressed = false;
  }
}

bool displayBegin() {
  if (!lcd.init()) {
    logPrintln("[DISPLAY] LCD-Init fehlgeschlagen");
    return false;
  }
  Preferences prefs;
  prefs.begin("display", false);
  vcoms = min<uint8_t>(prefs.getUChar("vcoms", DISPLAY_VCOMS_DEFAULT), DISPLAY_VCOMS_MAX);
  prefs.end();
  logPrintf("[DISPLAY] VCOMS 0x%02X\n", vcoms);
  lcdTunePanel();
  lcd.setRotation(0);
  lcd.fillScreen(TFT_BLACK);
  lcd.setBrightness(255);

  lv_init();
  lv_tick_set_cb([]() -> uint32_t { return millis(); });

  // Teilpuffer im internen, DMA-fähigen RAM (CLAUDE.md Abschnitt 10)
  const size_t bufBytes = 240 * DRAW_BUF_LINES * sizeof(lv_color16_t);
  drawBuf1 = (uint8_t *)heap_caps_malloc(bufBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  drawBuf2 = (uint8_t *)heap_caps_malloc(bufBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if (!drawBuf1 || !drawBuf2) {
    logPrintln("[DISPLAY] Kein Speicher für Draw-Buffer");
    return false;
  }

  lvDisplay = lv_display_create(240, 320);
  lv_display_set_color_format(lvDisplay, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(lvDisplay, flushCb);
  lv_display_set_buffers(lvDisplay, drawBuf1, drawBuf2, bufBytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

  touchLoadCalibration();
  {
    Preferences prefs;
    prefs.begin("display", false);
    ssDimMin = prefs.getUChar("dim", 5);
    ssOffMin = prefs.getUChar("off", 15);
    prefs.end();
  }
  lastActivityMs = millis();
  lvTouch = lv_indev_create();
  lv_indev_set_type(lvTouch, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(lvTouch, touchCb);

  logPrintln("[DISPLAY] Bereit");
  return true;
}

void displaySetBacklight(uint8_t percent) {
  lcd.setBrightness((uint16_t)percent * 255 / 100);
}

uint8_t displayGetVcoms() { return vcoms; }

void displaySetVcoms(uint8_t value, bool save) {
  vcoms = min<uint8_t>(value, DISPLAY_VCOMS_MAX);
  lcd.startWrite();
  lcdCmd(0xBB, {vcoms});
  lcd.endWrite();
  if (save) {
    Preferences prefs;
    prefs.begin("display", false);
    prefs.putUChar("vcoms", vcoms);
    prefs.end();
  }
}

uint32_t displayTouchReleaseCount() { return releaseCount; }

void displayTouchLastRaw(uint16_t &x, uint16_t &y) {
  x = lastRawX;
  y = lastRawY;
}

void displayTouchSetCalibration(float ax, float bx, float ay, float by, bool save) {
  calAx = ax;
  calBx = bx;
  calAy = ay;
  calBy = by;
  touchCalibrated = true;
  if (save) {
    Preferences prefs;
    prefs.begin("touch", false);
    prefs.putFloat("ax", ax);
    prefs.putFloat("bx", bx);
    prefs.putFloat("ay", ay);
    prefs.putFloat("by", by);
    prefs.putBool("cal", true);
    prefs.end();
  }
  logPrintf("[DISPLAY] Touch kalibriert (x: %.3f %.1f, y: %.3f %.1f)\n", ax, bx, ay, by);
}

void displayTouchRequestReset() { touchResetRequested = true; }

bool displayTouchCalibrated() { return touchCalibrated; }

// ---- Bildschirmschoner ----
void displayWake() { lastActivityMs = millis(); }

bool displayAsleep() { return brightness != BRIGHT_FULL; }

void displayPowerTick() {
  uint32_t idle = millis() - lastActivityMs;
  uint8_t target = BRIGHT_FULL;
  if (ssOffMin && idle >= (uint32_t)ssOffMin * 60000UL) {
    target = 0;
  } else if (ssDimMin && idle >= (uint32_t)ssDimMin * 60000UL) {
    target = BRIGHT_DIM;
  }
  if (target != brightness) {
    brightness = target;
    lcd.setBrightness(brightness);
    logPrintf("[DISPLAY] Helligkeit %u\n", brightness);
  }
}

void displaySetScreensaver(uint8_t dimMinutes, uint8_t offMinutes, bool save) {
  ssDimMin = dimMinutes;
  ssOffMin = offMinutes;
  lastActivityMs = millis();
  if (save) {
    Preferences prefs;
    prefs.begin("display", false);
    prefs.putUChar("dim", dimMinutes);
    prefs.putUChar("off", offMinutes);
    prefs.end();
  }
}

void displayGetScreensaver(uint8_t &dimMinutes, uint8_t &offMinutes) {
  dimMinutes = ssDimMin;
  offMinutes = ssOffMin;
}
