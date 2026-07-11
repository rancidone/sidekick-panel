#pragma once

namespace magicpanel {

constexpr float kDefaultLivenessTimeoutSeconds = 15.0f;

class LivenessTracker {
 public:
  explicit LivenessTracker(float timeout_seconds = kDefaultLivenessTimeoutSeconds);

  void mark_seen(float now_seconds);
  bool is_connected(float now_seconds) const;

 private:
  float timeout_seconds_;
  float last_seen_;
  bool has_seen_;
};

}  // namespace magicpanel
