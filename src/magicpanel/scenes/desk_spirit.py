"""Desk Spirit scene (docs/10-scene-desk-spirit.md).

A single character whose mood reacts to work events, using the engine's
shared reaction-persistence model. Rendered as the wizard sprite
(assets/wizard.png) with a per-mood color tint until dedicated per-mood
art exists — one pose, tint-shifted rather than distinct sprites.
"""

from __future__ import annotations

from magicpanel.canvas import Canvas
from magicpanel.liveness import LivenessTracker
from magicpanel.reactions import ReactionEngine, ReactionKind, ReactionRule
from magicpanel.scenes.base import Scene
from magicpanel.sprite import ASSET_DIR, Sprite
from magicpanel.state import AccumulatingStateStore

BACKGROUND = (10, 10, 20)

# Mood -> (tint color, blend strength). Strength 0 leaves the sprite
# untouched; higher values shift it further toward the tint color while
# still preserving its original shading (see Sprite.draw).
MOOD_TINTS = {
    "baseline": ((255, 255, 255), 0.0),
    "happy": ((255, 220, 80), 0.35),
    "angry": ((255, 40, 40), 0.5),
    "casting_spells": ((170, 90, 255), 0.45),
    "breathing_fire": ((255, 120, 30), 0.45),
    "sleeping": ((40, 40, 80), 0.55),
}

RULES = [
    ReactionRule("happy", ReactionKind.TRANSIENT, "tests_passed", duration_seconds=4.0),
    ReactionRule("happy", ReactionKind.TRANSIENT, "build_passed", duration_seconds=4.0),
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
    ReactionRule(
        "breathing_fire",
        ReactionKind.DURATION_BOUND,
        "ci_build_started",
        end_event="ci_build_finished",
    ),
]

# Priority order when multiple moods are simultaneously active.
MOOD_PRIORITY = ["angry", "casting_spells", "breathing_fire", "happy", "baseline"]


class DeskSpiritScene(Scene):
    name = "desk_spirit"

    def __init__(
        self,
        liveness: LivenessTracker,
        accumulator: AccumulatingStateStore | None = None,
    ) -> None:
        self._liveness = liveness
        self._reactions = ReactionEngine(RULES, accumulator=accumulator)
        self._sprite = Sprite(ASSET_DIR / "wizard.png")

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
        canvas.clear(BACKGROUND)

        mood = self._current_mood()
        tint, strength = MOOD_TINTS[mood]

        origin_x = (canvas.width - self._sprite.width) // 2
        origin_y = (canvas.height - self._sprite.height) // 2
        self._sprite.draw(canvas, origin_x, origin_y, tint, strength)
