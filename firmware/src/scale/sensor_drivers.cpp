#include "sensor.h"

#include <Arduino.h>
#include <HX711.h>
#include <Wire.h>

#include "bus/i2c_bus.h"

namespace {

// ---- HX711 (auch HX710B): Bibliothek bogde/HX711, Kanal A, Verstärkung 128 ----
class Hx711Sensor : public LoadSensor {
 public:
  explicit Hx711Sensor(const SensorConfig &c) : sck(c.pin[0]), dout(c.pin[1]) {}
  bool begin() override {
    hx.begin(dout, sck);
    return true;
  }
  bool ready() override { return hx.is_ready(); }
  bool read(long &raw) override {
    raw = hx.read();
    return true;
  }

 private:
  int sck, dout;
  HX711 hx;
};

// ---- ADS1232/ADS1231 (TI): DOUT low = Wert bereit, 24 Bit MSB zuerst, Daten nach der steigenden SCLK-Flanke gültig.
// Ein 25. Takt setzt DOUT wieder auf high. Verstärkung, Datenrate (SPEED) und Kanal stellt das Modul per Lötbrücke ein.
// Am Gerät nicht geprüft.
class Ads1232Sensor : public LoadSensor {
 public:
  explicit Ads1232Sensor(const SensorConfig &c) : sclk(c.pin[0]), dout(c.pin[1]), pdwn(c.pin[2]) {}
  bool begin() override {
    pinMode(sclk, OUTPUT);
    digitalWrite(sclk, LOW);
    pinMode(dout, INPUT_PULLUP);  // ohne Sensor bleibt DOUT high = nie bereit
    if (pdwn >= 0) {
      pinMode(pdwn, OUTPUT);
      digitalWrite(pdwn, LOW);
      delayMicroseconds(100);
      digitalWrite(pdwn, HIGH);  // Reset, danach läuft der Wandler (erste Werte nach der Einschwingzeit)
    }
    return true;
  }
  bool ready() override { return digitalRead(dout) == LOW; }
  bool read(long &raw) override {
    uint32_t v = 0;
    for (int i = 0; i < 24; i++) {
      digitalWrite(sclk, HIGH);
      delayMicroseconds(1);
      v = (v << 1) | (digitalRead(dout) ? 1u : 0u);
      digitalWrite(sclk, LOW);
      delayMicroseconds(1);
    }
    digitalWrite(sclk, HIGH);  // 25. Takt: DOUT auf high bis zum nächsten Wert
    delayMicroseconds(1);
    digitalWrite(sclk, LOW);
    raw = sensorSignExtend24(v);
    return true;
  }

 private:
  int sclk, dout, pdwn;
};

// ---- NAU7802 (Nuvoton), I²C 0x2A. Ablauf nach Datenblatt 9.1 und SparkFun-Bibliothek: Reset, Power-up, LDO 3,3 V,
// Verstärkung 128, 10 SPS, Offset-Kalibrierung. Am Touch-Bus (GPIO47/48) mit dem Bus-Mutex, sonst eigener Bus (Wire1).
// Am Gerät nicht geprüft.
class Nau7802Sensor : public LoadSensor {
 public:
  explicit Nau7802Sensor(const SensorConfig &c)
      : scl(c.pin[0]), sda(c.pin[1]), shared(sensorUsesTouchBus(c)), bus(shared ? Wire : Wire1) {}

