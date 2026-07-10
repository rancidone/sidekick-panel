"""Desktop config: the set of projects the panel watches.

A single TOML file (``~/.config/magicpanel/config.toml`` by default) lists
the tracked projects. Each project is a local repo path plus an optional
GitHub ``owner/repo`` slug for CI polling. This is the one place that
answers "what is the panel watching?" — the ``track`` command edits it, and
``setup``/``watch`` read it.

Reading uses stdlib ``tomllib``; writing is a small hand-rolled serializer
(the stdlib has no TOML writer) scoped to exactly this schema.
"""

from __future__ import annotations

import os
import tomllib
from dataclasses import dataclass, field
from pathlib import Path

DEFAULT_GITHUB_INTERVAL = 15.0
DEFAULT_HEARTBEAT_INTERVAL = 10.0
DEFAULT_IDLE_TIMEOUT = 60.0


def default_config_path() -> Path:
    base = os.environ.get("XDG_CONFIG_HOME")
    root = Path(base) if base else Path.home() / ".config"
    return root / "magicpanel" / "config.toml"


@dataclass
class Project:
    path: Path
    repo: str | None = None  # GitHub "owner/repo" for CI polling, if any
    git_hook: bool = True  # whether the commit hook should be installed


@dataclass
class Config:
    projects: list[Project] = field(default_factory=list)
    github_interval: float = DEFAULT_GITHUB_INTERVAL
    heartbeat_enabled: bool = True
    heartbeat_interval: float = DEFAULT_HEARTBEAT_INTERVAL
    idle_timeout: float = DEFAULT_IDLE_TIMEOUT

    def find(self, path: Path) -> Project | None:
        path = path.resolve()
        for project in self.projects:
            if project.path == path:
                return project
        return None

    def upsert(self, project: Project) -> None:
        """Add the project, replacing any existing entry for the same path."""
        project.path = project.path.resolve()
        self.projects = [p for p in self.projects if p.path != project.path]
        self.projects.append(project)

    def remove(self, path: Path) -> bool:
        """Drop the project at ``path``. Returns True if one was removed."""
        path = path.resolve()
        before = len(self.projects)
        self.projects = [p for p in self.projects if p.path != path]
        return len(self.projects) < before

    def github_repos(self) -> list[str]:
        return [p.repo for p in self.projects if p.repo]


def load(path: Path | None = None) -> Config:
    """Load config, returning an empty Config if the file doesn't exist."""
    path = path or default_config_path()
    if not path.exists():
        return Config()
    data = tomllib.loads(path.read_text())

    github = data.get("github", {})
    interval = float(github.get("interval", DEFAULT_GITHUB_INTERVAL))

    hb = data.get("heartbeat", {})

    projects = []
    for entry in data.get("project", []):
        projects.append(
            Project(
                path=Path(entry["path"]),
                repo=entry.get("repo"),
                git_hook=entry.get("git_hook", True),
            )
        )
    return Config(
        projects=projects,
        github_interval=interval,
        heartbeat_enabled=hb.get("enabled", True),
        heartbeat_interval=float(hb.get("interval", DEFAULT_HEARTBEAT_INTERVAL)),
        idle_timeout=float(hb.get("idle_timeout", DEFAULT_IDLE_TIMEOUT)),
    )


def _quote(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def dumps(config: Config) -> str:
    lines = ["[github]", f"interval = {config.github_interval}", ""]
    lines += [
        "[heartbeat]",
        f"enabled = {'true' if config.heartbeat_enabled else 'false'}",
        f"interval = {config.heartbeat_interval}",
        f"idle_timeout = {config.idle_timeout}",
        "",
    ]
    for project in config.projects:
        lines.append("[[project]]")
        lines.append(f"path = {_quote(str(project.path))}")
        if project.repo:
            lines.append(f"repo = {_quote(project.repo)}")
        lines.append(f"git_hook = {'true' if project.git_hook else 'false'}")
        lines.append("")
    return "\n".join(lines).rstrip("\n") + "\n"


def save(config: Config, path: Path | None = None) -> Path:
    path = path or default_config_path()
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(dumps(config))
    return path
