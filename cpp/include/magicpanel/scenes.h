#pragma once

#include <memory>

#include "magicpanel/liveness.h"
#include "magicpanel/reactions.h"
#include "magicpanel/scene.h"
#include "magicpanel/environment.h"

namespace magicpanel {

std::unique_ptr<Scene> make_desk_spirit_scene(
    LivenessTracker& liveness, AccumulatingStateStore& state, EnvironmentState& environment);
std::unique_ptr<Scene> make_arcane_tree_scene();
std::unique_ptr<Scene> make_meadow_cycle_scene(EnvironmentState& environment);

}  // namespace magicpanel
