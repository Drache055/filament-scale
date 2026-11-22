#include <unity.h>

#include "core/weigh_core.h"

void setUp() {}
void tearDown() {}

static WeighInput in(uint32_t now, float g, bool stable = true) {
  WeighInput i;
  i.now = now;
  i.grams = g;
  i.stable = stable;
  i.empty = stable && g <= 5 && g >= -5;
  return i;
}

void test_compute_ok() {
  WeighResult r = weighCompute(412, 1254, 250, 1000);
  TEST_ASSERT_EQUAL_FLOAT(842, r.gross);
  TEST_ASSERT_EQUAL_FLOAT(592, r.remaining);
  TEST_ASSERT_EQUAL(WeighCheck::Ok, r.check);
}

void test_compute_clamped_and_error() {
  TEST_ASSERT_EQUAL(WeighCheck::Clamped, weighCompute(0, 245, 250, -1).check);
  TEST_ASSERT_EQUAL_FLOAT(0, weighCompute(0, 245, 250, -1).remaining);
  TEST_ASSERT_EQUAL(WeighCheck::EmptyTooBig, weighCompute(0, 200, 250, -1).check);
}

void test_compute_too_heavy() {
  TEST_ASSERT_EQUAL(WeighCheck::TooHeavy, weighCompute(0, 1400, 250, 1000).check);
  TEST_ASSERT_EQUAL(WeighCheck::Ok, weighCompute(0, 1400, 250, -1).check);
}

void test_full_flow() {
  WeighFlow f;
  f.start(0);
  TEST_ASSERT_EQUAL(WeighState::SpoolIdentified, f.state());
  f.step(in(500, 0));  // zu früh
  TEST_ASSERT_EQUAL(WeighState::SpoolIdentified, f.state());
  f.step(in(2500, 0));
  TEST_ASSERT_EQUAL(WeighState::WaitBox, f.state());
  WeighInput box = in(3000, 412);
  f.step(box);  // ohne Box-Tag nichts
  TEST_ASSERT_EQUAL(WeighState::WaitBox, f.state());
  box.boxTag = true;
  box.stable = false;
  f.step(box);  // nicht stabil
  TEST_ASSERT_EQUAL(WeighState::WaitBox, f.state());
  box.stable = true;
  f.step(box);
  TEST_ASSERT_EQUAL(WeighState::WaitSpool, f.state());
  TEST_ASSERT_EQUAL_FLOAT(412, f.w1());
  f.step(in(4000, 440));  // zu wenig dazu
  TEST_ASSERT_EQUAL(WeighState::WaitSpool, f.state());
  f.step(in(5000, 1254));
  TEST_ASSERT_EQUAL(WeighState::Confirm, f.state());
  TEST_ASSERT_EQUAL_FLOAT(1254, f.w2());
  WeighInput save = in(6000, 1254);
  save.save = true;
  f.step(save);
  TEST_ASSERT_EQUAL(WeighState::Upload, f.state());
  WeighInput done = in(7000, 1254);
  done.uploadDone = true;
  f.step(done);
  TEST_ASSERT_EQUAL(WeighState::Done, f.state());
  f.step(in(8000, 1254));  // Box noch drauf
  TEST_ASSERT_EQUAL(WeighState::Done, f.state());
  f.step(in(9000, 0));
  TEST_ASSERT_EQUAL(WeighState::Idle, f.state());
  TEST_ASSERT_EQUAL(WeighEnd::Finished, f.end());
}

void test_cancel_and_timeout() {
  WeighFlow f;
  f.start(0);
  WeighInput c = in(100, 0);
  c.cancel = true;
  f.step(c);
  TEST_ASSERT_EQUAL(WeighState::Idle, f.state());
  TEST_ASSERT_EQUAL(WeighEnd::Cancelled, f.end());

  f.start(1000);
  f.step(in(4000, 0));
  TEST_ASSERT_EQUAL(WeighState::WaitBox, f.state());
  f.step(in(4000 + 119999, 0));
  TEST_ASSERT_EQUAL(WeighState::WaitBox, f.state());
  f.step(in(4000 + 120000, 0));
  TEST_ASSERT_EQUAL(WeighState::Idle, f.state());
  TEST_ASSERT_EQUAL(WeighEnd::Timeout, f.end());
}

