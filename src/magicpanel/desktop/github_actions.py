"""GitHub Actions poller: turns CI/CD runs into semantic events.

Polls ``gh run list`` on an interval and emits an event whenever a workflow
run changes phase — started or finished — mapping the run to the engine's
CI/deploy vocabulary. Only the derived semantic event is emitted; workflow
names, branches, and run details stay on the Mac.

Design notes:
- The first poll *primes* state instead of replaying history: already-
  finished runs are recorded silently (no burst of stale celebrations),
  while runs already in progress do emit their start event so the panel
  reflects work that's live when the poller comes up.
- Runs are classified purely by workflow name (deploy/release/publish ->
  deploy; test -> tests; anything else -> build), because that's all the
  signal we have and it's cheap to override later.
"""

from __future__ import annotations

import json
import subprocess
import time
from dataclasses import dataclass

from magicpanel.desktop.emit import emit

_ACTIVE_STATUSES = {"queued", "in_progress", "waiting", "requested", "pending"}
_GH_FIELDS = "databaseId,status,conclusion,workflowName"


@dataclass(frozen=True)
class Run:
    id: int
    status: str
    conclusion: str
    workflow: str


class GhError(RuntimeError):
    """The `gh` CLI was missing, unauthenticated, or failed."""


def classify(workflow_name: str) -> str:
    """Bucket a workflow into 'deploy', 'tests', or 'build' by name."""
    name = workflow_name.lower()
    if any(word in name for word in ("deploy", "release", "publish")):
        return "deploy"
    if "test" in name:
        return "tests"
    return "build"


def _start_event(kind: str) -> str:
    return "deploy_started" if kind == "deploy" else "ci_build_started"


def _finish_events(kind: str, conclusion: str) -> list[str]:
    """Events for a completed run: always the matching finish/clear event,
    plus a celebration on success.
    """
    if kind == "deploy":
        # deploy_finished already clears the casting mood; nothing extra on
        # success, and a failed deploy is left to git/incident signals
        # rather than inferred here.
        return ["deploy_finished"]
    finish = ["ci_build_finished"]
    if conclusion == "success":
        finish.append("tests_passed" if kind == "tests" else "build_passed")
    return finish


class GitHubActionsPoller:
    def __init__(self, repo: str | None = None, limit: int = 30) -> None:
        self._repo = repo
        self._limit = limit
        # run id -> last phase we emitted for it: "started" or "finished".
        self._phase: dict[int, str] = {}
        self._primed = False

    def _fetch_runs(self) -> list[Run]:
        cmd = ["gh", "run", "list", "--limit", str(self._limit), "--json", _GH_FIELDS]
        if self._repo:
            cmd += ["--repo", self._repo]
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        except FileNotFoundError as exc:
            raise GhError("the `gh` CLI is not installed or not on PATH") from exc
        except subprocess.CalledProcessError as exc:
            raise GhError(exc.stderr.strip() or "`gh run list` failed") from exc

        runs = []
        for item in json.loads(result.stdout or "[]"):
            runs.append(
                Run(
                    id=int(item["databaseId"]),
                    status=item.get("status", ""),
                    conclusion=item.get("conclusion", "") or "",
                    workflow=item.get("workflowName", "") or "",
                )
            )
        return runs

    def poll_once(self) -> list[str]:
        """Fetch current runs and return the events their phase changes imply
        (already emitted as a side effect is *not* done here — see ``run``).
        """
        emitted: list[str] = []
        for run in self._fetch_runs():
            kind = classify(run.workflow)
            active = run.status in _ACTIVE_STATUSES
            prev = self._phase.get(run.id)

            if active and prev is None:
                # Newly seen active run. Suppress on the priming pass unless
                # it's genuinely live work to reflect (which, when active, it
                # is) — so we still surface in-progress runs at startup.
                self._phase[run.id] = "started"
                emitted.append(_start_event(kind))
            elif not active and prev != "finished":
                self._phase[run.id] = "finished"
                if self._primed:
                    emitted.extend(_finish_events(kind, run.conclusion))
                # On the priming pass, record finished runs silently so we
                # don't replay history as a burst of celebrations.
        self._primed = True
        return emitted

    def run(self, interval: float = 15.0, iterations: int | None = None) -> None:
        """Poll forever (or ``iterations`` times, for tests), emitting events."""
        watch_repos([self._repo], interval=interval, iterations=iterations)


def watch_repos(
    repos: list[str | None],
    interval: float = 15.0,
    iterations: int | None = None,
) -> None:
    """Poll one or more repos in a single process, emitting events as their
    runs change phase. ``None`` in ``repos`` means "the current directory's
    repo" (gh infers it). A failing repo logs and is retried next tick rather
    than taking the whole watcher down.
    """
    pollers = [GitHubActionsPoller(repo=repo) for repo in repos] or [
        GitHubActionsPoller()
    ]
    count = 0
    while iterations is None or count < iterations:
        for poller in pollers:
            try:
                for event in poller.poll_once():
                    emit(event)
            except GhError as exc:
                label = poller._repo or "current repo"
                print(f"magicpanel: github poll failed for {label}: {exc}", flush=True)
        count += 1
        if iterations is not None and count >= iterations:
            break
        time.sleep(interval)
