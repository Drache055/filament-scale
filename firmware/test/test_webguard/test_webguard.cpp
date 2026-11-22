#include <unity.h>

#include "core/web_guard.h"

void setUp() {}
void tearDown() {}

static const char *NAME = "filamentwaage";

void test_ip_literals() {
  TEST_ASSERT_TRUE(webHostAllowed("192.168.4.1", NAME));
  TEST_ASSERT_TRUE(webHostAllowed("192.168.178.40:80", NAME));
  TEST_ASSERT_TRUE(webHostAllowed("10.0.0.255", NAME));
  TEST_ASSERT_TRUE(webHostAllowed("[fe80::1]", NAME));
  TEST_ASSERT_TRUE(webHostAllowed("[fe80::1%25en0]:80", NAME));
}

void test_device_names() {
  TEST_ASSERT_TRUE(webHostAllowed("filamentwaage", NAME));
  TEST_ASSERT_TRUE(webHostAllowed("filamentwaage.local", NAME));
  TEST_ASSERT_TRUE(webHostAllowed("FilamentWaage.LOCAL", NAME));
  TEST_ASSERT_TRUE(webHostAllowed("filamentwaage.local.", NAME));
  TEST_ASSERT_TRUE(webHostAllowed("filamentwaage.local:80", NAME));
}

void test_empty_host() {
  TEST_ASSERT_TRUE(webHostAllowed("", NAME));
  TEST_ASSERT_TRUE(webHostAllowed(nullptr, NAME));
}

void test_foreign_hosts() {
  TEST_ASSERT_FALSE(webHostAllowed("evil.example", NAME));
  TEST_ASSERT_FALSE(webHostAllowed("filamentwaage.evil.example", NAME));
  TEST_ASSERT_FALSE(webHostAllowed("filamentwaage.fritz.box", NAME));
  TEST_ASSERT_FALSE(webHostAllowed("xfilamentwaage.local", NAME));
  TEST_ASSERT_FALSE(webHostAllowed("filamentwaage.localhost", NAME));
  TEST_ASSERT_FALSE(webHostAllowed("192.168.4.1.evil.example", NAME));
  TEST_ASSERT_FALSE(webHostAllowed("192.168.4", NAME));
  TEST_ASSERT_FALSE(webHostAllowed("256.1.1.1", NAME));
  TEST_ASSERT_FALSE(webHostAllowed("010.1.1.1", NAME));
  TEST_ASSERT_FALSE(webHostAllowed(":80", NAME));
  TEST_ASSERT_FALSE(webHostAllowed(".", NAME));
  TEST_ASSERT_FALSE(webHostAllowed("[]", NAME));
  TEST_ASSERT_FALSE(webHostAllowed("[fe80::1]evil", NAME));
  TEST_ASSERT_FALSE(webHostAllowed("[::1", NAME));
  TEST_ASSERT_FALSE(webHostAllowed("filamentwaage", ""));
  TEST_ASSERT_FALSE(webHostAllowed("local", nullptr));
}

void test_guard_header() {
  TEST_ASSERT_TRUE(webGuardHeaderOk("fetch"));
  TEST_ASSERT_FALSE(webGuardHeaderOk(""));
  TEST_ASSERT_FALSE(webGuardHeaderOk(nullptr));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_ip_literals);
  RUN_TEST(test_device_names);
  RUN_TEST(test_empty_host);
  RUN_TEST(test_foreign_hosts);
  RUN_TEST(test_guard_header);
  return UNITY_END();
}
