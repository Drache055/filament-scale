#include "sensor_core.h"

#include <cstring>

#include "pins.h"

namespace {

// Header P1: IO2, IO4, IO6, IO16, IO17, IO18, IO21, IO8, IO7, IO10. Header P2: IO14, IO9.
// Nicht enthalten: IO19/20 (USB), IO43/44 (UART0, Bootmeldungen), IO47/48 (Touch-I²C), IO11/12/13/15 (NFC).
const int FREE_PINS[] = {2, 4, 6, 7, 8, 9, 10, 14, 16, 17, 18, 21};
constexpr int FREE_COUNT = sizeof(FREE_PINS) / sizeof(FREE_PINS[0]);

}  // namespace

SensorConfig sensorDefault(SensorType type) {
  SensorConfig c;
  c.type = type;
  switch (type) {
    case SensorType::NAU7802:
      c.pin[0] = PIN_I2C_SCL;
      c.pin[1] = PIN_I2C_SDA;
      c.pin[2] = -1;
      break;
    case SensorType::ADS1232:
    case SensorType::HX711:
    default:
      c.pin[0] = PIN_HX711_SCK;
      c.pin[1] = PIN_HX711_DOUT;
      c.pin[2] = -1;
      break;
  }
  return c;
}

const char *sensorTypeId(SensorType type) {
  switch (type) {
    case SensorType::NAU7802:
      return "nau7802";
    case SensorType::ADS1232:
      return "ads1232";
    default:
      return "hx711";
  }
}

const char *sensorTypeName(SensorType type) {
  switch (type) {
    case SensorType::NAU7802:
      return "NAU7802";
    case SensorType::ADS1232:
      return "ADS1232";
    default:
      return "HX711";
  }
}

bool sensorTypeParse(const char *id, SensorType &out) {
  if (!id) {
    return false;
  }
  for (int i = 0; i < SENSOR_TYPE_COUNT; i++) {
    SensorType t = (SensorType)i;
    if (strcmp(id, sensorTypeId(t)) == 0) {
      out = t;
      return true;
    }
  }
  return false;
}

int sensorPinsUsed(SensorType) { return 2; }

bool sensorPinOptional(SensorType type, int idx) { return type == SensorType::ADS1232 && idx == 2; }

const char *sensorCalSuffix(SensorType type) {
  switch (type) {
    case SensorType::NAU7802:
      return "_nau";
    case SensorType::ADS1232:
      return "_ads";
    default:
      return "";  // Schlüssel wie vor der Sensorwahl, die bestehende HX711-Kalibrierung bleibt gültig
  }
}

bool sensorPinFree(int gpio) {
  for (int p : FREE_PINS) {
    if (p == gpio) {
      return true;
    }
  }
  return false;
}

const int *sensorFreePins(int &count) {
  count = FREE_COUNT;
  return FREE_PINS;
}

bool sensorUsesTouchBus(const SensorConfig &cfg) {
  return cfg.type == SensorType::NAU7802 && cfg.pin[0] == PIN_I2C_SCL && cfg.pin[1] == PIN_I2C_SDA;
}

bool sensorValidate(const SensorConfig &cfg, const char *&error) {
  error = nullptr;
  if ((int)cfg.type >= SENSOR_TYPE_COUNT) {
    error = "Unbekannter Sensortyp";
    return false;
  }
  // Pins hinter den verwendeten müssen frei bleiben
  for (int i = 0; i < SENSOR_PIN_COUNT; i++) {
    bool used = i < sensorPinsUsed(cfg.type) || sensorPinOptional(cfg.type, i);
    if (!used && cfg.pin[i] != -1) {
      error = "Dieser Sensor braucht nur zwei Pins";
      return false;
    }
  }
  if (cfg.type == SensorType::NAU7802) {
    bool scl = cfg.pin[0] == PIN_I2C_SCL || cfg.pin[0] == PIN_I2C_SDA;
    bool sda = cfg.pin[1] == PIN_I2C_SCL || cfg.pin[1] == PIN_I2C_SDA;
    if (scl || sda) {
      if (!sensorUsesTouchBus(cfg)) {
        error = "Touch-Bus nur komplett: SCL = GPIO47, SDA = GPIO48";
        return false;
      }
      return true;
    }
  }
  for (int i = 0; i < SENSOR_PIN_COUNT; i++) {
    int p = cfg.pin[i];
    if (p == -1) {
      if (i < sensorPinsUsed(cfg.type)) {
        error = "Pin fehlt";
        return false;
      }
      continue;
    }
    if (!sensorPinFree(p)) {
      error = "Pin nicht frei (belegt von Display, Touch, NFC, USB oder UART, oder nicht am Header)";
      return false;
    }
    for (int j = 0; j < i; j++) {
      if (cfg.pin[j] == p) {
        error = "Ein Pin ist doppelt belegt";
        return false;
      }
    }
  }
  return true;
}

long sensorSignExtend24(uint32_t v) {
  v &= 0xFFFFFF;
  return (v & 0x800000) ? (long)v - 0x1000000L : (long)v;
}

bool sensorRawInvalid(SensorType type, long raw) {
  bool saturated = raw >= 0x7FFFFF || raw <= -0x800000;
  if (type == SensorType::NAU7802) {
    return saturated;  // Lesefehler erkennt der Treiber am I²C-Status, 0 und -1 sind gültige Messwerte
  }
  // HX711/ADS1232: Am freien DOUT-Pin liest man lauter Nullen (0) oder Einsen (-1)
  return saturated || raw == 0 || raw == -1;
}
