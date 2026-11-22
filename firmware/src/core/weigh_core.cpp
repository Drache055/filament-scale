#include "weigh_core.h"

WeighResult weighCompute(float w1, float w2, float emptySpool, float initialWeight) {
  WeighResult r;
  r.gross = w2 - w1;
  r.remaining = r.gross - emptySpool;
  if (r.remaining < -20.0f) {
    r.check = WeighCheck::EmptyTooBig;
    r.remaining = 0;
  } else if (r.remaining < 0) {
    r.check = WeighCheck::Clamped;
    r.remaining = 0;
  } else if (initialWeight >= 0 && r.remaining > initialWeight + 50.0f) {
    r.check = WeighCheck::TooHeavy;
  }
  return r;
}

void WeighFlow::start(uint32_t now) {
  end_ = WeighEnd::None;
  w1_ = w2_ = 0;
  enter(WeighState::SpoolIdentified, now);
}

static bool isWaiting(WeighState s) {
  return s == WeighState::WaitBox || s == WeighState::NewBox || s == WeighState::WaitSpool;
}

uint32_t WeighFlow::remainingMs(uint32_t now) const {
  if (!isWaiting(state_)) {
    return 0;
  }
  uint32_t used = now - entered_;
  return used >= cfg_.timeoutMs ? 0 : cfg_.timeoutMs - used;
}

void WeighFlow::step(const WeighInput &in) {
  if (state_ == WeighState::Idle) {
    return;
  }
  // Abbrechen gilt in jedem Zustand außer während des Sendens (der Auftrag läuft ohnehin weiter)
  if (in.cancel && state_ != WeighState::Upload) {
    finish(WeighEnd::Cancelled);
    return;
  }
  bool timedOut = isWaiting(state_) && in.now - entered_ >= cfg_.timeoutMs;
  if (timedOut) {
    finish(WeighEnd::Timeout);
    return;
  }

  switch (state_) {
    case WeighState::SpoolIdentified:
      if (in.empty && in.now - entered_ >= cfg_.showSpoolMs) {
        enter(WeighState::WaitBox, in.now);
      }
      break;
    case WeighState::WaitBox:
      if (in.stable && in.grams >= cfg_.addedG && in.boxTag) {
        w1_ = in.grams;
        enter(WeighState::WaitSpool, in.now);
      } else if (in.stable && in.grams >= cfg_.addedG && in.blankTag && !in.boxTag) {
        enter(WeighState::NewBox, in.now);  // Box steht, ihr Tag ist leer/unbekannt
      }
      break;
    case WeighState::NewBox:
      // Tag wurde beschrieben (oder ein gültiger Box-Tag aufgelegt): zurück, W1 wird dort wie gewohnt eingefroren
      if (in.boxTag) {
        enter(WeighState::WaitBox, in.now);
      }
      break;
    case WeighState::WaitSpool:
      if (in.stable && in.grams >= w1_ + cfg_.addedG) {
        w2_ = in.grams;
        enter(WeighState::Confirm, in.now);
      }
      break;
    case WeighState::Confirm:
      if (in.save) {
        enter(WeighState::Upload, in.now);
      }
      break;
    case WeighState::Upload:
      if (in.uploadDone) {
        enter(WeighState::Done, in.now);
      } else if (in.uploadFailed) {
        enter(WeighState::Failed, in.now);
      }
      break;
    case WeighState::Done:
      if (in.empty) {
        finish(WeighEnd::Finished);
      }
      break;
    case WeighState::Failed:
      if (in.save) {  // erneut versuchen
        enter(WeighState::Upload, in.now);
      }
      break;
    default:
      break;
  }
}
