#include "magicpanel/liveness.h"

namespace magicpanel {

LivenessTracker::LivenessTracker(float timeout_seconds)
    : timeout_seconds_(timeout_seconds), last_seen_(0.0f), has_seen_(false) {}

void LivenessTracker::mark_seen(float now_seconds) {
  last_seen_ = now_seconds;
  has_seen_ = true;
}

bool LivenessTracker::is_connected(float now_seconds) const {
  return has_seen_ && (now_seconds - last_seen_) < timeout_seconds_;
}

}  // namespace magicpanel
