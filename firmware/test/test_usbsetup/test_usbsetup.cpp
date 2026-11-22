#include <string>
#include <unity.h>

#include "core/usb_setup_core.h"

using namespace usbsetup;

void setUp() {}
void tearDown() {}

static Parser::Result feedAll(Parser &p, const std::vector<uint8_t> &bytes, Packet &pkt, std::string &text) {
  Parser::Result last = Parser::None;
  for (uint8_t b : bytes) {
    Parser::Result r = p.feed(b, pkt, text);
    if (r != Parser::None) last = r;
  }
  return last;
}

void test_roundtrip_setup() {
  Parser p;
  Packet pkt;
  std::string text;
  auto bytes = buildRpc(CmdSetup, {"MeinWLAN", "geheim123|x", "Admin:pw"});
  TEST_ASSERT_EQUAL(Parser::Got, feedAll(p, bytes, pkt, text));
  TEST_ASSERT_EQUAL_STRING("\n", text.c_str());  // nur das abschließende Zeilenende
  TEST_ASSERT_EQUAL(TypeRpc, pkt.type);
  uint8_t cmd = 0;
  std::vector<std::string> s;
  TEST_ASSERT_TRUE(parseRpc(pkt.data, cmd, s));
  TEST_ASSERT_EQUAL(CmdSetup, cmd);
  TEST_ASSERT_EQUAL(3, s.size());
  TEST_ASSERT_EQUAL_STRING("MeinWLAN", s[0].c_str());
  TEST_ASSERT_EQUAL_STRING("geheim123|x", s[1].c_str());
  TEST_ASSERT_EQUAL_STRING("Admin:pw", s[2].c_str());
}

void test_text_passthrough_and_mixed() {
  Parser p;
  Packet pkt;
  std::string text;
  std::vector<uint8_t> bytes;
  std::string pre = "wifi status\nIMx IIMPRO";
  bytes.insert(bytes.end(), pre.begin(), pre.end());
  auto pk = buildRpc(CmdHello, {});
  bytes.insert(bytes.end(), pk.begin(), pk.end());
  TEST_ASSERT_EQUAL(Parser::Got, feedAll(p, bytes, pkt, text));
  TEST_ASSERT_EQUAL_STRING("wifi status\nIMx IIMPRO\n", text.c_str());
}

void test_bad_checksum_not_passed_as_text() {
  Parser p;
  Packet pkt;
  std::string text;
  auto bytes = buildRpc(CmdSetup, {"a", "b", "secret"});
  bytes[bytes.size() - 2] ^= 0x55;  // Prüfsumme
  bytes.pop_back();                 // ohne Zeilenende
  TEST_ASSERT_EQUAL(Parser::BadChecksum, feedAll(p, bytes, pkt, text));
  TEST_ASSERT_EQUAL(std::string::npos, text.find("secret"));
  // danach wieder normal
  auto ok = buildRpc(CmdScan, {});
  TEST_ASSERT_EQUAL(Parser::Got, feedAll(p, ok, pkt, text));
}

void test_wrong_version_is_text() {
  Parser p;
  Packet pkt;
  std::string text;
  std::string s = "IMPROV9";
  TEST_ASSERT_EQUAL(Parser::None, feedAll(p, std::vector<uint8_t>(s.begin(), s.end()), pkt, text));
  TEST_ASSERT_EQUAL_STRING("IMPROV9", text.c_str());
}

void test_parse_rpc_invalid() {
  uint8_t cmd;
  std::vector<std::string> s;
  TEST_ASSERT_FALSE(parseRpc({}, cmd, s));
  TEST_ASSERT_FALSE(parseRpc({0x81, 3, 5, 'a', 'b'}, cmd, s));  // Längenangabe falsch
  TEST_ASSERT_FALSE(parseRpc({0x81, 2, 5, 'a'}, cmd, s));       // String zu kurz
  TEST_ASSERT_TRUE(parseRpc({0x80, 0}, cmd, s));
  TEST_ASSERT_EQUAL(0, s.size());
}

void test_build_packet_format() {
  auto p = buildState(StateProvisioned);
  const uint8_t expect[] = {'I', 'M', 'P', 'R', 'O', 'V', 1, TypeState, 1, StateProvisioned};
  TEST_ASSERT_EQUAL(12, p.size());
  TEST_ASSERT_EQUAL_UINT8_ARRAY(expect, p.data(), 10);
  uint8_t sum = 0;
  for (int i = 0; i < 10; i++) sum += expect[i];
  TEST_ASSERT_EQUAL_UINT8(sum, p[10]);
  TEST_ASSERT_EQUAL_UINT8('\n', p[11]);
}

void test_validate() {
  const char *d = "Filament";
  TEST_ASSERT_EQUAL(ErrNone, validateSetup("Netz", "", "neuesPW", d, 4, 32));
  TEST_ASSERT_EQUAL(ErrNone, validateSetup("Netz", "12345678", "neuesPW", d, 4, 32));
  TEST_ASSERT_EQUAL(ErrBadSsid, validateSetup("  ", "12345678", "neuesPW", d, 4, 32));
  TEST_ASSERT_EQUAL(ErrBadSsid, validateSetup(std::string(33, 'a'), "", "neuesPW", d, 4, 32));
  TEST_ASSERT_EQUAL(ErrBadWifiPw, validateSetup("Netz", "1234567", "neuesPW", d, 4, 32));
  TEST_ASSERT_EQUAL(ErrBadWifiPw, validateSetup("Netz", std::string(64, 'a'), "neuesPW", d, 4, 32));
  TEST_ASSERT_EQUAL(ErrBadAdminPw, validateSetup("Netz", "", "Filament", d, 4, 32));
  TEST_ASSERT_EQUAL(ErrBadAdminPw, validateSetup("Netz", "", "abc", d, 4, 32));
  TEST_ASSERT_EQUAL(ErrBadAdminPw, validateSetup("Netz", "", "ab:cd", d, 4, 32));
  TEST_ASSERT_EQUAL(ErrBadAdminPw, validateSetup("Netz", "", std::string(33, 'a'), d, 4, 32));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_roundtrip_setup);
  RUN_TEST(test_text_passthrough_and_mixed);
  RUN_TEST(test_bad_checksum_not_passed_as_text);
  RUN_TEST(test_wrong_version_is_text);
  RUN_TEST(test_parse_rpc_invalid);
  RUN_TEST(test_build_packet_format);
  RUN_TEST(test_validate);
  return UNITY_END();
}
