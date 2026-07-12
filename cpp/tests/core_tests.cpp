#include <cassert>
#include <cstdio>
#include <deque>
#include <memory>
#include <vector>

#include "magicpanel/atmosphere.h"
#include "magicpanel/engine.h"
#include "magicpanel/event.h"
#include "magicpanel/effects.h"
#include "magicpanel/liveness.h"
#include "magicpanel/reactions.h"
#include "magicpanel/render_layers.h"
#include "magicpanel/scene.h"
#include "magicpanel/scenes.h"
#include "magicpanel/sprite.h"
#include "magicpanel/sprite_assets.h"
#include "magicpanel/status_panel.h"

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
  auto weather = magicpanel::decode_event_line(
      "{\"event\":\"weather\",\"clouds\":\"dense\",\"fog\":\"light\",\"daylight\":180,\"wind\":130}\n");
  assert(weather);
  assert(weather->field_or("clouds") == "dense");
  assert(weather->field_or("fog") == "light");
  assert(weather->field_or("daylight") == "180");
  assert(weather->field_or("wind") == "130");
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

void test_environment_applies_weather_payload() {
  magicpanel::EnvironmentState environment;
  environment.handle_event(magicpanel::Event{"weather",
                                             {{"sky", "day"},
                                              {"daylight", "190"},
                                              {"moon", "sun"},
                                              {"clouds", "dense"},
                                              {"fog", "light"},
                                              {"weather", "thunderstorm"},
                                              {"wind", "180"}}});

  magicpanel::AtmosphereConfig config{magicpanel::Color{10, 10, 20}};
  config = environment.apply(config);
  assert(config.sky == magicpanel::SkyMode::Storm);
  assert(config.daylight == 190);
  assert(config.moon == magicpanel::MoonMode::Sun);
  assert(config.cloud_cover == magicpanel::CloudCover::Dense);
  assert(config.fog == magicpanel::FogDensity::Light);
  assert(config.weather == magicpanel::WeatherMode::Thunderstorm);
  assert(config.wind == 180);
  assert(config.clouds);
}

