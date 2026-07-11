#include <cassert>
#include <cstdio>
#include <deque>
#include <memory>
#include <vector>

#include "magicpanel/engine.h"
#include "magicpanel/event.h"
#include "magicpanel/effects.h"
#include "magicpanel/liveness.h"
#include "magicpanel/reactions.h"
#include "magicpanel/scene.h"
#include "magicpanel/scenes.h"
#include "magicpanel/sprite.h"
#include "magicpanel/sprite_assets.h"

namespace {

void test_reactions() {
  magicpanel::AccumulatingStateStore store;
  magicpanel::ReactionEngine engine(
      {
          {"happy", magicpanel::ReactionKind::Transient, "tests_passed", 5.0f, "", "", ""},
          {"angry", magicpanel::ReactionKind::Sticky, "production_incident", 0.0f,
           "incident_resolved", "", ""},
          {"casting", magicpanel::ReactionKind::DurationBound, "deploy_started", 0.0f, "",
           "deploy_finished", ""},
          {"leaves", magicpanel::ReactionKind::Accumulating, "git_commit", 0.0f, "", "", "sprint_ended"},
      },
      &store);

  engine.handle_event("tests_passed");
  assert(engine.is_active("happy"));
  engine.tick(4.0f);
  assert(engine.is_active("happy"));
  engine.tick(1.1f);
  assert(!engine.is_active("happy"));

  engine.handle_event("production_incident");
  engine.tick(1000.0f);
  assert(engine.is_active("angry"));
  engine.handle_event("incident_resolved");
  assert(!engine.is_active("angry"));

  engine.handle_event("deploy_started");
  assert(engine.is_active("casting"));
  engine.handle_event("deploy_finished");
  assert(!engine.is_active("casting"));

  engine.handle_event("git_commit");
  engine.handle_event("git_commit");
  assert(engine.accumulated("leaves") == 2);
  engine.handle_event("sprint_ended");
  assert(engine.accumulated("leaves") == 0);
}

void test_events() {
  auto event = magicpanel::decode_event_line("{\"event\":\"switch_scene\",\"to\":\"arcane_tree\"}\n");
  assert(event);
  assert(event->name == "switch_scene");
  assert(event->field_or("to") == "arcane_tree");
  assert(!magicpanel::decode_event_line("not json"));
  assert(!magicpanel::decode_event_line("{\"no_event_field\":true}"));
}

void test_liveness() {
  magicpanel::LivenessTracker liveness(15.0f);
  assert(!liveness.is_connected(0.0f));
  liveness.mark_seen(10.0f);
  assert(liveness.is_connected(20.0f));
  assert(!liveness.is_connected(26.0f));
}

void test_scenes_render_nonblank() {
  magicpanel::AccumulatingStateStore state;
  magicpanel::LivenessTracker liveness;
  liveness.mark_seen(0.0f);
  magicpanel::FrameBufferCanvas canvas;
  auto scene = magicpanel::make_desk_spirit_scene(liveness, state);
  scene->render(canvas, 1.0f / 60.0f, 0.0f);
  bool nonblank = false;
  for (int y = 0; y < canvas.height(); ++y) {
    for (int x = 0; x < canvas.width(); ++x) {
      auto p = canvas.get_pixel(x, y);
      nonblank = nonblank || p.r != 10 || p.g != 10 || p.b != 20;
    }
  }
  assert(nonblank);
}

void test_scene_manager_switches_active_scene() {
  magicpanel::AccumulatingStateStore state;
  magicpanel::LivenessTracker liveness;
  std::vector<std::unique_ptr<magicpanel::Scene>> scenes;
  scenes.push_back(magicpanel::make_desk_spirit_scene(liveness, state));
  scenes.push_back(magicpanel::make_arcane_tree_scene());
  magicpanel::SceneManager manager(std::move(scenes), "desk_spirit");

  assert(manager.active_name() == "desk_spirit");
  manager.handle_event(magicpanel::Event{"switch_scene", {{"to", "arcane_tree"}}});
  assert(manager.active_name() == "arcane_tree");
  manager.handle_event(magicpanel::Event{"switch_scene", {{"to", "missing"}}});
  assert(manager.active_name() == "arcane_tree");
}

void test_sprite_draw_respects_alpha_mask() {
  magicpanel::FrameBufferCanvas canvas;
  canvas.clear(magicpanel::Color{1, 2, 3});
  magicpanel::draw_sprite(canvas, magicpanel::assets::kwizard_idle, 0, 0);

  auto corner = canvas.get_pixel(0, 0);
  assert(corner.r == 1 && corner.g == 2 && corner.b == 3);

  bool changed = false;
  for (int y = 0; y < canvas.height(); ++y) {
    for (int x = 0; x < canvas.width(); ++x) {
      auto pixel = canvas.get_pixel(x, y);
      changed = changed || pixel.r != 1 || pixel.g != 2 || pixel.b != 3;
    }
  }
  assert(changed);
}

void test_effect_systems_are_bounded_and_fade() {
  magicpanel::Rng rng(123);
  magicpanel::ParticleSystem<2> particles;
  particles.burst(4.0f, 4.0f, magicpanel::Color{255, 0, 0}, 10, 0.0f, 1.0f, rng);
  assert(particles.size() == 2);

  magicpanel::FrameBufferCanvas canvas;
  canvas.clear(magicpanel::kBlack);
  particles.tick(0.5f);
  particles.draw(canvas, magicpanel::kBlack);
  auto mid_fade = canvas.get_pixel(4, 4);
  assert(mid_fade.r > 0 && mid_fade.r < 255);

  particles.tick(0.6f);
  assert(particles.size() == 0);

  magicpanel::LightSystem<1> lights;
  assert(lights.add(magicpanel::Light{2.0f, 2.0f, 5.0f, 1.0f, magicpanel::Color{255, 255, 255}}));
  assert(!lights.add(magicpanel::Light{2.0f, 2.0f, 5.0f, 1.0f, magicpanel::Color{255, 255, 255}}));
  auto shaded = lights.shade(magicpanel::Color{10, 10, 10}, 2.0f, 2.0f);
  assert(shaded.r > 10 && shaded.g > 10 && shaded.b > 10);
}

class TestPlatform final : public magicpanel::Platform {
 public:
  explicit TestPlatform(std::deque<magicpanel::Event> events) : events_(std::move(events)) {}

