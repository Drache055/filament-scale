#include <unity.h>

#include <cstring>
#include <initializer_list>

#include "core/sensor_core.h"
#include "pins.h"

void setUp() {}
void tearDown() {}

static SensorConfig cfg(SensorType t, int a, int b, int c = -1) {
  SensorConfig s;
  s.type = t;
  s.pin[0] = a;
  s.pin[1] = b;
  s.pin[2] = c;
  return s;
}

static bool ok(const SensorConfig &c) {
  const char *err = nullptr;
  bool r = sensorValidate(c, err);
  TEST_ASSERT_TRUE(r ? err == nullptr : err != nullptr);
  return r;
}

void test_defaults_valid() {
  for (int t = 0; t < SENSOR_TYPE_COUNT; t++) {
    TEST_ASSERT_TRUE(ok(sensorDefault((SensorType)t)));
  }
  SensorConfig hx = sensorDefault(SensorType::HX711);
  TEST_ASSERT_EQUAL(PIN_HX711_SCK, hx.pin[0]);
  TEST_ASSERT_EQUAL(PIN_HX711_DOUT, hx.pin[1]);
  TEST_ASSERT_TRUE(SensorConfig() == hx);  // Werkszustand = bisherige Verdrahtung
  TEST_ASSERT_TRUE(sensorUsesTouchBus(sensorDefault(SensorType::NAU7802)));
}

void test_type_ids() {
  for (int t = 0; t < SENSOR_TYPE_COUNT; t++) {
    SensorType out;
    TEST_ASSERT_TRUE(sensorTypeParse(sensorTypeId((SensorType)t), out));
    TEST_ASSERT_EQUAL((int)t, (int)out);
  }
  SensorType out;
  TEST_ASSERT_FALSE(sensorTypeParse("HX711", out));
  TEST_ASSERT_FALSE(sensorTypeParse("", out));
  TEST_ASSERT_FALSE(sensorTypeParse(nullptr, out));
}

void test_cal_suffix() {
  TEST_ASSERT_EQUAL_STRING("", sensorCalSuffix(SensorType::HX711));  // alte Kalibrierung bleibt gültig
  TEST_ASSERT_TRUE(strcmp(sensorCalSuffix(SensorType::NAU7802), sensorCalSuffix(SensorType::ADS1232)) != 0);
  // NVS-Schlüssel höchstens 15 Zeichen ("offset" + Anhang)
  for (int t = 0; t < SENSOR_TYPE_COUNT; t++) {
    TEST_ASSERT_TRUE(strlen("offset") + strlen(sensorCalSuffix((SensorType)t)) <= 15);
  }
}

// Kein freier Pin darf von Display, Touch, NFC oder USB belegt sein
void test_free_pins_not_reserved() {
  const int reserved[] = {PIN_LCD_SCLK, PIN_LCD_MOSI, PIN_LCD_MISO, PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_BL,
                          PIN_I2C_SDA,  PIN_I2C_SCL,  PIN_NFC_SCK,  PIN_NFC_MISO, PIN_NFC_MOSI, PIN_NFC_SS,
                          19,           20,           43,           44,           0,  3,  45,  46};
  for (int r : reserved) {
    TEST_ASSERT_FALSE_MESSAGE(sensorPinFree(r), "reservierter Pin als frei gelistet");
  }
  int n = 0;
  const int *p = sensorFreePins(n);
  TEST_ASSERT_TRUE(n >= 4);
  for (int i = 0; i < n; i++) {
    TEST_ASSERT_TRUE(p[i] >= 0 && p[i] <= 48);
  }
}