void test_scenes_render_nonblank() {
  magicpanel::AccumulatingStateStore state;
  magicpanel::LivenessTracker liveness;
  magicpanel::EnvironmentState environment;
  liveness.mark_seen(0.0f);
  magicpanel::FrameBufferCanvas canvas;
  auto scene = magicpanel::make_desk_spirit_scene(liveness, state, environment);
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

void test_atmosphere_renders_reusable_layers() {
  magicpanel::FrameBufferCanvas canvas;
  magicpanel::Color background{10, 10, 20};
  canvas.clear(background);
  magicpanel::AtmosphereSystem atmosphere;
  magicpanel::AtmosphereConfig config{background};
  config.moon = magicpanel::MoonMode::BloodMoon;
  config.weather = magicpanel::WeatherMode::Ash;
  config.clouds = true;
  config.stars = true;
  config.grass = true;
  config.motes = true;
  config.wind = 120;
  atmosphere.render(canvas, 1.25f, config);

  bool sky_detail = false;
  bool ground_detail = false;
  for (int y = 0; y < canvas.height(); ++y) {
    for (int x = 0; x < canvas.width(); ++x) {
      auto pixel = canvas.get_pixel(x, y);
      bool changed = pixel.r != background.r || pixel.g != background.g || pixel.b != background.b;
      if (changed && y < 40) {
        sky_detail = true;
      }
      if (changed && y >= canvas.height() - 5) {
        ground_detail = true;
      }
    }
  }
  assert(sky_detail);
  assert(ground_detail);
}

void test_atmosphere_morphs_over_time() {
  magicpanel::Color background{10, 10, 20};
  magicpanel::AtmosphereSystem atmosphere;
  magicpanel::FrameBufferCanvas first;
  magicpanel::FrameBufferCanvas later;
  first.clear(background);
  later.clear(background);
  magicpanel::AtmosphereConfig config{background};
  config.moon = magicpanel::MoonMode::Moon;
  config.weather = magicpanel::WeatherMode::None;
  config.clouds = false;
  config.stars = true;
  config.grass = false;
  config.motes = true;
  config.wind = 90;
  atmosphere.render(first, 0.0f, config);
  atmosphere.render(later, 12.0f, config);

  // The horizon silhouette is a static rolling profile now (no animation),
  // so check the whole canvas rather than just the horizon band — motes and
  // star twinkle are still expected to change over time.
  int changed = 0;
  for (int y = 0; y < first.height(); ++y) {
    for (int x = 0; x < first.width(); ++x) {
      auto a = first.get_pixel(x, y);
      auto b = later.get_pixel(x, y);
      if (a.r != b.r || a.g != b.g || a.b != b.b) {
        ++changed;
      }
    }
  }
  assert(changed > 0);
}

void test_meadow_ecology_changes_over_long_time() {
  magicpanel::Color background{10, 10, 20};
  magicpanel::AtmosphereSystem atmosphere;
  magicpanel::FrameBufferCanvas first;
  magicpanel::FrameBufferCanvas later;
  first.clear(background);
  later.clear(background);

  magicpanel::AtmosphereConfig config{background};
  config.moon = magicpanel::MoonMode::None;
  config.clouds = false;
  config.stars = false;
  config.grass = true;
  config.motes = false;
  config.horizon_dither = true;
  config.wind = 90;

  atmosphere.render(first, 0.0f, config);
  atmosphere.render(later, 220.0f, config);

  int changed = 0;
  for (int y = 20; y < first.height(); ++y) {
    for (int x = 0; x < first.width(); ++x) {
      auto a = first.get_pixel(x, y);
      auto b = later.get_pixel(x, y);
      if (a.r != b.r || a.g != b.g || a.b != b.b) {
        ++changed;
      }
    }
  }
  assert(changed > 180);
}

void test_atmosphere_daylight_brightens_sky() {
  magicpanel::Color night{10, 10, 20};
  magicpanel::AtmosphereSystem atmosphere;
  magicpanel::FrameBufferCanvas night_canvas;
  magicpanel::FrameBufferCanvas day_canvas;

  magicpanel::AtmosphereConfig night_config{night};
  night_config.sky = magicpanel::SkyMode::Night;
  night_config.moon = magicpanel::MoonMode::None;
  night_config.stars = false;
  night_config.grass = false;
  night_config.motes = false;
  night_config.horizon_dither = false;

  magicpanel::AtmosphereConfig day_config{night};
  day_config.sky = magicpanel::SkyMode::Day;
  day_config.daylight = 255;
  day_config.moon = magicpanel::MoonMode::None;
  day_config.stars = false;
  day_config.grass = false;
  day_config.motes = false;
  day_config.horizon_dither = false;

  night_canvas.clear(night);
  day_canvas.clear(night);
  atmosphere.render(night_canvas, 0.0f, night_config);
  atmosphere.render(day_canvas, 0.0f, day_config);

  auto night_pixel = night_canvas.get_pixel(16, 4);
  auto day_pixel = day_canvas.get_pixel(16, 4);
  int night_luma = night_pixel.r + night_pixel.g + night_pixel.b;
  int day_luma = day_pixel.r + day_pixel.g + day_pixel.b;
  assert(day_luma > night_luma + 120);
}

void test_clouds_have_separate_dark_undersides() {
  magicpanel::Color background{82, 118, 128};
  magicpanel::AtmosphereSystem atmosphere;
  magicpanel::FrameBufferCanvas sky_only;
  magicpanel::FrameBufferCanvas cloudy;

  magicpanel::AtmosphereConfig base{background};
  base.sky = magicpanel::SkyMode::Day;
  base.daylight = 255;
  base.moon = magicpanel::MoonMode::None;
  base.stars = false;
  base.grass = false;
  base.motes = false;
  base.horizon_dither = false;

  magicpanel::AtmosphereConfig with_clouds = base;
  with_clouds.clouds = true;

  sky_only.clear(background);
  cloudy.clear(background);
  atmosphere.render(sky_only, 0.0f, base);
  atmosphere.render(cloudy, 0.0f, with_clouds);

  int shaded_columns = 0;
  for (int x = 0; x < cloudy.width(); ++x) {
    int top_y = -1;
    int bottom_y = -1;
    for (int y = 0; y < 30; ++y) {
      auto clear = sky_only.get_pixel(x, y);
      auto cloud = cloudy.get_pixel(x, y);
      bool cloud_pixel = clear.r != cloud.r || clear.g != cloud.g || clear.b != cloud.b;
      if (cloud_pixel) {
        if (top_y < 0) {
          top_y = y;
        }
        bottom_y = y;
      }
    }
    if (top_y >= 0 && bottom_y > top_y + 2) {
      auto top = cloudy.get_pixel(x, top_y);
      auto bottom = cloudy.get_pixel(x, bottom_y);
      int top_luma = top.r + top.g + top.b;
      int bottom_luma = bottom.r + bottom.g + bottom.b;
      if (bottom_luma + 20 < top_luma) {
        ++shaded_columns;
      }
    }
  }
  assert(shaded_columns > 4);
}

void test_dense_fog_avoids_vertical_posts() {
  magicpanel::Color background{24, 34, 44};
  magicpanel::AtmosphereSystem atmosphere;
  magicpanel::FrameBufferCanvas without_fog;
  magicpanel::FrameBufferCanvas with_fog;

  magicpanel::AtmosphereConfig config{background};
  config.sky = magicpanel::SkyMode::Storm;
  config.daylight = 50;
  config.moon = magicpanel::MoonMode::None;
  config.clouds = false;
  config.stars = false;
  config.grass = false;
  config.motes = false;
  config.horizon_dither = false;
  config.weather = magicpanel::WeatherMode::None;

  magicpanel::AtmosphereConfig fog_config = config;
  fog_config.fog = magicpanel::FogDensity::Dense;

  without_fog.clear(background);
  with_fog.clear(background);
  atmosphere.render(without_fog, 0.0f, config);
  atmosphere.render(with_fog, 0.0f, fog_config);

  int vertical_posts = 0;
  for (int x = 0; x < with_fog.width(); ++x) {
    int run = 0;
    for (int y = 18; y < 60; ++y) {
      auto a = without_fog.get_pixel(x, y);
      auto b = with_fog.get_pixel(x, y);
      bool changed = a.r != b.r || a.g != b.g || a.b != b.b;
      run = changed ? run + 1 : 0;
      if (run >= 5) {
        ++vertical_posts;
        break;
      }
    }
  }
  assert(vertical_posts < 4);
}

void test_tree_fire_hazard_draws_hot_pixels() {
  magicpanel::Color background{10, 10, 20};
  magicpanel::AtmosphereSystem atmosphere;
  magicpanel::FrameBufferCanvas canvas;

  magicpanel::AtmosphereConfig config{background};
  config.moon = magicpanel::MoonMode::None;
  config.clouds = false;
  config.stars = false;
  config.grass = true;
  config.motes = false;
  config.horizon_dither = true;
  config.fire_hazard_intensity = 1.0f;
  config.fire_hazard_x = static_cast<float>(canvas.width()) / 2.0f;

  canvas.clear(background);
  atmosphere.render(canvas, 6.0f, config);

  int hot_pixels = 0;
  for (int y = 18; y < 54; ++y) {
    for (int x = 0; x < canvas.width(); ++x) {
      auto pixel = canvas.get_pixel(x, y);
      if (pixel.r > 180 && pixel.g > 80 && pixel.g < 230 && pixel.b < 120) {
        ++hot_pixels;
      }
    }
  }
  assert(hot_pixels > 0);
}

void test_meadow_cycle_transitions_day_to_night() {
  magicpanel::FrameBufferCanvas night_canvas;
  magicpanel::FrameBufferCanvas day_canvas;
  magicpanel::EnvironmentState environment;
  auto scene = magicpanel::make_meadow_cycle_scene(environment);

  scene->render(night_canvas, 1.0f / 60.0f, 0.0f);
  scene->render(day_canvas, 1.0f / 60.0f, 17.0f);

  auto night_pixel = night_canvas.get_pixel(64, 3);
  auto day_pixel = day_canvas.get_pixel(64, 3);
  int night_luma = night_pixel.r + night_pixel.g + night_pixel.b;
  int day_luma = day_pixel.r + day_pixel.g + day_pixel.b;
  assert(day_luma > night_luma + 100);
}

void test_deploy_status_scroll_renders_progress() {
  magicpanel::FrameBufferCanvas canvas;
  canvas.clear(magicpanel::Color{10, 10, 20});

  magicpanel::DeployStatusPanel panel;
  panel.kind = magicpanel::DeployStatusKind::Deploy;
  panel.result = magicpanel::DeployStatusResult::Running;
  panel.open = 255;
  panel.progress = 160;
  magicpanel::draw_deploy_status_scroll(canvas, 58, 18, 1.0f, panel);

  auto paper = canvas.get_pixel(100, 24);
  auto progress = canvas.get_pixel(66, 33);
  assert(paper.r > 100 && paper.g > 70);
  assert(progress.r != 10 || progress.g != 10 || progress.b != 20);
}

void test_deploy_event_opens_status_panel() {
  magicpanel::AccumulatingStateStore state;
  magicpanel::LivenessTracker liveness;
  magicpanel::EnvironmentState environment;
  liveness.mark_seen(0.0f);
  magicpanel::FrameBufferCanvas canvas;
  auto scene = magicpanel::make_desk_spirit_scene(liveness, state, environment);

  scene->handle_event(magicpanel::Event{"deploy_started", {}});
  scene->render(canvas, 0.6f, 0.6f);
  auto open_panel = canvas.get_pixel(100, 24);
  assert(open_panel.r > 60 && open_panel.g > 35);

  scene->handle_event(magicpanel::Event{"deploy_finished", {}});
  canvas.clear(magicpanel::Color{10, 10, 20});
  scene->render(canvas, 0.2f, 0.8f);
  auto success_panel = canvas.get_pixel(100, 24);
  assert(success_panel.r > 60 && success_panel.g > 35);
}

void test_deploy_panel_stays_open_while_running() {
  magicpanel::AccumulatingStateStore state;
  magicpanel::LivenessTracker liveness;
  magicpanel::EnvironmentState environment;
  liveness.mark_seen(0.0f);
  magicpanel::FrameBufferCanvas canvas;
  auto scene = magicpanel::make_desk_spirit_scene(liveness, state, environment);

  scene->handle_event(magicpanel::Event{"deploy_started", {}});
  scene->render(canvas, 0.6f, 0.6f);
  auto visible = canvas.get_pixel(100, 24);
  assert(visible.r > 60 && visible.g > 35);

  scene->render(canvas, 300.0f, 300.6f);
  canvas.clear(magicpanel::Color{10, 10, 20});
  scene->render(canvas, 60.0f, 360.6f);
  auto still_visible = canvas.get_pixel(100, 24);
  assert(still_visible.r > 60 && still_visible.g > 35);
}

void test_push_success_animates_in() {
  magicpanel::AccumulatingStateStore state;
  magicpanel::LivenessTracker liveness;
  magicpanel::EnvironmentState environment;
  liveness.mark_seen(0.0f);
  magicpanel::FrameBufferCanvas canvas;
  auto scene = magicpanel::make_desk_spirit_scene(liveness, state, environment);

  scene->handle_event(magicpanel::Event{"push_succeeded", {}});
  scene->render(canvas, 0.25f, 0.25f);

  int success_pixels = 0;
  for (int y = 20; y < 55; ++y) {
    for (int x = 68; x < 105; ++x) {
      auto pixel = canvas.get_pixel(x, y);
      if (pixel.g > 160 && pixel.r < 180 && pixel.b < 180) {
        ++success_pixels;
      }
    }
  }
  assert(success_pixels > 4);
}

void test_bug_kill_sequence_loops() {
  magicpanel::AccumulatingStateStore state;
  magicpanel::LivenessTracker liveness;
  magicpanel::EnvironmentState environment;
  liveness.mark_seen(0.0f);
  magicpanel::FrameBufferCanvas canvas;
  auto scene = magicpanel::make_desk_spirit_scene(liveness, state, environment);
  scene->handle_event(magicpanel::Event{"bug_squashed", {}});

  scene->render(canvas, 2.8f, 2.8f);
  auto first_impact = canvas.get_pixel(105, 32);
  assert(first_impact.r != 10 || first_impact.g != 10 || first_impact.b != 20);

  canvas.clear(magicpanel::Color{10, 10, 20});
  scene->render(canvas, 4.1f, 6.9f);
  auto looped_impact = canvas.get_pixel(105, 32);
  assert(looped_impact.r != 10 || looped_impact.g != 10 || looped_impact.b != 20);
}

void test_bug_kill_can_be_stopped() {
  magicpanel::AccumulatingStateStore state;
  magicpanel::LivenessTracker liveness;
  magicpanel::EnvironmentState environment;
  liveness.mark_seen(0.0f);
  magicpanel::FrameBufferCanvas canvas;
  auto scene = magicpanel::make_desk_spirit_scene(liveness, state, environment);
  scene->handle_event(magicpanel::Event{"bug_squashed", {}});
  scene->render(canvas, 1.0f, 1.0f);

  scene->handle_event(magicpanel::Event{"bug_kill_stopped", {}});
  canvas.clear(magicpanel::Color{10, 10, 20});
  scene->render(canvas, 0.1f, 1.1f);
  auto exclaim_pixel = canvas.get_pixel(49, 6);
  assert(!(exclaim_pixel.r == 255 && exclaim_pixel.g == 225 && exclaim_pixel.b == 70));
}

void test_scene_manager_switches_active_scene() {
  magicpanel::AccumulatingStateStore state;
  magicpanel::LivenessTracker liveness;
  magicpanel::EnvironmentState environment;
  std::vector<std::unique_ptr<magicpanel::Scene>> scenes;
  scenes.push_back(magicpanel::make_desk_spirit_scene(liveness, state, environment));
  scenes.push_back(magicpanel::make_arcane_tree_scene());
  scenes.push_back(magicpanel::make_meadow_cycle_scene(environment));
  magicpanel::SceneManager manager(std::move(scenes), "desk_spirit", &environment);

  assert(manager.active_name() == "desk_spirit");
  manager.handle_event(magicpanel::Event{"switch_scene", {{"to", "arcane_tree"}}});
  assert(manager.active_name() == "arcane_tree");
  manager.handle_event(magicpanel::Event{"switch_scene", {{"to", "meadow_cycle"}}});
  assert(manager.active_name() == "meadow_cycle");
  manager.handle_event(magicpanel::Event{"switch_scene", {{"to", "missing"}}});
  assert(manager.active_name() == "meadow_cycle");
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

bool sprite_mask_bit(magicpanel::Sprite const& sprite, int x, int y) {
  int index = y * sprite.width + x;
  return (sprite.alpha_mask[index / 8] & (1u << (index % 8))) != 0;
}

std::pair<int, int> raised_arm_only_pixel() {
  for (int y = 30; y <= 44; ++y) {
    for (int x = 26; x <= 42; ++x) {
      if (sprite_mask_bit(magicpanel::assets::kwizard_cast_no_wand, x, y) &&
          !sprite_mask_bit(magicpanel::assets::kwizard_idle, x, y)) {
        return {x, y};
      }
    }
  }
  assert(false && "expected a cast-pose arm pixel that is absent from idle");
  return {0, 0};
}

void test_cast_no_wand_preserves_non_wand_pixels() {
  // This catches the too-broad rectangle mask that erased nearby hat pixels.
  assert(sprite_mask_bit(magicpanel::assets::kwizard_cast, 34, 35));
  assert(sprite_mask_bit(magicpanel::assets::kwizard_cast_no_wand, 34, 35));
  assert(sprite_mask_bit(magicpanel::assets::kwizard_cast, 46, 38));
  assert(!sprite_mask_bit(magicpanel::assets::kwizard_cast_no_wand, 46, 38));
}

void test_sprite_projected_shadow_uses_alpha_shape() {
  magicpanel::FrameBufferCanvas canvas;
  magicpanel::Color background{20, 30, 40};
  canvas.clear(background);
  magicpanel::draw_sprite_projected_shadow(canvas,
                                           magicpanel::assets::kwizard_idle,
                                           4,
                                           0,
                                           63,
                                           -5,
                                           magicpanel::Color{0, 0, 0},
                                           0.5f);

  bool darkened = false;
  for (int y = 52; y < canvas.height(); ++y) {
    for (int x = 0; x < canvas.width(); ++x) {
      auto pixel = canvas.get_pixel(x, y);
      darkened = darkened || pixel.r < background.r || pixel.g < background.g || pixel.b < background.b;
    }
  }
  assert(darkened);
  auto untouched = canvas.get_pixel(120, 10);
  assert(untouched.r == background.r && untouched.g == background.g && untouched.b == background.b);
}

void test_bug_kill_reveal_syncs_wand_before_arm_raise() {
  magicpanel::AccumulatingStateStore state;
  magicpanel::LivenessTracker liveness;
  magicpanel::EnvironmentState environment;
  liveness.mark_seen(0.0f);
  magicpanel::FrameBufferCanvas canvas;
  auto scene = magicpanel::make_desk_spirit_scene(liveness, state, environment);
  magicpanel::AccumulatingStateStore baseline_state;
  magicpanel::LivenessTracker baseline_liveness;
  magicpanel::EnvironmentState baseline_environment;
  baseline_liveness.mark_seen(0.0f);
  magicpanel::FrameBufferCanvas baseline_canvas;
  auto baseline_scene = magicpanel::make_desk_spirit_scene(baseline_liveness, baseline_state, baseline_environment);
  auto [sprite_x, sprite_y] = raised_arm_only_pixel();
  int canvas_x = sprite_x + 4;
  int canvas_y = sprite_y;

  baseline_scene->render(baseline_canvas, 1.22f, 1.22f);
  auto baseline = baseline_canvas.get_pixel(canvas_x, canvas_y);

  scene->handle_event(magicpanel::Event{"bug_squashed", {}});
  scene->render(canvas, 1.22f, 1.22f);
  auto early_reveal = canvas.get_pixel(canvas_x, canvas_y);
  assert(early_reveal.r == baseline.r && early_reveal.g == baseline.g && early_reveal.b == baseline.b);

  scene->render(canvas, 0.32f, 1.54f);
  auto raised = canvas.get_pixel(canvas_x, canvas_y);
  assert(raised.r != baseline.r || raised.g != baseline.g || raised.b != baseline.b);
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

  magicpanel::FrameBufferCanvas light_canvas;
  magicpanel::Color background{10, 10, 20};
  light_canvas.clear(background);
  light_canvas.set_pixel(2, 2, magicpanel::Color{20, 20, 30});
  lights.apply_except(light_canvas, background);
  auto untouched_background = light_canvas.get_pixel(0, 0);
  assert(untouched_background.r == background.r);
  assert(untouched_background.g == background.g);
  assert(untouched_background.b == background.b);
  auto lit_pixel = light_canvas.get_pixel(2, 2);
  assert(lit_pixel.r > 20);

  magicpanel::LightSystem<1> star_light;
  assert(star_light.add(magicpanel::Light{25.0f, 51.0f, 6.5f, 0.18f, magicpanel::Color{255, 226, 140}}));
  auto chin = star_light.shade(magicpanel::Color{112, 80, 88}, 29.0f, 43.0f);
  assert(chin.r == 112 && chin.g == 80 && chin.b == 88);
}

void test_layered_lighting_targets_selected_layers() {
  magicpanel::FrameBufferCanvas raw;
  magicpanel::LayeredCanvas canvas(raw);
  canvas.clear(magicpanel::Color{10, 10, 20});
  canvas.set_layer(magicpanel::RenderLayer::Atmosphere);
  canvas.set_pixel(1, 1, magicpanel::Color{20, 40, 20});
  canvas.set_layer(magicpanel::RenderLayer::Actor);
  canvas.set_pixel(2, 1, magicpanel::Color{20, 40, 20});

  magicpanel::LightSystem<1> lights;
  assert(lights.add(magicpanel::Light{2.0f, 1.0f, 4.0f, 1.0f, magicpanel::Color{255, 255, 255}}));
  lights.apply_to_layers(canvas, magicpanel::layer_mask(magicpanel::RenderLayer::Actor));

  auto atmosphere = raw.get_pixel(1, 1);
  assert(atmosphere.r == 20 && atmosphere.g == 40 && atmosphere.b == 20);
  auto actor = raw.get_pixel(2, 1);
  assert(actor.r > 20 && actor.g > 40 && actor.b > 20);
  assert(canvas.pixel_layer(2, 1) == magicpanel::RenderLayer::Actor);
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
  magicpanel::EnvironmentState environment;
  std::vector<std::unique_ptr<magicpanel::Scene>> scenes;
  scenes.push_back(magicpanel::make_desk_spirit_scene(liveness, state, environment));
  scenes.push_back(magicpanel::make_arcane_tree_scene());
  magicpanel::SceneManager manager(std::move(scenes), "desk_spirit", &environment);
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
  test_environment_applies_weather_payload();
  test_scenes_render_nonblank();
  test_atmosphere_renders_reusable_layers();
  test_atmosphere_morphs_over_time();
  test_meadow_ecology_changes_over_long_time();
  test_atmosphere_daylight_brightens_sky();
  test_clouds_have_separate_dark_undersides();
  test_dense_fog_avoids_vertical_posts();
  test_tree_fire_hazard_draws_hot_pixels();
  test_meadow_cycle_transitions_day_to_night();
  test_deploy_status_scroll_renders_progress();
  test_deploy_event_opens_status_panel();
  test_deploy_panel_stays_open_while_running();
  test_push_success_animates_in();
  test_bug_kill_sequence_loops();
  test_bug_kill_can_be_stopped();
  test_scene_manager_switches_active_scene();
  test_sprite_draw_respects_alpha_mask();
  test_cast_no_wand_preserves_non_wand_pixels();
  test_sprite_projected_shadow_uses_alpha_shape();
  test_bug_kill_reveal_syncs_wand_before_arm_raise();
  test_effect_systems_are_bounded_and_fade();
  test_layered_lighting_targets_selected_layers();
  test_engine_processes_events_and_stops_cleanly();
  std::puts("magicpanel_cpp_tests passed");
}
