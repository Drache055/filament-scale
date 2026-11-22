#include <unity.h>

#include "core/user_name.h"

void setUp() {}
void tearDown() {}

void test_reserved() {
  TEST_ASSERT_TRUE(userNameReserved("admin"));
  TEST_ASSERT_TRUE(userNameReserved("Admin"));
  TEST_ASSERT_TRUE(userNameReserved("GENERIC"));
  TEST_ASSERT_TRUE(userNameReserved("  generic "));
}

void test_allowed() {
  TEST_ASSERT_FALSE(userNameReserved("Fabian"));
  TEST_ASSERT_FALSE(userNameReserved("administrator"));
  TEST_ASSERT_FALSE(userNameReserved("adm"));
  TEST_ASSERT_FALSE(userNameReserved(""));
  TEST_ASSERT_FALSE(userNameReserved(nullptr));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_reserved);
  RUN_TEST(test_allowed);
  return UNITY_END();
}
