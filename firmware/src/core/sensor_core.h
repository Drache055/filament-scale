#pragma once
#include <cstdint>

// Wägesensor (CLAUDE.md 6.4): Typ, Pins und Prüfung der Pinbelegung. Reine Logik ohne Hardwarezugriff.
// Die Treiber liegen in scale/sensor_*.cpp, die Messlogik (Fenster, Tara, Kalibrierung) bleibt gemeinsam in scale/scale.cpp.
enum class SensorType : uint8_t {
  HX711 = 0,    // 2-Draht (SCK, DOUT), auch HX710B
  NAU7802 = 1,  // I²C, Adresse 0x2A
  ADS1232 = 2,  // 2-Draht (SCLK, DOUT), optional PDWN; auch ADS1231
};

constexpr int SENSOR_TYPE_COUNT = 3;
constexpr int SENSOR_PIN_COUNT = 3;
constexpr uint8_t NAU7802_ADDR = 0x2A;

// Bedeutung der Pins je Typ:
//   HX711:   pin[0] = SCK,  pin[1] = DOUT, pin[2] = -1
//   ADS1232: pin[0] = SCLK, pin[1] = DOUT, pin[2] = PDWN (optional, -1 = fest auf 3,3 V)
//   NAU7802: pin[0] = SCL,  pin[1] = SDA,  pin[2] = -1. SCL 47 / SDA 48 = Touch-Bus mitbenutzen.
struct SensorConfig {
  SensorType type = SensorType::HX711;
  int8_t pin[SENSOR_PIN_COUNT] = {17, 18, -1};

  bool operator==(const SensorConfig &o) const {
    return type == o.type && pin[0] == o.pin[0] && pin[1] == o.pin[1] && pin[2] == o.pin[2];
  }
  bool operator!=(const SensorConfig &o) const { return !(*this == o); }
};

SensorConfig sensorDefault(SensorType type);  // Standardpins je Typ
const char *sensorTypeId(SensorType type);    // "hx711", "nau7802", "ads1232"
const char *sensorTypeName(SensorType type);  // "HX711", "NAU7802", "ADS1232"
bool sensorTypeParse(const char *id, SensorType &out);
int sensorPinsUsed(SensorType type);      // Anzahl Pflichtpins (2)
bool sensorPinOptional(SensorType type, int idx);
const char *sensorCalSuffix(SensorType type);  // Anhang der NVS-Schlüssel der Kalibrierung ("" für HX711)

// Frei verwendbare GPIOs (Header P1/P2 ohne Display, Touch, NFC, USB und UART0)
bool sensorPinFree(int gpio);
const int *sensorFreePins(int &count);

// Prüft Typ und Pins. Bei Fehler steht in error eine deutsche Meldung für Webseite und Log.
bool sensorValidate(const SensorConfig &cfg, const char *&error);

// Gemeinsamer I²C-Bus mit dem Touch (nur NAU7802)
bool sensorUsesTouchBus(const SensorConfig &cfg);

// Rohwerte
long sensorSignExtend24(uint32_t v);                // 24-Bit-Zweierkomplement auf long
bool sensorRawInvalid(SensorType type, long raw);  // offensichtlich keine Messung (Sättigung, freier Datenpin)
