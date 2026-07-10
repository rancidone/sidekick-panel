"""Desk Spirit scene (docs/10-scene-desk-spirit.md).

A single character whose mood reacts to work events, using the engine's
shared reaction-persistence model. Rendered as the wizard sprite
(assets/wizard*.png). Happy has dedicated art (wizard_excite); other moods
fall back to a color tint over the idle animation until dedicated art
exists for them too. The idle animation itself (blink + chest-star
twinkle) is what keeps the scene feeling alive rather than a static frame,
per the design doc's requirement that every mood have its own idle loop.
"""

from __future__ import annotations

import random

from magicpanel import enginelog
from magicpanel.canvas import Canvas
from magicpanel.liveness import LivenessTracker
from magicpanel.reactions import ReactionEngine, ReactionKind, ReactionRule
from magicpanel.scenes.base import Scene
from magicpanel.sprite import ASSET_DIR, IdleAnimator, Sprite

BLINK_HOLD = 0.08
STAR_HOLD = 0.35
DOUBLE_BLINK_CHANCE = 0.06
HAPPY_JUMP_STEP_SECONDS = 0.09
HAPPY_JUMP_HEIGHTS = (0, 1, 2, 1, 0)
BUG_KILL_STEP_SECONDS = 0.4
from magicpanel.state import AccumulatingStateStore

BACKGROUND = (10, 10, 20)

# Mood -> (tint color, blend strength), used for moods without dedicated
# art. Strength 0 leaves the sprite untouched; higher values shift it
# further toward the tint color while still preserving its original
# shading (see Sprite.draw). "happy" has real art instead (see
# _excite_sprite) and is not looked up here.
MOOD_TINTS = {
    "baseline": ((255, 255, 255), 0.0),
    "angry": ((255, 40, 40), 0.5),
    "breathing_fire": ((255, 120, 30), 0.45),
    "sleeping": ((40, 40, 80), 0.55),
}

RULES = [
    ReactionRule("happy", ReactionKind.TRANSIENT, "git_commit", duration_seconds=2.0),
    ReactionRule("happy", ReactionKind.TRANSIENT, "tests_passed", duration_seconds=4.0),
    ReactionRule("happy", ReactionKind.TRANSIENT, "build_passed", duration_seconds=4.0),
    ReactionRule(
        "bug_kill", ReactionKind.TRANSIENT, "bug_squashed", duration_seconds=1.6
    ),
    ReactionRule(
        "angry",
        ReactionKind.STICKY,
        "production_incident",
        resolve_event="incident_resolved",
    ),
    ReactionRule(
        "casting_spells",
        ReactionKind.DURATION_BOUND,
        "deploy_started",
        end_event="deploy_finished",
    ),
    # CI builds reuse the casting-spells animation for now: breathing_fire
    # has no dedicated art (only a tint that reads as "angry"), whereas
    # casting reads as active, purposeful work. Swap back once fire art
    # exists.
    ReactionRule(
        "casting_spells",
        ReactionKind.DURATION_BOUND,
        "ci_build_started",
        end_event="ci_build_finished",
    ),
]

# Mood -> the event that ends it (for the engine log), so a transition
# reads as e.g. "casting_spells (clears on ci_build_finished)".
_MOOD_CLEARS = {
    rule.name: (rule.resolve_event or rule.end_event)
    for rule in RULES
    if rule.resolve_event or rule.end_event
}

# Priority order when multiple moods are simultaneously active.
MOOD_PRIORITY = [
    "angry",
    "casting_spells",
    "breathing_fire",
    "bug_kill",
    "happy",
    "baseline",
]


