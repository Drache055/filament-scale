#include <unity.h>

#include "core/auth_throttle.h"

void setUp() {}
void tearDown() {}

static const uint32_t IP_A = 0x0A00A8C0;  // 192.168.0.10
static const uint32_t IP_B = 0x0B00A8C0;  // 192.168.0.11

static uint32_t failN(AuthThrottle &t, uint32_t ip, uint32_t now, int n) {
  uint32_t ms = 0;
  for (int k = 0; k < n; k++) ms = authThrottleFail(t, ip, now);
  return ms;
}

void test_lock_after_five() {
  AuthThrottle t;
  TEST_ASSERT_EQUAL_UINT32(0, failN(t, IP_A, 1000, 4));
  TEST_ASSERT_EQUAL_UINT32(0, authThrottleBlockedMs(t, IP_A, 1000));
  TEST_ASSERT_EQUAL_UINT32(30000, authThrottleFail(t, IP_A, 1000));
  TEST_ASSERT_EQUAL_UINT32(30000, authThrottleBlockedMs(t, IP_A, 1000));
  TEST_ASSERT_EQUAL_UINT32(1, authThrottleBlockedMs(t, IP_A, 30999));
  TEST_ASSERT_EQUAL_UINT32(0, authThrottleBlockedMs(t, IP_A, 31000));
}

void test_other_ip_not_blocked() {
  AuthThrottle t;
  failN(t, IP_A, 1000, 5);
  TEST_ASSERT_TRUE(authThrottleBlockedMs(t, IP_A, 2000) > 0);
  TEST_ASSERT_EQUAL_UINT32(0, authThrottleBlockedMs(t, IP_B, 2000));
}

void test_lock_doubles_and_caps() {
  TEST_ASSERT_EQUAL_UINT32(0, authThrottleLockMs(0));
  TEST_ASSERT_EQUAL_UINT32(30000, authThrottleLockMs(1));
  TEST_ASSERT_EQUAL_UINT32(60000, authThrottleLockMs(2));
  TEST_ASSERT_EQUAL_UINT32(120000, authThrottleLockMs(3));
  TEST_ASSERT_EQUAL_UINT32(480000, authThrottleLockMs(5));
  TEST_ASSERT_EQUAL_UINT32(AUTH_LOCK_MAX_MS, authThrottleLockMs(6));
  TEST_ASSERT_EQUAL_UINT32(AUTH_LOCK_MAX_MS, authThrottleLockMs(255));

  AuthThrottle t;
  uint32_t now = 0;
  uint32_t expect[] = {30000, 60000, 120000, 240000, 480000, 900000, 900000};
  for (uint32_t e : expect) {
    TEST_ASSERT_EQUAL_UINT32(e, failN(t, IP_A, now, 5));
    now += e;  // Sperre abwarten, gleich weiterraten
  }
}

void test_success_clears() {
  AuthThrottle t;
  failN(t, IP_A, 0, 5);
  failN(t, IP_A, 30000, 4);
  authThrottleSuccess(t, IP_A);
  TEST_ASSERT_EQUAL_UINT32(0, authThrottleBlockedMs(t, IP_A, 30000));
  // Zähler und Sperrstufe von vorn
  TEST_ASSERT_EQUAL_UINT32(0, failN(t, IP_A, 30000, 4));
  TEST_ASSERT_EQUAL_UINT32(30000, authThrottleFail(t, IP_A, 30000));
}

void test_idle_forgets() {
  AuthThrottle t;
  failN(t, IP_A, 0, 5);   // Sperre 1
  failN(t, IP_A, 30000, 4);
  uint32_t later = 30000 + AUTH_IDLE_FORGET_MS;
  // nach einer Stunde Ruhe: wieder 5 Versuche und nur 30 s
  TEST_ASSERT_EQUAL_UINT32(0, failN(t, IP_A, later, 4));
  TEST_ASSERT_EQUAL_UINT32(30000, authThrottleFail(t, IP_A, later));
}

void test_millis_wrap() {
  AuthThrottle t;
  uint32_t now = 0xFFFFF000u;
  TEST_ASSERT_EQUAL_UINT32(30000, failN(t, IP_A, now, 5));
  TEST_ASSERT_TRUE(authThrottleBlockedMs(t, IP_A, now + 20000) > 0);  // über den Überlauf
  TEST_ASSERT_EQUAL_UINT32(0, authThrottleBlockedMs(t, IP_A, now + 30000));
}

void test_full_table_keeps_blocked() {
  AuthThrottle t;
  failN(t, IP_A, 0, 5);  // A gesperrt, ältester Eintrag
  for (uint32_t k = 1; k < AUTH_THROTTLE_SLOTS; k++) authThrottleFail(t, 0x100 + k, 100 + k);
  // Tabelle voll: ein neuer Client verdrängt einen ungesperrten Eintrag, nicht A
  authThrottleFail(t, 0x999, 1000);
  TEST_ASSERT_TRUE(authThrottleBlockedMs(t, IP_A, 1000) > 0);
  // der verdrängte war der älteste ungesperrte (0x101): er fängt wieder bei 0 an
  TEST_ASSERT_EQUAL_UINT32(0, failN(t, 0x101, 1001, 4));
}

void test_full_table_all_blocked() {
  AuthThrottle t;
  for (uint32_t k = 0; k < AUTH_THROTTLE_SLOTS; k++) failN(t, 0x100 + k, k, 5);
  // alle gesperrt: der älteste (0x100) wird ersetzt, der neue Client wird erfasst
  authThrottleFail(t, 0x999, 100);
  TEST_ASSERT_EQUAL_UINT32(0, authThrottleBlockedMs(t, 0x100, 100));
  TEST_ASSERT_TRUE(authThrottleBlockedMs(t, 0x101, 100) > 0);
  TEST_ASSERT_EQUAL_UINT32(30000, failN(t, 0x999, 100, 4));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_lock_after_five);
  RUN_TEST(test_other_ip_not_blocked);
  RUN_TEST(test_lock_doubles_and_caps);
  RUN_TEST(test_success_clears);
  RUN_TEST(test_idle_forgets);
  RUN_TEST(test_millis_wrap);
  RUN_TEST(test_full_table_keeps_blocked);
  RUN_TEST(test_full_table_all_blocked);
  return UNITY_END();
}
