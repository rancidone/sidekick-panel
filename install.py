#!/usr/bin/env python3
"""Install Magic Panel's commands onto your system.

Run once:

    ./install.py

This installs `magicpanel` and `magicpanel-engine` as global commands (via
`uv tool install`, editable so source edits stay live) and makes sure the
install directory is on your shell PATH. After it finishes, use `magicpanel`
directly from any directory:

    magicpanel track .          # track the current repo
    magicpanel service install  # always-on watcher at login

Re-run any time to repair/update the install. `./install.py --uninstall`
removes the commands.
"""

from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent


def main(argv: list[str]) -> int:
    if argv and argv[0] in ("-h", "--help"):
        print(__doc__)
        return 0

    uv = shutil.which("uv")
    if uv is None:
        print(
            "could not find `uv` on PATH. Install it from "
            "https://docs.astral.sh/uv/ and re-run ./install.py",
            file=sys.stderr,
        )
        return 1

    if argv and argv[0] in ("--uninstall", "uninstall"):
        return subprocess.run([uv, "tool", "uninstall", "magicpanel"]).returncode

    rc = subprocess.run(
        [uv, "tool", "install", "--editable", "--force", "--python", "3.12", str(ROOT)]
    ).returncode
    if rc != 0:
        return rc

    # Best-effort: wire uv's tool bin dir into the user's shell PATH.
    subprocess.run([uv, "tool", "update-shell"])

    print("\nInstalled. Open a new shell (or source your profile), then:")
    print("  magicpanel track .          # track a repo")
    print("  magicpanel service install  # start the always-on watcher")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