class DeskSpiritScene(Scene):
    name = "desk_spirit"

    def __init__(
        self,
        liveness: LivenessTracker,
        accumulator: AccumulatingStateStore | None = None,
    ) -> None:
        self._liveness = liveness
        self._reactions = ReactionEngine(RULES, accumulator=accumulator)

        base = Sprite(ASSET_DIR / "wizard_0001.png")
        self._base_sprite = base
        self._excite_sprite = Sprite(ASSET_DIR / "wizard_excite_0001.png")
        jump_1 = Sprite(ASSET_DIR / "wizard_jump_0001.png")
        # Jump arc matching HAPPY_JUMP_HEIGHTS's 0,1,2,1,0 shape: crouch/rise
        # (jump_1), peak (excite's raised arms, since that's the pose that
        # reads as "cheering at the top of a hop"), descend.
        self._happy_jump_sprites = (base, jump_1, self._excite_sprite, jump_1, base)
        self._cast_sprite = Sprite(ASSET_DIR / "wizard_cast_0001.png")
        # Bug-kill is just a cast (wand raise -> effect) with its own
        # payoff frames tacked on the end; it shares the base/cast poses
        # rather than having dedicated rest/raise art of its own.
        self._bug_kill_sprites = (
            base,
            self._cast_sprite,
            Sprite(ASSET_DIR / "wizard_bugkill_0003.png"),
            Sprite(ASSET_DIR / "wizard_bugkill_0004.png"),
        )
        blink_1 = Sprite(ASSET_DIR / "wizard_blink_0001.png")
        blink_2 = Sprite(ASSET_DIR / "wizard_blink_0002.png")
        self._eyes_closing_sprite = blink_1
        self._eyes_closed_sprite = blink_2
        star_1 = Sprite(ASSET_DIR / "wizard_star_pulse_0001.png")
        star_2 = Sprite(ASSET_DIR / "wizard_star_pulse_0002.png")
        star_3 = Sprite(ASSET_DIR / "wizard_star_pulse_0003.png")
        star_4 = Sprite(ASSET_DIR / "wizard_star_pulse_0004.png")

        def blink_sequence() -> list[tuple[Sprite, float]]:
            seq = [(blink_1, BLINK_HOLD), (blink_2, BLINK_HOLD)]
            if self._idle_animation_rng.random() < DOUBLE_BLINK_CHANCE:
                seq += [(base, 0.12), (blink_1, BLINK_HOLD), (blink_2, BLINK_HOLD)]
            return seq

        def star_sequence() -> list[tuple[Sprite, float]]:
            return [
                (star_1, STAR_HOLD),
                (star_2, STAR_HOLD),
                (star_3, STAR_HOLD),
                (star_4, STAR_HOLD),
            ]

        self._idle_animation_rng = random.Random()
        self._idle_animation = IdleAnimator(
            base,
            [
                (blink_sequence, 16.0, 32.0),
                (star_sequence, 4.0, 8.0),
            ],
            rng=self._idle_animation_rng,
        )
        self._happy_jump_elapsed = 0.0
        self._bug_kill_elapsed = 0.0
        self._sleep_elapsed = 0.0
        self._last_logged_mood: str | None = None

    def handle_event(self, event: dict) -> None:
        event_name = event.get("event")
        if isinstance(event_name, str):
            self._reactions.handle_event(event_name)

    def _current_mood(self) -> str:
        if not self._liveness.is_connected():
            return "sleeping"
        for mood in MOOD_PRIORITY:
            if mood == "baseline":
                continue
            if self._reactions.is_active(mood):
                return mood
        return "baseline"

    def render(self, canvas: Canvas, dt: float) -> None:
        self._reactions.tick(dt)
        self._idle_animation.tick(dt)
        canvas.clear(BACKGROUND)

        mood = self._current_mood()
        if mood != self._last_logged_mood:
            clears = _MOOD_CLEARS.get(mood)
            detail = f" (clears on {clears})" if clears else ""
            enginelog.log(f"mood: {self._last_logged_mood or 'baseline'} -> {mood}{detail}")
            self._last_logged_mood = mood

        jump_offset = 0
        if mood == "happy":
            self._sleep_elapsed = 0.0
            self._bug_kill_elapsed = 0.0
            self._happy_jump_elapsed += dt
            step = int(self._happy_jump_elapsed / HAPPY_JUMP_STEP_SECONDS) % len(
                HAPPY_JUMP_HEIGHTS
            )
            sprite = self._happy_jump_sprites[step]
            jump_offset = -HAPPY_JUMP_HEIGHTS[step]
            tint, strength = (255, 255, 255), 0.0
        elif mood == "bug_kill":
            self._sleep_elapsed = 0.0
            self._happy_jump_elapsed = 0.0
            # One-shot: play through once and hold on the last (cleared)
            # frame for as long as the reaction stays active, rather than
            # looping back to the wand-raise pose.
            self._bug_kill_elapsed += dt
            step = min(
                int(self._bug_kill_elapsed / BUG_KILL_STEP_SECONDS),
                len(self._bug_kill_sprites) - 1,
            )
            sprite = self._bug_kill_sprites[step]
            tint, strength = (255, 255, 255), 0.0
        elif mood == "casting_spells":
            self._sleep_elapsed = 0.0
            self._happy_jump_elapsed = 0.0
            self._bug_kill_elapsed = 0.0
            sprite = self._cast_sprite
            tint, strength = (255, 255, 255), 0.0
        elif mood == "sleeping":
            self._happy_jump_elapsed = 0.0
            self._bug_kill_elapsed = 0.0
            # One-shot close: blink_1 briefly, then hold on blink_2
            # (closed eyes) for as long as sleeping persists, rather than
            # looping back open like the idle blink does.
            self._sleep_elapsed += dt
            sprite = (
                self._eyes_closing_sprite
                if self._sleep_elapsed < BLINK_HOLD
                else self._eyes_closed_sprite
            )
            tint, strength = MOOD_TINTS["sleeping"]
        else:
            self._happy_jump_elapsed = 0.0
            self._bug_kill_elapsed = 0.0
            self._sleep_elapsed = 0.0
            sprite = self._idle_animation.current()
            tint, strength = MOOD_TINTS[mood]

        origin_x = (canvas.width - sprite.width) // 2
        origin_y = (canvas.height - sprite.height) // 2 + jump_offset
        sprite.draw(canvas, origin_x, origin_y, tint, strength)
