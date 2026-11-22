#include <cstring>
#include <unity.h>

#include "core/devicepw_core.h"

using namespace devicepw;

void setUp() {}
void tearDown() {}

static uint32_t counter = 0;
static uint32_t seqRnd() { return counter++; }
static uint32_t maxRnd() { return 0xFFFFFFFFu; }

void test_compiled_usable() {
  TEST_ASSERT_FALSE(compiledUsable(nullptr));
  TEST_ASSERT_FALSE(compiledUsable(""));
  TEST_ASSERT_FALSE(compiledUsable("1234567"));        // zu kurz
  TEST_ASSERT_FALSE(compiledUsable("bitte-aendern"));  // Platzhalter der Vorlage
  TEST_ASSERT_TRUE(compiledUsable("12345678"));
  TEST_ASSERT_TRUE(compiledUsable("mein-eigenes-pw"));
}

void test_generate_length_and_alphabet() {
  counter = 0;
  char out[kLength + 1];
  memset(out, 'x', sizeof(out));
  generate(seqRnd, out, sizeof(out));
  TEST_ASSERT_EQUAL(kLength, strlen(out));
  TEST_ASSERT_TRUE(strlen(out) >= kMinLength);
  for (size_t i = 0; i < kLength; i++) {
    TEST_ASSERT_NOT_NULL(strchr(kAlphabet, out[i]));
  }
  TEST_ASSERT_EQUAL_STRING("ABCDEFGHJK", out);  // 0..9 der Reihe nach
  generate(maxRnd, out, sizeof(out));
  TEST_ASSERT_NOT_NULL(strchr(kAlphabet, out[0]));
}

void test_generate_no_confusable_chars() {
  for (const char *c = "0O1IL"; *c; c++) {
    TEST_ASSERT_NULL(strchr(kAlphabet, *c));
  }
}

void test_generate_small_buffer() {
  counter = 0;
  char out[4];
  generate(seqRnd, out, sizeof(out));
  TEST_ASSERT_EQUAL_STRING("ABC", out);
  generate(seqRnd, out, 0);  // darf nichts schreiben und nicht abstürzen
}

void test_show_on_home_only_without_credentials() {
  TEST_ASSERT_TRUE(showOnHome(false));   // Ersteinrichtung
  TEST_ASSERT_FALSE(showOnHome(true));   // Rückfall nach WLAN-Ausfall
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_compiled_usable);
  RUN_TEST(test_generate_length_and_alphabet);
  RUN_TEST(test_generate_no_confusable_chars);
  RUN_TEST(test_generate_small_buffer);
  RUN_TEST(test_show_on_home_only_without_credentials);
  return UNITY_END();
}
