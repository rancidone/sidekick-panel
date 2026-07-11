#pragma once

#include "magicpanel/canvas.h"
#include "magicpanel/event.h"
#include "magicpanel/liveness.h"
#include "magicpanel/scene.h"
#include "magicpanel/state_store.h"

namespace magicpanel {

class Platform {
 public:
  virtual ~Platform() = default;
  virtual bool poll_event(Event& event) = 0;
  virtual float now_seconds() = 0;
  virtual void sleep_seconds(float seconds) = 0;
};

struct EngineConfig {
  float fps = 60.0f;
};

void run_engine(Canvas& canvas,
                SceneManager& scenes,
                LivenessTracker& liveness,
                Platform& platform,
                EngineConfig config = {});

}  // namespace magicpanel
