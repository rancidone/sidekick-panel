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

# Las Vegas, NV — used until the user configures their own location.
DEFAULT_WEATHER_LAT = 36.1699
DEFAULT_WEATHER_LON = -115.1398
DEFAULT_WEATHER_CONDITIONS_INTERVAL = 600.0
DEFAULT_WEATHER_SOLAR_INTERVAL = 60.0


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
    weather_enabled: bool = True
    weather_lat: float = DEFAULT_WEATHER_LAT
    weather_lon: float = DEFAULT_WEATHER_LON
    weather_conditions_interval: float = DEFAULT_WEATHER_CONDITIONS_INTERVAL
    weather_solar_interval: float = DEFAULT_WEATHER_SOLAR_INTERVAL

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
    weather = data.get("weather", {})

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
        weather_enabled=weather.get("enabled", True),
        weather_lat=float(weather.get("lat", DEFAULT_WEATHER_LAT)),
        weather_lon=float(weather.get("lon", DEFAULT_WEATHER_LON)),
        weather_conditions_interval=float(
            weather.get("conditions_interval", DEFAULT_WEATHER_CONDITIONS_INTERVAL)
        ),
        weather_solar_interval=float(
            weather.get("solar_interval", DEFAULT_WEATHER_SOLAR_INTERVAL)
        ),
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
    lines += [
        "[weather]",
        f"enabled = {'true' if config.weather_enabled else 'false'}",
        f"lat = {config.weather_lat}",
        f"lon = {config.weather_lon}",
        f"conditions_interval = {config.weather_conditions_interval}",
        f"solar_interval = {config.weather_solar_interval}",
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
