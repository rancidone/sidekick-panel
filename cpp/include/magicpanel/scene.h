#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "magicpanel/canvas.h"
#include "magicpanel/event.h"

namespace magicpanel {

class Scene {
 public:
  virtual ~Scene() = default;
  virtual std::string name() const = 0;
  virtual void handle_event(Event const& event) { (void)event; }
  virtual void render(Canvas& canvas, float dt, float now_seconds) = 0;
};

class SceneManager {
 public:
  SceneManager(std::vector<std::unique_ptr<Scene>> scenes, std::string initial);

  std::string active_name() const;
  bool empty() const;
  void switch_to(std::string const& name);
  void handle_event(Event const& event);
  void render(Canvas& canvas, float dt, float now_seconds);

 private:
  std::unordered_map<std::string, std::unique_ptr<Scene>> scenes_;
  std::string active_name_;
};

}  // namespace magicpanel
