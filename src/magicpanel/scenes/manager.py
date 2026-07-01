"""Scene registry and manual (v1) scene switching
(docs/00-engine.md: "Scene lifecycle (v1)").

Switching is driven by a reserved control event (`switch_scene`) rather
than being wired into the general event->reaction mapping, since it's a
manager-level concern, not a scene reaction.
"""

from __future__ import annotations

from magicpanel.canvas import Canvas
from magicpanel.scenes.base import Scene

SWITCH_SCENE_EVENT = "switch_scene"


class SceneManager:
    def __init__(self, scenes: list[Scene], *, initial: str) -> None:
        self._scenes: dict[str, Scene] = {scene.name: scene for scene in scenes}
        if initial not in self._scenes:
            raise ValueError(f"unknown initial scene '{initial}'")
        self._active_name = initial

    @property
    def active_name(self) -> str:
        return self._active_name

    def switch_to(self, name: str) -> None:
        if name not in self._scenes:
            print(f"ignoring switch to unknown scene '{name}'")
            return
        self._active_name = name

    def handle_event(self, event: dict) -> None:
        event_name = event.get("event")
        if event_name == SWITCH_SCENE_EVENT:
            target = event.get("to")
            if isinstance(target, str):
                self.switch_to(target)
            return
        self._scenes[self._active_name].handle_event(event)

    def render(self, canvas: Canvas, dt: float) -> None:
        self._scenes[self._active_name].render(canvas, dt)
