import subprocess
from pathlib import Path

import pytest

from magicpanel.desktop import activity, config as config_mod
from magicpanel.desktop import github_actions, githooks, heartbeat, service, tracking
from magicpanel.desktop.config import Config, Project
from magicpanel.desktop.github_actions import GitHubActionsPoller, Run


# --- git commit classification -------------------------------------------

@pytest.mark.parametrize(
    "message, expected",
    [
        ("Fix crash on startup", "bug_squashed"),
        ("fixes #42", "bug_squashed"),
        ("Resolve flaky test", "bug_squashed"),
        ("hotfix: null deref", "bug_squashed"),
        ("Add wrapper to run app", "git_commit"),
        ("Refactor scene manager", "git_commit"),
        ("", "git_commit"),
        # Only the subject (first line) is inspected.
        ("Add feature\n\nthis fixes nothing conceptually", "git_commit"),
    ],
)
def test_classify_commit(message: str, expected: str) -> None:
    assert githooks.classify_commit(message) == expected


# --- git hook installation ------------------------------------------------

def test_install_writes_executable_hook(tmp_path: Path) -> None:
    (tmp_path / ".git" / "hooks").mkdir(parents=True)
    hook = githooks.install(tmp_path)
    assert hook.exists()
    assert hook.stat().st_mode & 0o111  # executable
    assert githooks._HOOK_MARKER in hook.read_text()


def test_install_rejects_non_repo(tmp_path: Path) -> None:
    with pytest.raises(ValueError):
        githooks.install(tmp_path)


def test_install_is_idempotent(tmp_path: Path) -> None:
    (tmp_path / ".git" / "hooks").mkdir(parents=True)
    githooks.install(tmp_path)
    # Re-installing over our own marker is fine.
    githooks.install(tmp_path)


def test_install_preserves_foreign_hook(tmp_path: Path) -> None:
    hooks = tmp_path / ".git" / "hooks"
    hooks.mkdir(parents=True)
    (hooks / "post-commit").write_text("#!/bin/sh\necho not ours\n")
    with pytest.raises(FileExistsError):
        githooks.install(tmp_path)


# --- github actions classification ---------------------------------------

@pytest.mark.parametrize(
    "workflow, kind",
    [
        ("Deploy to prod", "deploy"),
        ("Release", "deploy"),
        ("Publish package", "deploy"),
        ("Unit Tests", "tests"),
        ("CI", "build"),
        ("Build", "build"),
    ],
)
def test_classify_workflow(workflow: str, kind: str) -> None:
    assert github_actions.classify(workflow) == kind


# --- github actions poller transitions -----------------------------------

def _poller_with(fetches: list[list[Run]]) -> GitHubActionsPoller:
    """A poller whose _fetch_runs yields each canned list in turn."""
    poller = GitHubActionsPoller()
    calls = iter(fetches)
    poller._fetch_runs = lambda: next(calls)  # type: ignore[assignment]
    return poller


def test_priming_pass_suppresses_finished_history() -> None:
    poller = _poller_with([[Run(1, "completed", "success", "CI")]])
    # First poll primes: an already-finished run must not celebrate.
    assert poller.poll_once() == []


def test_priming_pass_surfaces_in_progress_runs() -> None:
    poller = _poller_with([[Run(1, "in_progress", "", "CI")]])
    assert poller.poll_once() == ["ci_build_started"]


def test_build_lifecycle_emits_start_then_finish() -> None:
    poller = _poller_with(
        [
            [],  # prime with nothing
            [Run(1, "in_progress", "", "CI")],
            [Run(1, "completed", "success", "CI")],
        ]
    )
    assert poller.poll_once() == []
    assert poller.poll_once() == ["ci_build_started"]
    assert poller.poll_once() == ["ci_build_finished", "build_passed"]


def test_tests_success_emits_tests_passed() -> None:
    poller = _poller_with(
        [[], [Run(9, "completed", "success", "Unit Tests")]]
    )
    poller.poll_once()
    assert poller.poll_once() == ["ci_build_finished", "tests_passed"]


def test_deploy_lifecycle() -> None:
    poller = _poller_with(
        [
            [],
            [Run(2, "in_progress", "", "Deploy prod")],
            [Run(2, "completed", "success", "Deploy prod")],
        ]
    )
    poller.poll_once()
    assert poller.poll_once() == ["deploy_started"]
    assert poller.poll_once() == ["deploy_finished"]


def test_failed_build_finishes_without_celebration() -> None:
    poller = _poller_with(
        [[], [Run(3, "in_progress", "", "CI")], [Run(3, "completed", "failure", "CI")]]
    )
    poller.poll_once()
    poller.poll_once()
    assert poller.poll_once() == ["ci_build_finished"]


def test_run_is_not_re_emitted_when_stable() -> None:
    poller = _poller_with(
        [[], [Run(4, "in_progress", "", "CI")], [Run(4, "in_progress", "", "CI")]]
    )
    poller.poll_once()
    assert poller.poll_once() == ["ci_build_started"]
    assert poller.poll_once() == []  # unchanged -> silent


# --- config round-trip ----------------------------------------------------

def test_config_roundtrip(tmp_path: Path) -> None:
    cfg = Config(github_interval=30.0)
    cfg.upsert(Project(path=tmp_path / "a", repo="owner/a"))
    cfg.upsert(Project(path=tmp_path / "b", repo=None, git_hook=False))
    p = config_mod.save(cfg, tmp_path / "config.toml")

    loaded = config_mod.load(p)
    assert loaded.github_interval == 30.0
    assert {pr.path for pr in loaded.projects} == {
        (tmp_path / "a").resolve(),
        (tmp_path / "b").resolve(),
    }
    assert loaded.github_repos() == ["owner/a"]
    assert loaded.find(tmp_path / "b").git_hook is False


