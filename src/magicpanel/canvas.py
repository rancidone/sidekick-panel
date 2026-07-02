"""Rendering-canvas abstraction.

Scene and engine code render against this interface only. `PygameCanvas`
backs it during development; a real-hardware implementation targeting
`rpi-rgb-led-matrix` can be swapped in later without touching scene code.
"""

from __future__ import annotations

from abc import ABC, abstractmethod

PANEL_WIDTH = 128
PANEL_HEIGHT = 64

Color = tuple[int, int, int]


class Canvas(ABC):
    """A 128x64 RGB pixel surface."""

    width: int = PANEL_WIDTH
    height: int = PANEL_HEIGHT

    @abstractmethod
    def clear(self, color: Color) -> None:
        """Fill the entire canvas with a single color."""

    @abstractmethod
    def set_pixel(self, x: int, y: int, color: Color) -> None:
        """Set a single pixel. Out-of-bounds coordinates are ignored."""

    @abstractmethod
    def present(self) -> None:
        """Flush the current frame to the physical/emulated output."""

    @abstractmethod
    def poll_quit(self) -> bool:
        """Return True if the canvas has received a request to close."""


class PygameCanvas(Canvas):
    """Emulated canvas: a scaled-up Pygame window standing in for the panel."""

    def __init__(self, *, scale: int = 8) -> None:
        import pygame

        self._pygame = pygame
        self._scale = scale

        pygame.init()
        pygame.display.set_caption("Magic Panel (emulated)")
        self._screen = pygame.display.set_mode(
            (self.width * scale, self.height * scale)
        )
        self._surface = pygame.Surface((self.width, self.height))

    def clear(self, color: Color) -> None:
        self._surface.fill(color)

    def set_pixel(self, x: int, y: int, color: Color) -> None:
        if 0 <= x < self.width and 0 <= y < self.height:
            self._surface.set_at((x, y), color)

    def present(self) -> None:
        scaled = self._pygame.transform.scale(
            self._surface, (self.width * self._scale, self.height * self._scale)
        )
        self._screen.blit(scaled, (0, 0))
        self._pygame.display.flip()

    def poll_quit(self) -> bool:
        for event in self._pygame.event.get():
            if event.type == self._pygame.QUIT:
                return True
        return False
