"""CLI entrypoint: sends events to the engine's local socket.

Stands in for what will eventually run on the Mac.
"""

from __future__ import annotations

import argparse
import asyncio
import sys
from pathlib import Path

from magicpanel.events import send_event

# Kept in sync with the reaction rules actually wired in each scene
# (see scenes/desk_spirit.py RULES, scenes/arcane_tree.py) rather than the
# full aspirational vocabulary in docs/ — this is what the engine responds
# to today.
KNOWN_EVENTS = {
    "git_commit": "Desk Spirit: happy (brief)",
    "tests_passed": "Desk Spirit: happy (brief)",
    "build_passed": "Desk Spirit: happy (brief)",
    "bug_squashed": "Desk Spirit: bug kill animation (brief)",
    "production_incident": "Desk Spirit: angry (until incident_resolved)",
    "incident_resolved": "Desk Spirit: clears angry",
    "deploy_started": "Desk Spirit: casting spells (until deploy_finished)",
    "deploy_finished": "Desk Spirit: clears casting spells",
    "ci_build_started": "Desk Spirit: breathing fire (until ci_build_finished)",
    "ci_build_finished": "Desk Spirit: clears breathing fire",
}

KNOWN_SCENES = ["desk_spirit", "arcane_tree"]


def _parse_fields(args: list[str]) -> dict[str, str]:
    fields = {}
    for arg in args:
        if "=" not in arg:
            print(f"ignoring malformed field '{arg}' (expected key=value)", file=sys.stderr)
            continue
        key, value = arg.split("=", 1)
        fields[key] = value
    return fields


def _send(event: str, **fields: str) -> None:
    try:
        asyncio.run(send_event(event, **fields))
    except (ConnectionRefusedError, FileNotFoundError):
        print(
            "could not reach the engine (is `magicpanel-engine` running?)",
            file=sys.stderr,
        )
        sys.exit(1)


def _cmd_send(args: argparse.Namespace) -> None:
    if args.event not in KNOWN_EVENTS:
        print(
            f"note: '{args.event}' isn't a known event (see `magicpanel events`); "
            "sending it anyway, the engine will just ignore it if unrecognized.",
            file=sys.stderr,
        )
    _send(args.event, **_parse_fields(args.fields))


def _cmd_scene(args: argparse.Namespace) -> None:
    _send("switch_scene", to=args.name)


def _cmd_events(_args: argparse.Namespace) -> None:
    for name, description in KNOWN_EVENTS.items():
        print(f"  {name:<24} {description}")


def _cmd_scenes(_args: argparse.Namespace) -> None:
    for name in KNOWN_SCENES:
        print(f"  {name}")


