#include "scale_filter.h"

#include <algorithm>
#include <cmath>

WindowStats scaleWindowStats(const long *samples, int n, int trim) {
  WindowStats s;
  if (n <= 0) {
    return s;
  }
  if (n > 32) {
    n = 32;
  }
  long sorted[32];
  std::copy(samples, samples + n, sorted);
  std::sort(sorted, sorted + n);
  int t = std::max(0, std::min(trim, (n - 1) / 2));
  long long sum = 0;
  for (int i = t; i < n - t; i++) {
    sum += sorted[i];
  }
  s.used = n - 2 * t;
  s.mean = (long)(sum / s.used);
  s.lo = sorted[t];
  s.hi = sorted[n - 1 - t];
  return s;
}

ZeroAction ZeroTracker::step(const ZeroInput &in) {
  if (!haveData_) {
    haveData_ = true;
    dataSince_ = in.now;
    holding_ = false;
    if (!everData_) {
      everData_ = true;
      bootStart_ = in.now;
    }
  }
  if (in.now - dataSince_ < cfg_.settleMs) {
    return ZeroAction::None;
  }
  float g = std::fabs(in.grams);

  if (!bootDone_) {
    if (in.now - bootStart_ > cfg_.bootWindowMs) {
      bootDone_ = true;  // Waage war in der Startphase nie leer: nicht mehr automatisch nullen
    } else if (in.stable && in.idle && g <= cfg_.bootG) {
      bootDone_ = true;
      holding_ = false;
      return ZeroAction::BootZero;
    }
  }

  if (!(in.stable && in.idle && g < cfg_.trackG)) {
    holding_ = false;
    return ZeroAction::None;
  }
  if (!holding_) {
    holding_ = true;
    holdSince_ = in.now;
    return ZeroAction::None;
  }
  if (in.now - holdSince_ >= cfg_.trackHoldMs) {
    holdSince_ = in.now;
    return ZeroAction::Track;
  }
  return ZeroAction::None;
}
