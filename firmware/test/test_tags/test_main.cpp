#include <unity.h>

#include <cstring>

#include "nfc/tags.h"

void setUp() {}
void tearDown() {}

static TagContent parse(const char *json) { return tagParse(json, strlen(json)); }

void test_openspool_tag() {
  TagContent t = parse(
      "{\"protocol\":\"openspool\",\"version\":\"1.0\",\"type\":\"PLA\",\"color_hex\":\"FF5733\","
      "\"brand\":\"Generic\",\"min_temp\":\"190\",\"max_temp\":\"220\",\"spoolman_id\":42}");
  TEST_ASSERT_EQUAL((int)TagKind::Spool, (int)t.kind);
  TEST_ASSERT_EQUAL_STRING("PLA", t.spool.type);
  TEST_ASSERT_EQUAL_STRING("FF5733", t.spool.colorHex);
  TEST_ASSERT_EQUAL_STRING("Generic", t.spool.brand);
  TEST_ASSERT_EQUAL(190, t.spool.minTemp);  // Strings werden in Zahlen gewandelt
  TEST_ASSERT_EQUAL(220, t.spool.maxTemp);
  TEST_ASSERT_EQUAL(42, t.spool.spoolmanId);
}

void test_spool_without_spoolman_id_and_numeric_temps() {
  TagContent t = parse("{\"protocol\":\"openspool\",\"type\":\"ABS\",\"brand\":\"X\",\"min_temp\":230,\"max_temp\":260}");
  TEST_ASSERT_EQUAL((int)TagKind::Spool, (int)t.kind);
  TEST_ASSERT_EQUAL(-1, t.spool.spoolmanId);
  TEST_ASSERT_EQUAL(230, t.spool.minTemp);
}

void test_box_tag() {
  TagContent t = parse("{\"type\":\"box\",\"id\":\"BOX-03\",\"name\":\"Box 03\"}");
  TEST_ASSERT_EQUAL((int)TagKind::Box, (int)t.kind);
  TEST_ASSERT_EQUAL_STRING("BOX-03", t.box.id);
  TEST_ASSERT_EQUAL_STRING("Box 03", t.box.name);
}

void test_user_tag_roundtrip() {
  char buf[128];
  size_t n = tagBuildUser(3, "Fabian", buf, sizeof buf);
  TEST_ASSERT_GREATER_THAN(0, n);
  TagContent t = tagParse(buf, n);
  TEST_ASSERT_EQUAL((int)TagKind::User, (int)t.kind);
  TEST_ASSERT_EQUAL(3, t.user.id);
  TEST_ASSERT_EQUAL_STRING("Fabian", t.user.name);
  TEST_ASSERT_EQUAL(0, tagBuildUser(3, "Fabian", buf, 10));  // Puffer zu klein
}

void test_admin_tag_roundtrip() {
  char buf[128];
  size_t n = tagBuildAdmin("0123456789ABCDEF0123456789ABCDEF", buf, sizeof buf);
  TEST_ASSERT_GREATER_THAN(0, n);
  TagContent t = tagParse(buf, n);
  TEST_ASSERT_EQUAL((int)TagKind::Admin, (int)t.kind);
  TEST_ASSERT_EQUAL_STRING("0123456789ABCDEF0123456789ABCDEF", t.admin.token);
  TEST_ASSERT_EQUAL(0, tagBuildAdmin("x", buf, 5));  // Puffer zu klein
  // Ein Admin-Tag ohne Token ist trotzdem als Admin erkannt, das Token ist dann leer (wird nie akzeptiert)
  TagContent e = parse("{\"type\":\"admin\"}");
  TEST_ASSERT_EQUAL((int)TagKind::Admin, (int)e.kind);
  TEST_ASSERT_EQUAL_STRING("", e.admin.token);
}

void test_unknown_content() {
  TEST_ASSERT_EQUAL((int)TagKind::Unknown, (int)parse("kein json").kind);
  TEST_ASSERT_EQUAL((int)TagKind::Unknown, (int)parse("{\"foo\":1}").kind);
  // type "PLA" ohne protocol ist keine Spule
  TEST_ASSERT_EQUAL((int)TagKind::Unknown, (int)parse("{\"type\":\"PLA\"}").kind);
}

void test_spool_build_roundtrip() {
  SpoolTag sp;
  strcpy(sp.type, "PETG");
  strcpy(sp.colorHex, "FFFFFF");
  strcpy(sp.brand, "Elegoo");
  sp.minTemp = 220;
  sp.maxTemp = 250;
  char buf[256];
  size_t n = tagBuildSpool(sp, buf, sizeof buf);
  TEST_ASSERT_GREATER_THAN(0, n);
  TEST_ASSERT_NULL(strstr(buf, "spoolman_id"));  // nur mit gültiger ID
  TagContent t = tagParse(buf, n);
  TEST_ASSERT_EQUAL((int)TagKind::Spool, (int)t.kind);
  TEST_ASSERT_EQUAL(220, t.spool.minTemp);
  TEST_ASSERT_EQUAL_STRING("Elegoo", t.spool.brand);

  sp.spoolmanId = 17;
  n = tagBuildSpool(sp, buf, sizeof buf);
  t = tagParse(buf, n);
  TEST_ASSERT_EQUAL(17, t.spool.spoolmanId);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_openspool_tag);
  RUN_TEST(test_spool_without_spoolman_id_and_numeric_temps);
  RUN_TEST(test_box_tag);
  RUN_TEST(test_user_tag_roundtrip);
  RUN_TEST(test_admin_tag_roundtrip);
  RUN_TEST(test_unknown_content);
  RUN_TEST(test_spool_build_roundtrip);
  return UNITY_END();
}
