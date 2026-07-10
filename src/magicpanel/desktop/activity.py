"""Desktop activity sensing (macOS).

Answers "is the user actually here right now?" so the heartbeat can keep the
panel awake while you work and let it fall asleep when you lock the screen or
step away. Two dependency-free signals read from ``ioreg``:

- ``IOConsoleLocked`` — is the screen locked (explicit, immediate).
- ``HIDIdleTime`` — nanoseconds since the last keyboard/mouse input.

Everything degrades safely: if a signal can't be read (non-macOS, error), the
sensor returns ``None`` and ``is_active`` treats "unknown" as active, so the
panel stays awake rather than falsely sleeping.
"""

from __future__ import annotations

import re
import subprocess

_HID_IDLE_RE = re.compile(r'"HIDIdleTime"\s*=\s*(\d+)')


def screen_locked() -> bool | None:
    """True/False if the login screen lock state is known, else None."""
    try:
        registry = subprocess.run(
            ["ioreg", "-n", "Root", "-d1", "-a"],
            capture_output=True,
            text=True,
            check=True,
        )
        result = subprocess.run(
            ["plutil", "-extract", "IOConsoleLocked", "raw", "-"],
            input=registry.stdout,
            capture_output=True,
            text=True,
        )
    except (FileNotFoundError, subprocess.CalledProcessError):
        return None
    value = result.stdout.strip()
    if value == "true":
        return True
    if value == "false":
        return False
    return None


def idle_seconds() -> float | None:
    """Seconds since the last HID input, or None if it can't be read."""
    try:
        result = subprocess.run(
            ["ioreg", "-c", "IOHIDSystem"],
            capture_output=True,
            text=True,
            check=True,
        )
    except (FileNotFoundError, subprocess.CalledProcessError):
        return None
    match = _HID_IDLE_RE.search(result.stdout)
    if not match:
        return None
    return int(match.group(1)) / 1_000_000_000  # ns -> s


def is_active(idle_timeout: float) -> bool:
    """Whether the user is present: not locked, and active within
    ``idle_timeout`` seconds. Unknown signals count as active.
    """
    if screen_locked() is True:
        return False
    idle = idle_seconds()
    if idle is None:
        return True
    return idle < idle_timeout
