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

from magicpanel import loop
from magicpanel.canvas import Canvas, PygameCanvas
from magicpanel.events import EventServer
from magicpanel.liveness import LivenessTracker
from magicpanel.scenes.arcane_tree import ArcaneTreeScene
from magicpanel.scenes.desk_spirit import DeskSpiritScene
from magicpanel.scenes.manager import SceneManager
from magicpanel.state import AccumulatingStateStore


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

    scene_manager = SceneManager(
        [
            DeskSpiritScene(liveness, accumulator=accumulator),
            ArcaneTreeScene(),
        ],
        initial="desk_spirit",
    )

    server_thread = threading.Thread(
        target=_run_event_server, args=(event_queue,), daemon=True
    )
    server_thread.start()

    def frame_callback(canvas: Canvas, dt: float) -> None:
        while True:
            try:
                payload = event_queue.get_nowait()
            except queue.Empty:
                break
            liveness.mark_seen()
            scene_manager.handle_event(payload)

        scene_manager.render(canvas, dt)

    loop.run(canvas, frame_callback)


if __name__ == "__main__":
    main()
