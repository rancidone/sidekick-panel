#!/usr/bin/env bash
set -u

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="$ROOT/cpp/build"
HOST="$BUILD_DIR/magicpanel_host"
POLL_SECONDS="${MAGICPANEL_HOT_RELOAD_POLL_SECONDS:-1}"

host_pid=""
last_stamp=""

stop_host() {
  if [[ -n "$host_pid" ]] && kill -0 "$host_pid" 2>/dev/null; then
    kill "$host_pid" 2>/dev/null || true
    wait "$host_pid" 2>/dev/null || true
  fi
  host_pid=""
}

cleanup() {
  stop_host
}

interrupt() {
  cleanup
  exit 0
}

build_host() {
  cmake --build "$BUILD_DIR" --target magicpanel_host
}

launch_host() {
  stop_host
  "$HOST" "$@" &
  host_pid="$!"
}

source_stamp() {
  find "$ROOT/cpp/src" "$ROOT/cpp/include" "$ROOT/cpp/generated" "$ROOT/cpp/CMakeLists.txt" \
    -type f \( -name '*.cpp' -o -name '*.h' -o -name 'CMakeLists.txt' \) \
    -print0 |
    xargs -0 stat -f '%m %N' 2>/dev/null |
    sort |
    shasum
}

trap interrupt INT TERM
trap cleanup EXIT

if [[ ! -d "$BUILD_DIR" ]]; then
  cmake -S "$ROOT/cpp" -B "$BUILD_DIR"
fi

if ! build_host; then
  echo "Initial build failed; waiting for changes." >&2
else
  launch_host "$@"
fi

last_stamp="$(source_stamp)"
echo "Watching C++ engine sources. Press Ctrl-C to stop."

while true; do
  sleep "$POLL_SECONDS"
  next_stamp="$(source_stamp)"
  if [[ "$next_stamp" == "$last_stamp" ]]; then
    if [[ -n "$host_pid" ]] && ! kill -0 "$host_pid" 2>/dev/null; then
      wait "$host_pid" 2>/dev/null || true
      host_pid=""
      echo "Host exited; waiting for source changes before relaunch."
    fi
    continue
  fi

  last_stamp="$next_stamp"
  echo "Change detected; rebuilding magicpanel_host..."
  if build_host; then
    launch_host "$@"
  else
    echo "Build failed; keeping watcher alive." >&2
  fi
done
