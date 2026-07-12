#pragma once

#include <cstdint>

#include "magicpanel/canvas.h"

namespace magicpanel {

enum class DeployStatusKind {
  Deploy,
  Build,
};

enum class DeployStatusResult {
  Running,
  Success,
  Failed,
};

struct DeployStatusPanel {
  DeployStatusKind kind = DeployStatusKind::Deploy;
  DeployStatusResult result = DeployStatusResult::Running;
  std::uint8_t open = 255;
  std::uint8_t progress = 0;
};

void draw_deploy_status_scroll(Canvas& canvas,
                               int x,
                               int y,
                               float now_seconds,
                               DeployStatusPanel const& panel);

}  // namespace magicpanel