void test_conflicts() {
  TEST_ASSERT_TRUE(ok(cfg(SensorType::HX711, 4, 6)));
  TEST_ASSERT_FALSE(ok(cfg(SensorType::HX711, 17, 17)));                 // doppelt
  TEST_ASSERT_FALSE(ok(cfg(SensorType::HX711, PIN_NFC_SCK, 18)));        // NFC
  TEST_ASSERT_FALSE(ok(cfg(SensorType::HX711, PIN_LCD_DC, 18)));         // Display
  TEST_ASSERT_FALSE(ok(cfg(SensorType::HX711, PIN_I2C_SCL, 18)));        // Touch-Bus nur für NAU7802
  TEST_ASSERT_FALSE(ok(cfg(SensorType::HX711, 19, 18)));                 // USB
  TEST_ASSERT_FALSE(ok(cfg(SensorType::HX711, 17, -1)));                 // Pin fehlt
  TEST_ASSERT_FALSE(ok(cfg(SensorType::HX711, 17, 18, 4)));              // dritter Pin nicht vorgesehen
  TEST_ASSERT_FALSE(ok(cfg(SensorType::HX711, 17, 99)));                 // gibt es nicht
}

void test_ads1232_pdwn() {
  TEST_ASSERT_TRUE(ok(cfg(SensorType::ADS1232, 17, 18)));
  TEST_ASSERT_TRUE(ok(cfg(SensorType::ADS1232, 17, 18, 16)));
  TEST_ASSERT_FALSE(ok(cfg(SensorType::ADS1232, 17, 18, 18)));  // doppelt
  TEST_ASSERT_FALSE(ok(cfg(SensorType::ADS1232, 17, 18, PIN_NFC_SS)));
}

void test_nau7802_bus() {
  TEST_ASSERT_TRUE(ok(cfg(SensorType::NAU7802, PIN_I2C_SCL, PIN_I2C_SDA)));
  TEST_ASSERT_FALSE(ok(cfg(SensorType::NAU7802, PIN_I2C_SDA, PIN_I2C_SCL)));  // vertauscht
  TEST_ASSERT_FALSE(ok(cfg(SensorType::NAU7802, PIN_I2C_SCL, 18)));           // halb Touch-Bus
  TEST_ASSERT_TRUE(ok(cfg(SensorType::NAU7802, 8, 7)));                       // eigener Bus
  TEST_ASSERT_FALSE(sensorUsesTouchBus(cfg(SensorType::NAU7802, 8, 7)));
  TEST_ASSERT_FALSE(ok(cfg(SensorType::NAU7802, PIN_I2C_SCL, PIN_I2C_SDA, 4)));
}

void test_sign_extend() {
  TEST_ASSERT_EQUAL(0, sensorSignExtend24(0));
  TEST_ASSERT_EQUAL(1, sensorSignExtend24(1));
  TEST_ASSERT_EQUAL(0x7FFFFF, sensorSignExtend24(0x7FFFFF));
  TEST_ASSERT_EQUAL(-1, sensorSignExtend24(0xFFFFFF));
  TEST_ASSERT_EQUAL(-0x800000, sensorSignExtend24(0x800000));
  TEST_ASSERT_EQUAL(-2, sensorSignExtend24(0xFFFFFFFE));  // obere Bits werden ignoriert
}

void test_raw_invalid() {
  for (SensorType t : {SensorType::HX711, SensorType::ADS1232}) {
    TEST_ASSERT_TRUE(sensorRawInvalid(t, 0));
    TEST_ASSERT_TRUE(sensorRawInvalid(t, -1));
    TEST_ASSERT_FALSE(sensorRawInvalid(t, 123456));
  }
  TEST_ASSERT_FALSE(sensorRawInvalid(SensorType::NAU7802, 0));
  TEST_ASSERT_FALSE(sensorRawInvalid(SensorType::NAU7802, -1));
  for (int t = 0; t < SENSOR_TYPE_COUNT; t++) {
    TEST_ASSERT_TRUE(sensorRawInvalid((SensorType)t, 0x7FFFFF));
    TEST_ASSERT_TRUE(sensorRawInvalid((SensorType)t, -0x800000));
    TEST_ASSERT_FALSE(sensorRawInvalid((SensorType)t, -500000));
  }
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_defaults_valid);
  RUN_TEST(test_type_ids);
  RUN_TEST(test_cal_suffix);
  RUN_TEST(test_free_pins_not_reserved);
  RUN_TEST(test_conflicts);
  RUN_TEST(test_ads1232_pdwn);
  RUN_TEST(test_nau7802_bus);
  RUN_TEST(test_sign_extend);
  RUN_TEST(test_raw_invalid);
  return UNITY_END();
}
