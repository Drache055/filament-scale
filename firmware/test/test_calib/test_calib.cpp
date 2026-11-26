#include <unity.h>

#include "core/calib_core.h"

void setUp() {}
void tearDown() {}

static const int32_t MIN = 1000;

void test_legacy_factor_only() {
  CalModel m;
  m.factor = 420.0f;
  TEST_ASSERT_TRUE(calValid(m));
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 100.0f, calToGrams(m, 42000));
  m.factor = 0.0f;
  TEST_ASSERT_FALSE(calValid(m));
}

void test_single_point_matches_old_formula() {
  CalModel m;
  TEST_ASSERT_EQUAL(CalError::None, calSetSingle(m, 84000, 200.0f, MIN));
  TEST_ASSERT_EQUAL(1, m.count);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 420.0f, m.factor);  // delta / knownGrams wie bisher
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 500.0f, calToGrams(m, 210000));
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, -10.0f, calToGrams(m, -4200));
}

void test_single_point_errors() {
  CalModel m;
  TEST_ASSERT_EQUAL(CalError::BadWeight, calSetSingle(m, 84000, 0.0f, MIN));
  TEST_ASSERT_EQUAL(CalError::SmallSignal, calSetSingle(m, 999, 200.0f, MIN));
  TEST_ASSERT_EQUAL(CalError::SmallSignal, calSetSingle(m, -999, 200.0f, MIN));
  TEST_ASSERT_EQUAL(0, m.count);
}

void test_multi_point_piecewise_exact_at_points() {
  CalModel m;
  // Leicht nichtlinear: 400 raw/g bis 500 g, darüber 410 raw/g
  TEST_ASSERT_EQUAL(CalError::None, calAddPoint(m, 405000, 1000.0f, MIN));
  TEST_ASSERT_EQUAL(CalError::None, calAddPoint(m, 200000, 500.0f, MIN));  // unsortiert eingefügt
  TEST_ASSERT_EQUAL(2, m.count);
  TEST_ASSERT_EQUAL_FLOAT(500.0f, m.pt[0].grams);
  TEST_ASSERT_EQUAL_FLOAT(1000.0f, m.pt[1].grams);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 500.0f, calToGrams(m, 200000));
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1000.0f, calToGrams(m, 405000));
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 250.0f, calToGrams(m, 100000));   // erster Abschnitt
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 750.0f, calToGrams(m, 302500));   // zweiter Abschnitt
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1500.0f, calToGrams(m, 610000));  // oberhalb: Steigung des letzten Abschnitts
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, -5.0f, calToGrams(m, -2000));     // unter Null: Steigung des ersten Abschnitts
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, calToGrams(m, 0));
}

void test_negative_direction() {
  CalModel m;  // Wägezelle andersherum verdrahtet: Rohwert sinkt mit dem Gewicht
  TEST_ASSERT_EQUAL(CalError::None, calAddPoint(m, -200000, 500.0f, MIN));
  TEST_ASSERT_EQUAL(CalError::None, calAddPoint(m, -405000, 1000.0f, MIN));
  TEST_ASSERT_TRUE(m.factor < 0);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 750.0f, calToGrams(m, -302500));
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 250.0f, calToGrams(m, -100000));
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1500.0f, calToGrams(m, -610000));
  TEST_ASSERT_EQUAL(CalError::WrongSign, calAddPoint(m, 300000, 750.0f, MIN));
}

void test_replace_same_weight() {
  CalModel m;
  calAddPoint(m, 200000, 500.0f, MIN);
  calAddPoint(m, 400000, 1000.0f, MIN);
  TEST_ASSERT_EQUAL(CalError::None, calAddPoint(m, 201000, 500.4f, MIN));
  TEST_ASSERT_EQUAL(2, m.count);
  TEST_ASSERT_EQUAL(201000, m.pt[0].raw);
}

