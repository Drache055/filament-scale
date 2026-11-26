#include <unity.h>

#include "core/boot_check_core.h"

void setUp() {}
void tearDown() {}

static BootCheckInput in(uint32_t ms, bool setup, bool configured, bool connected) {
  BootCheckInput i;
  i.uptimeMs = ms;
  i.setupDone = setup;
  i.wifiConfigured = configured;
  i.wifiConnected = connected;
  return i;
}

void test_waits_minimum_uptime() {
  TEST_ASSERT_EQUAL(BootVerdict::Wait, bootCheckDecide(in(0, true, true, true)));
  TEST_ASSERT_EQUAL(BootVerdict::Wait, bootCheckDecide(in(BOOT_CHECK_MIN_MS - 1, true, true, true)));
  TEST_ASSERT_EQUAL(BootVerdict::Valid, bootCheckDecide(in(BOOT_CHECK_MIN_MS, true, true, true)));
}

void test_wifi_required_when_configured() {
  TEST_ASSERT_EQUAL(BootVerdict::Wait, bootCheckDecide(in(BOOT_CHECK_MIN_MS, true, true, false)));
  TEST_ASSERT_EQUAL(BootVerdict::Wait, bootCheckDecide(in(BOOT_CHECK_WIFI_MS - 1, true, true, false)));
  TEST_ASSERT_EQUAL(BootVerdict::Invalid, bootCheckDecide(in(BOOT_CHECK_WIFI_MS, true, true, false)));
  // WLAN kommt spät, aber vor dem Limit
  TEST_ASSERT_EQUAL(BootVerdict::Valid, bootCheckDecide(in(BOOT_CHECK_WIFI_MS - 1, true, true, true)));
}

void test_hotspot_without_credentials() {
  TEST_ASSERT_EQUAL(BootVerdict::Wait, bootCheckDecide(in(BOOT_CHECK_MIN_MS - 1, true, false, false)));
  TEST_ASSERT_EQUAL(BootVerdict::Valid, bootCheckDecide(in(BOOT_CHECK_MIN_MS, true, false, false)));
}

void test_setup_hangs() {
  TEST_ASSERT_EQUAL(BootVerdict::Wait, bootCheckDecide(in(BOOT_CHECK_MIN_MS, false, true, true)));
  TEST_ASSERT_EQUAL(BootVerdict::Wait, bootCheckDecide(in(BOOT_CHECK_MIN_MS, false, false, false)));
  TEST_ASSERT_EQUAL(BootVerdict::Invalid, bootCheckDecide(in(BOOT_CHECK_WIFI_MS, false, true, true)));
  TEST_ASSERT_EQUAL(BootVerdict::Invalid, bootCheckDecide(in(BOOT_CHECK_WIFI_MS, false, false, false)));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_waits_minimum_uptime);
  RUN_TEST(test_wifi_required_when_configured);
  RUN_TEST(test_hotspot_without_credentials);
  RUN_TEST(test_setup_hangs);
  return UNITY_END();
}
