#include "magicpanel/scenes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "magicpanel/atmosphere.h"
#include "magicpanel/drawing.h"
#include "magicpanel/effects.h"
#include "magicpanel/render_layers.h"
#include "magicpanel/sprite.h"
#include "magicpanel/sprite_assets.h"
#include "magicpanel/status_panel.h"

namespace magicpanel {
namespace {

struct Point {
  float x;
  float y;
};

constexpr Color kDeskBackground{10, 10, 20};
constexpr Color kStar{255, 226, 140};
constexpr Color kAngryTint{255, 40, 40};
constexpr Color kCastTint{255, 120, 30};
constexpr Color kSleepTint{40, 40, 80};
constexpr float kBlinkHoldSeconds = 0.08f;
constexpr float kStarHoldSeconds = 0.35f;
constexpr float kHappyJumpStepSeconds = 0.09f;
constexpr float kBugAnnounceSeconds = 1.2f;
constexpr float kBugCastSeconds = 0.5f;
constexpr float kBugTravelSeconds = 0.9f;
constexpr float kBugImpactSeconds = 0.5f;
constexpr float kBugPayoffSeconds = 1.0f;
constexpr float kBugTotalSeconds =
    kBugAnnounceSeconds + kBugCastSeconds + kBugTravelSeconds + kBugImpactSeconds + kBugPayoffSeconds;
constexpr int kWizardLeftMargin = 4;
constexpr int kWizardGroundY = 63;
constexpr Point kStarGlowCenter{static_cast<float>(kWizardLeftMargin + 21), 51.0f};
constexpr float kStarGlowRadius = 6.5f;
constexpr float kWandRevealFraction = 0.5f;
constexpr float kWandRevealStartAngle = 1.7f;
constexpr float kWandBaseHalfWidth = 1.3f;
constexpr float kWandTipHalfWidth = 0.4f;
constexpr float kCometMinSize = 1.0f;
constexpr float kCometMaxSize = 4.6f;
constexpr float kCometTailBaseLength = 3.0f;
constexpr float kCometTailProgressLength = 8.4f;
constexpr float kCometHue = 0.07f;

constexpr Color kWandOutline{16, 22, 74};
constexpr Color kWandCore{121, 69, 27};
constexpr Color kWandShadow{74, 42, 16};
constexpr Color kCastCool{255, 250, 235};
constexpr Color kCastHot{255, 80, 30};

struct MissileSpec {
  enum class Kind { Straight, Arc, Wave };
  Kind kind;
  float amplitude;
  float stagger;
  int cycles;
  float hue;
};

constexpr std::array<MissileSpec, 3> kMissileSpecs{{
    {MissileSpec::Kind::Straight, 0.0f, 0.0f, 0, 0.58f},
    {MissileSpec::Kind::Arc, 22.0f, 0.09f, 0, 0.76f},
    {MissileSpec::Kind::Wave, 7.0f, 0.18f, 1, 0.86f},
}};

std::vector<ReactionRule> desk_rules() {
  std::vector<ReactionRule> rules;
  rules.push_back(ReactionRule{"happy", ReactionKind::Transient, "git_commit", 2.0f, "", "", ""});
  rules.push_back(
      ReactionRule{"happy", ReactionKind::Transient, "tests_passed", 4.0f, "", "", ""});
  rules.push_back(
      ReactionRule{"happy", ReactionKind::Transient, "build_passed", 4.0f, "", "", ""});
  rules.push_back(
      ReactionRule{"push_success", ReactionKind::Transient, "push_succeeded", 4.0f, "", "", ""});
  rules.push_back(
      ReactionRule{"push_success", ReactionKind::Transient, "git_push_succeeded", 4.0f, "", "", ""});
  rules.push_back(
      ReactionRule{"push_success", ReactionKind::Transient, "push_success", 4.0f, "", "", ""});
  rules.push_back(
      ReactionRule{"bug_kill", ReactionKind::Sticky, "bug_squashed", 0.0f, "bug_kill_stopped", "", ""});
  rules.push_back(ReactionRule{"angry", ReactionKind::Sticky, "production_incident", 0.0f,
                                "incident_resolved", "", ""});
  rules.push_back(ReactionRule{"casting_spells", ReactionKind::DurationBound, "deploy_started",
                                0.0f, "deploy_failed", "deploy_finished", ""});
  rules.push_back(ReactionRule{"casting_spells", ReactionKind::DurationBound, "ci_build_started",
                                0.0f, "ci_build_failed", "ci_build_finished", ""});
  return rules;
}

class DeskSpiritScene final : public Scene {
 public:
  DeskSpiritScene(LivenessTracker& liveness, AccumulatingStateStore& state, EnvironmentState& environment)
      : liveness_(liveness), environment_(environment), reactions_(desk_rules(), &state) {}

  std::string name() const override { return "desk_spirit"; }

  void handle_event(Event const& event) override {
    reactions_.handle_event(event.name);
    if (event.name == "bug_squashed") {
      bug_elapsed_ = 0.0f;
      impact_spawned_ = false;
      bug_phase_ = BugPhase::None;
      shot_index_++;
    } else if (event.name == "push_succeeded" || event.name == "git_push_succeeded" ||
               event.name == "push_success") {
      push_elapsed_ = 0.0f;
    } else if (event.name == "deploy_started" || event.name == "ci_build_started") {
      deploy_visible_ = true;
      deploy_elapsed_ = 0.0f;
      deploy_done_elapsed_ = 0.0f;
      deploy_kind_ = event.name == "ci_build_started" ? DeployStatusKind::Build : DeployStatusKind::Deploy;
      deploy_result_ = DeployStatusResult::Running;
    } else if (event.name == "deploy_finished" || event.name == "ci_build_finished") {
      deploy_visible_ = true;
      deploy_done_elapsed_ = 0.0f;
      deploy_result_ = DeployStatusResult::Success;
    } else if (event.name == "deploy_failed" || event.name == "ci_build_failed") {
      deploy_visible_ = true;
      deploy_done_elapsed_ = 0.0f;
      deploy_result_ = DeployStatusResult::Failed;
    }
  }

