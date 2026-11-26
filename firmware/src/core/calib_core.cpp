#include "core/calib_core.h"

#include <cmath>
#include <cstdlib>

namespace {

float sign(float v) { return v < 0 ? -1.0f : 1.0f; }

// Ausgleichsgerade durch Null: Steigung k = Σ(r·g) / Σ(g²)
float lsqFactor(const CalModel &m) {
  double rg = 0, gg = 0;
  for (int i = 0; i < m.count; i++) {
    rg += (double)m.pt[i].raw * m.pt[i].grams;
    gg += (double)m.pt[i].grams * m.pt[i].grams;
  }
  return gg > 0 ? (float)(rg / gg) : m.factor;
}

}  // namespace

bool calValid(const CalModel &m) {
  if (m.count < 0 || m.count > CAL_MAX_POINTS) {
    return false;
  }
  if (m.count == 0) {
    return m.factor != 0.0f && std::isfinite(m.factor);
  }
  float s = sign((float)m.pt[0].raw);
  float prevG = 0, prevR = 0;
  for (int i = 0; i < m.count; i++) {
    const CalPoint &p = m.pt[i];
    float r = s * (float)p.raw;  // auf positive Richtung gedreht
    if (!(p.grams > 0) || !std::isfinite(p.grams) || p.raw == 0 || sign((float)p.raw) != s) {
      return false;
    }
    if (p.grams <= prevG || r <= prevR) {
      return false;
    }
    prevG = p.grams;
    prevR = r;
  }
  return true;
}

float calToGrams(const CalModel &m, float rawNet) {
  if (m.count < 2) {
    float k = m.count == 1 ? m.pt[0].raw / m.pt[0].grams : m.factor;
    return k != 0.0f ? rawNet / k : 0.0f;
  }
  // Abschnitt suchen: (0,0)-P0, P0-P1, ... Außerhalb mit dem ersten bzw. letzten Abschnitt weiter
  float s = sign((float)m.pt[0].raw);
  float r = s * rawNet;
  float r0 = 0, g0 = 0;
  int i = 0;
  while (i < m.count - 1 && r > s * (float)m.pt[i].raw) {
    r0 = s * (float)m.pt[i].raw;
    g0 = m.pt[i].grams;
    i++;
  }
  float r1 = s * (float)m.pt[i].raw;
  float g1 = m.pt[i].grams;
  return g0 + (r - r0) * (g1 - g0) / (r1 - r0);
}

void calUpdateFactor(CalModel &m) {
  if (m.count > 0) {
    m.factor = lsqFactor(m);
  }
}

CalError calSetSingle(CalModel &m, int32_t raw, float grams, int32_t minDelta) {
  if (!(grams > 0) || !std::isfinite(grams)) {
    return CalError::BadWeight;
  }
  if (std::labs((long)raw) < minDelta) {
    return CalError::SmallSignal;
  }
  m.count = 1;
  m.pt[0].raw = raw;
  m.pt[0].grams = grams;
  calUpdateFactor(m);
  return CalError::None;
}

CalError calAddPoint(CalModel &m, int32_t raw, float grams, int32_t minDelta) {
  if (!(grams > 0) || !std::isfinite(grams)) {
    return CalError::BadWeight;
  }
  if (std::labs((long)raw) < minDelta) {
    return CalError::SmallSignal;
  }
  CalModel n = m;
  // Gleiches Gewicht ersetzen
  int same = -1;
  for (int i = 0; i < n.count; i++) {
    if (std::fabs(n.pt[i].grams - grams) < CAL_SAME_POINT_G) {
      same = i;
    }
  }
  if (same < 0) {
    if (n.count >= CAL_MAX_POINTS) {
      return CalError::Full;
    }
    if (n.count > 0 && sign((float)raw) != sign((float)n.pt[0].raw)) {
      return CalError::WrongSign;
    }
    n.pt[n.count].raw = raw;
    n.pt[n.count].grams = grams;
    n.count++;
  } else {
    n.pt[same].raw = raw;
    n.pt[same].grams = grams;
    if (n.count > 1) {
      float s = sign((float)n.pt[same == 0 ? 1 : 0].raw);
      if (sign((float)raw) != s) {
        return CalError::WrongSign;
      }
    }
  }
  // Nach Gewicht sortieren (Einfügen, höchstens 5 Punkte)
  for (int i = 1; i < n.count; i++) {
    CalPoint p = n.pt[i];
    int j = i - 1;
    while (j >= 0 && n.pt[j].grams > p.grams) {
      n.pt[j + 1] = n.pt[j];
      j--;
    }
    n.pt[j + 1] = p;
  }
  if (!calValid(n)) {
    return CalError::NotMonotonic;
  }
  calUpdateFactor(n);
  m = n;
  return CalError::None;
}

CalLinearity calLinearity(const CalModel &m) {
  CalLinearity l;
  l.count = m.count;
  if (m.count == 0) {
    return l;
  }
  float k = lsqFactor(m);
  float maxG = 0;
  for (int i = 0; i < m.count; i++) {
    l.dev[i] = k != 0.0f ? m.pt[i].raw / k - m.pt[i].grams : 0.0f;
    if (std::fabs(l.dev[i]) > l.maxAbsDev) {
      l.maxAbsDev = std::fabs(l.dev[i]);
    }
    if (m.pt[i].grams > maxG) {
      maxG = m.pt[i].grams;
    }
  }
  l.maxAbsDevPct = maxG > 0 ? 100.0f * l.maxAbsDev / maxG : 0.0f;
  return l;
}
