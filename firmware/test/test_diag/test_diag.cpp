#include <cstring>
#include <unity.h>

#include "core/diag_core.h"

void setUp() {}
void tearDown() {}

static DiagInput healthy() {
  DiagInput i;
  i.uptimeS = 600;
  i.sensorValid = true;
  i.calibrated = true;
  i.nfcReady = true;
  i.wifiConnected = true;
  i.wifiHasCreds = true;
  i.wifiRssi = -55;
  i.smChecked = true;
  i.smReachable = true;
  i.smHttp = 200;
  return i;
}

static const DiagItem *find(const DiagItem *items, int n, DiagComp c) {
  for (int k = 0; k < n; k++) {
    if (items[k].comp == c) return &items[k];
  }
  return nullptr;
}

static void expect(const DiagInput &in, DiagComp c, DiagLevel l, const char *code) {
  DiagItem items[DIAG_MAX_ITEMS];
  int n = diagRun(in, items);
  const DiagItem *it = find(items, n, c);
  TEST_ASSERT_NOT_NULL(it);
  TEST_ASSERT_EQUAL_STRING(code, it->code);
  TEST_ASSERT_EQUAL((int)l, (int)it->level);
}

void test_all_ok() {
  DiagItem items[DIAG_MAX_ITEMS];
  int n = diagRun(healthy(), items);
  TEST_ASSERT_EQUAL(5, n);
  for (int k = 0; k < n; k++) {
    TEST_ASSERT_EQUAL((int)DiagLevel::Ok, (int)items[k].level);
  }
}

void test_sensor() {
  DiagInput i = healthy();
  i.sensorValid = false;
  expect(i, DiagComp::Sensor, DiagLevel::Error, "no_data");
  i.uptimeS = 5;
  expect(i, DiagComp::Sensor, DiagLevel::Info, "starting");
  i = healthy();
  i.calibrated = false;
  expect(i, DiagComp::Sensor, DiagLevel::Warn, "not_calibrated");
  i = healthy();
  i.overload = true;
  expect(i, DiagComp::Sensor, DiagLevel::Warn, "overload");
}

void test_nfc() {
  DiagInput i = healthy();
  i.nfcReady = false;
  expect(i, DiagComp::Nfc, DiagLevel::Error, "no_module");
  i.uptimeS = 10;
  expect(i, DiagComp::Nfc, DiagLevel::Info, "starting");
}

void test_wifi() {
  DiagInput i = healthy();
  i.wifiRssi = -85;
  expect(i, DiagComp::Wifi, DiagLevel::Warn, "weak");
  i = healthy();
  i.wifiConnected = false;
  expect(i, DiagComp::Wifi, DiagLevel::Error, "disconnected");
  i.wifiHasCreds = false;
  expect(i, DiagComp::Wifi, DiagLevel::Warn, "no_creds");
}

void test_spoolman() {
  DiagInput i = healthy();
  i.smReachable = false;
  i.smHttp = 0;
  expect(i, DiagComp::Spoolman, DiagLevel::Error, "unreachable");
  i.smHttp = 404;
  expect(i, DiagComp::Spoolman, DiagLevel::Error, "wrong_url");
  i.smHttp = 403;
  expect(i, DiagComp::Spoolman, DiagLevel::Error, "denied");
  i.smHttp = 502;
  expect(i, DiagComp::Spoolman, DiagLevel::Error, "server_error");
  i.smHttp = 200;
  expect(i, DiagComp::Spoolman, DiagLevel::Error, "not_spoolman");
  i.smHttp = 418;
  expect(i, DiagComp::Spoolman, DiagLevel::Error, "http_error");
  i.smChecked = false;
  expect(i, DiagComp::Spoolman, DiagLevel::Info, "starting");
  i.wifiConnected = false;  // Ursache ist das WLAN, nicht Spoolman
  expect(i, DiagComp::Spoolman, DiagLevel::Warn, "no_wifi");
}

void test_queue() {
  DiagInput i = healthy();
  i.queueCount = 3;
  expect(i, DiagComp::Queue, DiagLevel::Info, "sending");
  i.smReachable = false;
  expect(i, DiagComp::Queue, DiagLevel::Warn, "waiting");
  i.queueDropped = 1;
  expect(i, DiagComp::Queue, DiagLevel::Warn, "dropped");
}

void test_simple_mode() {
  DiagInput i = healthy();
  i.simple = true;
  i.nfcReady = false;
  i.smReachable = false;
  DiagItem items[DIAG_MAX_ITEMS];
  int n = diagRun(i, items);
  TEST_ASSERT_EQUAL(2, n);
  TEST_ASSERT_NULL(find(items, n, DiagComp::Nfc));
  TEST_ASSERT_NULL(find(items, n, DiagComp::Spoolman));
}

void test_names() {
  TEST_ASSERT_EQUAL_STRING("sensor", diagCompName(DiagComp::Sensor));
  TEST_ASSERT_EQUAL_STRING("error", diagLevelName(DiagLevel::Error));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_all_ok);
  RUN_TEST(test_sensor);
  RUN_TEST(test_nfc);
  RUN_TEST(test_wifi);
  RUN_TEST(test_spoolman);
  RUN_TEST(test_queue);
  RUN_TEST(test_simple_mode);
  RUN_TEST(test_names);
  return UNITY_END();
}
