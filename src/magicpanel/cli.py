"""CLI entrypoint: sends events to the engine's local socket.

Stands in for what will eventually run on the Mac.
"""

from __future__ import annotations

import argparse
import asyncio
import sys

from magicpanel.events import send_event

# Kept in sync with the reaction rules actually wired in each scene
# (see scenes/desk_spirit.py RULES, scenes/arcane_tree.py) rather than the
# full aspirational vocabulary in docs/ — this is what the engine responds
# to today.
KNOWN_EVENTS = {
    "tests_passed": "Desk Spirit: happy (brief)",
    "build_passed": "Desk Spirit: happy (brief)",
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

    return parser


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