def test_config_upsert_replaces_same_path(tmp_path: Path) -> None:
    cfg = Config()
    cfg.upsert(Project(path=tmp_path, repo="old/x"))
    cfg.upsert(Project(path=tmp_path, repo="new/x"))
    assert len(cfg.projects) == 1
    assert cfg.find(tmp_path).repo == "new/x"


def test_config_missing_file_is_empty(tmp_path: Path) -> None:
    cfg = config_mod.load(tmp_path / "nope.toml")
    assert cfg.projects == []


# --- tracking (uses a real temp git repo, gh may be absent) ---------------

@pytest.fixture
def git_repo(tmp_path: Path) -> Path:
    subprocess.run(["git", "init", "-q", str(tmp_path)], check=True)
    return tmp_path


def test_track_installs_hook_and_records(git_repo: Path, tmp_path: Path) -> None:
    cfg_path = tmp_path / "config.toml"
    result = tracking.track(git_repo, config_path=cfg_path)
    assert result.hook_installed
    assert (git_repo / ".git" / "hooks" / "post-commit").exists()

    cfg = config_mod.load(cfg_path)
    assert cfg.find(git_repo) is not None


def test_track_rejects_non_repo(tmp_path: Path) -> None:
    with pytest.raises(ValueError):
        tracking.track(tmp_path / "not-a-repo", config_path=tmp_path / "c.toml")


def test_untrack_removes_hook_and_entry(git_repo: Path, tmp_path: Path) -> None:
    cfg_path = tmp_path / "config.toml"
    tracking.track(git_repo, config_path=cfg_path)
    removed = tracking.untrack(git_repo, config_path=cfg_path)
    assert removed
    assert not (git_repo / ".git" / "hooks" / "post-commit").exists()
    assert config_mod.load(cfg_path).find(git_repo) is None


def test_untrack_leaves_foreign_hook(git_repo: Path, tmp_path: Path) -> None:
    hook = git_repo / ".git" / "hooks" / "post-commit"
    hook.parent.mkdir(parents=True, exist_ok=True)
    hook.write_text("#!/bin/sh\necho foreign\n")
    tracking.untrack(git_repo, config_path=tmp_path / "c.toml")
    assert hook.exists()  # not ours -> untouched


def test_setup_reinstalls_missing_hooks(git_repo: Path, tmp_path: Path) -> None:
    cfg_path = tmp_path / "config.toml"
    tracking.track(git_repo, config_path=cfg_path)
    (git_repo / ".git" / "hooks" / "post-commit").unlink()  # simulate loss

    results = tracking.setup(config_path=cfg_path)
    assert results and results[0].hook_installed
    assert (git_repo / ".git" / "hooks" / "post-commit").exists()


# --- launchd plist --------------------------------------------------------

def test_plist_renders_valid_xml() -> None:
    import plistlib

    xml = service._render_plist()
    parsed = plistlib.loads(xml.encode())
    assert parsed["Label"] == service.LABEL
    assert parsed["RunAtLoad"] is True
    assert parsed["KeepAlive"] is True
    assert parsed["ProgramArguments"][-1] == "serve"


# --- activity sensing -----------------------------------------------------

def test_is_active_false_when_locked(monkeypatch) -> None:
    monkeypatch.setattr(activity, "screen_locked", lambda: True)
    monkeypatch.setattr(activity, "idle_seconds", lambda: 0.0)
    assert activity.is_active(60) is False


def test_is_active_by_idle_threshold(monkeypatch) -> None:
    monkeypatch.setattr(activity, "screen_locked", lambda: False)
    monkeypatch.setattr(activity, "idle_seconds", lambda: 10.0)
    assert activity.is_active(60) is True
    monkeypatch.setattr(activity, "idle_seconds", lambda: 120.0)
    assert activity.is_active(60) is False


def test_is_active_defaults_active_when_unknown(monkeypatch) -> None:
    # Non-macOS / unreadable signals: stay awake rather than falsely sleep.
    monkeypatch.setattr(activity, "screen_locked", lambda: None)
    monkeypatch.setattr(activity, "idle_seconds", lambda: None)
    assert activity.is_active(60) is True


# --- heartbeat loop -------------------------------------------------------

def test_heartbeat_emits_only_while_active(monkeypatch) -> None:
    beats: list[str] = []
    monkeypatch.setattr(heartbeat, "emit", lambda event: beats.append(event))
    monkeypatch.setattr(heartbeat.time, "sleep", lambda _s: None)

    states = iter([True, False, True])
    monkeypatch.setattr(activity, "is_active", lambda _t: next(states))

    heartbeat.run_heartbeat(interval=0, idle_timeout=60, iterations=3)
    assert beats == ["heartbeat", "heartbeat"]  # skipped the idle tick


def test_heartbeat_interval_below_liveness_timeout() -> None:
    from magicpanel.liveness import DEFAULT_TIMEOUT_SECONDS

    assert heartbeat.DEFAULT_INTERVAL < DEFAULT_TIMEOUT_SECONDS


# --- heartbeat config -----------------------------------------------------

def test_config_heartbeat_roundtrip(tmp_path: Path) -> None:
    cfg = Config(heartbeat_enabled=False, heartbeat_interval=5.0, idle_timeout=30.0)
    p = config_mod.save(cfg, tmp_path / "c.toml")
    loaded = config_mod.load(p)
    assert loaded.heartbeat_enabled is False
    assert loaded.heartbeat_interval == 5.0
    assert loaded.idle_timeout == 30.0
