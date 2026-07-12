#include "magicpanel/scene.h"

#include <cstdio>

namespace magicpanel {

SceneManager::SceneManager(std::vector<std::unique_ptr<Scene>> scenes, std::string initial)
    : SceneManager(std::move(scenes), std::move(initial), nullptr) {}

SceneManager::SceneManager(std::vector<std::unique_ptr<Scene>> scenes,
                           std::string initial,
                           EnvironmentState* environment)
    : active_name_(std::move(initial)) {
  environment_ = environment;
  for (auto& scene : scenes) {
    scenes_[scene->name()] = std::move(scene);
  }
  if (scenes_.find(active_name_) == scenes_.end()) {
    active_name_ = scenes_.empty() ? "" : scenes_.begin()->first;
  }
}

std::string SceneManager::active_name() const {
  return active_name_;
}

bool SceneManager::empty() const {
  return scenes_.empty();
}

void SceneManager::switch_to(std::string const& name) {
  if (scenes_.find(name) == scenes_.end()) {
    std::fprintf(stderr, "ignoring switch to unknown scene '%s'\n", name.c_str());
    return;
  }
  active_name_ = name;
}

void SceneManager::handle_event(Event const& event) {
  if (empty()) {
    return;
  }
  if (environment_ != nullptr) {
    environment_->handle_event(event);
  }
  if (event.name == "switch_scene") {
    auto target = event.field_or("to");
    if (!target.empty()) {
      switch_to(target);
    }
    return;
  }
  scenes_.at(active_name_)->handle_event(event);
}

void SceneManager::render(Canvas& canvas, float dt, float now_seconds) {
  if (empty()) {
    canvas.clear(kBlack);
    return;
  }
  scenes_.at(active_name_)->render(canvas, dt, now_seconds);
}

}  // namespace magicpanel
