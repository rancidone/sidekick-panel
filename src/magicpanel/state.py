"""Durable persistence for Accumulating reaction state
(docs/00-engine.md: "Accumulating state is durable across restarts").

A simple JSON file on local storage. Loaded once on startup, written on
every change. Deliberately not a database — accumulating events are
low-frequency (commits, merged PRs), so a full-file rewrite per change is
fine.
"""

from __future__ import annotations

import json
import tempfile
from pathlib import Path

DEFAULT_STATE_PATH = Path(tempfile.gettempdir()) / "magicpanel-state.json"


class AccumulatingStateStore:
    def __init__(self, path: Path = DEFAULT_STATE_PATH) -> None:
        self._path = path
        self._data: dict[str, int] = self._load()

    def _load(self) -> dict[str, int]:
        if not self._path.exists():
            return {}
        try:
            return json.loads(self._path.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError):
            return {}

    def _save(self) -> None:
        self._path.write_text(json.dumps(self._data), encoding="utf-8")

    def get(self, key: str) -> int:
        return self._data.get(key, 0)

    def increment(self, key: str, amount: int = 1) -> int:
        self._data[key] = self._data.get(key, 0) + amount
        self._save()
        return self._data[key]

    def reset(self, key: str) -> None:
        self._data[key] = 0
        self._save()
