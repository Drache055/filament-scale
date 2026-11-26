#include <unity.h>

#include "core/scale_filter.h"

void setUp() {}
void tearDown() {}

void test_trimmed_mean_ignores_outlier() {
  long s[10] = {1000, 1002, 998, 1001, 999, 1000, 90000, 1000, 1001, 999};
  WindowStats w = scaleWindowStats(s, 10, 2);
  TEST_ASSERT_EQUAL(6, w.used);
  TEST_ASSERT_INT_WITHIN(1, 1000, w.mean);
  TEST_ASSERT_TRUE(w.hi - w.lo <= 2);  // Ausreißer blockiert die Stabilität nicht
}

void test_trimmed_two_sided_outliers() {
  long s[10] = {500, -80000, 501, 499, 500, 80000, 500, 70000, -70000, 500};
  WindowStats w = scaleWindowStats(s, 10, 2);
  TEST_ASSERT_EQUAL(500, w.mean);
  TEST_ASSERT_EQUAL(499, w.lo);
  TEST_ASSERT_EQUAL(501, w.hi);
}

void test_small_window_trims_less() {
  long one[1] = {42};
  WindowStats w = scaleWindowStats(one, 1, 2);
  TEST_ASSERT_EQUAL(1, w.used);
  TEST_ASSERT_EQUAL(42, w.mean);
  long three[3] = {10, 1000, 20};
  w = scaleWindowStats(three, 3, 2);
  TEST_ASSERT_EQUAL(1, w.used);  // Median
  TEST_ASSERT_EQUAL(20, w.mean);
  long four[4] = {10, 20, 30, 40};
  w = scaleWindowStats(four, 4, 2);
  TEST_ASSERT_EQUAL(2, w.used);
  TEST_ASSERT_EQUAL(25, w.mean);
  TEST_ASSERT_EQUAL(0, scaleWindowStats(four, 0, 2).used);
}

void test_negative_values() {
  long s[5] = {-1003, -1001, -1002, -5000, -1002};
  WindowStats w = scaleWindowStats(s, 5, 1);
  TEST_ASSERT_EQUAL(-1002, w.mean);
}

static ZeroInput zin(uint32_t now, float g, bool stable = true, bool idle = true) {
  ZeroInput i;
  i.now = now;
  i.grams = g;
  i.stable = stable;
  i.idle = idle;
  return i;
}

void test_boot_zero_waits_for_settle() {
  ZeroTracker z;
  TEST_ASSERT_EQUAL(ZeroAction::None, z.step(zin(1000, 3.0f)));
  TEST_ASSERT_EQUAL(ZeroAction::None, z.step(zin(3900, 3.0f)));
  TEST_ASSERT_EQUAL(ZeroAction::BootZero, z.step(zin(4000, 3.0f)));
  TEST_ASSERT_TRUE(z.bootDone());
  // kein zweites Mal, 3 g liegt über der Nachführgrenze
  TEST_ASSERT_EQUAL(ZeroAction::None, z.step(zin(4100, 3.0f)));
  TEST_ASSERT_EQUAL(ZeroAction::None, z.step(zin(9000, 3.0f)));
}

void test_boot_zero_retried_until_empty() {
  ZeroTracker z;
  z.step(zin(0, 800.0f));
  TEST_ASSERT_EQUAL(ZeroAction::None, z.step(zin(5000, 800.0f)));   // Spule liegt drauf
  TEST_ASSERT_EQUAL(ZeroAction::None, z.step(zin(6000, 4.0f, false)));  // unruhig
  TEST_ASSERT_EQUAL(ZeroAction::None, z.step(zin(7000, 4.0f, true, false)));  // nicht IDLE
  TEST_ASSERT_EQUAL(ZeroAction::BootZero, z.step(zin(8000, 4.0f)));
}

void test_boot_zero_gives_up() {
  ZeroConfig c;
  c.bootWindowMs = 10000;
  ZeroTracker z(c);
  z.step(zin(0, 800.0f));
  z.step(zin(5000, 800.0f));
  TEST_ASSERT_EQUAL(ZeroAction::None, z.step(zin(10001, 4.0f)));
  TEST_ASSERT_TRUE(z.bootDone());
}