  void render(Canvas& canvas, float dt, float now_seconds) override {
    reactions_.tick(dt);
    particles_.tick(dt);
    idle_elapsed_ += dt;
    push_elapsed_ += dt;
    fireball_ignite_timer_ = std::max(0.0f, fireball_ignite_timer_ - dt);
    tick_deploy_panel(dt);
    LayeredCanvas frame(canvas);
    frame.clear(kDeskBackground);

    std::string mood = current_mood(now_seconds);
    AtmosphereConfig current_atmosphere = atmosphere_config(mood);
    {
      ScopedRenderLayer layer(frame, RenderLayer::Atmosphere);
      atmosphere_.render(frame, now_seconds, current_atmosphere);
    }
    if (mood == "bug_kill") {
      float previous = bug_elapsed_;
      bug_elapsed_ = std::fmod(bug_elapsed_ + dt, kBugTotalSeconds);
      if (bug_elapsed_ < previous) {
        bug_phase_ = BugPhase::None;
        impact_spawned_ = false;
        shot_index_++;
      }
    } else {
      bug_elapsed_ = kBugTotalSeconds + 1.0f;
      impact_spawned_ = false;
      bug_phase_ = BugPhase::None;
    }
    Color tint{255, 255, 255};
    float tint_strength = 0.0f;
    if (mood == "angry") {
      tint = kAngryTint;
      tint_strength = 0.5f;
    } else if (mood == "casting_spells") {
      tint = kCastTint;
      tint_strength = 0.18f;
    } else if (mood == "sleeping") {
      tint = kSleepTint;
      tint_strength = 0.55f;
    }

    BugPhase render_bug_phase = mood == "bug_kill" ? phase_for_elapsed(bug_elapsed_) : BugPhase::None;
    int hop = mood == "happy" || mood == "push_success" ? happy_jump_offset() : 0;
    Sprite const* wizard_sprite = wizard_sprite_for(mood, render_bug_phase);
    {
      ScopedRenderLayer layer(frame, RenderLayer::Atmosphere);
      draw_sprite_projected_shadow(frame, *wizard_sprite, 4, hop, kWizardGroundY, -5, Color{0, 7, 14}, 0.34f);
    }
    {
      ScopedRenderLayer layer(frame, RenderLayer::Actor);
      draw_sprite(frame, *wizard_sprite, 4, hop, tint, tint_strength);
    }

    local_lights_.clear();
    world_lights_.clear();
    // Ambient moon/starlight glow — fades out with daylight so it doesn't
    // persist as a stray blue tint over everything at noon.
    float night_amount = 1.0f - static_cast<float>(current_atmosphere.daylight) / 255.0f;
    world_lights_.add(Light{108.0f, 9.0f, 90.0f, (mood == "sleeping" ? 0.12f : 0.18f) * night_amount,
                            Color{126, 146, 170}});
    local_lights_.add(Light{32.0f, 27.0f, 70.0f, (mood == "sleeping" ? 0.08f : 0.13f) * night_amount,
                            Color{118, 136, 160}});
    {
      ScopedRenderLayer layer(frame, RenderLayer::Effect);
      if (mood == "casting_spells") {
        Point wand_tip{static_cast<float>(kWizardLeftMargin + 48), 37.0f};
        Point wand_hand{static_cast<float>(kWizardLeftMargin + 38), 41.0f};
        float charge = 0.55f + 0.35f * (0.5f + 0.5f * std::sin(now_seconds * 5.0f));
        draw_wand_line(frame, wand_hand, wand_tip, Color{210, 144, 58}, charge * 0.18f);
        local_lights_.add(Light{wand_tip.x, wand_tip.y, 19.0f, 0.12f, Color{226, 168, 88}});
        world_lights_.add(Light{wand_tip.x, wand_tip.y, 18.0f, 0.06f, Color{210, 132, 62}});
        draw_wand_shimmer(frame, wand_tip, now_seconds, charge * 0.38f);
        spawn_trail(wand_tip, Color{255, 200, 80});
      } else if (mood == "bug_kill") {
        draw_bug_kill(frame, now_seconds);
      }
      if (mood == "push_success") {
        draw_push_success(frame, now_seconds);
      }
      if (deploy_visible_) {
        DeployStatusPanel panel;
        panel.kind = deploy_kind_;
        panel.result = deploy_result_;
        panel.open = deploy_open();
        panel.progress = deploy_progress();
        draw_deploy_status_scroll(frame, 58, 18, now_seconds, panel);
      }
    }
    if (mood == "baseline") {
      float star_strength = idle_star_glow_strength();
      if (star_strength > 0.0f) {
        local_lights_.add(Light{kStarGlowCenter.x, kStarGlowCenter.y, kStarGlowRadius, star_strength, kStar});
      }
    }
    world_lights_.apply_to_layers(frame, layer_mask(RenderLayer::Atmosphere));
    local_lights_.apply_to_layers(frame, layer_mask(RenderLayer::Actor) | layer_mask(RenderLayer::Effect));
    {
      ScopedRenderLayer layer(frame, RenderLayer::Effect);
      particles_.draw(frame, kDeskBackground);
    }
  }

 private:
  enum class BugPhase { None, Announce, Cast, Travel, Impact, Payoff };

  AtmosphereConfig atmosphere_config(std::string const& mood) const {
    AtmosphereConfig config{kDeskBackground};
    config.sky = SkyMode::Night;
    config.stars = true;
    config.clouds = true;
    config.cloud_cover = CloudCover::Light;
    config.grass = true;
    config.horizon_dither = true;
    config.fire_hazard_intensity = std::clamp(fireball_ignite_timer_ / 7.0f, 0.0f, 1.0f);
    config.fire_hazard_x = fireball_ignite_x_;
    config.wind = mood == "bug_kill" ? 145 : 70;
    if (mood == "angry") {
      config.moon = MoonMode::BloodMoon;
      config.weather = WeatherMode::Ash;
      config.motes = false;
    } else if (mood == "bug_kill") {
      config.moon = MoonMode::Moon;
      config.weather = WeatherMode::Ash;
      config.motes = true;
    } else if (mood == "sleeping") {
      config.moon = MoonMode::Moon;
      config.weather = WeatherMode::None;
      config.motes = false;
      config.wind = 20;
    }
    return environment_.apply(config);
  }

