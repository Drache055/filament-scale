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

void test_classify_reason() {
  TEST_ASSERT_TRUE(wifiClassifyReason(0) == WifiFail::None);
  TEST_ASSERT_TRUE(wifiClassifyReason(201) == WifiFail::NotFound);
  TEST_ASSERT_TRUE(wifiClassifyReason(212) == WifiFail::NotFound);
  TEST_ASSERT_TRUE(wifiClassifyReason(15) == WifiFail::Auth);
  TEST_ASSERT_TRUE(wifiClassifyReason(202) == WifiFail::Auth);
  TEST_ASSERT_TRUE(wifiClassifyReason(204) == WifiFail::Auth);
  TEST_ASSERT_TRUE(wifiClassifyReason(200) == WifiFail::Other);
  TEST_ASSERT_EQUAL_STRING("notfound", wifiFailName(WifiFail::NotFound));
  TEST_ASSERT_EQUAL_STRING("auth", wifiFailName(WifiFail::Auth));
  TEST_ASSERT_EQUAL_STRING("other", wifiFailName(WifiFail::Other));
  TEST_ASSERT_EQUAL_STRING("", wifiFailName(WifiFail::None));
}

void test_reconnect_backoff_forever() {
  ReconnectPolicy p(15000, 120000);
  p.started(0);  // erster Versuch beim Start
  TEST_ASSERT_FALSE(p.due(14999, false, false));
  TEST_ASSERT_FALSE(p.due(15000, false, false));  // Versuch gescheitert, Pause 5 s (1. Fehlschlag)
  TEST_ASSERT_EQUAL(1, p.failures());
  TEST_ASSERT_FALSE(p.due(19999, false, false));
  TEST_ASSERT_TRUE(p.due(20000, false, false));
  uint32_t t = 20000;
  const uint32_t pauses[] = {10000, 20000, 30000, 60000, 60000, 60000, 60000};
  for (uint32_t pause : pauses) {
    p.started(t);
    t += 15000;
    TEST_ASSERT_FALSE(p.due(t, false, false));
    TEST_ASSERT_FALSE(p.due(t + pause - 1, false, false));
    t += pause;
    TEST_ASSERT_TRUE(p.due(t, false, false));  // es geht immer weiter, höchstens 60 s Pause
  }
}

void test_reconnect_reset_and_drop() {
  ReconnectPolicy p(15000, 120000);
  p.started(0);
  TEST_ASSERT_FALSE(p.due(15000, false, false));
  TEST_ASSERT_EQUAL(1, p.failures());
  TEST_ASSERT_FALSE(p.due(16000, true, false));  // verbunden: alles zurück
  TEST_ASSERT_EQUAL(0, p.failures());
  TEST_ASSERT_FALSE(p.due(500000, true, false));
  TEST_ASSERT_FALSE(p.due(600000, false, false));  // gerade getrennt: 2 s Pause
  TEST_ASSERT_FALSE(p.due(601999, false, false));
  TEST_ASSERT_TRUE(p.due(602000, false, false));
}

void test_reconnect_hotspot_clients_and_external_fail() {
  ReconnectPolicy p(15000, 120000);
  p.failed(1000);  // z. B. USB-Einrichtung gescheitert
  TEST_ASSERT_EQUAL(1, p.failures());
  TEST_ASSERT_FALSE(p.due(6000, false, true));     // jemand im Hotspot: mindestens 120 s
  TEST_ASSERT_FALSE(p.due(120999, false, true));
  TEST_ASSERT_TRUE(p.due(121000, false, true));
  TEST_ASSERT_TRUE(p.due(6000, false, false));     // ohne Hotspot-Gäste reichen 5 s
  TEST_ASSERT_EQUAL(120000, p.pauseMs(true));
}

void test_reconnect_millis_wrap() {
  ReconnectPolicy p(15000, 120000);
  uint32_t t0 = 0xFFFFF000u;
  p.started(t0);
  TEST_ASSERT_FALSE(p.due(t0 + 14999, false, false));
  TEST_ASSERT_FALSE(p.due(t0 + 15000, false, false));
  TEST_ASSERT_TRUE(p.due(t0 + 20000, false, false));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_merge_dedup_hidden_limit);
  RUN_TEST(test_scan_shared_and_cached);
  RUN_TEST(test_scan_failure_and_timeout);
  RUN_TEST(test_scan_millis_wrap);
  RUN_TEST(test_provision_flow);
  RUN_TEST(test_provision_timeout_and_fail);
  RUN_TEST(test_classify_reason);
  RUN_TEST(test_reconnect_backoff_forever);
  RUN_TEST(test_reconnect_reset_and_drop);
  RUN_TEST(test_reconnect_hotspot_clients_and_external_fail);
  RUN_TEST(test_reconnect_millis_wrap);
  return UNITY_END();
}