  bool poll_event(magicpanel::Event& event) override {
    if (events_.empty()) {
      return false;
    }
    event = events_.front();
    events_.pop_front();
    return true;
  }

  float now_seconds() override {
    return now_;
  }

  void sleep_seconds(float seconds) override {
    now_ += seconds;
    slept_ = true;
  }

  bool slept() const { return slept_; }

 private:
  std::deque<magicpanel::Event> events_;
  float now_ = 0.0f;
  bool slept_ = false;
};

class OneFrameCanvas final : public magicpanel::FrameBufferCanvas {
 public:
  bool poll_quit() override {
    return presented_;
  }

  void present() override {
    presented_ = true;
    FrameBufferCanvas::present();
  }

 private:
  bool presented_ = false;
};

void test_engine_processes_events_and_stops_cleanly() {
  magicpanel::AccumulatingStateStore state;
  magicpanel::LivenessTracker liveness;
  std::vector<std::unique_ptr<magicpanel::Scene>> scenes;
  scenes.push_back(magicpanel::make_desk_spirit_scene(liveness, state));
  scenes.push_back(magicpanel::make_arcane_tree_scene());
  magicpanel::SceneManager manager(std::move(scenes), "desk_spirit");
  TestPlatform platform({magicpanel::Event{"switch_scene", {{"to", "arcane_tree"}}}});
  OneFrameCanvas canvas;

  magicpanel::run_engine(canvas, manager, liveness, platform, magicpanel::EngineConfig{60.0f});
  assert(manager.active_name() == "arcane_tree");
  assert(liveness.is_connected(0.0f));
  assert(platform.slept());
}

}  // namespace

int main() {
  test_reactions();
  test_events();
  test_liveness();
  test_scenes_render_nonblank();
  test_scene_manager_switches_active_scene();
  test_sprite_draw_respects_alpha_mask();
  test_effect_systems_are_bounded_and_fade();
  test_engine_processes_events_and_stops_cleanly();
  std::puts("magicpanel_cpp_tests passed");
}
