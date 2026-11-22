#include <unity.h>

#include "core/simple_core.h"

void setUp() {}
void tearDown() {}

static SimpleInput in(uint32_t now, float g, bool stable = true) {
  SimpleInput i;
  i.now = now;
  i.grams = g;
  i.stable = stable;
  i.empty = stable && g <= 5 && g >= -5;
  return i;
}

void test_filament() {
  TEST_ASSERT_EQUAL_FLOAT(592, simpleFilament(842, 250));
  TEST_ASSERT_EQUAL_FLOAT(842, simpleFilament(842, 0));
  TEST_ASSERT_EQUAL_FLOAT(0, simpleFilament(200, 250));
}

void test_spool_only() {
  SimpleFlow f;
  f.start(0, false);
  TEST_ASSERT_EQUAL(SimpleState::Clear, f.state());
  f.step(in(100, 300));  // noch etwas drauf
  TEST_ASSERT_EQUAL(SimpleState::Clear, f.state());
  f.step(in(200, 2));  // leer, Nullpunkt 2 g
  TEST_ASSERT_EQUAL(SimpleState::Measure, f.state());
  TEST_ASSERT_EQUAL_FLOAT(2, f.zero());
  TEST_ASSERT_EQUAL_FLOAT(840, f.spoolGross(842));
  f.step(in(300, 842, false));
  TEST_ASSERT_FALSE(f.measured());
  f.step(in(400, 842));
  TEST_ASSERT_TRUE(f.measured());
  TEST_ASSERT_EQUAL(SimpleState::Measure, f.state());
  f.step(in(500, 0));  // Spule abgenommen
  TEST_ASSERT_EQUAL(SimpleState::Idle, f.state());
  TEST_ASSERT_EQUAL(SimpleEnd::Finished, f.end());
}

void test_box_and_spool() {
  SimpleFlow f;
  f.start(0, true);
  f.step(in(100, 0));
  TEST_ASSERT_EQUAL(SimpleState::WaitBox, f.state());
  SimpleInput b = in(200, 412, false);
  TEST_ASSERT_FALSE(f.canTakeBox(b));  // nicht stabil
  b.takeBox = true;
  f.step(b);
  TEST_ASSERT_EQUAL(SimpleState::WaitBox, f.state());
  SimpleInput light = in(300, 30);
  light.takeBox = true;
  f.step(light);  // unter der Schwelle
  TEST_ASSERT_EQUAL(SimpleState::WaitBox, f.state());
  b = in(400, 412);
  TEST_ASSERT_TRUE(f.canTakeBox(b));
  f.step(b);  // ohne Bestätigung passiert nichts
  TEST_ASSERT_EQUAL(SimpleState::WaitBox, f.state());
  b.takeBox = true;
  f.step(b);
  TEST_ASSERT_EQUAL(SimpleState::Measure, f.state());
  TEST_ASSERT_EQUAL_FLOAT(412, f.boxWeight());
  TEST_ASSERT_EQUAL_FLOAT(0, f.spoolGross(400));  // nie negativ
  f.step(in(500, 1254));
  TEST_ASSERT_TRUE(f.measured());
  TEST_ASSERT_EQUAL_FLOAT(842, f.spoolGross(1254));
  f.step(in(600, 412));  // Spule wieder heraus, Box steht noch
  TEST_ASSERT_EQUAL(SimpleState::Measure, f.state());
  f.step(in(700, 0));  // Box abgenommen
  TEST_ASSERT_EQUAL(SimpleEnd::Finished, f.end());
}

void test_cancel_and_done() {
  SimpleFlow f;
  f.start(0, true);
  SimpleInput c = in(100, 0);
  c.cancel = true;
  f.step(c);
  TEST_ASSERT_EQUAL(SimpleEnd::Cancelled, f.end());

  f.start(0, false);
  f.step(in(100, 0));
  f.step(in(200, 900));
  c = in(300, 900);
  c.cancel = true;  // [Fertig] nach einer Messung
  f.step(c);
  TEST_ASSERT_EQUAL(SimpleEnd::Finished, f.end());
}

void test_timeout() {
  SimpleConfig cfg;
  cfg.timeoutMs = 1000;
  SimpleFlow f(cfg);
  f.start(0, true);
  f.step(in(100, 0));
  TEST_ASSERT_EQUAL(SimpleState::WaitBox, f.state());
  TEST_ASSERT_EQUAL_UINT32(1000, f.remainingMs(100));
  f.step(in(1100, 412, false));
  TEST_ASSERT_EQUAL(SimpleEnd::Timeout, f.end());

  // Nach einer Messung läuft kein Timeout mehr
  f.start(0, false);
  f.step(in(0, 0));
  f.step(in(500, 900));
  TEST_ASSERT_EQUAL_UINT32(0, f.remainingMs(500));
  f.step(in(5000, 900));
  TEST_ASSERT_EQUAL(SimpleState::Measure, f.state());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_filament);
  RUN_TEST(test_spool_only);
  RUN_TEST(test_box_and_spool);
  RUN_TEST(test_cancel_and_done);
  RUN_TEST(test_timeout);
  return UNITY_END();
}
