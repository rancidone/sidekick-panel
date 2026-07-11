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

import colorsys
import math
import random
from dataclasses import dataclass
from typing import Callable

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

# The wizard sits at a fixed left-side spot (rather than dead-center) so the
# right side of the panel is free for a target (currently: the bug) to
# announce itself and for effects (currently: the fireball) to travel
# across open space to reach it.
WIZARD_LEFT_MARGIN = 4

# bug_kill plays out as a beat-by-beat sequence rather than a fixed-rate
# flipbook, since the fireball's travel needs to interpolate smoothly across
# open panel space rather than jump between a couple of fixed offsets:
#   announce -> bug pops in alone, on the right, jiggling a taunt, wizard
#               throws up an anime-style "!!" reaction
#   cast     -> wand raises, charge sparks at the tip
#   travel   -> comet/missiles fly wand -> bug, trailing sparks
#   impact   -> strike pose, bug gone, explosion at the impact point
#   payoff   -> cheer pose, embers/smoke fade
BUG_KILL_ANNOUNCE_SECONDS = 1.2
BUG_KILL_CAST_SECONDS = 0.5
BUG_KILL_TRAVEL_SECONDS = 0.9
BUG_KILL_IMPACT_SECONDS = 0.5
# Long enough for the payoff cheer (see _happy_jump_sprites) to play through
# two full arm-raise bounces at its normal (HAPPY_JUMP_STEP_SECONDS) speed
# rather than being squeezed to fit: 2 * len(_happy_jump_sprites) frames.
BUG_KILL_PAYOFF_SECONDS = 1.0
BUG_KILL_TOTAL_SECONDS = (
    BUG_KILL_ANNOUNCE_SECONDS
    + BUG_KILL_CAST_SECONDS
    + BUG_KILL_TRAVEL_SECONDS
    + BUG_KILL_IMPACT_SECONDS
    + BUG_KILL_PAYOFF_SECONDS
)

# Sprite-local (i.e. relative to the wizard's own origin, not the canvas)
# position of the wand tip in the cast pose, used as the fireball's launch
# point regardless of where the wizard is drawn on the panel.
WAND_TIP_OFFSET = (48, 37)
# Sprite-local rest position of the hand at the wizard's side (base/idle
# pose — arm down, wand hidden in the sleeve/robe), used as the start point
# for the wand-reveal animation: the wand telescopes out from here toward
# WAND_TIP_OFFSET before the cast pose (raised arm, wand already drawn in
# the raster art) takes over.
# Matches the isolated hand blob visible in wizard_0001.png's own alpha
# mask (rows ~50-51, cols ~35-38) — the previous guess (40, 44) didn't
# line up with the sprite's actual hand and made the wand look disconnected.
HAND_REST_OFFSET = (37, 50)
# Fraction of the cast phase spent on the reveal (procedural wand growing
# out) before switching to the cast_sprite's raised-arm pose for the
# remaining charge-up shimmer.
BUG_KILL_REVEAL_FRACTION = 0.5
# The reveal sweeps the wand from this steep downward angle (radians;
# ~straight down, canvas y-down convention) up to its final raised angle,
# rather than lerping the tip in a straight line — reads as drawing the
# wand out of the robe pointed down, then raising it into the cast stance.
WAND_REVEAL_START_ANGLE = 1.7
# Sampled directly from the wand pixels baked into wizard_cast_0001.png, so
# the procedural reveal line matches the raster art's own outline/shading
# instead of being a flat single-color stroke.
WAND_OUTLINE_COLOR = (16, 22, 74)
WAND_CORE_COLOR = (121, 69, 27)
WAND_SHADOW_COLOR = (74, 42, 16)
# A proper tapered staff (thick at the hand, narrowing to a point) rather
# than a uniform 1px stroke — half-widths in pixels at each end.
WAND_BASE_HALF_WIDTH = 1.3
WAND_TIP_HALF_WIDTH = 0.4
# Bounding box (rows ~36-41, cols ~34-49 of wizard_cast_0001.png) covering
# just the raster wand's own pixels — not the sleeve/arm above it — so
# .masked() can erase only the wand and let the procedural one (already
# built for the reveal) stand in for it the rest of the way too.
WAND_MASK_REGION = (33, 35, 50, 42)

# Comet ("single-fireball") flight-progress tuning: starts as a single
# pixel mote and grows a ring/tail as it nears the bug, like a tiny comet
# closing in.
COMET_HUE = 0.07
COMET_MIN_SIZE = 1.0
COMET_MAX_SIZE = 6.5  # 2.5x the original 2.6 — still starts at COMET_MIN_SIZE
# How far the comet's light reaches, and how strong the tint gets at zero
# distance — a linear falloff, not physically-accurate inverse-square, but
# plenty convincing at 128x64.
COMET_GLOW_RADIUS = 22.0
COMET_GLOW_MAX_STRENGTH = 0.5

