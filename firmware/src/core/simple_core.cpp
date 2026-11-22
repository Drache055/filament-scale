#include "simple_core.h"

float simpleFilament(float spoolGross, float emptySpool) {
  float f = spoolGross - emptySpool;
  return f < 0 ? 0 : f;
}

void SimpleFlow::start(uint32_t now, bool withBox) {
  end_ = SimpleEnd::None;
  withBox_ = withBox;
  measured_ = false;
  zero_ = box_ = 0;
  enter(SimpleState::Clear, now);
}

bool SimpleFlow::canTakeBox(const SimpleInput &in) const {
  return state_ == SimpleState::WaitBox && in.stable && in.grams >= zero_ + cfg_.addedG;
}

float SimpleFlow::spoolGross(float grams) const {
  float g = grams - reference();
  return g < 0 ? 0 : g;
}

bool SimpleFlow::timeoutActive() const {
  return state_ == SimpleState::Clear || state_ == SimpleState::WaitBox || (state_ == SimpleState::Measure && !measured_);
}

uint32_t SimpleFlow::remainingMs(uint32_t now) const {
  if (!timeoutActive()) {
    return 0;
  }
  uint32_t used = now - entered_;
  return used >= cfg_.timeoutMs ? 0 : cfg_.timeoutMs - used;
}

void SimpleFlow::step(const SimpleInput &in) {
  if (state_ == SimpleState::Idle) {
    return;
  }
  if (in.cancel) {
    finish(measured_ ? SimpleEnd::Finished : SimpleEnd::Cancelled);
    return;
  }
  if (timeoutActive() && in.now - entered_ >= cfg_.timeoutMs) {
    finish(SimpleEnd::Timeout);
    return;
  }
  switch (state_) {
    case SimpleState::Clear:
      if (in.empty) {
        zero_ = in.grams;
        enter(withBox_ ? SimpleState::WaitBox : SimpleState::Measure, in.now);
      }
      break;
    case SimpleState::WaitBox:
      if (in.takeBox && canTakeBox(in)) {
        box_ = in.grams;
        enter(SimpleState::Measure, in.now);
      }
      break;
    case SimpleState::Measure:
      if (in.stable && in.grams >= reference() + cfg_.addedG) {
        measured_ = true;
      } else if (measured_ && in.empty) {
        finish(SimpleEnd::Finished);  // alles abgenommen: fertig
      }
      break;
    default:
      break;
  }
}
