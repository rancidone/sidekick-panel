"""Heartbeat: keep the panel awake while the user is present.

Emits a ``heartbeat`` event on a cadence shorter than the engine's liveness
timeout, but only while the user is active (see activity.py). When you lock
the screen or go idle, heartbeats stop and the engine's liveness lapses, so
the scene falls asleep on its own — no explicit "sleep" event needed.

``heartbeat`` is intentionally not a reaction trigger in any scene; the
engine treats every received event as a sign of life (liveness.py), so this
marks liveness without producing any visible reaction.
"""

from __future__ import annotations

import time

from magicpanel.desktop import activity
from magicpanel.desktop.emit import emit

# Must stay below LivenessTracker.DEFAULT_TIMEOUT_SECONDS (15s) so an active
# user's heartbeats never lapse between beats.
DEFAULT_INTERVAL = 10.0
DEFAULT_IDLE_TIMEOUT = 60.0

HEARTBEAT_EVENT = "heartbeat"


def run_heartbeat(
    interval: float = DEFAULT_INTERVAL,
    idle_timeout: float = DEFAULT_IDLE_TIMEOUT,
    iterations: int | None = None,
) -> None:
    """Loop: emit a heartbeat each ``interval`` while the user is active."""
    count = 0
    while iterations is None or count < iterations:
        if activity.is_active(idle_timeout):
            emit(HEARTBEAT_EVENT)
        count += 1
        if iterations is not None and count >= iterations:
            break
        time.sleep(interval)
