"""macOS LaunchAgent for the always-on desktop watcher.

Installs a per-user LaunchAgent that runs ``magicpanel serve`` at login and
keeps it alive — that's the heartbeat (keep-awake while present) plus GitHub
CI/CD polling, in one process. Only this desktop side is serviced here — the
display engine runs on the Pi in the real system (and as a manual dev
emulator via ``run.py``), so it is deliberately not a login service on the
Mac.

launchd is macOS-only; the CLI guards on platform before calling in.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

LABEL = "com.magicpanel.watcher"


def plist_path() -> Path:
    return Path.home() / "Library" / "LaunchAgents" / f"{LABEL}.plist"


def log_dir() -> Path:
    return Path.home() / "Library" / "Logs" / "magicpanel"


def _magicpanel_command() -> list[str]:
    """The command the agent runs. Prefer an installed ``magicpanel`` (so the
    plist is stable), else the current interpreter + module.
    """
    exe = shutil.which("magicpanel")
    if exe:
        return [exe, "serve"]
    return [sys.executable, "-m", "magicpanel.cli", "serve"]


def _agent_path() -> str:
    """PATH for the agent. launchd gives daemons a bare PATH that omits
    Homebrew (/opt/homebrew/bin) where `gh` lives, so we bake in the PATH
    from the shell that ran the install, plus the usual Homebrew dirs.
    """
    parts = os.environ.get("PATH", "").split(":")
    for extra in ("/opt/homebrew/bin", "/usr/local/bin"):
        if extra not in parts:
            parts.append(extra)
    return ":".join(p for p in parts if p)


def _render_plist() -> str:
    args = _magicpanel_command()
    out = log_dir() / "watcher.log"
    err = log_dir() / "watcher.err.log"
    program_args = "\n".join(f"        <string>{a}</string>" for a in args)
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>{LABEL}</string>
    <key>ProgramArguments</key>
    <array>
{program_args}
    </array>
    <key>EnvironmentVariables</key>
    <dict>
        <key>PATH</key>
        <string>{_agent_path()}</string>
    </dict>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <true/>
    <key>StandardOutPath</key>
    <string>{out}</string>
    <key>StandardErrorPath</key>
    <string>{err}</string>
</dict>
</plist>
"""


def install() -> Path:
    """Write the plist and load it. Returns the plist path."""
    log_dir().mkdir(parents=True, exist_ok=True)
    path = plist_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(_render_plist())
    # Reload if already loaded, so re-install picks up a changed command.
    subprocess.run(["launchctl", "unload", str(path)], capture_output=True)
    subprocess.run(["launchctl", "load", "-w", str(path)], check=True)
    return path


def uninstall() -> bool:
    """Unload and remove the plist. Returns True if it existed."""
    path = plist_path()
    if not path.exists():
        return False
    subprocess.run(["launchctl", "unload", str(path)], capture_output=True)
    path.unlink()
    return True


def status() -> str:
    """Human-readable status line."""
    path = plist_path()
    if not path.exists():
        return "not installed"
    result = subprocess.run(
        ["launchctl", "list", LABEL], capture_output=True, text=True
    )
    if result.returncode == 0:
        return f"installed and loaded ({path})"
    return f"installed but not loaded ({path})"
