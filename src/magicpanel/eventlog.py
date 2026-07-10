"""Append-only event log — an audit trail of what the panel saw and sent.

Every event is recorded as one JSON line so "what made it angry, and when?"
is always answerable after the fact. Both sides write to the same file:
the engine records events it *received* (``dir="recv"`` — the ground truth
of what drove the display) and senders record what they *sent*
(``dir="sent"``), each tagged with the emitting process id.

Logging is strictly best-effort: any failure is swallowed so it can never
break the event flow it's observing. Pure-liveness ``heartbeat`` events are
skipped so they don't bury real activity.
"""

from __future__ import annotations

import json
import os
import sys
from datetime import datetime
from pathlib import Path

# Events that are pure liveness noise and would drown the log.
_SKIP = {"heartbeat"}

# A single rolling log: when it passes _MAX_BYTES, the oldest lines are
# dropped in place (keeping the most recent ~half) rather than spawning
# backup files. Overridable by env for tuning without a code change.
_MAX_BYTES = int(os.environ.get("MAGICPANEL_EVENT_LOG_MAX_BYTES", 1_000_000))


def default_log_path() -> Path:
    override = os.environ.get("MAGICPANEL_EVENT_LOG")
    if override:
        return Path(override)
    if sys.platform == "darwin":
        base = Path.home() / "Library" / "Logs" / "magicpanel"
    else:
        state = os.environ.get("XDG_STATE_HOME")
        base = (Path(state) if state else Path.home() / ".local" / "state") / "magicpanel"
    return base / "events.jsonl"


def record(
    direction: str,
    event: str | None,
    fields: dict | None = None,
    path: Path | None = None,
) -> None:
    """Append one event to the log. ``direction`` is "sent" or "recv"."""
    if not event or event in _SKIP:
        return
    path = path or default_log_path()
    entry: dict = {
        "ts": datetime.now().astimezone().isoformat(timespec="seconds"),
        "dir": direction,
        "event": event,
        "pid": os.getpid(),
    }
    if fields:
        entry["fields"] = fields
    try:
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("a") as handle:
            handle.write(json.dumps(entry) + "\n")
        if path.stat().st_size > _MAX_BYTES:
            _roll(path)
    except OSError:
        pass


def _roll(path: Path) -> None:
    """Drop the oldest lines in place, keeping the most recent ~half of the
    size cap. Best-effort: a rare cross-process race just trims slightly
    differently, never corrupts the flow.
    """
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


def read(
    path: Path | None = None,
    limit: int | None = None,
    direction: str | None = None,
) -> list[dict]:
    """Read logged entries oldest-first, optionally filtered and tail-limited."""
    path = path or default_log_path()
    if not path.exists():
        return []
    entries = []
    for line in path.read_text().splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            entry = json.loads(line)
        except json.JSONDecodeError:
            continue
        if direction and entry.get("dir") != direction:
            continue
        entries.append(entry)
    if limit is not None:
        entries = entries[-limit:]
    return entries
