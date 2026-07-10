"""Best-effort synchronous event emission for desktop adapters.

Wraps the async ``send_event`` transport in a plain blocking call and
swallows "engine isn't running" errors: emitters are ambient background
processes (git hooks, pollers) that must never fail the user's real work
just because the panel happens to be off. This mirrors the best-effort,
non-durable delivery contract of the transport itself (events.py).
"""

from __future__ import annotations

import asyncio
import sys
from pathlib import Path

from magicpanel.events import DEFAULT_SOCKET_PATH, send_event


def emit(event: str, *, socket_path: Path = DEFAULT_SOCKET_PATH, **fields: object) -> bool:
    """Send one event to the engine. Returns True if delivered, False if the
    engine could not be reached. Never raises for a down engine.
    """
    try:
        asyncio.run(send_event(event, socket_path=socket_path, **fields))
        return True
    except (ConnectionRefusedError, FileNotFoundError):
        return False
    except OSError as exc:
        # Any other socket-level problem (stale socket, permissions) is still
        # non-fatal for the emitter; report it but don't crash the caller.
        print(f"magicpanel: could not emit '{event}': {exc}", file=sys.stderr)
        return False
