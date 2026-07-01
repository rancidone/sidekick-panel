"""Connection-liveness tracking (docs/00-engine.md: "Connection liveness
and sleep").

The engine owns this, not individual scenes. Any received event or
heartbeat counts as a sign of life; scenes read `is_connected()` to decide
their own "asleep" representation.
"""

from __future__ import annotations

import time

DEFAULT_TIMEOUT_SECONDS = 15.0


class LivenessTracker:
    def __init__(self, timeout_seconds: float = DEFAULT_TIMEOUT_SECONDS) -> None:
        self._timeout_seconds = timeout_seconds
        self._last_seen: float | None = None

    def mark_seen(self) -> None:
        self._last_seen = time.monotonic()

    def is_connected(self) -> bool:
        if self._last_seen is None:
            return False
        return (time.monotonic() - self._last_seen) < self._timeout_seconds
