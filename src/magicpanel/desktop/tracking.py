"""Track / untrack / setup: the easy install surface.

``track`` is the one command a user runs inside (or against) a repo to make
the panel watch it — it resolves the repo root, detects its GitHub slug,
installs the commit hook, and records it in the config. ``setup`` reconciles
everything already in the config (re-installs missing hooks). ``untrack``
reverses ``track``.
"""

from __future__ import annotations

import subprocess
from dataclasses import dataclass
from pathlib import Path

from magicpanel.desktop import config as config_mod
from magicpanel.desktop import githooks


@dataclass
class TrackResult:
    path: Path
    repo: str | None
    hook_installed: bool
    note: str = ""


def git_toplevel(path: Path) -> Path | None:
    """Resolve the git repo root containing ``path``, or None if not a repo."""
    try:
        result = subprocess.run(
            ["git", "-C", str(path), "rev-parse", "--show-toplevel"],
            capture_output=True,
            text=True,
            check=True,
        )
        return Path(result.stdout.strip())
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None


def detect_repo_slug(path: Path) -> str | None:
    """Best-effort GitHub ``owner/repo`` for the repo at ``path`` via gh."""
    try:
        result = subprocess.run(
            ["gh", "repo", "view", "--json", "nameWithOwner", "-q", ".nameWithOwner"],
            cwd=str(path),
            capture_output=True,
            text=True,
            check=True,
        )
        slug = result.stdout.strip()
        return slug or None
    except (subprocess.CalledProcessError, FileNotFoundError):
        return None


def track(path: Path, config_path: Path | None = None) -> TrackResult:
    """Add the repo at ``path`` to the watched set: install the commit hook,
    detect its GitHub slug, and record it in the config.
    """
    root = git_toplevel(path)
    if root is None:
        raise ValueError(f"{path} is not inside a git repository")

    repo = detect_repo_slug(root)

    hook_installed = False
    note = ""
    try:
        githooks.install(root)
        hook_installed = True
    except FileExistsError as exc:
        note = str(exc)

    cfg = config_mod.load(config_path)
    cfg.upsert(config_mod.Project(path=root, repo=repo, git_hook=hook_installed))
    config_mod.save(cfg, config_path)

    return TrackResult(path=root, repo=repo, hook_installed=hook_installed, note=note)


def untrack(path: Path, config_path: Path | None = None) -> bool:
    """Remove the repo from the config and delete our commit hook (only if
    it's ours). Returns True if it was tracked.
    """
    root = git_toplevel(path) or path.resolve()

    hook = root / ".git" / "hooks" / githooks.HOOK_NAME
    if hook.exists() and githooks._HOOK_MARKER in hook.read_text():
        hook.unlink()

    cfg = config_mod.load(config_path)
    removed = cfg.remove(root)
    config_mod.save(cfg, config_path)
    return removed


def setup(config_path: Path | None = None) -> list[TrackResult]:
    """Reconcile the config: ensure every tracked project that wants a hook
    has one. Returns per-project results.
    """
    cfg = config_mod.load(config_path)
    results = []
    for project in cfg.projects:
        if not project.git_hook:
            results.append(TrackResult(project.path, project.repo, False, "hook disabled"))
            continue
        note = ""
        installed = False
        try:
            githooks.install(project.path)
            installed = True
        except (ValueError, FileExistsError) as exc:
            note = str(exc)
        results.append(TrackResult(project.path, project.repo, installed, note))
    return results
