"""Engine operational log — the display side's own diary.

Complements the event log (eventlog.py, which records the raw events that
crossed the socket) by recording what the *engine* did with them: startup,
crashes, and mood transitions with what clears each one. So "why is it
showing X?" is answerable from the engine's side, not just "what event
arrived".

Plain timestamped text, a single rolling file (mirrors the watcher's log),
best-effort so it never interferes with rendering.
"""

from __future__ import annotations

import os
import sys
from datetime import datetime
from pathlib import Path

_MAX_BYTES = int(os.environ.get("MAGICPANEL_ENGINE_LOG_MAX_BYTES", 1_000_000))


def default_log_path() -> Path:
    override = os.environ.get("MAGICPANEL_ENGINE_LOG")
    if override:
        return Path(override)
    if sys.platform == "darwin":
        base = Path.home() / "Library" / "Logs" / "magicpanel"
    else:
        state = os.environ.get("XDG_STATE_HOME")
        base = (Path(state) if state else Path.home() / ".local" / "state") / "magicpanel"
    return base / "engine.log"


def log(message: str, path: Path | None = None) -> None:
    """Append one timestamped line, rolling the file if it grows too large."""
    path = path or default_log_path()
    ts = datetime.now().astimezone().isoformat(timespec="seconds")
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("a") as handle:
            handle.write(f"{ts}  {message}\n")
        if path.stat().st_size > _MAX_BYTES:
            _roll(path)
    except OSError:
        pass


def _roll(path: Path) -> None:
    keep_bytes = _MAX_BYTES // 2
    lines = path.read_bytes().splitlines(keepends=True)
    kept: list[bytes] = []
    total = 0
    for line in reversed(lines):
        total += len(line)
        kept.append(line)
        if total >= keep_bytes:
            break
    kept.reverse()
    path.write_bytes(b"".join(kept))
