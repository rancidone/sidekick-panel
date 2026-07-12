"""Local event transport: a Unix domain socket carrying newline-delimited
JSON events.

Delivery is best-effort, not durable (docs/00-engine.md) — there is no
outbox or retry on the sender side. This module owns the wire format so
the transport can later be swapped for BLE without scene/engine code
changing.
"""

from __future__ import annotations

import asyncio
import json
from pathlib import Path
from typing import Awaitable, Callable

# Fixed rather than tempfile.gettempdir(): on macOS that resolves to a
# per-process /var/folders/.../T path, which would silently disagree with
# the C++ engine's hardcoded /tmp/magicpanel.sock (see cpp/src/host_main.cpp)
# and every event would just fail to be delivered.
DEFAULT_SOCKET_PATH = Path("/tmp/magicpanel.sock")

EventHandler = Callable[[dict], Awaitable[None] | None]


class EventServer:
    """Asyncio Unix-socket server that dispatches parsed JSON events."""

    def __init__(self, socket_path: Path = DEFAULT_SOCKET_PATH) -> None:
        self._socket_path = socket_path
        self._server: asyncio.base_events.Server | None = None
        self._handlers: list[EventHandler] = []

    def on_event(self, handler: EventHandler) -> None:
        self._handlers.append(handler)

    async def start(self) -> None:
        if self._socket_path.exists():
            self._socket_path.unlink()
        self._server = await asyncio.start_unix_server(
            self._handle_connection, path=str(self._socket_path)
        )

    async def serve_forever(self) -> None:
        if self._server is None:
            await self.start()
        assert self._server is not None
        async with self._server:
            await self._server.serve_forever()

    async def _handle_connection(
        self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter
    ) -> None:
        try:
            while True:
                line = await reader.readline()
                if not line:
                    break
                await self._dispatch_line(line)
        finally:
            writer.close()

    async def _dispatch_line(self, line: bytes) -> None:
        try:
            payload = json.loads(line.decode("utf-8").strip())
        except (UnicodeDecodeError, json.JSONDecodeError):
            return
        if not isinstance(payload, dict) or "event" not in payload:
            return
        for handler in self._handlers:
            result = handler(payload)
            if asyncio.iscoroutine(result):
                await result

    def close(self) -> None:
        if self._server is not None:
            self._server.close()
        if self._socket_path.exists():
            self._socket_path.unlink()


def encode_event(event: str, **fields: object) -> bytes:
    """Encode a single event as one newline-delimited JSON line."""
    payload = {"event": event, **fields}
    return (json.dumps(payload) + "\n").encode("utf-8")


async def send_event(
    event: str, socket_path: Path = DEFAULT_SOCKET_PATH, **fields: object
) -> None:
    """Connect, send one event line, and disconnect. Best-effort: raises on
    failure to connect rather than queueing for retry (see module docstring).
    """
    reader, writer = await asyncio.open_unix_connection(path=str(socket_path))
    try:
        writer.write(encode_event(event, **fields))
        await writer.drain()
        # Record only after a successful write, so the log reflects events
        # that actually reached the socket.
        from magicpanel import eventlog

        eventlog.record("sent", event, dict(fields) or None)
    finally:
        writer.close()
        await writer.wait_closed()