  std::string current_mood(float now_seconds) const {
    if (!liveness_.is_connected(now_seconds)) {
      return "sleeping";
    }
    for (auto const& mood : {"angry", "casting_spells", "bug_kill", "push_success", "happy"}) {
      if (reactions_.is_active(mood)) {
        return mood;
      }
    }
    return "baseline";
  }

  Sprite const* wizard_sprite_for(std::string const& mood, BugPhase render_bug_phase) {
    Sprite const* sprite = &assets::kwizard_idle;
    if (mood == "happy" || mood == "push_success") {
      sprite = happy_sprite();
    } else if (mood == "bug_kill") {
      if (render_bug_phase == BugPhase::Payoff) {
        sprite = happy_sprite();
      } else if (render_bug_phase == BugPhase::Announce || render_bug_phase == BugPhase::None ||
                 (render_bug_phase == BugPhase::Cast && bug_cast_charge() < kWandRevealFraction)) {
        sprite = &assets::kwizard_idle;
      } else {
        sprite = &assets::kwizard_cast_no_wand;
      }
    } else if (mood == "casting_spells") {
      sprite = &assets::kwizard_cast_no_wand;
    } else {
      sprite = idle_sprite();
    }
    return sprite;
  }

  Sprite const* idle_sprite() {
    // Deterministic approximation of Python's randomized IdleAnimator: long
    // base holds, brief blink, and a regular chest-star pulse.
    float cycle = std::fmod(idle_elapsed_, 8.0f);
    if (cycle < kBlinkHoldSeconds) {
      return &assets::kwizard_blink_1;
    }
    if (cycle < kBlinkHoldSeconds * 2.0f) {
      return &assets::kwizard_blink_2;
    }

    float star_cycle = std::fmod(idle_elapsed_ + 1.3f, 5.8f);
    if (star_cycle < kStarHoldSeconds) {
      return &assets::kwizard_star_1;
    }
    if (star_cycle < kStarHoldSeconds * 2.0f) {
      return &assets::kwizard_star_2;
    }
    if (star_cycle < kStarHoldSeconds * 3.0f) {
      return &assets::kwizard_star_3;
    }
    if (star_cycle < kStarHoldSeconds * 4.0f) {
      return &assets::kwizard_star_4;
    }
    return &assets::kwizard_idle;
  }

  float idle_star_glow_strength() const {
    float star_cycle = std::fmod(idle_elapsed_ + 1.3f, 5.8f);
    if (star_cycle < kStarHoldSeconds) {
      return 0.08f;
    }
    if (star_cycle < kStarHoldSeconds * 2.0f) {
      return 0.18f;
    }
    if (star_cycle < kStarHoldSeconds * 3.0f) {
      return 0.18f;
    }
    if (star_cycle < kStarHoldSeconds * 4.0f) {
      return 0.08f;
    }
    return 0.0f;
  }

  Sprite const* happy_sprite() const {
    int step = static_cast<int>(idle_elapsed_ / kHappyJumpStepSeconds) % 5;
    if (step == 1 || step == 3) {
      return &assets::kwizard_jump;
    }
    if (step == 2) {
      return &assets::kwizard_excite;
    }
    return &assets::kwizard_idle;
  }

  int happy_jump_offset() const {
    constexpr int offsets[5] = {0, -1, -2, -1, 0};
    int step = static_cast<int>(idle_elapsed_ / kHappyJumpStepSeconds) % 5;
    return offsets[step];
  }

  static Color fade(Color color, float strength) {
    strength = std::clamp(strength, 0.0f, 1.0f);
    return Color{static_cast<std::uint8_t>(static_cast<float>(color.r) * strength),
                 static_cast<std::uint8_t>(static_cast<float>(color.g) * strength),
                 static_cast<std::uint8_t>(static_cast<float>(color.b) * strength)};
  }

  void draw_push_success(Canvas& canvas, float now_seconds) {
    float progress = std::clamp(push_elapsed_ / 4.0f, 0.0f, 1.0f);
    int base_x = 78;
    int base_y = 43 - static_cast<int>(progress * 16.0f);
    Color green{80, 220, 112};
    Color mint{160, 240, 180};
    Color gold{244, 196, 70};

    int arrow_y = base_y + static_cast<int>(std::sin(now_seconds * 10.0f) * 1.4f);
    draw::line(canvas, base_x, arrow_y + 9, base_x, arrow_y + 2, green);
    draw::line(canvas, base_x, arrow_y + 2, base_x - 3, arrow_y + 5, mint);
    draw::line(canvas, base_x, arrow_y + 2, base_x + 3, arrow_y + 5, mint);
    draw::line(canvas, base_x + 8, arrow_y + 8, base_x + 11, arrow_y + 11, green);
    draw::line(canvas, base_x + 11, arrow_y + 11, base_x + 17, arrow_y + 3, mint);

    for (int i = 0; i < 8; ++i) {
      float seed = static_cast<float>(i) * 1.618f;
      float angle = seed + progress * 3.8f;
      float radius = 4.0f + progress * (9.0f + static_cast<float>(i % 3));
      int x = base_x + static_cast<int>(std::round(std::cos(angle) * radius));
      int y = arrow_y + 5 + static_cast<int>(std::round(std::sin(angle) * radius * 0.55f));
      Color color = (i % 3) == 0 ? gold : (i & 1) == 0 ? green : mint;
      canvas.set_pixel(x, y, color);
      if (progress < 0.55f && (i & 1) == 0) {
        canvas.set_pixel(x, y + 1, fade(color, 0.55f));
      }
    }

    if (progress < 0.25f) {
      particles_.burst(static_cast<float>(base_x), static_cast<float>(arrow_y + 6),
                       Color{120, 230, 140}, 4, 18.0f, 0.45f, rng_);
    }
    local_lights_.add(Light{static_cast<float>(base_x + 5),
                            static_cast<float>(arrow_y + 5),
                            28.0f,
                            0.18f * (1.0f - progress * 0.35f),
                            Color{120, 230, 140}});
  }

