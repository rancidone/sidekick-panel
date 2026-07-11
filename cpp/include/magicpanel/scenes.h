#pragma once

#include <memory>

#include "magicpanel/liveness.h"
#include "magicpanel/reactions.h"
#include "magicpanel/scene.h"

namespace magicpanel {

std::unique_ptr<Scene> make_desk_spirit_scene(
    LivenessTracker& liveness, AccumulatingStateStore& state);
std::unique_ptr<Scene> make_arcane_tree_scene();

}  // namespace magicpanel