void test_tracking_needs_hold_idle_and_band() {
  ZeroTracker z;
  z.step(zin(0, 0.0f));
  TEST_ASSERT_EQUAL(ZeroAction::BootZero, z.step(zin(3000, 0.0f)));
  TEST_ASSERT_EQUAL(ZeroAction::None, z.step(zin(3100, 1.5f)));   // Haltezeit beginnt
  TEST_ASSERT_EQUAL(ZeroAction::None, z.step(zin(5000, 1.5f)));
  TEST_ASSERT_EQUAL(ZeroAction::Track, z.step(zin(5100, 1.5f)));
  // nach dem Nachführen beginnt die Haltezeit neu
  TEST_ASSERT_EQUAL(ZeroAction::None, z.step(zin(6000, 0.2f)));
  TEST_ASSERT_EQUAL(ZeroAction::Track, z.step(zin(7100, 0.2f)));
  // 2 g und mehr: keine Nachführung
  TEST_ASSERT_EQUAL(ZeroAction::None, z.step(zin(8000, 2.0f)));
  TEST_ASSERT_EQUAL(ZeroAction::None, z.step(zin(20000, 2.5f)));
  // Unterbrechung (unruhig) setzt die Haltezeit zurück
  z.step(zin(21000, 0.5f));
  z.step(zin(22000, 0.5f, false));
  TEST_ASSERT_EQUAL(ZeroAction::None, z.step(zin(23100, 0.5f)));
  TEST_ASSERT_EQUAL(ZeroAction::Track, z.step(zin(25100, 0.5f)));
  // nicht IDLE (Wiegeablauf): nie
  TEST_ASSERT_EQUAL(ZeroAction::None, z.step(zin(26000, 0.5f, true, false)));
  TEST_ASSERT_EQUAL(ZeroAction::None, z.step(zin(40000, 0.5f, true, false)));
}

void test_data_lost_restarts_settle() {
  ZeroTracker z;
  z.step(zin(0, 0.0f));
  z.step(zin(3000, 0.0f));  // BootZero
  z.dataLost();
  TEST_ASSERT_EQUAL(ZeroAction::None, z.step(zin(10000, 0.5f)));
  TEST_ASSERT_EQUAL(ZeroAction::None, z.step(zin(12900, 0.5f)));
  TEST_ASSERT_EQUAL(ZeroAction::None, z.step(zin(13000, 0.5f)));  // Haltezeit beginnt
  TEST_ASSERT_EQUAL(ZeroAction::Track, z.step(zin(15000, 0.5f)));
}

void test_manual_zero_restarts_hold() {
  ZeroTracker z;
  z.step(zin(0, 0.0f));
  z.step(zin(3000, 0.0f));
  z.step(zin(3100, 0.5f));
  z.manualZero();
  TEST_ASSERT_EQUAL(ZeroAction::None, z.step(zin(5100, 0.0f)));
  TEST_ASSERT_EQUAL(ZeroAction::Track, z.step(zin(7100, 0.0f)));
}

void test_millis_wraparound() {
  ZeroTracker z;
  uint32_t t0 = 0xFFFFF000u;
  z.step(zin(t0, 0.0f));
  TEST_ASSERT_EQUAL(ZeroAction::None, z.step(zin(t0 + 2000, 0.0f)));
  TEST_ASSERT_EQUAL(ZeroAction::BootZero, z.step(zin(t0 + 3000, 0.0f)));  // über den Überlauf
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_trimmed_mean_ignores_outlier);
  RUN_TEST(test_trimmed_two_sided_outliers);
  RUN_TEST(test_small_window_trims_less);
  RUN_TEST(test_negative_values);
  RUN_TEST(test_boot_zero_waits_for_settle);
  RUN_TEST(test_boot_zero_retried_until_empty);
  RUN_TEST(test_boot_zero_gives_up);
  RUN_TEST(test_tracking_needs_hold_idle_and_band);
  RUN_TEST(test_data_lost_restarts_settle);
  RUN_TEST(test_manual_zero_restarts_hold);
  RUN_TEST(test_millis_wraparound);
  return UNITY_END();
}
