#include <unity.h>

#include <cstring>

#include "core/drying_core.h"

void setUp() {}
void tearDown() {}

void test_day_number_and_format() {
  TEST_ASSERT_EQUAL(0, dryingDayNumber(1970, 1, 1));
  TEST_ASSERT_EQUAL(19723, dryingDayNumber(2024, 1, 1));
  char buf[11];
  dryingFormatDate(dryingDayNumber(2026, 11, 22), buf);
  TEST_ASSERT_EQUAL_STRING("2026-11-22", buf);
  dryingFormatDate(dryingDayNumber(2024, 2, 29), buf);
  TEST_ASSERT_EQUAL_STRING("2024-02-29", buf);
}

void test_parse_date() {
  long d = 0;
  TEST_ASSERT_TRUE(dryingParseDate("2026-11-22", d));
  TEST_ASSERT_EQUAL(dryingDayNumber(2026, 11, 22), d);
  TEST_ASSERT_FALSE(dryingParseDate("2026-02-30", d));
  TEST_ASSERT_FALSE(dryingParseDate("2026-13-01", d));
  TEST_ASSERT_FALSE(dryingParseDate("22.11.2026", d));
  TEST_ASSERT_FALSE(dryingParseDate("", d));
  TEST_ASSERT_FALSE(dryingParseDate(nullptr, d));
}

void test_intervals() {
  DryingConfig c = dryingDefaults();
  TEST_ASSERT_EQUAL(30, dryingBaseDays(c, "petg"));
  TEST_ASSERT_EQUAL(90, dryingEffectiveDays(c, "PETG", true));
  TEST_ASSERT_EQUAL(30, dryingEffectiveDays(c, "PETG", false));
  TEST_ASSERT_EQUAL(0, dryingEffectiveDays(c, "Exotic", true));  // unbekannt, defaultDays 0
  c.defaultDays = 20;
  TEST_ASSERT_EQUAL(20, dryingEffectiveDays(c, "Exotic", false));
  c.boxFactor = 1;
  TEST_ASSERT_EQUAL(30, dryingEffectiveDays(c, "PETG", true));
  c.enabled = false;
  TEST_ASSERT_EQUAL(0, dryingEffectiveDays(c, "PETG", false));
}

void test_status() {
  DryingConfig c = dryingDefaults();
  long today = dryingDayNumber(2026, 11, 22);
  DryingStatus s = dryingStatus(c, "PETG", false, "2026-11-01", today);  // 30 Tage, 21 vergangen
  TEST_ASSERT_EQUAL(DryingState::Ok, s.state);
  TEST_ASSERT_EQUAL(9, s.daysLeft);
  s = dryingStatus(c, "PETG", false, "2026-10-23", today);  // genau 30 Tage
  TEST_ASSERT_EQUAL(DryingState::Due, s.state);
  TEST_ASSERT_EQUAL(0, s.daysLeft);
  s = dryingStatus(c, "PETG", false, "2026-09-01", today);
  TEST_ASSERT_EQUAL(DryingState::Due, s.state);
  TEST_ASSERT_TRUE(s.daysLeft < 0);
  s = dryingStatus(c, "PETG", true, "2026-09-01", today);  // Box: 90 Tage
  TEST_ASSERT_EQUAL(DryingState::Ok, s.state);
  s = dryingStatus(c, "PETG", false, "", today);
  TEST_ASSERT_EQUAL(DryingState::NoRecord, s.state);
  s = dryingStatus(c, "PETG", false, "kaputt", today);
  TEST_ASSERT_EQUAL(DryingState::NoRecord, s.state);
  s = dryingStatus(c, "PETG", false, "2026-09-01", -1);  // Uhr nicht gestellt
  TEST_ASSERT_EQUAL(DryingState::Off, s.state);
  s = dryingStatus(c, "Exotic", false, "2026-09-01", today);
  TEST_ASSERT_EQUAL(DryingState::Off, s.state);
}

void test_json_roundtrip_and_validation() {
  DryingConfig c = dryingDefaults();
  c.boxFactor = 4;
  c.defaultDays = 45;
  DryingConfig back;
  back.materials.clear();
  TEST_ASSERT_TRUE(dryingConfigFromJson(dryingConfigToJson(c), back));
  TEST_ASSERT_EQUAL(4, back.boxFactor);
  TEST_ASSERT_EQUAL(45, back.defaultDays);
  TEST_ASSERT_EQUAL(30, dryingBaseDays(back, "PETG"));

  DryingConfig keep = c;
  std::string err;
  TEST_ASSERT_FALSE(dryingConfigFromJson("{\"box_factor\":0}", keep, &err));
  TEST_ASSERT_FALSE(dryingConfigFromJson("{\"box_factor\":11}", keep, &err));
  TEST_ASSERT_FALSE(dryingConfigFromJson("{\"default_days\":-1}", keep, &err));
  TEST_ASSERT_FALSE(dryingConfigFromJson("{\"materials\":{\"PETG\":99999}}", keep, &err));
  TEST_ASSERT_FALSE(dryingConfigFromJson("{\"materials\":{\"EinSehrLangerMaterialName\":5}}", keep, &err));
  TEST_ASSERT_FALSE(dryingConfigFromJson("nonsense", keep, &err));
  TEST_ASSERT_EQUAL(4, keep.boxFactor);  // unverändert
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_day_number_and_format);
  RUN_TEST(test_parse_date);
  RUN_TEST(test_intervals);
  RUN_TEST(test_status);
  RUN_TEST(test_json_roundtrip_and_validation);
  return UNITY_END();
}