# The bug jiggles in place for the announce beat (taunting the wizard
# before the shot) — purely cosmetic, applied only to where it's drawn, not
# to the fixed target point the projectile(s) actually aim for.
BUG_TAUNT_JIGGLE_X = 1.6
BUG_TAUNT_JIGGLE_Y = 1.0

# Anime-style "!!" the wizard throws up on spotting the bug, shown for the
# first stretch of the announce beat with a little bounce-in.
EXCLAIM_SECONDS = 0.6
EXCLAIM_OFFSET = (44, 6)
EXCLAIM_COLOR = (255, 225, 70)

# Every bug-kill shot (both the single-fireball variant and each of the
# three magic-missile variant's missiles) rides a curve: a perpendicular
# offset from the straight wand->bug line, as a function of travel progress,
# that's zero at both ends so any shape still launches from the wand tip and
# lands exactly on the bug. (kind, amplitude_px, launch_stagger_seconds,
# wave_cycles — cycles only matters for "wave").
BUG_KILL_MISSILE_CHANCE = 0.5

# Magic-missile variant: three missiles, staggered slightly, on a bigger
# swing than the single-fireball case so they visibly fan out before
# reconverging on the bug. Each is a procedurally-drawn bolt (not the
# fireball sprite) so its color is a plain hue (0..1, colorsys convention)
# rather than baked into a raster image — straight/arc/wave get progressively
# "hotter" (further into blue-violet-magenta) so the wilder the curve, the
# more electric it reads. (kind, amplitude_px, launch_stagger_seconds,
# wave_cycles, hue).
BUG_KILL_MISSILE_SPECS = (
    ("straight", 0.0, 0.0, 0, 0.58),
    ("arc", 22.0, 0.09, 0, 0.76),
    ("wave", 7.0, 0.18, 1, 0.86),
)
# One of the three missiles (the wave one) sometimes goes big: a bit more
# amplitude and one extra half-oscillation, for a visibly-bigger (but still
# readable, not a wild squiggle) S-curve — bumped further into magenta/hot-
# pink so it also *looks* like the wild one.
BUG_KILL_EXAGGERATED_WAVE_CHANCE = 0.35
BUG_KILL_EXAGGERATED_WAVE_SPEC = ("wave", 11.0, 0.18, 2, 0.94)

# Single-fireball ("comet") variant: picks one of these each time rather
# than always flying straight. (kind, amplitude_range, wave_cycles_range).
BUG_KILL_SINGLE_CURVE_POOL = (
    ("straight", (0.0, 0.0), (0, 0)),
    ("arc", (8.0, 14.0), (0, 0)),
    ("wave", (4.0, 7.0), (1, 1)),
)

from magicpanel.state import AccumulatingStateStore

BACKGROUND = (10, 10, 20)


