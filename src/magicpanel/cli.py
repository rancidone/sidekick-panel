"""CLI entrypoint: sends a single event to the engine's local socket.

Stands in for what will eventually run on the Mac. Usage:

    magicpanel <event_name> [field=value ...]
    magicpanel scene <scene_name>
"""

from __future__ import annotations

import asyncio
import sys

from magicpanel.events import send_event


def _parse_fields(args: list[str]) -> dict[str, str]:
    fields = {}
    for arg in args:
        if "=" not in arg:
            print(f"ignoring malformed field '{arg}' (expected key=value)", file=sys.stderr)
            continue
        key, value = arg.split("=", 1)
        fields[key] = value
    return fields


def main() -> None:
    if len(sys.argv) < 2:
        print("usage: magicpanel <event_name> [field=value ...]", file=sys.stderr)
        sys.exit(1)

    event_name = sys.argv[1]

    if event_name == "scene":
        if len(sys.argv) < 3:
            print("usage: magicpanel scene <scene_name>", file=sys.stderr)
            sys.exit(1)
        event_name = "switch_scene"
        fields: dict[str, str] = {"to": sys.argv[2]}
    else:
        fields = _parse_fields(sys.argv[2:])

    try:
        asyncio.run(send_event(event_name, **fields))
    except (ConnectionRefusedError, FileNotFoundError):
        print(
            "could not reach the engine (is `magicpanel-engine` running?)",
            file=sys.stderr,
        )
        sys.exit(1)


if __name__ == "__main__":
    main()