  void draw_bug_kill(Canvas& canvas, float now_seconds) {
    float announce_end = kBugAnnounceSeconds;
    float cast_end = announce_end + kBugCastSeconds;
    float travel_end = cast_end + kBugTravelSeconds;
    Point wand_tip{static_cast<float>(kWizardLeftMargin + 48), 38.0f};
    Point hand_rest{static_cast<float>(kWizardLeftMargin + 37), 50.0f};
    Point hand_cast{static_cast<float>(kWizardLeftMargin + 38), 41.0f};
    int bug_x = canvas.width() - assets::kbug.width - 8;
    int bug_y = 18;
    Point bug_center{bug_x + assets::kbug.width / 2.0f, bug_y + assets::kbug.height / 2.0f};
    float travel_dx = bug_center.x - wand_tip.x;
    float travel_dy = bug_center.y - wand_tip.y;
    float travel_len = std::max(1.0f, std::hypot(travel_dx, travel_dy));
    Point perp{-travel_dy / travel_len, travel_dx / travel_len};

    BugPhase phase = phase_for_elapsed(bug_elapsed_);
    if (phase != bug_phase_) {
      enter_bug_phase(phase, wand_tip, bug_center);
      bug_phase_ = phase;
    }

    bool show_bug = bug_elapsed_ < travel_end;
    if (show_bug) {
      int jiggle_x = bug_elapsed_ < announce_end
                         ? static_cast<int>(std::round(std::sin(now_seconds * 16.0f) * 1.6f))
                         : 0;
      int jiggle_y = bug_elapsed_ < announce_end
                         ? static_cast<int>(std::round(std::cos(now_seconds * 11.0f)))
                         : 0;
      draw_sprite(canvas, assets::kbug, bug_x + jiggle_x, bug_y + jiggle_y);
    }

    if (phase == BugPhase::Announce) {
      if (bug_elapsed_ < 0.6f) {
        draw_exclaim(canvas, kWizardLeftMargin + 44, 6, std::min(1.0f, bug_elapsed_ / 0.15f));
      }
      return;
    }

    float cast_charge = (bug_elapsed_ - announce_end) / kBugCastSeconds;
    if (phase == BugPhase::Cast && cast_charge < kWandRevealFraction) {
      float reveal_progress = cast_charge / kWandRevealFraction;
      float full_dx = wand_tip.x - hand_rest.x;
      float full_dy = wand_tip.y - hand_rest.y;
      float final_angle = std::atan2(full_dy, full_dx);
      float final_length = std::hypot(full_dx, full_dy);
      float angle = kWandRevealStartAngle + (final_angle - kWandRevealStartAngle) * reveal_progress;
      Point tip{hand_rest.x + std::cos(angle) * final_length * reveal_progress,
                hand_rest.y + std::sin(angle) * final_length * reveal_progress};
      draw_wand_line(canvas, hand_rest, tip, kCastHot, cast_charge);
      draw_wand_shimmer(canvas, tip, now_seconds, reveal_progress);
      return;
    }

    if (phase == BugPhase::Cast) {
      float post_reveal = (cast_charge - kWandRevealFraction) / (1.0f - kWandRevealFraction);
      draw_wand_line(canvas, hand_cast, wand_tip, kCastHot, std::max(0.0f, cast_charge));
      draw_wand_shimmer(canvas, wand_tip, now_seconds, std::clamp(post_reveal, 0.0f, 1.0f));
      spawn_trail(wand_tip, Color{255, 200, 80});
      local_lights_.add(Light{wand_tip.x, wand_tip.y, 34.0f, 0.8f * std::clamp(cast_charge, 0.0f, 1.0f),
                              lerp(kCastCool, kCastHot, std::clamp(cast_charge, 0.0f, 1.0f))});
      world_lights_.add(Light{wand_tip.x, wand_tip.y, 28.0f, 0.18f * std::clamp(cast_charge, 0.0f, 1.0f),
                              Color{255, 130, 48}});
      return;
    }

    if (phase == BugPhase::Travel) {
      float progress = std::clamp((bug_elapsed_ - cast_end) / kBugTravelSeconds, 0.0f, 1.0f);
      bool missiles = (shot_index_ % 2) == 1;
      if (missiles) {
        for (auto const& spec : kMissileSpecs) {
          float missile_progress = std::clamp((bug_elapsed_ - cast_end - spec.stagger) / kBugTravelSeconds,
                                              0.0f, 1.0f);
          if (missile_progress <= 0.0f || missile_progress >= 1.0f) {
            continue;
          }
          Point p = missile_position(spec, missile_progress, wand_tip, travel_dx, travel_dy, perp);
          Point ahead = missile_position(spec, missile_progress + 0.02f, wand_tip, travel_dx, travel_dy, perp);
          draw_missile_bolt(canvas, p, std::atan2(ahead.y - p.y, ahead.x - p.x), spec.hue);
          spawn_trail(p, hsv(spec.hue, 0.9f, 0.65f));
          world_lights_.add(Light{p.x, p.y, 20.0f, 0.18f, hsv(spec.hue, 0.75f, 0.95f)});
        }
      } else {
        MissileSpec spec = single_curve_spec();
        Point p = missile_position(spec, progress, wand_tip, travel_dx, travel_dy, perp);
        Point ahead = missile_position(spec, progress + 0.02f, wand_tip, travel_dx, travel_dy, perp);
        float heading = std::atan2(ahead.y - p.y, ahead.x - p.x);
        draw_comet(canvas, p, heading, progress);
        spawn_trail(p, Color{255, 165, 50});
        spawn_plume(p, heading);
        local_lights_.add(Light{p.x, p.y, 22.0f, 0.5f, Color{255, 120, 30}});
        float world_strength = 0.18f + 0.22f * progress;
        world_lights_.add(Light{p.x, p.y, 38.0f, world_strength, Color{255, 115, 32}});
      }
      return;
    }

    if (phase == BugPhase::Impact) {
      float impact_progress = std::clamp((bug_elapsed_ - travel_end) / kBugImpactSeconds, 0.0f, 1.0f);
      float flash_strength = 1.0f - impact_progress;
      float impact_heading = std::atan2(travel_dy, travel_dx);
      world_lights_.add(Light{bug_center.x, bug_center.y, 42.0f, 0.45f * flash_strength, Color{255, 130, 42}});
      local_lights_.add(Light{bug_center.x, bug_center.y, 24.0f, 0.55f * flash_strength, Color{255, 190, 95}});
      draw_impact_flash(canvas, bug_center, impact_progress, impact_heading);
      if (!impact_spawned_) {
        particles_.burst(bug_center.x, bug_center.y, Color{255, 245, 190}, 20, 82.0f, 0.16f, rng_);
        particles_.burst(bug_center.x, bug_center.y, Color{255, 150, 40}, 38, 64.0f, 0.52f, rng_,
                         110.0f, 2.2f);
        particles_.burst(bug_center.x, bug_center.y, Color{165, 55, 20}, 22, 42.0f, 0.78f, rng_,
                         75.0f, 1.35f);
        spawn_momentum_debris(bug_center, impact_heading);
        spawn_smoke(bug_center, 18);
        fireball_ignite_timer_ = 7.0f;
        fireball_ignite_x_ = bug_center.x;
        impact_spawned_ = true;
      }
    } else {
      impact_spawned_ = false;
    }
  }

