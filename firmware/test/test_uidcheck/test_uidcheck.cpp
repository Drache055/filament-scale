#include <unity.h>

#include "core/uid_check.h"

void setUp() {}
void tearDown() {}

void test_valid() {
  TEST_ASSERT_TRUE(isValidTagUid("04A1B2C3"));
  TEST_ASSERT_TRUE(isValidTagUid("04a1b2c3d4e5f6"));
  TEST_ASSERT_TRUE(isValidTagUid("04A1B2C3D4E5F6A7B8C9"));
}

void test_invalid_chars() {
  TEST_ASSERT_FALSE(isValidTagUid("04A1B2\"}"));
  TEST_ASSERT_FALSE(isValidTagUid("04A1&tag"));
  TEST_ASSERT_FALSE(isValidTagUid("04A1 B2C3"));
  TEST_ASSERT_FALSE(isValidTagUid("04A1B2G3"));
  TEST_ASSERT_FALSE(isValidTagUid("04A1%32C3"));
}

void test_invalid_length() {
  TEST_ASSERT_FALSE(isValidTagUid(nullptr));
  TEST_ASSERT_FALSE(isValidTagUid(""));
  TEST_ASSERT_FALSE(isValidTagUid("04A1B2"));
  TEST_ASSERT_FALSE(isValidTagUid("04A1B2C3D"));
  TEST_ASSERT_FALSE(isValidTagUid("04A1B2C3D4E5F6A7B8C9D0"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_valid);
  RUN_TEST(test_invalid_chars);
  RUN_TEST(test_invalid_length);
  return UNITY_END();
}
