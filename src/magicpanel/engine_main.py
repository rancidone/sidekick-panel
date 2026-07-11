"""Engine entrypoint: opens the emulated canvas, runs the render loop, and
listens for events on a background thread.

The event server runs its own asyncio loop on a separate thread and hands
events to the render loop via a thread-safe queue, so rendering never
blocks on socket I/O (docs/00-engine.md).

Stands in for what will eventually run on the Raspberry Pi.
"""

from __future__ import annotations

import asyncio
import queue
import threading
from pathlib import Path

import traceback

from magicpanel import enginelog, eventlog, loop
from magicpanel.canvas import Canvas, PygameCanvas
from magicpanel.events import EventServer
from magicpanel.hot_reload import HotReloader
from magicpanel.liveness import LivenessTracker
from magicpanel.scenes import arcane_tree as arcane_tree_module
from magicpanel.scenes import desk_spirit as desk_spirit_module
from magicpanel.scenes import manager as manager_module
from magicpanel.scenes.manager import SceneManager
from magicpanel.sprite import ASSET_DIR
from magicpanel.state import AccumulatingStateStore

# Reloaded in this order (dependencies first) whenever their source changes.
# Deliberately excludes engine_main/canvas/loop/events themselves: those
# hold the open pygame display and the event-server thread, which a reload
# can't safely swap out mid-run.
_HOT_RELOAD_MODULES = [
    "magicpanel.sprite",
    "magicpanel.reactions",
    "magicpanel.scenes.base",
    "magicpanel.scenes.desk_spirit",
    "magicpanel.scenes.arcane_tree",
    "magicpanel.scenes.manager",
]


def _run_event_server(event_queue: "queue.Queue[dict]") -> None:
    async def _main() -> None:
        server = EventServer()

        def on_event(payload: dict) -> None:
            event_queue.put(payload)

        server.on_event(on_event)
        await server.start()
        await server.serve_forever()

    asyncio.run(_main())


def main() -> None:
    event_queue: "queue.Queue[dict]" = queue.Queue()
    liveness = LivenessTracker()
    accumulator = AccumulatingStateStore()

    # Canvas must exist before any scene loads sprites: Pygame's
    # convert_alpha() requires a display surface to already be set.
    canvas = PygameCanvas()

    def build_scene_manager(initial: str = "desk_spirit") -> SceneManager:
        # Read the scene classes off the module objects (rather than using
        # names bound by a plain `from ... import Class`) so that after a
        # hot reload — which mutates these module objects in place — this
        # always picks up the reloaded class, not a stale reference to the
        # pre-reload one.
        return manager_module.SceneManager(
            [
                desk_spirit_module.DeskSpiritScene(
                    liveness, accumulator=accumulator
                ),
                arcane_tree_module.ArcaneTreeScene(),
            ],
            initial=initial,
        )

    scene_manager = build_scene_manager()

    def rebuild_scenes() -> None:
        nonlocal scene_manager
        scene_manager = build_scene_manager(scene_manager.active_name)
        enginelog.log("hot-reload: scenes rebuilt")

    hot_reloader = HotReloader(
        [Path(__file__).resolve().parent, ASSET_DIR],
        _HOT_RELOAD_MODULES,
        rebuild_scenes,
    )

    server_thread = threading.Thread(
        target=_run_event_server, args=(event_queue,), daemon=True
    )
    server_thread.start()

    def frame_callback(canvas: Canvas, dt: float) -> None:
        hot_reloader.tick(dt)
        while True:
            try:
                payload = event_queue.get_nowait()
            except queue.Empty:
                break
            liveness.mark_seen()
            event_name = payload.get("event")
            eventlog.record(
                "recv",
                event_name if isinstance(event_name, str) else None,
                {k: v for k, v in payload.items() if k != "event"},
            )
            scene_manager.handle_event(payload)

        scene_manager.render(canvas, dt)

    enginelog.log("engine started")
    try:
        loop.run(canvas, frame_callback)
    except Exception:
        enginelog.log("engine crashed:\n" + traceback.format_exc())
        raise
    finally:
        enginelog.log("engine stopped")


if __name__ == "__main__":
    main()
