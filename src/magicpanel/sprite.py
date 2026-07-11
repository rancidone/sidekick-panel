"""Sprite loading, decoded once into a backend-agnostic pixel grid.

Uses pygame only to decode the PNG; scenes draw sprites via `Canvas.set_pixel`
(not a Pygame-specific blit), so this stays compatible with a future
non-Pygame canvas backend.
"""

from __future__ import annotations

from pathlib import Path

Color = tuple[int, int, int]

ASSET_DIR = Path(__file__).resolve().parents[2] / "assets"


class Sprite:
    def __init__(self, path: Path) -> None:
        import pygame

        surface = pygame.image.load(str(path)).convert_alpha()
        self._load_from_surface(surface)

    def _load_from_surface(self, surface) -> None:
        self.width = surface.get_width()
        self.height = surface.get_height()
        self._pixels: list[list[tuple[int, int, int, int] | None]] = []
        for y in range(self.height):
            row: list[tuple[int, int, int, int] | None] = []
            for x in range(self.width):
                r, g, b, a = surface.get_at((x, y))
                row.append(None if a == 0 else (r, g, b, a))
            self._pixels.append(row)

    def rotated(self, degrees: float) -> "Sprite":
        """A new Sprite rotated by `degrees` (pygame convention: positive is
        counterclockwise on screen). Used for projectiles whose art is drawn
        pointing along one fixed axis but that travel at an arbitrary angle,
        e.g. the bug-kill fireball.
        """
        import pygame

        source = pygame.Surface((self.width, self.height), pygame.SRCALPHA)
        for y in range(self.height):
            for x in range(self.width):
                pixel = self._pixels[y][x]
                if pixel is not None:
                    source.set_at((x, y), pixel)
        rotated_surface = pygame.transform.rotate(source, degrees)
        result = Sprite.__new__(Sprite)
        result._load_from_surface(rotated_surface)
        return result

    def masked(
        self,
        region: tuple[int, int, int, int],
        colors: set[tuple[int, int, int]],
    ) -> "Sprite":
        """A copy with any pixel inside `region` (x0, y0, x1, y1, inclusive)
        whose RGB is in `colors` made transparent — e.g. erasing a raster
        wand from a wizard pose so a procedural one can stand in for it
        without touching the original asset file or the shared Sprite
        instance (other moods/poses may still want the untouched art).
        """
        x0, y0, x1, y1 = region
        result = Sprite.__new__(Sprite)
        result.width = self.width
        result.height = self.height
        result._pixels = [row[:] for row in self._pixels]
        for y in range(max(0, y0), min(self.height, y1 + 1)):
            for x in range(max(0, x0), min(self.width, x1 + 1)):
                pixel = result._pixels[y][x]
                if pixel is not None and pixel[:3] in colors:
                    result._pixels[y][x] = None
        return result

    def draw(
        self,
        canvas,
        origin_x: int,
        origin_y: int,
        tint: Color = (255, 255, 255),
        tint_strength: float = 0.0,
    ) -> None:
        """Draw the sprite onto canvas, alpha-blended toward `tint` by
        `tint_strength` (0 = original colors, 1 = solid tint color). A
        blend (rather than a multiply) keeps the sprite's shading visible
        on dark source art instead of crushing it toward black.
        """
        for y in range(self.height):
            for x in range(self.width):
                pixel = self._pixels[y][x]
                if pixel is None:
                    continue
                r, g, b, _a = pixel
                blended = (
                    int(r * (1 - tint_strength) + tint[0] * tint_strength),
                    int(g * (1 - tint_strength) + tint[1] * tint_strength),
                    int(b * (1 - tint_strength) + tint[2] * tint_strength),
                )
                canvas.set_pixel(origin_x + x, origin_y + y, blended)


import random
from typing import Callable

Sequence = list[tuple[Sprite, float]]
SequenceFactory = Callable[[], Sequence]


class IdleAnimator:
    """Shows a base sprite most of the time, occasionally interrupting with
    one of several randomly-scheduled "tics" (blink, twinkle, etc.).

    Generic and scene-agnostic — any scene can register its own idle tics,
    not just Desk Spirit. Each behavior is (sequence_factory, min_interval,
    max_interval); the factory is called fresh each time the tic fires, so
    it can vary the sequence (e.g. an occasional double blink).

    Intervals are drawn from a triangular distribution skewed toward the
    low end rather than a flat uniform range, so tics mostly land on the
    shorter side with an occasional longer pause — closer to how blinking
    or twinkling actually clusters than evenly-spaced randomness.
    """

    def __init__(
        self,
        base: Sprite,
        behaviors: list[tuple[SequenceFactory, float, float]],
        *,
        rng: random.Random | None = None,
    ) -> None:
        self._base = base
        self._rng = rng or random.Random()
        self._behaviors = [
            {
                "factory": factory,
                "min": lo,
                "max": hi,
                "timer": self._sample_interval(lo, hi),
            }
            for factory, lo, hi in behaviors
        ]
        self._queue: Sequence = []
        self._current_sprite = base
        self._frame_remaining = 0.0

    def _sample_interval(self, lo: float, hi: float) -> float:
        mode = lo + (hi - lo) * 0.3
        return self._rng.triangular(lo, hi, mode)

    def tick(self, dt: float) -> None:
        if self._queue:
            # Actively playing a sequence: advance frame-by-frame. Only
            # decrement while something is queued, so idle time never
            # accrues as a debt that would fast-forward through an entire
            # sequence the instant it fires.
            self._frame_remaining -= dt
            while self._frame_remaining <= 0 and self._queue:
                self._current_sprite, hold = self._queue.pop(0)
                self._frame_remaining += hold
            return

        # Idle: show base, and count down toward the next scheduled tic.
        self._current_sprite = self._base
        for behavior in self._behaviors:
            behavior["timer"] -= dt
        for behavior in self._behaviors:
            if behavior["timer"] <= 0:
                self._queue = behavior["factory"]()
                behavior["timer"] = self._sample_interval(behavior["min"], behavior["max"])
                # Show the sequence's first frame immediately rather than
                # waiting for the next tick.
                self._current_sprite, self._frame_remaining = self._queue.pop(0)
                break

    def current(self) -> Sprite:
        return self._current_sprite
