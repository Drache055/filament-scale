#include <unity.h>

#include <cstring>

#include "core/queue_core.h"

void setUp() {}
void tearDown() {}

static QueueEntry entry(int id, float remaining, const char *loc = nullptr, const char *owner = "", uint32_t ts = 100) {
  QueueEntry e;
  e.spoolId = id;
  e.remaining = remaining;
  if (loc) {
    e.hasLocation = true;
    strcpy(e.location, loc);
  }
  strcpy(e.owner, owner);
  e.timestamp = ts;
  return e;
}

void test_line_roundtrip() {
  QueueEntry in = entry(3, 612.5f, "Box 03", "Jörg", 1760000000u);
  QueueEntry out;
  TEST_ASSERT_TRUE(queueEntryFromLine(queueEntryToLine(in), out));
  TEST_ASSERT_EQUAL(3, out.spoolId);
  TEST_ASSERT_EQUAL_FLOAT(612.5f, out.remaining);
  TEST_ASSERT_TRUE(out.hasLocation);
  TEST_ASSERT_EQUAL_STRING("Box 03", out.location);
  TEST_ASSERT_EQUAL_STRING("Jörg", out.owner);
  TEST_ASSERT_EQUAL_UINT32(1760000000u, out.timestamp);
}

void test_archive_roundtrip() {
  QueueEntry in = entry(5, 0, "Box 01");
  in.archive = true;
  strcpy(in.tagUid, "04A1B2C3D4E5F6");
  QueueEntry out;
  TEST_ASSERT_TRUE(queueEntryFromLine(queueEntryToLine(in), out));
  TEST_ASSERT_TRUE(out.archive);
  TEST_ASSERT_EQUAL_STRING("04A1B2C3D4E5F6", out.tagUid);
  // Ohne Archivieren bleibt die Zeile unverändert
  TEST_ASSERT_NULL(strstr(queueEntryToLine(entry(5, 0)).c_str(), "archive"));
  TEST_ASSERT_FALSE(queueEntryFromLine("{\"spool_id\":5}", out) ? out.archive : true);
}

void test_optional_fields_stay_optional() {
  QueueEntry in = entry(7, -1);  // nur Owner/Ort/Gewicht weglassen
  std::string line = queueEntryToLine(in);
  TEST_ASSERT_NULL(strstr(line.c_str(), "remaining_weight"));
  TEST_ASSERT_NULL(strstr(line.c_str(), "location"));
  TEST_ASSERT_NULL(strstr(line.c_str(), "owner"));
  QueueEntry out;
  TEST_ASSERT_TRUE(queueEntryFromLine(line, out));
  TEST_ASSERT_TRUE(out.remaining < 0);
  TEST_ASSERT_FALSE(out.hasLocation);
}

void test_newest_entry_per_spool_wins() {
  std::vector<QueueEntry> list;
  queueAddEntry(list, entry(1, 800), 10);
  queueAddEntry(list, entry(2, 500), 10);
  queueAddEntry(list, entry(1, 750), 10);  // ersetzt den ersten Eintrag zu Spule 1
  TEST_ASSERT_EQUAL(2, list.size());
  TEST_ASSERT_EQUAL(2, list[0].spoolId);
  TEST_ASSERT_EQUAL(1, list[1].spoolId);
  TEST_ASSERT_EQUAL_FLOAT(750, list[1].remaining);
}

void test_merge_keeps_older_archive_owner_and_tag() {
  std::vector<QueueEntry> list;
  QueueEntry a = entry(1, 5, "Box 01", "Anna", 100);
  a.archive = true;
  strcpy(a.tagUid, "04A1B2C3D4E5F6");
  queueAddEntry(list, a, 10);
  QueueEntry b = entry(1, 3, nullptr, "", 200);  // nur neues Gewicht
  queueAddEntry(list, b, 10);
  TEST_ASSERT_EQUAL(1, list.size());
  TEST_ASSERT_EQUAL_FLOAT(3, list[0].remaining);
  TEST_ASSERT_TRUE(list[0].archive);
  TEST_ASSERT_EQUAL_STRING("04A1B2C3D4E5F6", list[0].tagUid);
  TEST_ASSERT_EQUAL_STRING("Anna", list[0].owner);
  TEST_ASSERT_TRUE(list[0].hasLocation);
  TEST_ASSERT_EQUAL_STRING("Box 01", list[0].location);
  TEST_ASSERT_EQUAL_UINT32(200u, list[0].timestamp);
}

void test_merge_newer_values_override() {
  std::vector<QueueEntry> list;
  queueAddEntry(list, entry(1, 500, "Box 01", "Anna", 100), 10);
  queueAddEntry(list, entry(1, 400, "Box 02", "Ben", 200), 10);
  TEST_ASSERT_EQUAL(1, list.size());
  TEST_ASSERT_EQUAL_FLOAT(400, list[0].remaining);
  TEST_ASSERT_EQUAL_STRING("Box 02", list[0].location);
  TEST_ASSERT_EQUAL_STRING("Ben", list[0].owner);
  TEST_ASSERT_FALSE(list[0].archive);
}

void test_oldest_entries_drop_when_full() {
  std::vector<QueueEntry> list;
  for (int i = 1; i <= 5; i++) {
    queueAddEntry(list, entry(i, 100.0f * i), 3);
  }
  TEST_ASSERT_EQUAL(3, list.size());
  TEST_ASSERT_EQUAL(3, list[0].spoolId);
  TEST_ASSERT_EQUAL(5, list[2].spoolId);
}

void test_parse_skips_broken_lines() {
  std::string text = queueEntryToLine(entry(1, 10)) + "\nkaputt\n{\"foo\":1}\n{\"spool_id\":0}\n" + queueEntryToLine(entry(2, 20)) + "\n";
  std::vector<QueueEntry> list = queueParse(text);
  TEST_ASSERT_EQUAL(2, list.size());
  TEST_ASSERT_EQUAL(1, list[0].spoolId);
  TEST_ASSERT_EQUAL(2, list[1].spoolId);
}

void test_serialize_parse_roundtrip() {
  std::vector<QueueEntry> list;
  queueAddEntry(list, entry(4, 300, "A"), 10);
  queueAddEntry(list, entry(5, 200, nullptr, "Anna"), 10);
  std::vector<QueueEntry> again = queueParse(queueSerialize(list));
  TEST_ASSERT_EQUAL(2, again.size());
  TEST_ASSERT_EQUAL_STRING("A", again[0].location);
  TEST_ASSERT_EQUAL_STRING("Anna", again[1].owner);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_line_roundtrip);
  RUN_TEST(test_archive_roundtrip);
  RUN_TEST(test_optional_fields_stay_optional);
  RUN_TEST(test_newest_entry_per_spool_wins);
  RUN_TEST(test_merge_keeps_older_archive_owner_and_tag);
  RUN_TEST(test_merge_newer_values_override);
  RUN_TEST(test_oldest_entries_drop_when_full);
  RUN_TEST(test_parse_skips_broken_lines);
  RUN_TEST(test_serialize_parse_roundtrip);
  return UNITY_END();
}
