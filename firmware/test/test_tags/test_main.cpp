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
  char buf[192];
  size_t n = tagBuildUser(3, "Fabian", "0123456789ABCDEF0123456789ABCDEF", buf, sizeof buf);
  TEST_ASSERT_GREATER_THAN(0, n);
  TagContent t = tagParse(buf, n);
  TEST_ASSERT_EQUAL((int)TagKind::User, (int)t.kind);
  TEST_ASSERT_EQUAL(3, t.user.id);
  TEST_ASSERT_EQUAL_STRING("Fabian", t.user.name);
  TEST_ASSERT_EQUAL_STRING("0123456789ABCDEF0123456789ABCDEF", t.user.token);
  TEST_ASSERT_EQUAL(0, tagBuildUser(3, "Fabian", "0123456789ABCDEF0123456789ABCDEF", buf, 40));  // Puffer zu klein
  // Längster Name (23 Zeichen) mit Token passt in den Puffer der Webseite (192 Byte)
  TEST_ASSERT_GREATER_THAN(0, tagBuildUser(65535, "ABCDEFGHIJKLMNOPQRSTUVW", "0123456789ABCDEF0123456789ABCDEF", buf,
                                           sizeof buf));
}

void test_user_tag_old_format_has_no_token() {
  TagContent t = parse("{\"type\":\"user\",\"id\":1,\"name\":\"Fabian\"}");
  TEST_ASSERT_EQUAL((int)TagKind::User, (int)t.kind);
  TEST_ASSERT_EQUAL(1, t.user.id);
  TEST_ASSERT_EQUAL_STRING("", t.user.token);
}

void test_user_token_check() {
  const char *tok = "0123456789ABCDEF0123456789ABCDEF";
  TEST_ASSERT_EQUAL((int)UserTagCheck::Ok, (int)tagCheckUserToken(tok, "0123456789ABCDEF0123456789ABCDEF"));
  // Falsches Token, abweichende Länge
  TEST_ASSERT_EQUAL((int)UserTagCheck::Invalid, (int)tagCheckUserToken(tok, "0123456789ABCDEF0123456789ABCDEE"));
  TEST_ASSERT_EQUAL((int)UserTagCheck::Invalid, (int)tagCheckUserToken(tok, "0123"));
  // Tag mit Token, aber der Benutzer hat keins gespeichert (z. B. selbst geschriebener Tag)
  TEST_ASSERT_EQUAL((int)UserTagCheck::Invalid, (int)tagCheckUserToken("", tok));
  TEST_ASSERT_EQUAL((int)UserTagCheck::Invalid, (int)tagCheckUserToken(nullptr, tok));
  // Tag im alten Format (nur ID und Name)
  TEST_ASSERT_EQUAL((int)UserTagCheck::Outdated, (int)tagCheckUserToken(tok, ""));
  TEST_ASSERT_EQUAL((int)UserTagCheck::Outdated, (int)tagCheckUserToken("", ""));
  TEST_ASSERT_EQUAL((int)UserTagCheck::Outdated, (int)tagCheckUserToken(tok, nullptr));
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

void test_box_number_build() {
  char buf[128];
  size_t n = tagBuildBoxNumber(7, buf, sizeof(buf));
  TEST_ASSERT_TRUE(n > 0);
  TagContent t = tagParse(buf, n);
  TEST_ASSERT_EQUAL(TagKind::Box, t.kind);
  TEST_ASSERT_EQUAL_STRING("BOX-07", t.box.id);
  TEST_ASSERT_EQUAL_STRING("Box 07", t.box.name);
  TEST_ASSERT_EQUAL(0, tagBuildBoxNumber(0, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL(0, tagBuildBoxNumber(100, buf, sizeof(buf)));
  TEST_ASSERT_EQUAL(0, tagBuildBoxNumber(5, buf, 10));  // passt nicht
}

void test_blank_for_box() {
  TEST_ASSERT_TRUE(tagIsBlankForBox(TagContent()));  // kein NDEF
  const char *noName = "{\"type\":\"box\",\"id\":\"BOX-01\"}";
  TEST_ASSERT_TRUE(tagIsBlankForBox(tagParse(noName, strlen(noName))));
  const char *box = "{\"type\":\"box\",\"id\":\"BOX-01\",\"name\":\"Box 01\"}";
  TEST_ASSERT_FALSE(tagIsBlankForBox(tagParse(box, strlen(box))));
  const char *spool = "{\"protocol\":\"openspool\",\"type\":\"PLA\"}";
  TEST_ASSERT_FALSE(tagIsBlankForBox(tagParse(spool, strlen(spool))));
  const char *user = "{\"type\":\"user\",\"id\":1,\"name\":\"A\"}";
  TEST_ASSERT_FALSE(tagIsBlankForBox(tagParse(user, strlen(user))));
  const char *other = "{\"foo\":1}";
  TEST_ASSERT_TRUE(tagIsBlankForBox(tagParse(other, strlen(other))));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_openspool_tag);
  RUN_TEST(test_spool_without_spoolman_id_and_numeric_temps);
  RUN_TEST(test_box_tag);
  RUN_TEST(test_user_tag_roundtrip);
  RUN_TEST(test_user_tag_old_format_has_no_token);
  RUN_TEST(test_user_token_check);
  RUN_TEST(test_admin_tag_roundtrip);
  RUN_TEST(test_unknown_content);
  RUN_TEST(test_spool_build_roundtrip);
  RUN_TEST(test_box_number_build);
  RUN_TEST(test_blank_for_box);
  return UNITY_END();
}
