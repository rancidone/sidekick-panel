import asyncio
import tempfile
import uuid
from pathlib import Path

from magicpanel.events import EventServer, send_event


def _short_socket_path() -> Path:
    # AF_UNIX paths are limited to ~104 chars on macOS; pytest's tmp_path
    # is often too long, so use the system tempdir directly with a short
    # unique name instead.
    return Path(tempfile.gettempdir()) / f"mp-test-{uuid.uuid4().hex[:8]}.sock"


def test_server_dispatches_valid_event() -> None:
    async def scenario() -> list[dict]:
        socket_path = _short_socket_path()
        server = EventServer(socket_path)
        received: list[dict] = []
        server.on_event(lambda payload: received.append(payload))

        await server.start()
        serve_task = asyncio.create_task(server.serve_forever())
        try:
            await send_event("tests_passed", socket_path, foo="bar")
            await asyncio.sleep(0.05)
        finally:
            serve_task.cancel()
            server.close()
        return received

    received = asyncio.run(scenario())
    assert received == [{"event": "tests_passed", "foo": "bar"}]


def test_server_ignores_malformed_lines() -> None:
    async def scenario() -> list[dict]:
        socket_path = _short_socket_path()
        server = EventServer(socket_path)
        received: list[dict] = []
        server.on_event(lambda payload: received.append(payload))

        await server.start()
        serve_task = asyncio.create_task(server.serve_forever())
        try:
            reader, writer = await asyncio.open_unix_connection(path=str(socket_path))
            writer.write(b"not json\n")
            writer.write(b'{"no_event_field": true}\n')
            await writer.drain()
            writer.close()
            await writer.wait_closed()
            await asyncio.sleep(0.05)
        finally:
            serve_task.cancel()
            server.close()
        return received

    received = asyncio.run(scenario())
    assert received == []