@dataclass
class Particle:
    x: float
    y: float
    vx: float
    vy: float
    ttl: float
    max_ttl: float
    color: tuple[int, int, int]
    gravity: float = 0.0
    drag: float = 0.0

    def tick(self, dt: float) -> bool:
        if self.drag:
            decay = max(0.0, 1.0 - self.drag * dt)
            self.vx *= decay
            self.vy *= decay
        self.vy += self.gravity * dt
        self.x += self.vx * dt
        self.y += self.vy * dt
        self.ttl -= dt
        return self.ttl > 0

    def draw(self, canvas: Canvas) -> None:
        # Fade toward the background as ttl runs out, so particles wink out
        # rather than popping off abruptly.
        fade = max(0.0, min(1.0, self.ttl / self.max_ttl))
        color = tuple(
            int(c * fade + b * (1 - fade)) for c, b in zip(self.color, BACKGROUND)
        )
        canvas.set_pixel(int(self.x), int(self.y), color)

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
        "bug_kill",
        ReactionKind.TRANSIENT,
        "bug_squashed",
        duration_seconds=BUG_KILL_TOTAL_SECONDS,
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
        # bug_kill draws its own procedural wand throughout (reveal, cast,
        # travel, impact) rather than only for the reveal beat, so it needs
        # the raised-arm pose with its raster wand erased; casting_spells
        # (CI/deploy) keeps using self._cast_sprite as-is, wand and all.
        self._cast_sprite_no_wand = self._cast_sprite.masked(
            WAND_MASK_REGION,
            {WAND_OUTLINE_COLOR, WAND_CORE_COLOR, WAND_SHADOW_COLOR},
        )
        # No dedicated bug-kill art exists yet (the strike/payoff frames
        # that used to live here were wrong and got pulled). Reuse existing
        # poses instead: base for the wind-up, cast held through the
        # strike, excite as the "got it" cheer. The bug is the only raster
        # sprite involved in the fight itself — the comet/missiles and all
        # particle effects are drawn procedurally (see _draw_comet,
        # _draw_missile_bolt).
        self._bug_sprite = Sprite(ASSET_DIR / "bug.png")
        self._particles: list[Particle] = []
        self._particle_rng = random.Random()
        self._bug_kill_phase: str | None = None
        self._bug_kill_variant: str = "single"
        self._bug_kill_single_curve: tuple[str, float, int] = ("straight", 0.0, 0)
        self._bug_kill_missile_specs: tuple = BUG_KILL_MISSILE_SPECS
        self._bug_kill_geom: dict = {}
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

    def _spawn_burst(
        self,
        x: float,
        y: float,
        color: tuple[int, int, int],
        count: int,
        speed: float,
        ttl: float,
        *,
        gravity: float = 0.0,
        drag: float = 0.0,
    ) -> None:
        for _ in range(count):
            angle = self._particle_rng.uniform(0, 6.283)
            spd = self._particle_rng.uniform(speed * 0.3, speed)
            self._particles.append(
                Particle(
                    x=x,
                    y=y,
                    vx=spd * math.cos(angle),
                    vy=spd * math.sin(angle),
                    ttl=ttl,
                    max_ttl=ttl,
                    color=color,
                    gravity=gravity,
                    drag=drag,
                )
            )

    def _spawn_trail(
        self, x: float, y: float, color: tuple[int, int, int]
    ) -> None:
        for _ in range(2):
            self._particles.append(
                Particle(
                    x=x + self._particle_rng.uniform(-1, 1),
                    y=y + self._particle_rng.uniform(-1, 1),
                    vx=self._particle_rng.uniform(-4, 4),
                    vy=self._particle_rng.uniform(-4, 4),
                    ttl=0.2,
                    max_ttl=0.2,
                    color=color,
                )
            )

    def _spawn_smoke(self, x: float, y: float, count: int) -> None:
        """Slow-drifting gray puffs that linger after the impact flash and
        debris have already faded, so the hit still reads as "something
        just got destroyed here" a beat later.
        """
        for _ in range(count):
            shade = self._particle_rng.randint(55, 95)
            ttl = self._particle_rng.uniform(0.8, 1.3)
            self._particles.append(
                Particle(
                    x=x + self._particle_rng.uniform(-2, 2),
                    y=y + self._particle_rng.uniform(-2, 2),
                    vx=self._particle_rng.uniform(-6, 6),
                    vy=self._particle_rng.uniform(-15, -5),
                    ttl=ttl,
                    max_ttl=ttl,
                    color=(shade, shade, shade + 6),
                    gravity=-4.0,
                    drag=0.6,
                )
            )

    @staticmethod
    def _hue_rgb(hue: float, saturation: float, value: float) -> tuple[int, int, int]:
        r, g, b = colorsys.hsv_to_rgb(hue % 1.0, saturation, value)
        return (int(r * 255), int(g * 255), int(b * 255))

    def _draw_missile_bolt(
        self, canvas: Canvas, x: float, y: float, heading: float, hue: float
    ) -> None:
        """Procedural arcane bolt: near-white core (so it reads as an
        energy source rather than a flat-colored disc), a saturated ring at
        the chosen hue, and a couple of dimmer tail pixels streaming back
        along the heading. No sprite/PNG involved, so the color is a plain
        hue turn-of-the-dial rather than baked into a raster image.
        """
        core = self._hue_rgb(hue, 0.25, 1.0)
        ring = self._hue_rgb(hue, 0.85, 0.9)
        tail = self._hue_rgb(hue, 0.9, 0.55)
        canvas.set_pixel(int(x), int(y), core)
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            canvas.set_pixel(int(x + dx), int(y + dy), ring)
        for step in (1, 2):
            tx = x - math.cos(heading) * step * 1.3
            ty = y - math.sin(heading) * step * 1.3
            canvas.set_pixel(int(tx), int(ty), tail)

    def _draw_comet(
        self, canvas: Canvas, x: float, y: float, heading: float, progress: float
    ) -> None:
        """The single-fireball "comet": starts as one small hot pixel and
        grows a ring (then, closer to impact, a wider diagonal ring) plus a
        lengthening tail as `progress` (0..1 across the whole flight) climbs
        — replaces the old static fireball.png with something that visibly
        builds up speed and mass as it closes in, comet-style.
        """
        # One filled teardrop rather than a disc head + dotted tail: a
        # rounded leading edge (radius all around at u=0) whose allowed
        # width tapers linearly to a point at the tail's far end, scanned
        # in the comet's own forward/perpendicular frame so the "circle"
        # and "taper" pieces meet with no seam.
        radius = COMET_MIN_SIZE + (COMET_MAX_SIZE - COMET_MIN_SIZE) * progress
        tail_length = 4.0 + progress * 12.0
        core = self._hue_rgb(COMET_HUE, 0.15, 1.0)
        mid = self._hue_rgb(COMET_HUE, 0.8, 1.0)
        edge = self._hue_rgb(COMET_HUE, 0.9, 0.6)
        tail_tip = self._hue_rgb(COMET_HUE, 0.9, 0.2)

        fx, fy = math.cos(heading), math.sin(heading)
        px, py = -fy, fx
        r_int = max(1, int(round(radius)))
        tail_int = int(round(tail_length))

        for du in range(-tail_int, r_int + 1):
            if du >= 0:
                v_max = math.sqrt(max(0.0, radius * radius - du * du))
            else:
                t = -du / tail_length if tail_length > 0 else 1.0
                if t > 1.0:
                    continue
                v_max = radius * (1.0 - t)
            v_int = max(0, int(round(v_max)))
            for dv in range(-v_int, v_int + 1):
                if du >= 0:
                    frac = math.hypot(du, dv) / radius if radius > 0 else 0.0
                    color = core if frac < 0.35 else mid if frac < 0.7 else edge
                else:
                    t = -du / tail_length if tail_length > 0 else 1.0
                    color = tuple(
                        int(a + (b - a) * t) for a, b in zip(mid, tail_tip)
                    )
                dx = du * fx + dv * px
                dy = du * fy + dv * py
                canvas.set_pixel(int(x + dx), int(y + dy), color)

    def _draw_wand_shimmer(
        self, canvas: Canvas, x: float, y: float, elapsed: float, charge: float
    ) -> None:
        """Charge-up shimmer at the wand tip through the whole cast phase —
        a flickering near-white core plus four diagonal rays that grow with
        `charge` (0..1, how far through casting we are), reading as energy
        visibly building up rather than a single flash. `elapsed` just
        drives the flicker's timing, independent of how charged it is. Not
        a light source that actually illuminates anything around it, just
        a scripted flourish.
        """
        charge = max(0.0, min(1.0, charge))
        flicker = 0.6 + 0.4 * math.sin(elapsed * 45.0)
        alpha = charge * flicker
        core = self._hue_rgb(0.13, 0.1, min(1.0, 0.5 + alpha))
        ray = self._hue_rgb(0.13, 0.35, 0.7 * alpha)
        canvas.set_pixel(int(x), int(y), core)
        ray_len = 1 + charge * 2
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, -1), (1, -1), (-1, 1)):
            rx, ry = int(x + dx * ray_len), int(y + dy * ray_len)
            canvas.set_pixel(rx, ry, ray)

    def _draw_wand_line(
        self, canvas: Canvas, x0: float, y0: float, x1: float, y1: float
    ) -> None:
        """The wand itself: a proper tapered staff (beefy at the hand,
        narrowing to a point) with the same three-tone treatment as the
        raster art it hands off to (dark outline, mid-brown core darkening
        toward the tip), rather than a thin uniform stroke — used only
        while it's telescoping out during the reveal beat.
        """
        dx, dy = x1 - x0, y1 - y0
        length = math.hypot(dx, dy)
        if length < 0.01:
            return
        fx, fy = dx / length, dy / length
        perp_x, perp_y = -fy, fx
        # One pass over integer (u, v) in the wand's own frame — each
        # world pixel is written at most once — rather than stepping along
        # the length and re-deriving nearby pixels from float centers each
        # time, which left independently-rounded, slightly-offset outline
        # rows from adjacent steps (reading as doubled/jagged edges).
        # round(), not the earlier `length < 0.01` guard, is what could
        # actually hit zero here: any length below 0.5 rounds to 0 (not
        # just sub-0.01 ones), which crashed the u/u_max division below.
        u_max = max(1, round(length))
        for u in range(u_max + 1):
            t = u / u_max
            core = tuple(
                round(a + (b - a) * t) for a, b in zip(WAND_CORE_COLOR, WAND_SHADOW_COLOR)
            )
            half_width = (
                WAND_BASE_HALF_WIDTH
                + (WAND_TIP_HALF_WIDTH - WAND_BASE_HALF_WIDTH) * t
            )
            w = max(0, round(half_width))
            for v in range(-w, w + 1):
                color = WAND_OUTLINE_COLOR if abs(v) == w and w > 0 else core
                px = x0 + fx * u + perp_x * v
                py = y0 + fy * u + perp_y * v
                canvas.set_pixel(round(px), round(py), color)

    def _draw_exclaim(self, canvas: Canvas, x: float, y: float, bounce: float) -> None:
        """Two pixel-font "!" glyphs (3px bar + 1px dot each), popped up a
        couple pixels on entry per `bounce` (1 -> fully raised, 0 -> back at
        rest) for the wizard's anime-style reaction to spotting the bug.
        """
        y = y - int(round(bounce * 2))
        for glyph_dx in (0, 2):
            gx = int(x + glyph_dx)
            for dy in (0, 1, 2):
                canvas.set_pixel(gx, int(y) + dy, EXCLAIM_COLOR)
            canvas.set_pixel(gx, int(y) + 4, EXCLAIM_COLOR)

    @staticmethod
    def _missile_offset(
        kind: str, amplitude: float, cycles: int, progress: float
    ) -> float:
        """Perpendicular displacement from the straight wand->bug line, as a
        function of travel progress (0..1). Zero at both ends so every
        curve still launches from the wand tip and lands exactly on the
        bug regardless of shape.
        """
        if kind == "straight":
            return 0.0
        if kind == "arc":
            return -amplitude * math.sin(math.pi * progress)
        if kind == "wave":
            return amplitude * math.sin(progress * 2 * math.pi * cycles) * math.sin(
                math.pi * progress
            )
        raise ValueError(f"unknown missile curve kind: {kind}")

    def _missile_position(
        self,
        kind: str,
        amplitude: float,
        cycles: int,
        progress: float,
        wand_tip: tuple[float, float],
        travel_dx: float,
        travel_dy: float,
        perp_x: float,
        perp_y: float,
    ) -> tuple[float, float]:
        progress = max(0.0, min(1.0, progress))
        base_x = wand_tip[0] + travel_dx * progress
        base_y = wand_tip[1] + travel_dy * progress
        offset = self._missile_offset(kind, amplitude, cycles, progress)
        return base_x + perp_x * offset, base_y + perp_y * offset

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
        # Callables rather than (Sprite, x, y) tuples so procedurally-drawn
        # effects (the magic-missile bolts) can share the same "goes on top
        # of the wizard sprite" draw order as image-based overlays (the bug,
        # the single fireball) without needing a Sprite to wrap them in.
        overlays: list[Callable[[Canvas], None]] = []
        if mood == "happy":
            self._sleep_elapsed = 0.0
            self._bug_kill_elapsed = 0.0
            self._bug_kill_phase = None
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
            # One-shot: play through the beats once and hold on the last
            # (cheer) frame for as long as the reaction stays active,
            # rather than looping back to the wind-up.
            self._bug_kill_elapsed = min(
                self._bug_kill_elapsed + dt, BUG_KILL_TOTAL_SECONDS
            )
            elapsed = self._bug_kill_elapsed

            # This geometry is fixed for the whole shot (only a function of
            # constants + canvas size, never of elapsed/dt), so it's
            # computed once per activation rather than redone every frame.
            if self._bug_kill_phase is None:
                wizard_origin_x = WIZARD_LEFT_MARGIN
                wand_tip = (
                    wizard_origin_x + WAND_TIP_OFFSET[0],
                    WAND_TIP_OFFSET[1],
                )
                bug_x = canvas.width - self._bug_sprite.width - 8
                bug_y = 18
                bug_center = (
                    bug_x + self._bug_sprite.width / 2,
                    bug_y + self._bug_sprite.height / 2,
                )
                impact_point = bug_center
                travel_dx = impact_point[0] - wand_tip[0]
                travel_dy = impact_point[1] - wand_tip[1]
                travel_len = math.hypot(travel_dx, travel_dy) or 1.0
                unit_dx, unit_dy = travel_dx / travel_len, travel_dy / travel_len
                self._bug_kill_geom = {
                    "wizard_origin_x": wizard_origin_x,
                    "wand_tip": wand_tip,
                    "hand": (
                        wizard_origin_x + HAND_REST_OFFSET[0],
                        HAND_REST_OFFSET[1],
                    ),
                    "bug_x": bug_x,
                    "bug_y": bug_y,
                    "bug_center": bug_center,
                    "impact_point": impact_point,
                    "travel_dx": travel_dx,
                    "travel_dy": travel_dy,
                    "perp_x": -unit_dy,
                    "perp_y": unit_dx,
                }
            geom = self._bug_kill_geom
            wizard_origin_x = geom["wizard_origin_x"]
            wand_tip = geom["wand_tip"]
            hand = geom["hand"]
            bug_x = geom["bug_x"]
            bug_y = geom["bug_y"]
            bug_center = geom["bug_center"]
            impact_point = geom["impact_point"]
            travel_dx = geom["travel_dx"]
            travel_dy = geom["travel_dy"]
            perp_x = geom["perp_x"]
            perp_y = geom["perp_y"]

            t_announce_end = BUG_KILL_ANNOUNCE_SECONDS
            t_cast_end = t_announce_end + BUG_KILL_CAST_SECONDS
            t_travel_end = t_cast_end + BUG_KILL_TRAVEL_SECONDS
            t_impact_end = t_travel_end + BUG_KILL_IMPACT_SECONDS

            if elapsed < t_announce_end:
                phase = "announce"
                sprite = self._base_sprite
            elif elapsed < t_cast_end:
                phase = "cast"
                sprite = self._cast_sprite_no_wand
            elif elapsed < t_travel_end:
                phase = "travel"
                sprite = self._cast_sprite_no_wand
            elif elapsed < t_impact_end:
                phase = "impact"
                sprite = self._cast_sprite_no_wand
            else:
                phase = "payoff"
                # Two arm-raise bounces at the same speed as the "happy"
                # mood (not stretched/squeezed to fit BUG_KILL_PAYOFF_SECONDS)
                # then hold on the resting frame the cycle ends on.
                payoff_elapsed = elapsed - t_impact_end
                cycle_len = len(self._happy_jump_sprites)
                step = min(
                    int(payoff_elapsed / HAPPY_JUMP_STEP_SECONDS),
                    2 * cycle_len - 1,
                )
                cheer_step = step % cycle_len
                sprite = self._happy_jump_sprites[cheer_step]
                jump_offset = -HAPPY_JUMP_HEIGHTS[cheer_step]
            tint, strength = (255, 255, 255), 0.0

            if phase != self._bug_kill_phase:
                if phase == "announce":
                    self._spawn_burst(*bug_center, (80, 220, 90), 10, 18, 0.4)
                    # Picked once per activation (not per phase check further
                    # down) so the whole shot stays one variant start to
                    # finish.
                    self._bug_kill_variant = (
                        "missiles"
                        if self._particle_rng.random() < BUG_KILL_MISSILE_CHANCE
                        else "single"
                    )
                    if self._bug_kill_variant == "single":
                        kind, amp_range, cycle_range = self._particle_rng.choice(
                            BUG_KILL_SINGLE_CURVE_POOL
                        )
                        self._bug_kill_single_curve = (
                            kind,
                            self._particle_rng.uniform(*amp_range),
                            self._particle_rng.randint(*cycle_range),
                        )
                    else:
                        specs = list(BUG_KILL_MISSILE_SPECS)
                        if self._particle_rng.random() < BUG_KILL_EXAGGERATED_WAVE_CHANCE:
                            specs[2] = BUG_KILL_EXAGGERATED_WAVE_SPEC
                        self._bug_kill_missile_specs = specs
                elif phase == "travel":
                    # Launch-moment pop, regardless of which variant is
                    # about to fly — the sustained charge-up shimmer itself
                    # lives in the cast phase below, not here.
                    wand_x, wand_y = wand_tip
                    self._spawn_burst(wand_x, wand_y, (255, 250, 210), 8, 20, 0.3)
                elif phase == "impact":
                    # Flash, then heavier debris that arcs under gravity and
                    # decelerates under drag (i.e. actually has momentum,
                    # rather than every particle just flying outward at a
                    # constant speed until it fades), then smoke lingering
                    # after both have burned out.
                    self._spawn_burst(*impact_point, (255, 235, 160), 10, 55, 0.22)
                    self._spawn_burst(
                        *impact_point,
                        (255, 150, 40),
                        24,
                        46,
                        0.55,
                        gravity=90,
                        drag=1.8,
                    )
                    self._spawn_burst(
                        *impact_point,
                        (200, 70, 20),
                        10,
                        26,
                        0.7,
                        gravity=60,
                        drag=1.1,
                    )
                    self._spawn_smoke(*impact_point, 12)
                self._bug_kill_phase = phase

            if phase in ("announce", "cast", "travel"):
                if phase == "announce":
                    # Taunting jiggle while it's still safe to gloat —
                    # cosmetic only: bug_center/impact_point (the actual aim
                    # point) stay fixed regardless.
                    bx = bug_x + int(round(math.sin(elapsed * 16) * BUG_TAUNT_JIGGLE_X))
                    by = bug_y + int(round(math.cos(elapsed * 11) * BUG_TAUNT_JIGGLE_Y))
                else:
                    bx, by = bug_x, bug_y
                # A mutable holder (not a plain default-arg capture) since
                # the comet's proximity glow, set further down once the
                # comet's position for this frame is known, needs to reach
                # a lambda that was already built by that point.
                bug_glow = {"strength": 0.0, "color": (255, 255, 255)}
                overlays.append(
                    lambda c, bx=bx, by=by, glow=bug_glow: self._bug_sprite.draw(
                        c, bx, by, glow["color"], glow["strength"]
                    )
                )

            if phase == "announce" and elapsed < EXCLAIM_SECONDS:
                # Quick pop-in then hold, rather than a linear rise, so it
                # reads as a snap reaction rather than a slow float-up.
                bounce = min(1.0, elapsed / 0.15)
                ex = wizard_origin_x + EXCLAIM_OFFSET[0]
                ey = EXCLAIM_OFFSET[1]
                overlays.append(
                    lambda c, ex=ex, ey=ey, bounce=bounce: self._draw_exclaim(
                        c, ex, ey, bounce
                    )
                )

            if phase == "cast":
                charge = (elapsed - t_announce_end) / BUG_KILL_CAST_SECONDS
                reveal_end = BUG_KILL_REVEAL_FRACTION
                if charge < reveal_end:
                    # Reveal beat: wand telescopes out from the sleeve
                    # (hand-rest pose, arm still down) toward its final
                    # position — before the raised-arm cast_sprite (with
                    # the wand already drawn in its own art) takes over.
                    reveal_progress = charge / reveal_end
                    hand_x, hand_y = hand
                    full_dx = wand_tip[0] - hand_x
                    full_dy = wand_tip[1] - hand_y
                    final_angle = math.atan2(full_dy, full_dx)
                    final_length = math.hypot(full_dx, full_dy)
                    # Sweep angle (steep down -> final raised angle) and
                    # length (0 -> full) together, rather than lerping the
                    # tip point in a straight line, so the wand visibly
                    # points down and out of the robe before swinging up.
                    angle = (
                        WAND_REVEAL_START_ANGLE
                        + (final_angle - WAND_REVEAL_START_ANGLE) * reveal_progress
                    )
                    length = final_length * reveal_progress
                    tip_x = hand_x + math.cos(angle) * length
                    tip_y = hand_y + math.sin(angle) * length
                    sprite = self._base_sprite
                    overlays.append(
                        lambda c, x0=hand_x, y0=hand_y, x1=tip_x, y1=tip_y: (
                            self._draw_wand_line(c, x0, y0, x1, y1)
                        )
                    )
                    overlays.append(
                        lambda c, x=tip_x, y=tip_y, elapsed=elapsed, p=reveal_progress: (
                            self._draw_wand_shimmer(c, x, y, elapsed, p)
                        )
                    )
                else:
                    # Raise + charge-up: raised-arm pose (wand erased from
                    # its raster art — see _cast_sprite_no_wand) now with
                    # the wand held statically at full length/final angle,
                    # and the shimmer settling at its fixed tip.
                    post_reveal = (charge - reveal_end) / (1.0 - reveal_end)
                    hx, hy = hand
                    wx, wy = wand_tip
                    overlays.append(
                        lambda c, x0=hx, y0=hy, x1=wx, y1=wy: (
                            self._draw_wand_line(c, x0, y0, x1, y1)
                        )
                    )
                    overlays.append(
                        lambda c, wx=wx, wy=wy, elapsed=elapsed, charge=post_reveal: (
                            self._draw_wand_shimmer(c, wx, wy, elapsed, charge)
                        )
                    )
                    self._spawn_trail(*wand_tip, (255, 200, 80))
            elif phase == "travel" and self._bug_kill_variant == "single":
                kind, amplitude, cycles = self._bug_kill_single_curve
                progress = (elapsed - t_cast_end) / BUG_KILL_TRAVEL_SECONDS
                fx, fy = self._missile_position(
                    kind, amplitude, cycles, progress, wand_tip,
                    travel_dx, travel_dy, perp_x, perp_y,
                )
                ahead_x, ahead_y = self._missile_position(
                    kind, amplitude, cycles, progress + 0.02, wand_tip,
                    travel_dx, travel_dy, perp_x, perp_y,
                )
                heading = math.atan2(ahead_y - fy, ahead_x - fx)
                overlays.append(
                    lambda c, x=fx, y=fy, heading=heading, progress=progress: (
                        self._draw_comet(c, x, y, heading, progress)
                    )
                )
                # Sparks trail behind the comet's center (toward the wand),
                # or they'd just read as a glow sitting on top of the head.
                tail_x = fx - math.cos(heading) * 13
                tail_y = fy - math.sin(heading) * 13
                self._spawn_trail(tail_x, tail_y, (255, 140, 40))

                # The comet lights up whichever of the wizard/bug it's
                # currently closest to — not a real light source, just a
                # proximity-based tint on the two sprites (Sprite.draw
                # already supports blending toward a tint color).
                glow_color = self._hue_rgb(COMET_HUE, 0.6, 1.0)

                def glow_strength(distance: float) -> float:
                    return max(0.0, 1.0 - distance / COMET_GLOW_RADIUS) * (
                        COMET_GLOW_MAX_STRENGTH
                    )

                wizard_dist = math.hypot(fx - wand_tip[0], fy - wand_tip[1])
                wizard_glow = glow_strength(wizard_dist)
                if wizard_glow > 0:
                    tint, strength = glow_color, max(strength, wizard_glow)

                bug_dist = math.hypot(fx - bug_center[0], fy - bug_center[1])
                bug_glow["color"] = glow_color
                bug_glow["strength"] = glow_strength(bug_dist)
            elif phase == "travel":
                # Three missiles launched together but staggered slightly,
                # each riding its own curve, all landing on the bug at the
                # same moment (progress reaches 1 at the same wall-clock
                # time for all three, since the stagger eats into each
                # missile's own travel window rather than delaying its
                # arrival). Drawn as procedural hue-tinted bolts (see
                # _draw_missile_bolt) rather than the fireball sprite.
                base_progress = (elapsed - t_cast_end) / BUG_KILL_TRAVEL_SECONDS
                for kind, amplitude, stagger, cycles, hue in self._bug_kill_missile_specs:
                    window = BUG_KILL_TRAVEL_SECONDS - stagger
                    progress = (
                        (elapsed - t_cast_end - stagger) / window
                        if window > 0
                        else base_progress
                    )
                    if progress < 0.0:
                        continue
                    x, y = self._missile_position(
                        kind, amplitude, cycles, progress, wand_tip,
                        travel_dx, travel_dy, perp_x, perp_y,
                    )
                    # Finite-difference tangent for this instant, since arc
                    # and wave curves change heading continuously (unlike a
                    # straight shot, this can't be computed once up front).
                    ahead_x, ahead_y = self._missile_position(
                        kind, amplitude, cycles, progress + 0.02, wand_tip,
                        travel_dx, travel_dy, perp_x, perp_y,
                    )
                    heading = math.atan2(ahead_y - y, ahead_x - x)
                    overlays.append(
                        lambda c, x=x, y=y, heading=heading, hue=hue: (
                            self._draw_missile_bolt(c, x, y, heading, hue)
                        )
                    )
                    tail_x = x - math.cos(heading) * 13
                    tail_y = y - math.sin(heading) * 13
                    self._spawn_trail(
                        tail_x, tail_y, self._hue_rgb(hue, 0.8, 0.8)
                    )

            if phase in ("travel", "impact"):
                # Held at its final resting position/angle — same wand the
                # reveal built up to, still in hand through the strike,
                # since _cast_sprite_no_wand no longer draws one itself.
                hx, hy = hand
                wx, wy = wand_tip
                overlays.append(
                    lambda c, x0=hx, y0=hy, x1=wx, y1=wy: (
                        self._draw_wand_line(c, x0, y0, x1, y1)
                    )
                )
        elif mood == "casting_spells":
            self._sleep_elapsed = 0.0
            self._happy_jump_elapsed = 0.0
            self._bug_kill_elapsed = 0.0
            self._bug_kill_phase = None
            sprite = self._cast_sprite
            tint, strength = (255, 255, 255), 0.0
        elif mood == "sleeping":
            self._happy_jump_elapsed = 0.0
            self._bug_kill_elapsed = 0.0
            self._bug_kill_phase = None
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
            self._bug_kill_phase = None
            self._sleep_elapsed = 0.0
            sprite = self._idle_animation.current()
            tint, strength = MOOD_TINTS[mood]

        origin_x = WIZARD_LEFT_MARGIN
        origin_y = (canvas.height - sprite.height) // 2 + jump_offset
        sprite.draw(canvas, origin_x, origin_y, tint, strength)
        for draw_overlay in overlays:
            draw_overlay(canvas)

        self._particles = [p for p in self._particles if p.tick(dt)]
        for particle in self._particles:
            particle.draw(canvas)