  float bug_cast_charge() const {
    return std::clamp((bug_elapsed_ - kBugAnnounceSeconds) / kBugCastSeconds, 0.0f, 1.0f);
  }

  void tick_deploy_panel(float dt) {
    if (!deploy_visible_) {
      return;
    }
    deploy_elapsed_ += dt;
    if (deploy_result_ == DeployStatusResult::Running) {
      return;
    }
    deploy_done_elapsed_ += dt;
    if (deploy_done_elapsed_ > 2.5f) {
      deploy_visible_ = false;
    }
  }

  std::uint8_t deploy_open() const {
    float open = std::clamp(deploy_elapsed_ / 0.55f, 0.0f, 1.0f);
    if (deploy_result_ == DeployStatusResult::Running || deploy_done_elapsed_ < 1.7f) {
      return static_cast<std::uint8_t>(open * 255.0f);
    }
    float close = std::clamp((deploy_done_elapsed_ - 1.7f) / 0.8f, 0.0f, 1.0f);
    return static_cast<std::uint8_t>(open * (1.0f - close) * 255.0f);
  }

  std::uint8_t deploy_progress() const {
    if (deploy_result_ != DeployStatusResult::Running) {
      return 255;
    }
    float progress = 0.08f + 0.88f * std::clamp(deploy_elapsed_ / 7.0f, 0.0f, 1.0f);
    return static_cast<std::uint8_t>(progress * 255.0f);
  }

  BugPhase phase_for_elapsed(float elapsed) const {
    float cast_end = kBugAnnounceSeconds + kBugCastSeconds;
    float travel_end = cast_end + kBugTravelSeconds;
    float impact_end = travel_end + kBugImpactSeconds;
    if (elapsed < kBugAnnounceSeconds) {
      return BugPhase::Announce;
    }
    if (elapsed < cast_end) {
      return BugPhase::Cast;
    }
    if (elapsed < travel_end) {
      return BugPhase::Travel;
    }
    if (elapsed < impact_end) {
      return BugPhase::Impact;
    }
    return BugPhase::Payoff;
  }

  void enter_bug_phase(BugPhase phase, Point wand_tip, Point bug_center) {
    if (phase == BugPhase::Announce) {
      particles_.burst(bug_center.x, bug_center.y, Color{80, 220, 90}, 10, 18.0f, 0.4f, rng_);
    } else if (phase == BugPhase::Travel) {
      particles_.burst(wand_tip.x, wand_tip.y, Color{255, 250, 210}, 8, 20.0f, 0.3f, rng_);
    } else if (phase == BugPhase::Impact) {
      impact_spawned_ = false;
    }
  }

  static Color lerp(Color a, Color b, float t) {
    return blend(a, b, std::clamp(t, 0.0f, 1.0f));
  }

  static Color hsv(float hue, float saturation, float value) {
    hue = hue - std::floor(hue);
    saturation = std::clamp(saturation, 0.0f, 1.0f);
    value = std::clamp(value, 0.0f, 1.0f);
    float c = value * saturation;
    float h = hue * 6.0f;
    float x = c * (1.0f - std::fabs(std::fmod(h, 2.0f) - 1.0f));
    float m = value - c;
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    if (h < 1.0f) {
      r = c;
      g = x;
    } else if (h < 2.0f) {
      r = x;
      g = c;
    } else if (h < 3.0f) {
      g = c;
      b = x;
    } else if (h < 4.0f) {
      g = x;
      b = c;
    } else if (h < 5.0f) {
      r = x;
      b = c;
    } else {
      r = c;
      b = x;
    }
    return Color{static_cast<std::uint8_t>((r + m) * 255.0f),
                 static_cast<std::uint8_t>((g + m) * 255.0f),
                 static_cast<std::uint8_t>((b + m) * 255.0f)};
  }

  static float missile_offset(MissileSpec const& spec, float progress) {
    progress = std::clamp(progress, 0.0f, 1.0f);
    if (spec.kind == MissileSpec::Kind::Straight) {
      return 0.0f;
    }
    if (spec.kind == MissileSpec::Kind::Arc) {
      return -spec.amplitude * std::sin(3.1415926535f * progress);
    }
    return spec.amplitude * std::sin(progress * 2.0f * 3.1415926535f * spec.cycles) *
           std::sin(3.1415926535f * progress);
  }

