#include "magicpanel/app.h"

#include <utility>
#include <vector>

#include "magicpanel/scenes.h"

namespace magicpanel {
namespace {

std::vector<std::unique_ptr<Scene>> default_scenes(LivenessTracker& liveness,
                                                   AccumulatingStateStore& state) {
  std::vector<std::unique_ptr<Scene>> scenes;
  scenes.push_back(make_desk_spirit_scene(liveness, state));
  scenes.push_back(make_arcane_tree_scene());
  return scenes;
}

}  // namespace

MagicPanelApp::MagicPanelApp(std::string state_path)
    : state_(std::move(state_path)),
      liveness_(),
      scenes_(std::make_unique<SceneManager>(default_scenes(liveness_, state_), "desk_spirit")) {}

}  // namespace magicpanel
