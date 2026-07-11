"""Dev-loop hot reload for the emulated engine.

Watches the package's source files (plus the assets directory, since sprite
edits should also trigger a rebuild even with no .py change) and, on any
mtime change, reloads the given modules in dependency order and calls a
rebuild callback. This is purely a local-Mac development convenience for
iterating on scenes without restarting the pygame window each time — it has
no bearing on how the eventual Raspberry Pi build works.
"""

from __future__ import annotations

import importlib
import sys
import traceback
from pathlib import Path
from typing import Callable


class HotReloader:
    def __init__(
        self,
        watch_dirs: list[Path],
        module_names: list[str],
        rebuild: Callable[[], None],
        *,
        check_interval: float = 0.5,
    ) -> None:
        # Order matters: each name is reloaded in turn, so list a module's
        # dependencies before the module itself (e.g. sprite before the
        # scenes that import it).
        self._watch_dirs = watch_dirs
        self._module_names = module_names
        self._rebuild = rebuild
        self._check_interval = check_interval
        self._since_check = 0.0
        self._mtimes = self._scan()

    def _scan(self) -> dict[str, float]:
        mtimes: dict[str, float] = {}
        for directory in self._watch_dirs:
            for path in directory.rglob("*"):
                if path.is_file():
                    try:
                        mtimes[str(path)] = path.stat().st_mtime
                    except OSError:
                        pass
        return mtimes

    def tick(self, dt: float) -> None:
        self._since_check += dt
        if self._since_check < self._check_interval:
            return
        self._since_check = 0.0
        current = self._scan()
        if current != self._mtimes:
            self._mtimes = current
            self._reload()

    def _reload(self) -> None:
        for name in self._module_names:
            module = sys.modules.get(name)
            if module is None:
                continue
            try:
                importlib.reload(module)
            except Exception:
                print(f"hot-reload: failed to reload {name}:\n{traceback.format_exc()}")
                return
        try:
            self._rebuild()
        except Exception:
            print(f"hot-reload: rebuild failed:\n{traceback.format_exc()}")