  static Point missile_position(MissileSpec const& spec,
                                float progress,
                                Point wand_tip,
                                float travel_dx,
                                float travel_dy,
                                Point perp) {
    progress = std::clamp(progress, 0.0f, 1.0f);
    float offset = missile_offset(spec, progress);
    return Point{wand_tip.x + travel_dx * progress + perp.x * offset,
                 wand_tip.y + travel_dy * progress + perp.y * offset};
  }

  MissileSpec single_curve_spec() const {
    switch (shot_index_ % 3) {
      case 1:
        return MissileSpec{MissileSpec::Kind::Arc, 12.0f, 0.0f, 0, kCometHue};
      case 2:
        return MissileSpec{MissileSpec::Kind::Wave, 6.0f, 0.0f, 1, kCometHue};
      default:
        return MissileSpec{MissileSpec::Kind::Straight, 0.0f, 0.0f, 0, kCometHue};
    }
  }

  void draw_wand_line(Canvas& canvas, Point start, Point end, Color glow_tint, float glow_strength) {
    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float length = std::hypot(dx, dy);
    if (length < 0.01f) {
      return;
    }
    float fx = dx / length;
    float fy = dy / length;
    float px = -fy;
    float py = fx;
    int u_max = std::max(1, static_cast<int>(std::round(length)));
    Color outline = lerp(kWandOutline, glow_tint, glow_strength);
    for (int u = 0; u <= u_max; ++u) {
      float t = static_cast<float>(u) / static_cast<float>(u_max);
      Color core = lerp(lerp(kWandCore, kWandShadow, t), glow_tint, glow_strength);
      float half_width = kWandBaseHalfWidth + (kWandTipHalfWidth - kWandBaseHalfWidth) * t;
      int w = std::max(0, static_cast<int>(std::round(half_width)));
      for (int v = -w; v <= w; ++v) {
        Color color = (std::abs(v) == w && w > 0) ? outline : core;
        int x = static_cast<int>(std::round(start.x + fx * u + px * v));
        int y = static_cast<int>(std::round(start.y + fy * u + py * v));
        canvas.set_pixel(x, y, color);
      }
    }
  }

  void draw_wand_shimmer(Canvas& canvas, Point pos, float elapsed, float charge) {
    charge = std::clamp(charge, 0.0f, 1.0f);
    float flicker = 0.6f + 0.4f * std::sin(elapsed * 45.0f);
    float alpha = charge * flicker;
    Color core = hsv(0.13f, 0.1f, std::min(1.0f, 0.5f + alpha));
    Color ray = hsv(0.13f, 0.35f, 0.7f * alpha);
    int x = static_cast<int>(pos.x);
    int y = static_cast<int>(pos.y);
    canvas.set_pixel(x, y, core);
    int ray_len = 1 + static_cast<int>(charge * 2.0f);
    for (auto const& d : std::array<Point, 8>{{{1, 0}, {-1, 0}, {0, 1}, {0, -1},
                                               {1, 1}, {-1, -1}, {1, -1}, {-1, 1}}}) {
      canvas.set_pixel(x + static_cast<int>(d.x) * ray_len, y + static_cast<int>(d.y) * ray_len, ray);
    }
  }

  void draw_missile_bolt(Canvas& canvas, Point pos, float heading, float hue) {
    Color core = hsv(hue, 0.25f, 1.0f);
    Color ring = hsv(hue, 0.85f, 0.9f);
    Color tail = hsv(hue, 0.9f, 0.55f);
    int x = static_cast<int>(pos.x);
    int y = static_cast<int>(pos.y);
    canvas.set_pixel(x, y, core);
    canvas.set_pixel(x + 1, y, ring);
    canvas.set_pixel(x - 1, y, ring);
    canvas.set_pixel(x, y + 1, ring);
    canvas.set_pixel(x, y - 1, ring);
    for (int step = 1; step <= 2; ++step) {
      int tx = static_cast<int>(pos.x - std::cos(heading) * step * 1.3f);
      int ty = static_cast<int>(pos.y - std::sin(heading) * step * 1.3f);
      canvas.set_pixel(tx, ty, tail);
    }
  }

  void draw_comet(Canvas& canvas, Point pos, float heading, float progress) {
    float radius = kCometMinSize + (kCometMaxSize - kCometMinSize) * progress;
    float tail_length = kCometTailBaseLength + progress * kCometTailProgressLength;
    Color core = hsv(kCometHue, 0.15f, 1.0f);
    Color mid = hsv(kCometHue, 0.8f, 1.0f);
    Color edge = hsv(kCometHue, 0.9f, 0.6f);
    Color tail_tip = hsv(kCometHue, 0.9f, 0.2f);
    float fx = std::cos(heading);
    float fy = std::sin(heading);
    float px = -fy;
    float py = fx;
    int r_int = std::max(1, static_cast<int>(std::round(radius)));
    int tail_int = static_cast<int>(std::round(tail_length));
    for (int du = -tail_int; du <= r_int; ++du) {
      float v_max = 0.0f;
      if (du >= 0) {
        v_max = std::sqrt(std::max(0.0f, radius * radius - static_cast<float>(du * du)));
      } else {
        float t = static_cast<float>(-du) / tail_length;
        if (t > 1.0f) {
          continue;
        }
        v_max = radius * (1.0f - t);
      }
      int v_int = std::max(0, static_cast<int>(std::round(v_max)));
      for (int dv = -v_int; dv <= v_int; ++dv) {
        Color color = mid;
        if (du >= 0) {
          float frac = std::hypot(static_cast<float>(du), static_cast<float>(dv)) / radius;
          color = frac < 0.35f ? core : frac < 0.7f ? mid : edge;
        } else {
          float t = static_cast<float>(-du) / tail_length;
          color = lerp(mid, tail_tip, t);
        }
        int x = static_cast<int>(pos.x + du * fx + dv * px);
        int y = static_cast<int>(pos.y + du * fy + dv * py);
        canvas.set_pixel(x, y, color);
      }
    }
  }

