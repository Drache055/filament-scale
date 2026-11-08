#pragma once
// Zentrale Pinbelegung. Quelle Display/Touch: Waveshare-Demo ESP32-S3-Touch-LCD-2 (Arduino/examples/01_factory).

// LCD (ST7789T3, SPI)
#define PIN_LCD_SCLK 39
#define PIN_LCD_MOSI 38
#define PIN_LCD_MISO 40
#define PIN_LCD_DC   42
#define PIN_LCD_CS   45
#define PIN_LCD_RST  -1  // nicht belegt
#define PIN_LCD_BL   1

// I²C-Bus (Touch CST816 = 0x15)
#define PIN_I2C_SDA 48
#define PIN_I2C_SCL 47
#define PIN_TP_RST  -1   // nicht belegt

// HX711 (eigene 2-Draht-Schnittstelle, kein I²C). Beide Pins liegen auf Header P1, Pin 5 und 6.
// Quelle: Schaltplan ESP32-S3-Touch-LCD-2 (PinOut). IO17/IO18 sind sonst nur mit der unbestückten Kamera verbunden bzw. frei.
#define PIN_HX711_SCK  17  // Header P1 Pin 5
#define PIN_HX711_DOUT 18  // Header P1 Pin 6

// NFC (PN532, SPI-Modus: DIP SW1 = OFF, SW2 = ON), Software-SPI an der rechten Header-Leiste P2.
// Am I²C-Modus hielt das Modul SCL am Waagen-Board dauerhaft auf low (an IO2/4/6/13/15 und an IO47/48), per SPI nicht.
// Versorgung 3V3 = P2 Pin 1, GND = P2 Pin 2.
#define PIN_NFC_SCK  15  // Header P2, Bild-Nr. 7
#define PIN_NFC_MISO 13  // Header P2, Bild-Nr. 8
#define PIN_NFC_MOSI 11  // Header P2, Bild-Nr. 9
#define PIN_NFC_SS   12  // Header P2, Bild-Nr. 10
