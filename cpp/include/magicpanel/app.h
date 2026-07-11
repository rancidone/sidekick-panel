#pragma once

#include <memory>
#include <string>

#include "magicpanel/engine.h"
#include "magicpanel/liveness.h"
#include "magicpanel/scene.h"
#include "magicpanel/state_store.h"

namespace magicpanel {

class MagicPanelApp {
 public:
  explicit MagicPanelApp(std::string state_path = "");

  SceneManager& scenes() { return *scenes_; }
  LivenessTracker& liveness() { return liveness_; }

 private:
  AccumulatingStateStore state_;
  LivenessTracker liveness_;
  std::unique_ptr<SceneManager> scenes_;
};

}  // namespace magicpanel