  void draw_impact_flash(Canvas& canvas, Point center, float progress, float heading) {
    progress = std::clamp(progress, 0.0f, 1.0f);
    float flash = 1.0f - progress;
    float forward_x = std::cos(heading);
    float forward_y = std::sin(heading);
    Color core = lerp(Color{255, 235, 160}, kDeskBackground, progress * 0.55f);
    Color mid = lerp(Color{255, 150, 40}, kDeskBackground, progress * 0.7f);
    Color ember = lerp(Color{200, 70, 20}, kDeskBackground, progress * 0.8f);
    Color smoke = lerp(Color{86, 82, 78}, kDeskBackground, progress * 0.35f);
    Color dark_smoke = lerp(Color{48, 48, 52}, kDeskBackground, progress * 0.25f);
    int cx = static_cast<int>(std::round(center.x));
    int cy = static_cast<int>(std::round(center.y));
    int core_push_x = static_cast<int>(std::round(forward_x * progress * 3.0f));
    int core_push_y = static_cast<int>(std::round(forward_y * progress * 2.0f));

    int core_half_w = std::max(1, static_cast<int>(4.0f - progress * 5.0f));
    int core_half_h = std::max(1, static_cast<int>(3.0f - progress * 4.0f));
    for (int y = cy - core_half_h; y <= cy + core_half_h; ++y) {
      for (int x = cx - core_half_w; x <= cx + core_half_w; ++x) {
        int jag = ((x * 11 + y * 7) & 3) - 1;
        if (std::abs(x - cx - core_push_x) + std::abs(y - cy - core_push_y) <= core_half_w + jag) {
          canvas.set_pixel(x, y, flash > 0.55f ? core : mid);
        }
      }
    }

    struct FlyingChunk {
      float forward;
      float side;
      float lift;
      std::uint8_t size;
      std::uint8_t hot;
    };
    constexpr std::array<FlyingChunk, 12> kChunks{{
        {22.0f, -7.0f, 7.0f, 2, 1}, {28.0f, 4.0f, 4.0f, 1, 1},
        {34.0f, 10.0f, 12.0f, 1, 0}, {18.0f, 13.0f, 3.0f, 1, 0},
        {39.0f, -12.0f, 9.0f, 1, 1}, {25.0f, -18.0f, 5.0f, 1, 0},
        {45.0f, 1.0f, 15.0f, 2, 1}, {15.0f, -4.0f, 10.0f, 1, 0},
        {31.0f, 18.0f, 8.0f, 1, 0}, {36.0f, -2.0f, 2.0f, 1, 1},
        {20.0f, 7.0f, 14.0f, 1, 0}, {42.0f, -15.0f, 6.0f, 1, 0},
    }};
    float px = -forward_y;
    float py = forward_x;
    for (auto const& chunk : kChunks) {
      float t = std::clamp(progress * 1.25f, 0.0f, 1.0f);
      float gravity = 18.0f * t * t;
      int x = static_cast<int>(std::round(center.x + forward_x * chunk.forward * t +
                                          px * chunk.side * t));
      int y = static_cast<int>(std::round(center.y + forward_y * chunk.forward * t +
                                          py * chunk.side * t - chunk.lift * t + gravity));
      Color color = chunk.hot ? mid : ember;
      canvas.set_pixel(x, y, color);
      if (chunk.size > 1 && progress < 0.55f) {
        canvas.set_pixel(x + 1, y, color);
        canvas.set_pixel(x, y + 1, ember);
      }
      if (progress < 0.45f && chunk.forward > 30.0f) {
        canvas.set_pixel(static_cast<int>(std::round(static_cast<float>(x) - forward_x)),
                         static_cast<int>(std::round(static_cast<float>(y) - forward_y)),
                         ember);
      }
    }

    if (progress > 0.18f) {
      int rise = static_cast<int>(progress * 8.0f);
      for (int i = 0; i < 13; ++i) {
        int ox = ((i * 5) % 17) - 8;
        int oy = -static_cast<int>((i * 7) % 8) - rise / 2;
        ox += static_cast<int>(std::round(forward_x * progress * static_cast<float>(4 + (i % 5))));
        oy += static_cast<int>(std::round(forward_y * progress * static_cast<float>(2 + (i % 4))));
        int w = 1 + static_cast<int>((i + static_cast<int>(progress * 10.0f)) % 3);
        Color color = (i & 1) == 0 ? smoke : dark_smoke;
        draw::rect(canvas, cx + ox, cy + oy, cx + ox + w, cy + oy + 1, color);
      }
    }
  }

  void spawn_momentum_debris(Point pos, float heading) {
    float fx = std::cos(heading);
    float fy = std::sin(heading);
    float px = -fy;
    float py = fx;
    for (int i = 0; i < 18; ++i) {
      float forward_speed = rng_.range(34.0f, 86.0f);
      float side_speed = rng_.range(-28.0f, 28.0f);
      float ttl = rng_.range(0.28f, 0.7f);
      Color color = i < 7 ? Color{255, 175, 50} : Color{165, 55, 20};
      particles_.emit(Particle{pos.x + rng_.range(-1.5f, 1.5f),
                               pos.y + rng_.range(-1.5f, 1.5f),
                               fx * forward_speed + px * side_speed,
                               fy * forward_speed + py * side_speed - rng_.range(4.0f, 18.0f),
                               ttl,
                               ttl,
                               95.0f,
                               1.6f,
                               color});
    }
  }

  void spawn_trail(Point pos, Color color) {
    for (int i = 0; i < 2; ++i) {
      particles_.emit(Particle{pos.x + rng_.range(-1.0f, 1.0f),
                               pos.y + rng_.range(-1.0f, 1.0f),
                               rng_.range(-4.0f, 4.0f),
                               rng_.range(-4.0f, 4.0f),
                               0.2f,
                               0.2f,
                               0.0f,
                               0.0f,
                               color});
    }
  }

