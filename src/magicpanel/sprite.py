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
        self.width = surface.get_width()
        self.height = surface.get_height()
        self._pixels: list[list[tuple[int, int, int, int] | None]] = []
        for y in range(self.height):
            row: list[tuple[int, int, int, int] | None] = []
            for x in range(self.width):
                r, g, b, a = surface.get_at((x, y))
                row.append(None if a == 0 else (r, g, b, a))
            self._pixels.append(row)

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
