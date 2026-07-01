"""Arcane Tree scene stub (docs/11-scene-arcane-tree.md).

Registered so the scene manager supports more than one scene. No reaction
wiring or growth/wildlife logic yet — that's a future build step once this
scene gets its own implementation pass.
"""

from __future__ import annotations

from magicpanel.canvas import Canvas
from magicpanel.scenes.base import Scene

BACKGROUND = (8, 16, 10)
TRUNK_COLOR = (90, 60, 40)
CANOPY_COLOR = (40, 110, 50)


class ArcaneTreeScene(Scene):
    name = "arcane_tree"

    def render(self, canvas: Canvas, dt: float) -> None:
        canvas.clear(BACKGROUND)

        # Placeholder silhouette: a short trunk and a wide, low canopy,
        # positioned as one element within the frame per the
        # horizontal-composition constraint (not centered/vertical-dominant).
        trunk_x = canvas.width // 4
        for y in range(canvas.height - 6, canvas.height):
            canvas.set_pixel(trunk_x, y, TRUNK_COLOR)

        canopy_cy = canvas.height - 8
        for y in range(canopy_cy - 3, canopy_cy + 3):
            for x in range(trunk_x - 6, trunk_x + 6):
                canvas.set_pixel(x, y, CANOPY_COLOR)