  void spawn_plume(Point pos, float heading) {
    float back_x = -std::cos(heading);
    float back_y = -std::sin(heading);
    for (int i = 0; i < 3; ++i) {
      float speed = rng_.range(10.0f, 28.0f);
      particles_.emit(Particle{pos.x + rng_.range(-1.5f, 1.5f),
                               pos.y + rng_.range(-1.5f, 1.5f),
                               back_x * speed + rng_.range(-5.0f, 5.0f),
                               back_y * speed + rng_.range(-5.0f, 5.0f),
                               rng_.range(0.18f, 0.34f),
                               0.34f,
                               0.0f,
                               1.0f,
                               Color{255, static_cast<std::uint8_t>(rng_.range(80.0f, 180.0f)), 25}});
    }
  }

  void spawn_smoke(Point pos, int count) {
    for (int i = 0; i < count; ++i) {
      std::uint8_t shade = static_cast<std::uint8_t>(rng_.range(55.0f, 95.0f));
      float ttl = rng_.range(0.8f, 1.3f);
      particles_.emit(Particle{pos.x + rng_.range(-2.0f, 2.0f),
                               pos.y + rng_.range(-2.0f, 2.0f),
                               rng_.range(-6.0f, 6.0f),
                               rng_.range(-15.0f, -5.0f),
                               ttl,
                               ttl,
                               -4.0f,
                               0.6f,
                               Color{shade, shade, static_cast<std::uint8_t>(shade + 6)}});
    }
  }

  void draw_exclaim(Canvas& canvas, int x, int y, float progress) {
    int bounce = progress < 0.35f ? static_cast<int>(std::round(progress / 0.35f * 2.0f)) : 2;
    y -= bounce;
    for (int glyph_dx : {0, 2}) {
      int gx = x + glyph_dx;
      for (int dy = 0; dy < 3; ++dy) {
        canvas.set_pixel(gx, y + dy, Color{255, 225, 70});
      }
      canvas.set_pixel(gx, y + 4, Color{255, 225, 70});
    }
  }

  LivenessTracker& liveness_;
  EnvironmentState& environment_;
  ReactionEngine reactions_;
  AtmosphereSystem atmosphere_;
  ParticleSystem<160> particles_;
  LightSystem<4> local_lights_;
  LightSystem<4> world_lights_;
  Rng rng_;
  float idle_elapsed_ = 0.0f;
  float bug_elapsed_ = 999.0f;
  float push_elapsed_ = 999.0f;
  BugPhase bug_phase_ = BugPhase::None;
  bool impact_spawned_ = false;
  int shot_index_ = 0;
  bool deploy_visible_ = false;
  float deploy_elapsed_ = 0.0f;
  float deploy_done_elapsed_ = 0.0f;
  float fireball_ignite_timer_ = 0.0f;
  float fireball_ignite_x_ = 0.0f;
  DeployStatusKind deploy_kind_ = DeployStatusKind::Deploy;
  DeployStatusResult deploy_result_ = DeployStatusResult::Running;
};

class ArcaneTreeScene final : public Scene {
 public:
  std::string name() const override { return "arcane_tree"; }

  void render(Canvas& canvas, float, float) override {
    canvas.clear(Color{8, 16, 10});
    int trunk_x = canvas.width() / 4;
    draw::rect(canvas, trunk_x - 1, canvas.height() - 12, trunk_x + 1, canvas.height() - 1,
               Color{90, 60, 40});
    draw::rect(canvas, trunk_x - 11, canvas.height() - 20, trunk_x + 13,
               canvas.height() - 10, Color{40, 110, 50});
  }
};

class MeadowCycleScene final : public Scene {
 public:
  explicit MeadowCycleScene(EnvironmentState& environment) : environment_(environment) {}

  std::string name() const override { return "meadow_cycle"; }

  void render(Canvas& canvas, float, float now_seconds) override {
    float daylight = daylight_for_time(now_seconds);
    Color night_background{10, 10, 20};
    Color day_background{82, 118, 128};
    Color background = blend(night_background, day_background, daylight);
    canvas.clear(background);
    AtmosphereConfig config{background};
    config.sky = daylight > 0.18f ? SkyMode::Day : SkyMode::Night;
    config.daylight = static_cast<std::uint8_t>(std::round(daylight * 255.0f));
    config.moon = daylight > 0.55f ? MoonMode::Sun : MoonMode::Moon;
    config.weather = weather_for_time(now_seconds);
    config.clouds = true;
    config.cloud_cover = CloudCover::Light;
    config.stars = daylight < 0.72f;
    config.grass = true;
    config.motes = daylight < 0.45f;
    config.horizon_dither = true;
    config.wind = config.weather == WeatherMode::Rain ? 155 : static_cast<std::uint8_t>(75 + daylight * 35.0f);
    config = environment_.apply(config);
    atmosphere_.render(canvas, now_seconds, config);
  }

 private:
  static float daylight_for_time(float now_seconds) {
    constexpr float kCycleSeconds = 34.0f;
    float phase = std::fmod(now_seconds, kCycleSeconds) / kCycleSeconds;
    float wave = 0.5f - 0.5f * std::cos(phase * 6.2831853f);
    return std::clamp(wave, 0.0f, 1.0f);
  }

  static WeatherMode weather_for_time(float now_seconds) {
    float cycle = std::fmod(now_seconds, 26.0f);
    if (cycle > 9.0f && cycle < 14.5f) {
      return WeatherMode::Rain;
    }
    return WeatherMode::None;
  }

  AtmosphereSystem atmosphere_;
  EnvironmentState& environment_;
};

}  // namespace

std::unique_ptr<Scene> make_desk_spirit_scene(LivenessTracker& liveness,
                                              AccumulatingStateStore& state,
                                              EnvironmentState& environment) {
  return std::make_unique<DeskSpiritScene>(liveness, state, environment);
}

std::unique_ptr<Scene> make_arcane_tree_scene() {
  return std::make_unique<ArcaneTreeScene>();
}

std::unique_ptr<Scene> make_meadow_cycle_scene(EnvironmentState& environment) {
  return std::make_unique<MeadowCycleScene>(environment);
}

}  // namespace magicpanel
