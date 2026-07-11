"""Fixed-timestep render loop, decoupled from event I/O.

The loop only knows how to call a per-frame callback and present the
result; it has no knowledge of scenes, events, or sockets, so rendering
never blocks on communication (see docs/00-engine.md).
"""

from __future__ import annotations

import time
from typing import Callable, Protocol

from magicpanel.canvas import Canvas

FrameCallback = Callable[[Canvas, float], None]

FPS = 60
FRAME_SECONDS = 1.0 / FPS


class StopSignal(Protocol):
    def __call__(self) -> bool: ...


def run(canvas: Canvas, frame_callback: FrameCallback, *, should_stop: StopSignal | None = None) -> None:
    """Run the render loop until the window is closed or should_stop() is True."""
    last = time.monotonic()
    while True:
        if canvas.poll_quit():
            return
        if should_stop is not None and should_stop():
            return

        now = time.monotonic()
        dt = now - last
        last = now

        frame_callback(canvas, dt)
        canvas.present()

        elapsed = time.monotonic() - now
        remaining = FRAME_SECONDS - elapsed
        if remaining > 0:
            time.sleep(remaining)
