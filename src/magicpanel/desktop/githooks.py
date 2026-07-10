"""Git-hook adapter: turns local commits into semantic events.

A ``post-commit`` hook installed into a repo calls back into
``magicpanel _commit-hook``, which reads the just-made commit *locally*,
classifies it, and emits a single semantic event. The commit message is
inspected only on the Mac to pick the event name — the message itself is
never sent to the engine (docs/discovery-brief.md: no proprietary data
leaves the Mac).
"""

from __future__ import annotations

import re
import shutil
import subprocess
import sys
from pathlib import Path

from magicpanel.desktop.emit import emit

# Words in a commit subject that suggest a bug was fixed. Matched locally;
# only the resulting event name ("bug_squashed" vs "git_commit") is emitted.
_FIX_PATTERN = re.compile(
    r"\b(fix(?:e[sd])?|bug(?:fix)?|hotfix|patch(?:e[sd])?|resolve[sd]?|"
    r"close[sd]?)\b",
    re.IGNORECASE,
)

HOOK_NAME = "post-commit"
# Marker so we can recognize (and safely re-install/overwrite) our own hook
# without clobbering an unrelated one the user already has.
_HOOK_MARKER = "# magicpanel-post-commit-hook"


def classify_commit(message: str) -> str:
    """Map a commit message to a semantic event name. Bug-fix-flavored
    commits become ``bug_squashed``; everything else is a plain
    ``git_commit``.
    """
    subject = message.strip().splitlines()[0] if message.strip() else ""
    if _FIX_PATTERN.search(subject):
        return "bug_squashed"
    return "git_commit"


def _last_commit_message() -> str:
    try:
        result = subprocess.run(
            ["git", "log", "-1", "--pretty=%B"],
            capture_output=True,
            text=True,
            check=True,
        )
        return result.stdout
    except (subprocess.CalledProcessError, FileNotFoundError):
        return ""


def run_commit_hook() -> None:
    """Entry point invoked by the installed post-commit hook."""
    event = classify_commit(_last_commit_message())
    emit(event)


def _emitter_command() -> str:
    """Resolve a command line the hook can use to reach the CLI, preferring
    an installed ``magicpanel`` on PATH and falling back to the current
    interpreter running the module.
    """
    exe = shutil.which("magicpanel")
    if exe:
        return f'"{exe}" _commit-hook'
    return f'"{sys.executable}" -m magicpanel.cli _commit-hook'


def _hook_script() -> str:
    return (
        "#!/bin/sh\n"
        f"{_HOOK_MARKER}\n"
        "# Emits a semantic event to the Magic Panel engine after each commit.\n"
        "# Best-effort: never blocks or fails the commit.\n"
        f"{_emitter_command()} >/dev/null 2>&1 || true\n"
    )


def install(repo_path: Path) -> Path:
    """Install the post-commit hook into ``repo_path``'s git repo. Returns
    the path to the written hook. Raises ValueError if the path is not a git
    repository, or FileExistsError if a foreign hook is already present.
    """
    hooks_dir = repo_path / ".git" / "hooks"
    if not (repo_path / ".git").is_dir():
        raise ValueError(f"{repo_path} is not a git repository (no .git directory)")
    hooks_dir.mkdir(parents=True, exist_ok=True)

    hook_file = hooks_dir / HOOK_NAME
    if hook_file.exists() and _HOOK_MARKER not in hook_file.read_text():
        raise FileExistsError(
            f"a non-magicpanel {HOOK_NAME} hook already exists at {hook_file}; "
            "leaving it untouched"
        )

    hook_file.write_text(_hook_script())
    hook_file.chmod(0o755)
    return hook_file


if __name__ == "__main__":
    run_commit_hook()