void test_upload_failed_retry() {
  WeighFlow f;
  f.start(0);
  f.step(in(2000, 0));
  WeighInput b = in(2100, 412);
  b.boxTag = true;
  f.step(b);
  f.step(in(2200, 1254));
  WeighInput s = in(2300, 1254);
  s.save = true;
  f.step(s);
  WeighInput x = in(2400, 1254);
  x.uploadFailed = true;
  f.step(x);
  TEST_ASSERT_EQUAL(WeighState::Failed, f.state());
  f.step(s);
  TEST_ASSERT_EQUAL(WeighState::Upload, f.state());
}

// Leerer/unbekannter Box-Tag: "Neue Box anlegen", nach dem Beschreiben weiter wie gewohnt
void test_new_box() {
  WeighFlow f;
  f.start(0);
  f.step(in(2000, 0));
  TEST_ASSERT_EQUAL(WeighState::WaitBox, f.state());
  WeighInput blank = in(3000, 412, false);
  blank.blankTag = true;
  f.step(blank);  // nicht stabil
  TEST_ASSERT_EQUAL(WeighState::WaitBox, f.state());
  blank = in(3100, 20);
  blank.blankTag = true;
  f.step(blank);  // keine Box auf der Waage (unter Schwelle)
  TEST_ASSERT_EQUAL(WeighState::WaitBox, f.state());
  blank = in(3200, 412);
  blank.blankTag = true;
  f.step(blank);
  TEST_ASSERT_EQUAL(WeighState::NewBox, f.state());
  blank.now = 60000;
  f.step(blank);  // bleibt, bis der Tag beschrieben ist
  TEST_ASSERT_EQUAL(WeighState::NewBox, f.state());
  WeighInput written = in(61000, 412);
  written.boxTag = true;
  f.step(written);
  TEST_ASSERT_EQUAL(WeighState::WaitBox, f.state());
  TEST_ASSERT_EQUAL(61000u, f.enteredAt());  // neuer Timeout ab hier
  f.step(written);
  TEST_ASSERT_EQUAL(WeighState::WaitSpool, f.state());
  TEST_ASSERT_EQUAL_FLOAT(412, f.w1());
}

void test_new_box_cancel_and_timeout() {
  WeighFlow f;
  f.start(0);
  f.step(in(2000, 0));
  WeighInput blank = in(2100, 412);
  blank.blankTag = true;
  f.step(blank);
  TEST_ASSERT_EQUAL(WeighState::NewBox, f.state());
  TEST_ASSERT_EQUAL(120000u - 900u, f.remainingMs(3000));
  WeighInput c = in(3000, 412);
  c.cancel = true;
  f.step(c);
  TEST_ASSERT_EQUAL(WeighState::Idle, f.state());
  TEST_ASSERT_EQUAL(WeighEnd::Cancelled, f.end());

  f.start(0);
  f.step(in(2000, 0));
  f.step(blank);
  TEST_ASSERT_EQUAL(WeighState::NewBox, f.state());
  blank.now = 2100 + 120000;
  f.step(blank);
  TEST_ASSERT_EQUAL(WeighState::Idle, f.state());
  TEST_ASSERT_EQUAL(WeighEnd::Timeout, f.end());
}

// Gültiger Box-Tag hat Vorrang: kein "Neue Box anlegen"
void test_box_tag_wins_over_blank() {
  WeighFlow f;
  f.start(0);
  f.step(in(2000, 0));
  WeighInput b = in(2100, 412);
  b.boxTag = true;
  b.blankTag = true;
  f.step(b);
  TEST_ASSERT_EQUAL(WeighState::WaitSpool, f.state());
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_compute_ok);
  RUN_TEST(test_compute_clamped_and_error);
  RUN_TEST(test_compute_too_heavy);
  RUN_TEST(test_full_flow);
  RUN_TEST(test_cancel_and_timeout);
  RUN_TEST(test_upload_failed_retry);
  RUN_TEST(test_new_box);
  RUN_TEST(test_new_box_cancel_and_timeout);
  RUN_TEST(test_box_tag_wins_over_blank);
  return UNITY_END();
}