def _cmd_install_git_hooks(args: argparse.Namespace) -> None:
    from magicpanel.desktop import githooks

    repo = Path(args.path).resolve()
    try:
        hook_file = githooks.install(repo)
    except (ValueError, FileExistsError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(1)
    print(f"installed post-commit hook: {hook_file}")
    print("commits in this repo will now nudge the panel (fixes -> bug_squashed).")


def _cmd_commit_hook(_args: argparse.Namespace) -> None:
    # Invoked by the installed git hook, not meant for direct use.
    from magicpanel.desktop.githooks import run_commit_hook

    run_commit_hook()


def _cmd_watch(args: argparse.Namespace) -> None:
    if args.source == "github":
        from magicpanel.desktop import config as config_mod
        from magicpanel.desktop.github_actions import watch_repos

        cfg = config_mod.load()
        if args.repo:
            repos: list = [args.repo]
        elif cfg.github_repos():
            repos = list(cfg.github_repos())
        else:
            # Nothing configured and no override: watch the current repo.
            repos = [None]

        interval = args.interval if args.interval is not None else cfg.github_interval
        where = ", ".join(r or "current repo" for r in repos)
        print(f"watching GitHub Actions for {where} every {interval}s (ctrl-c to stop)")
        try:
            watch_repos(repos, interval=interval)
        except KeyboardInterrupt:
            print("\nstopped.")


def _cmd_serve(_args: argparse.Namespace) -> None:
    import threading

    from magicpanel.desktop import config as config_mod
    from magicpanel.desktop import heartbeat
    from magicpanel.desktop.github_actions import watch_repos

    cfg = config_mod.load()

    if cfg.heartbeat_enabled:
        hb_thread = threading.Thread(
            target=heartbeat.run_heartbeat,
            args=(cfg.heartbeat_interval, cfg.idle_timeout),
            daemon=True,
        )
        hb_thread.start()
        print(
            f"heartbeat: every {cfg.heartbeat_interval}s while active "
            f"(sleeps after {cfg.idle_timeout}s idle or on screen lock)",
            flush=True,
        )

    repos = list(cfg.github_repos()) or [None]
    where = ", ".join(r or "current repo" for r in repos)
    print(
        f"watching GitHub Actions for {where} every {cfg.github_interval}s (ctrl-c to stop)",
        flush=True,
    )
    try:
        watch_repos(repos, interval=cfg.github_interval)
    except KeyboardInterrupt:
        print("\nstopped.")


def _cmd_track(args: argparse.Namespace) -> None:
    from magicpanel.desktop import tracking

    try:
        result = tracking.track(Path(args.path))
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(1)
    print(f"tracking {result.path}")
    print(f"  github repo: {result.repo or '(none detected — git commits only)'}")
    if result.hook_installed:
        print("  commit hook: installed")
    else:
        print(f"  commit hook: not installed ({result.note})")
    print("run `magicpanel setup` / `magicpanel service install` to (re)wire everything.")


def _cmd_untrack(args: argparse.Namespace) -> None:
    from magicpanel.desktop import tracking

    removed = tracking.untrack(Path(args.path))
    print("untracked." if removed else "was not tracked (removed any hook anyway).")


def _cmd_setup(_args: argparse.Namespace) -> None:
    from magicpanel.desktop import config as config_mod
    from magicpanel.desktop import tracking

    results = tracking.setup()
    if not results:
        print("no projects tracked yet. Use `magicpanel track` in a repo.")
        print(f"config: {config_mod.default_config_path()}")
        return
    for r in results:
        state = "hook ok" if r.hook_installed else f"skipped ({r.note})"
        print(f"  {r.path} [{r.repo or 'no github'}] — {state}")


def _cmd_service(args: argparse.Namespace) -> None:
    if sys.platform != "darwin":
        print("the launchd service is macOS-only.", file=sys.stderr)
        sys.exit(1)
    from magicpanel.desktop import service

    if args.action == "install":
        path = service.install()
        print(f"installed and loaded LaunchAgent: {path}")
        print(f"logs: {service.log_dir()}")
    elif args.action == "uninstall":
        print("uninstalled." if service.uninstall() else "was not installed.")
    elif args.action == "status":
        print(service.status())


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="magicpanel",
        description="Send semantic events to the Magic Panel engine.",
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    send_parser = subparsers.add_parser("send", help="send a semantic event")
    send_parser.add_argument("event", help="event name, e.g. tests_passed")
    send_parser.add_argument(
        "fields", nargs="*", help="optional key=value fields, e.g. severity=high"
    )
    send_parser.set_defaults(func=_cmd_send)

    scene_parser = subparsers.add_parser("scene", help="switch the active scene")
    scene_parser.add_argument("name", choices=KNOWN_SCENES)
    scene_parser.set_defaults(func=_cmd_scene)

    events_parser = subparsers.add_parser("events", help="list known events")
    events_parser.set_defaults(func=_cmd_events)

    scenes_parser = subparsers.add_parser("scenes", help="list known scenes")
    scenes_parser.set_defaults(func=_cmd_scenes)

    watch_parser = subparsers.add_parser(
        "watch", help="watch a desktop signal source and emit events"
    )
    watch_parser.add_argument("source", choices=["github"], help="signal source to watch")
    watch_parser.add_argument(
        "--repo",
        help="OWNER/REPO to watch (default: tracked repos from config, "
        "else the current directory's repo)",
    )
    watch_parser.add_argument(
        "--interval",
        type=float,
        default=None,
        help="poll interval in seconds (default: config value, else 15)",
    )
    watch_parser.set_defaults(func=_cmd_watch)

    serve_parser = subparsers.add_parser(
        "serve",
        help="run the always-on desktop watcher: heartbeat + GitHub polling",
    )
    serve_parser.set_defaults(func=_cmd_serve)

    track_parser = subparsers.add_parser(
        "track", help="track a repo: install its commit hook and add it to config"
    )
    track_parser.add_argument(
        "path", nargs="?", default=".", help="path inside the repo (default: .)"
    )
    track_parser.set_defaults(func=_cmd_track)

    untrack_parser = subparsers.add_parser(
        "untrack", help="stop tracking a repo (removes hook and config entry)"
    )
    untrack_parser.add_argument(
        "path", nargs="?", default=".", help="path inside the repo (default: .)"
    )
    untrack_parser.set_defaults(func=_cmd_untrack)

    setup_parser = subparsers.add_parser(
        "setup", help="reconcile config: (re)install hooks for all tracked repos"
    )
    setup_parser.set_defaults(func=_cmd_setup)

    service_parser = subparsers.add_parser(
        "service", help="manage the always-on watcher LaunchAgent (macOS)"
    )
    service_parser.add_argument(
        "action", choices=["install", "uninstall", "status"]
    )
    service_parser.set_defaults(func=_cmd_service)

    hooks_parser = subparsers.add_parser(
        "install-git-hooks", help="install a post-commit hook into a repo"
    )
    hooks_parser.add_argument(
        "path", nargs="?", default=".", help="path to the git repo (default: .)"
    )
    hooks_parser.set_defaults(func=_cmd_install_git_hooks)

    commit_hook_parser = subparsers.add_parser("_commit-hook")
    commit_hook_parser.set_defaults(func=_cmd_commit_hook)

    return parser


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
