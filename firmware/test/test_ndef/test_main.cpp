#include <unity.h>

#include <cstring>
#include <string>

#include "nfc/ndef.h"

static const char *SPOOL_JSON =
    "{\"protocol\":\"openspool\",\"version\":\"1.0\",\"type\":\"PLA\",\"color_hex\":\"FF5733\","
    "\"brand\":\"Generic\",\"min_temp\":\"190\",\"max_temp\":\"220\",\"spoolman_id\":42}";

void setUp() {}
void tearDown() {}

void test_build_locate_parse_roundtrip() {
  uint8_t buf[1024];
  size_t n = ndefBuildMimeTlv("application/json", (const uint8_t *)SPOOL_JSON, strlen(SPOOL_JSON), 872, buf, sizeof buf);
  TEST_ASSERT_GREATER_THAN(0, n);
  TEST_ASSERT_EQUAL(0, n % 4);  // ganze Pages
  TEST_ASSERT_EQUAL_HEX8(0x03, buf[0]);
  TEST_ASSERT_EQUAL_HEX8(0xD2, buf[2]);  // MB ME SR TNF=MIME

  NdefLocation loc;
  // Mit zu kurzem Puffer muss die benötigte Länge gemeldet werden
  TEST_ASSERT_EQUAL((int)NdefLocate::NeedMore, (int)ndefLocate(buf, 32, loc));
  TEST_ASSERT_GREATER_THAN(32, loc.neededBytes);
  TEST_ASSERT_EQUAL((int)NdefLocate::Found, (int)ndefLocate(buf, loc.neededBytes, loc));

  NdefMime m;
  TEST_ASSERT_TRUE(ndefParseMimeRecord(buf + loc.recordOffset, loc.recordLength, m));
  TEST_ASSERT_EQUAL_STRING("application/json", m.type);
  TEST_ASSERT_EQUAL(strlen(SPOOL_JSON), m.payloadLength);
  TEST_ASSERT_EQUAL_MEMORY(SPOOL_JSON, m.payload, m.payloadLength);
}

void test_lock_control_tlv_is_skipped() {
  uint8_t rec[] = {0xD2, 0x01, 0x01, 'x', 'Y'};
  uint8_t data[32] = {0x01, 0x03, 0xA0, 0x10, 0x44, 0x03, sizeof rec};
  memcpy(data + 7, rec, sizeof rec);
  data[7 + sizeof rec] = 0xFE;
  NdefLocation loc;
  TEST_ASSERT_EQUAL((int)NdefLocate::Found, (int)ndefLocate(data, sizeof data, loc));
  TEST_ASSERT_EQUAL(7, loc.recordOffset);
  NdefMime m;
  TEST_ASSERT_TRUE(ndefParseMimeRecord(data + loc.recordOffset, loc.recordLength, m));
  TEST_ASSERT_EQUAL(1, m.payloadLength);
  TEST_ASSERT_EQUAL('Y', m.payload[0]);
}

void test_empty_and_terminated_tags() {
  NdefLocation loc;
  uint8_t blank[16] = {0};
  TEST_ASSERT_EQUAL((int)NdefLocate::NeedMore, (int)ndefLocate(blank, 16, loc));  // nur Nullbytes
  uint8_t term[16] = {0xFE};
  TEST_ASSERT_EQUAL((int)NdefLocate::None, (int)ndefLocate(term, 16, loc));
  uint8_t empty[16] = {0x03, 0x00, 0xFE};
  TEST_ASSERT_EQUAL((int)NdefLocate::None, (int)ndefLocate(empty, 16, loc));
}

void test_long_record_uses_three_byte_length_and_long_form() {
  std::string big(400, 'a');
  uint8_t buf[1024];
  size_t n = ndefBuildMimeTlv("application/json", (const uint8_t *)big.data(), big.size(), 872, buf, sizeof buf);
  TEST_ASSERT_GREATER_THAN(0, n);
  TEST_ASSERT_EQUAL_HEX8(0xFF, buf[1]);  // 3-Byte-Länge im TLV
  TEST_ASSERT_EQUAL_HEX8(0xC2, buf[4]);  // kein Short Record
  NdefLocation loc;
  TEST_ASSERT_EQUAL((int)NdefLocate::Found, (int)ndefLocate(buf, n, loc));
  NdefMime m;
  TEST_ASSERT_TRUE(ndefParseMimeRecord(buf + loc.recordOffset, loc.recordLength, m));
  TEST_ASSERT_EQUAL(400, m.payloadLength);
}

void test_capacity_is_enforced() {
  uint8_t buf[1024];
  TEST_ASSERT_EQUAL(0, ndefBuildMimeTlv("application/json", (const uint8_t *)SPOOL_JSON, strlen(SPOOL_JSON), 64, buf, sizeof buf));
  TEST_ASSERT_EQUAL(0, ndefBuildMimeTlv("application/json", (const uint8_t *)SPOOL_JSON, strlen(SPOOL_JSON), 872, buf, 16));
}

void test_capability_container() {
  NdefCc c;
  const uint8_t ntag216[4] = {0xE1, 0x10, 0x6D, 0x00};
  const uint8_t ntag215[4] = {0xE1, 0x10, 0x3E, 0x00};
  const uint8_t readOnly[4] = {0xE1, 0x10, 0x6D, 0x0F};
  const uint8_t invalid[4] = {0, 0, 0, 0};
  TEST_ASSERT_TRUE(ndefParseCc(ntag216, c));
  TEST_ASSERT_EQUAL(872, c.capacityBytes);
  TEST_ASSERT_FALSE(c.readOnly);
  TEST_ASSERT_TRUE(ndefParseCc(ntag215, c));
  TEST_ASSERT_EQUAL(496, c.capacityBytes);
  TEST_ASSERT_TRUE(ndefParseCc(readOnly, c));
  TEST_ASSERT_TRUE(c.readOnly);  // unteres Nibble = Schreibzugriff
  TEST_ASSERT_FALSE(ndefParseCc(invalid, c));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_build_locate_parse_roundtrip);
  RUN_TEST(test_lock_control_tlv_is_skipped);
  RUN_TEST(test_empty_and_terminated_tags);
  RUN_TEST(test_long_record_uses_three_byte_length_and_long_form);
  RUN_TEST(test_capacity_is_enforced);
  RUN_TEST(test_capability_container);
  return UNITY_END();
}
