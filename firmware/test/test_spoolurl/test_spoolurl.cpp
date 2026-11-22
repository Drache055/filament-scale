#include <string.h>
#include <unity.h>

#include "core/spool_url.h"

void setUp() {}
void tearDown() {}

void test_basic() {
  char b[96];
  TEST_ASSERT_EQUAL(38, spoolShowUrl("http://192.168.1.50:7912", 42, b, sizeof(b)));
  TEST_ASSERT_EQUAL_STRING("http://192.168.1.50:7912/spool/show/42", b);
}

void test_trailing_slash() {
  char b[96];
  spoolShowUrl("http://spoolman.local:7912//", 7, b, sizeof(b));
  TEST_ASSERT_EQUAL_STRING("http://spoolman.local:7912/spool/show/7", b);
}

void test_invalid() {
  char b[96];
  TEST_ASSERT_EQUAL(0, spoolShowUrl("", 1, b, sizeof(b)));
  TEST_ASSERT_EQUAL(0, spoolShowUrl("///", 1, b, sizeof(b)));
  TEST_ASSERT_EQUAL(0, spoolShowUrl(nullptr, 1, b, sizeof(b)));
  TEST_ASSERT_EQUAL(0, spoolShowUrl("http://x", -1, b, sizeof(b)));
  TEST_ASSERT_EQUAL_STRING("", b);
}

void test_too_small() {
  char b[10];
  TEST_ASSERT_EQUAL(0, spoolShowUrl("http://192.168.1.50:7912", 42, b, sizeof(b)));
  TEST_ASSERT_EQUAL_STRING("", b);
  TEST_ASSERT_EQUAL(0, spoolShowUrl("http://x", 1, nullptr, 0));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_basic);
  RUN_TEST(test_trailing_slash);
  RUN_TEST(test_invalid);
  RUN_TEST(test_too_small);
  return UNITY_END();
}
