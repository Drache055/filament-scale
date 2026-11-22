#include <unity.h>

#include "core/wifi_core.h"

void setUp() {}
void tearDown() {}

static WifiNet net(const char *ssid, int rssi, bool open = false) {
  WifiNet n;
  n.ssid = ssid;
  n.rssi = rssi;
  n.open = open;
  return n;
}

void test_merge_dedup_hidden_limit() {
  std::vector<WifiNet> raw = {net("A", -70), net("", -40), net("B", -60, true), net("A", -50), net("B", -80)};
  std::vector<WifiNet> out = wifiMergeNetworks(raw);
  TEST_ASSERT_EQUAL(2, out.size());
  TEST_ASSERT_EQUAL_STRING("A", out[0].ssid.c_str());
  TEST_ASSERT_EQUAL(-50, out[0].rssi);  // stärkstes Signal
  TEST_ASSERT_EQUAL_STRING("B", out[1].ssid.c_str());
  TEST_ASSERT_EQUAL(-60, out[1].rssi);
  TEST_ASSERT_TRUE(out[1].open);

  std::vector<WifiNet> many;
  for (int i = 0; i < 40; i++) many.push_back(net(std::to_string(i).c_str(), -i));
  many.push_back(net("0", 5));  // Duplikat nach der Grenze aktualisiert trotzdem
  out = wifiMergeNetworks(many, 30);
  TEST_ASSERT_EQUAL(30, out.size());
  TEST_ASSERT_EQUAL(5, out[0].rssi);
}

void test_scan_shared_and_cached() {
  ScanJob j(10000, 3000, 15000);
  TEST_ASSERT_EQUAL((int)ScanState::Pending, (int)j.request(0));
  TEST_ASSERT_TRUE(j.wantsStart());
  j.started(10);
  TEST_ASSERT_FALSE(j.wantsStart());
  TEST_ASSERT_EQUAL((int)ScanState::Running, (int)j.request(500));  // zweite Anfrage teilt den Scan
  j.finished(4000, {net("A", -50)});
  TEST_ASSERT_EQUAL((int)ScanState::Done, (int)j.request(5000));
  TEST_ASSERT_EQUAL(1, j.networks().size());
  TEST_ASSERT_EQUAL((int)ScanState::Done, (int)j.request(13999));
  TEST_ASSERT_EQUAL((int)ScanState::Pending, (int)j.request(14000));  // Ergebnis zu alt
}

void test_scan_failure_and_timeout() {
  ScanJob j(10000, 3000, 15000);
  j.request(0);
  j.started(0);
  TEST_ASSERT_FALSE(j.timedOut(14999));
  TEST_ASSERT_TRUE(j.timedOut(15000));
  j.failed(15000);
  TEST_ASSERT_EQUAL((int)ScanState::Failed, (int)j.request(16000));
  TEST_ASSERT_EQUAL(0, j.networks().size());
  TEST_ASSERT_EQUAL((int)ScanState::Pending, (int)j.request(18000));  // nach failHoldMs neuer Versuch
}

void test_scan_millis_wrap() {
  ScanJob j(10000, 3000, 15000);
  j.request(0xFFFFF000u);
  j.started(0xFFFFF000u);
  TEST_ASSERT_FALSE(j.timedOut(1000));
  j.finished(0xFFFFFF00u, {});
  TEST_ASSERT_EQUAL((int)ScanState::Done, (int)j.request(100));
}

void test_provision_flow() {
  ProvisionJob p;
  TEST_ASSERT_TRUE(p.request(20000));
  TEST_ASSERT_TRUE(p.busy());
  TEST_ASSERT_FALSE(p.request(20000));  // läuft schon
  TEST_ASSERT_TRUE(p.wantsStart());
  TEST_ASSERT_EQUAL((int)ProvState::Pending, (int)p.step(0, true));  // vor started() keine Auswertung
  p.started(100);
  TEST_ASSERT_EQUAL((int)ProvState::Connecting, (int)p.step(5000, false));
  TEST_ASSERT_EQUAL((int)ProvState::Connected, (int)p.step(6000, true));
  TEST_ASSERT_FALSE(p.busy());
  TEST_ASSERT_TRUE(p.request(20000));  // neuer Versuch nach Abschluss
}

void test_provision_timeout_and_fail() {
  ProvisionJob p;
  p.request(20000);
  p.started(1000);
  TEST_ASSERT_EQUAL((int)ProvState::Connecting, (int)p.step(20999, false));
  TEST_ASSERT_EQUAL((int)ProvState::Failed, (int)p.step(21000, false));
  TEST_ASSERT_EQUAL((int)ProvState::Failed, (int)p.step(22000, true));  // bleibt beim Ergebnis
  p.request(20000);
  p.started(0);
  p.step(10, true);
  p.fail();  // Speichern gescheitert
  TEST_ASSERT_EQUAL((int)ProvState::Failed, (int)p.state());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_merge_dedup_hidden_limit);
  RUN_TEST(test_scan_shared_and_cached);
  RUN_TEST(test_scan_failure_and_timeout);
  RUN_TEST(test_scan_millis_wrap);
  RUN_TEST(test_provision_flow);
  RUN_TEST(test_provision_timeout_and_fail);
  return UNITY_END();
}