  bool begin() override {
    if (!shared && !busStarted) {
      bus.begin(sda, scl);
      bus.setClock(100000);
      busStarted = true;
    }
    if (!probe()) {
      return false;
    }
    bool ok = write(REG_PU_CTRL, 0x01);  // RR: Register zurücksetzen
    delay(1);
    ok = ok && write(REG_PU_CTRL, 0x06);  // RR aus, PUD + PUA
    uint32_t t0 = millis();
    uint8_t v = 0;
    while (ok && millis() - t0 < 200) {
      if (readReg(REG_PU_CTRL, v) && (v & 0x08)) {  // PUR: Power-up fertig
        break;
      }
      delay(2);
    }
    ok = ok && (v & 0x08);
    ok = ok && write(REG_CTRL1, (0b100 << 3) | 0b111);  // VLDO 3,3 V, GAINS 128
    ok = ok && write(REG_PU_CTRL, 0x80 | 0x06);         // AVDDS: interner LDO
    ok = ok && write(REG_CTRL2, 0x00);                   // CRS 10 SPS, Kanal 1
    ok = ok && write(REG_ADC, 0x30);                     // REG_CHPS aus (Datenblatt 9.1)
    ok = ok && readReg(REG_PGA_PWR, v) && write(REG_PGA_PWR, v | 0x80);  // 330-pF-Entkopplung an Kanal 2
    ok = ok && write(REG_PU_CTRL, 0x80 | 0x10 | 0x06);  // CS: Wandlung starten
    ok = ok && calibrateAfe();
    return ok;
  }

  bool ready() override {
    uint8_t v = 0;
    return readReg(REG_PU_CTRL, v) && (v & 0x20);  // CR: Wert bereit
  }

  bool read(long &raw) override {
    uint8_t b[3];
    if (!readRegs(REG_ADCO_B2, b, 3)) {
      return false;
    }
    raw = sensorSignExtend24(((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | b[2]);
    return true;
  }

 private:
  static constexpr uint8_t REG_PU_CTRL = 0x00, REG_CTRL1 = 0x01, REG_CTRL2 = 0x02, REG_ADCO_B2 = 0x12, REG_ADC = 0x15,
                           REG_PGA_PWR = 0x1C;
  int scl, sda;
  bool shared;
  TwoWire &bus;
  bool busStarted = false;

  bool lock() { return !shared || i2cBusLock(50); }
  void unlock() {
    if (shared) {
      i2cBusUnlock();
    }
  }
  bool probe() {
    if (!lock()) {
      return false;
    }
    bus.beginTransmission(NAU7802_ADDR);
    bool ok = bus.endTransmission(true) == 0;
    unlock();
    return ok;
  }
  bool write(uint8_t reg, uint8_t val) {
    if (!lock()) {
      return false;
    }
    bus.beginTransmission(NAU7802_ADDR);
    bus.write(reg);
    bus.write(val);
    bool ok = bus.endTransmission(true) == 0;
    unlock();
    return ok;
  }
  bool readRegs(uint8_t reg, uint8_t *buf, uint8_t n) {
    if (!lock()) {
      return false;
    }
    bus.beginTransmission(NAU7802_ADDR);
    bus.write(reg);
    bool ok = bus.endTransmission(false) == 0 && bus.requestFrom((uint8_t)NAU7802_ADDR, n) == n;
    if (ok) {
      for (uint8_t i = 0; i < n; i++) {
        buf[i] = bus.read();
      }
    }
    unlock();
    return ok;
  }
  bool readReg(uint8_t reg, uint8_t &v) { return readRegs(reg, &v, 1); }

  // Interne Offset-Kalibrierung (CALS in CTRL2), Ende abwarten, CAL_ERR prüfen
  bool calibrateAfe() {
    uint8_t v = 0;
    if (!readReg(REG_CTRL2, v) || !write(REG_CTRL2, v | 0x04)) {
      return false;
    }
    uint32_t t0 = millis();
    while (millis() - t0 < 1000) {
      if (readReg(REG_CTRL2, v) && !(v & 0x04)) {
        return !(v & 0x08);
      }
      delay(5);
    }
    return false;
  }
};

}  // namespace

LoadSensor *sensorCreate(const SensorConfig &cfg) {
  switch (cfg.type) {
    case SensorType::NAU7802:
      return new Nau7802Sensor(cfg);
    case SensorType::ADS1232:
      return new Ads1232Sensor(cfg);
    case SensorType::HX711:
    default:
      return new Hx711Sensor(cfg);
  }
}
