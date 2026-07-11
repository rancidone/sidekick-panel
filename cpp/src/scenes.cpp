#include "magicpanel/scenes.h"

#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "magicpanel/drawing.h"
#include "magicpanel/effects.h"
#include "magicpanel/sprite.h"
#include "magicpanel/sprite_assets.h"

namespace magicpanel {
namespace {

constexpr Color kDeskBackground{10, 10, 20};
constexpr Color kStar{255, 226, 90};
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

std::vector<ReactionRule> desk_rules() {
  std::vector<ReactionRule> rules;
  rules.push_back(ReactionRule{"happy", ReactionKind::Transient, "git_commit", 2.0f, "", "", ""});
  rules.push_back(
      ReactionRule{"happy", ReactionKind::Transient, "tests_passed", 4.0f, "", "", ""});
  rules.push_back(
      ReactionRule{"happy", ReactionKind::Transient, "build_passed", 4.0f, "", "", ""});
  rules.push_back(
      ReactionRule{"bug_kill", ReactionKind::Transient, "bug_squashed", 4.1f, "", "", ""});
  rules.push_back(ReactionRule{"angry", ReactionKind::Sticky, "production_incident", 0.0f,
                                "incident_resolved", "", ""});
  rules.push_back(ReactionRule{"casting_spells", ReactionKind::DurationBound, "deploy_started",
                                0.0f, "", "deploy_finished", ""});
  rules.push_back(ReactionRule{"casting_spells", ReactionKind::DurationBound, "ci_build_started",
                                0.0f, "", "ci_build_finished", ""});
  return rules;
}

class DeskSpiritScene final : public Scene {
 public:
  DeskSpiritScene(LivenessTracker& liveness, AccumulatingStateStore& state)
      : liveness_(liveness), reactions_(desk_rules(), &state) {}

  std::string name() const override { return "desk_spirit"; }

  void handle_event(Event const& event) override {
    reactions_.handle_event(event.name);
    if (event.name == "bug_squashed") {
      bug_elapsed_ = 0.0f;
      impact_spawned_ = false;
    }
  }

  void render(Canvas& canvas, float dt, float now_seconds) override {
    reactions_.tick(dt);
    particles_.tick(dt);
    idle_elapsed_ += dt;
    bug_elapsed_ = std::min(kBugTotalSeconds + 1.0f, bug_elapsed_ + dt);
    canvas.clear(kDeskBackground);

    std::string mood = current_mood(now_seconds);
    Color tint{255, 255, 255};
    float tint_strength = 0.0f;
    if (mood == "angry") {
      tint = kAngryTint;
      tint_strength = 0.5f;
    } else if (mood == "casting_spells" || mood == "bug_kill") {
      tint = kCastTint;
      tint_strength = 0.35f;
    } else if (mood == "sleeping") {
      tint = kSleepTint;
      tint_strength = 0.55f;
    }

    int hop = mood == "happy" ? happy_jump_offset() : 0;
    draw_wizard(canvas, 4, hop, tint, tint_strength, mood);

    lights_.clear();
    if (mood == "casting_spells") {
      lights_.add(Light{52.0f, 38.0f, 28.0f, 0.35f, Color{255, 190, 80}});
      draw_spell_charge(canvas, 52, 38, now_seconds);
    } else if (mood == "bug_kill") {
      draw_bug_kill(canvas, now_seconds);
    }
    lights_.add(Light{23.0f, 45.0f, 8.0f, 0.2f, kStar});
    lights_.apply(canvas);
    particles_.draw(canvas, kDeskBackground);
  }

 private:
  std::string current_mood(float now_seconds) const {
    if (!liveness_.is_connected(now_seconds)) {
      return "sleeping";
    }
    for (auto const& mood : {"angry", "casting_spells", "bug_kill", "happy"}) {
      if (reactions_.is_active(mood)) {
        return mood;
      }
    }
    return "baseline";
  }

  void draw_wizard(Canvas& canvas,
                   int x,
                   int y,
                   Color tint,
                   float tint_strength,
                   std::string const& mood) {
    Sprite const* sprite = &assets::kwizard_idle;
    if (mood == "happy") {
      sprite = happy_sprite();
    } else if (mood == "casting_spells" || mood == "bug_kill") {
      sprite = &assets::kwizard_cast;
    } else {
      sprite = idle_sprite();
    }
    draw_sprite(canvas, *sprite, x, y, tint, tint_strength);
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

  void draw_spell_charge(Canvas& canvas, int x, int y, float now_seconds) {
    Color hot{255, 210, 80};
    Color spark{255, 80, 30};
    canvas.set_pixel(x, y, hot);
    int radius = 1 + static_cast<int>(std::fmod(now_seconds * 8.0f, 3.0f));
    draw::plus(canvas, x, y, radius, spark);
  }

  void draw_bug_kill(Canvas& canvas, float now_seconds) {
    float announce_end = kBugAnnounceSeconds;
    float cast_end = announce_end + kBugCastSeconds;
    float travel_end = cast_end + kBugTravelSeconds;
    float impact_end = travel_end + kBugImpactSeconds;

    bool show_bug = bug_elapsed_ < travel_end;
    if (show_bug) {
      int jiggle_x = bug_elapsed_ < announce_end ? static_cast<int>(std::round(std::sin(now_seconds * 18.0f))) : 0;
      int jiggle_y = bug_elapsed_ < announce_end ? static_cast<int>(std::round(std::cos(now_seconds * 13.0f))) : 0;
      draw_sprite(canvas, assets::kbug, 90 + jiggle_x, 18 + jiggle_y);
    }

    if (bug_elapsed_ < announce_end) {
      draw_exclaim(canvas, 49, 7, bug_elapsed_ / announce_end);
      return;
    }

    float progress = std::min(1.0f, std::max(0.0f, (bug_elapsed_ - cast_end) / kBugTravelSeconds));
    int x = static_cast<int>(52 + (105 - 52) * progress);
    int y = static_cast<int>(38 + (30 - 38) * progress + std::sin(progress * 3.14159f) * -8.0f);
    draw_spell_charge(canvas, x, y, now_seconds);
    if (bug_elapsed_ >= travel_end && bug_elapsed_ < impact_end) {
      draw::rect(canvas, 104, 18, 118, 30, Color{180, 70, 25});
      draw::rect(canvas, 107, 21, 115, 27, Color{255, 210, 80});
      if (!impact_spawned_) {
        particles_.burst(111.0f, 24.0f, Color{255, 210, 80}, 18, 28.0f, 0.45f, rng_,
                         10.0f, 1.2f);
        impact_spawned_ = true;
      }
    } else {
      impact_spawned_ = false;
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
  ReactionEngine reactions_;
  ParticleSystem<96> particles_;
  LightSystem<4> lights_;
  Rng rng_;
  float idle_elapsed_ = 0.0f;
  float bug_elapsed_ = 999.0f;
  bool impact_spawned_ = false;
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

}  // namespace

std::unique_ptr<Scene> make_desk_spirit_scene(LivenessTracker& liveness,
                                              AccumulatingStateStore& state) {
  return std::make_unique<DeskSpiritScene>(liveness, state);
}

std::unique_ptr<Scene> make_arcane_tree_scene() {
  return std::make_unique<ArcaneTreeScene>();
}

}  // namespace magicpanel
