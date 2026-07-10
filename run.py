#!/usr/bin/env python3
"""Wrapper to run the Magic Panel app.

Bootstraps the environment with `uv` (installing dependencies into the
project's virtualenv on first run) and launches one of the app's
entrypoints:

    ./run.py                       # start the engine (emulated display)
    ./run.py engine                # same as above, explicit
    ./run.py cli send tests_passed # send an event to a running engine
    ./run.py cli events            # anything after `cli` is passed through

This exists so the app can be started without knowing the uv/console-script
details: it finds `uv`, ensures deps are synced, then execs the right script.
To install `magicpanel` as a global command instead, use ./install.py.
"""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent

ENTRYPOINTS = {
    "engine": "magicpanel-engine",
    "cli": "magicpanel",
}


def _die(message: str) -> None:
    print(message, file=sys.stderr)
    raise SystemExit(1)


def main(argv: list[str]) -> int:
    uv = shutil.which("uv")
    if uv is None:
        _die(
            "could not find `uv` on PATH. Install it from "
            "https://docs.astral.sh/uv/ and try again."
        )

    target = argv[0] if argv else "engine"
    if target in ("-h", "--help", "help"):
        print(__doc__)
        return 0
    if target not in ENTRYPOINTS:
        _die(
            f"unknown target '{target}'. "
            f"expected one of: {', '.join(ENTRYPOINTS)} (default: engine)"
        )

    script = ENTRYPOINTS[target]
    passthrough = argv[1:]

    # `uv run` syncs the project environment (per pyproject.toml / uv.lock)
    # before executing, so a fresh checkout works with no separate install.
    cmd = [uv, "run", "--project", str(ROOT), script, *passthrough]
    return subprocess.run(cmd).returncode


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
