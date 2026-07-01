"""Scene abstraction (docs/00-engine.md: "Scene system as the core
abstraction").
"""

from __future__ import annotations

from abc import ABC, abstractmethod

from magicpanel.canvas import Canvas


class Scene(ABC):
    name: str

    @abstractmethod
    def render(self, canvas: Canvas, dt: float) -> None:
        """Draw the current frame. Responsible for clearing/filling the
        background itself.
        """

    def handle_event(self, event: dict) -> None:
        """React to an incoming semantic event. Default: no-op."""