void test_rejects_and_keeps_model() {
  CalModel m;
  calAddPoint(m, 200000, 500.0f, MIN);
  calAddPoint(m, 400000, 1000.0f, MIN);
  CalModel before = m;
  TEST_ASSERT_EQUAL(CalError::NotMonotonic, calAddPoint(m, 150000, 750.0f, MIN));  // schwerer, aber weniger Signal
  TEST_ASSERT_EQUAL(CalError::SmallSignal, calAddPoint(m, 500, 1.0f, MIN));
  TEST_ASSERT_EQUAL(CalError::BadWeight, calAddPoint(m, 300000, -1.0f, MIN));
  TEST_ASSERT_EQUAL(before.count, m.count);
  TEST_ASSERT_EQUAL(before.pt[0].raw, m.pt[0].raw);
  TEST_ASSERT_EQUAL_FLOAT(before.factor, m.factor);
  calAddPoint(m, 100000, 250.0f, MIN);
  calAddPoint(m, 300000, 750.0f, MIN);
  calAddPoint(m, 500000, 1250.0f, MIN);
  TEST_ASSERT_EQUAL(5, m.count);
  TEST_ASSERT_EQUAL(CalError::Full, calAddPoint(m, 600000, 1500.0f, MIN));
  TEST_ASSERT_EQUAL(CalError::None, calAddPoint(m, 501000, 1250.0f, MIN));  // Ersetzen geht auch voll
}

void test_linearity_and_factor() {
  CalModel m;
  // Perfekt linear: keine Abweichung, Faktor exakt
  calAddPoint(m, 40000, 100.0f, MIN);
  calAddPoint(m, 200000, 500.0f, MIN);
  calAddPoint(m, 400000, 1000.0f, MIN);
  TEST_ASSERT_FLOAT_WITHIN(1e-4f, 400.0f, m.factor);
  CalLinearity l = calLinearity(m);
  TEST_ASSERT_EQUAL(3, l.count);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.0f, l.maxAbsDev);

  // Mitte 2 g zu hoch: Abweichung sichtbar, Umrechnung an den Punkten trotzdem exakt
  CalModel n;
  calAddPoint(n, 40000, 100.0f, MIN);
  calAddPoint(n, 200800, 500.0f, MIN);
  calAddPoint(n, 400000, 1000.0f, MIN);
  CalLinearity ln = calLinearity(n);
  TEST_ASSERT_TRUE(ln.maxAbsDev > 1.0f && ln.maxAbsDev < 2.0f);
  TEST_ASSERT_TRUE(ln.dev[1] > 0);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 100.0f * ln.maxAbsDev / 1000.0f, ln.maxAbsDevPct);
  TEST_ASSERT_FLOAT_WITHIN(1e-3f, 500.0f, calToGrams(n, 200800));
}

void test_valid_rejects_corrupt() {
  CalModel m;
  calAddPoint(m, 200000, 500.0f, MIN);
  calAddPoint(m, 400000, 1000.0f, MIN);
  TEST_ASSERT_TRUE(calValid(m));
  CalModel bad = m;
  bad.count = 9;
  TEST_ASSERT_FALSE(calValid(bad));
  bad = m;
  bad.pt[1].grams = 400.0f;  // unsortiert
  TEST_ASSERT_FALSE(calValid(bad));
  bad = m;
  bad.pt[1].raw = -400000;
  TEST_ASSERT_FALSE(calValid(bad));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_legacy_factor_only);
  RUN_TEST(test_single_point_matches_old_formula);
  RUN_TEST(test_single_point_errors);
  RUN_TEST(test_multi_point_piecewise_exact_at_points);
  RUN_TEST(test_negative_direction);
  RUN_TEST(test_replace_same_weight);
  RUN_TEST(test_rejects_and_keeps_model);
  RUN_TEST(test_linearity_and_factor);
  RUN_TEST(test_valid_rejects_corrupt);
  return UNITY_END();
}
