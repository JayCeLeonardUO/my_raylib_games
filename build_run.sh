#!/bin/bash
cd "$(dirname "$0")"

TARGET="${1:-example_game}"
DEBUG_MODE="${2:-}"

# If only "gdb" or "gdbgui" passed, use default target
if [[ "$1" == "gdb" || "$1" == "gdbgui" ]]; then
    DEBUG_MODE="$1"
    TARGET="${2:-example_game}"
fi

TMUX_DEBUG_SESSION="gdbgui-debug"
TMUX_RUN_SESSION="game-run"

if [[ "$DEBUG_MODE" == "gdb" ]]; then
    cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --target "$TARGET" || exit 1
    gdb -x .gdbinit ./build/"$TARGET"
elif [[ "$DEBUG_MODE" == "gdbgui" ]]; then
    cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --target "$TARGET" || exit 1

    # Kill existing gdbgui server if running (ignore exit code)
    # Use pgrep/kill to avoid killing this script
    for pid in $(pgrep -f "python.*gdbgui" 2>/dev/null); do
        kill "$pid" 2>/dev/null || true
    done
    sleep 0.3

    # Kill existing session and create fresh one
    tmux kill-session -t "$TMUX_DEBUG_SESSION" 2>/dev/null || true
    sleep 0.2
    tmux new-session -d -s "$TMUX_DEBUG_SESSION" "gdbgui ./build/$TARGET"

    echo "gdbgui started in tmux session '$TMUX_DEBUG_SESSION'"
    echo "Open http://127.0.0.1:5000 in your browser"
    exit 0
else
    cmake -B build && cmake --build build --target "$TARGET" || exit 1

    # Kill existing session and create fresh one
    tmux kill-session -t "$TMUX_RUN_SESSION" 2>/dev/null || true
    sleep 0.2
    tmux new-session -d -s "$TMUX_RUN_SESSION" "./build/$TARGET"

    echo "Running in tmux session '$TMUX_RUN_SESSION'"
    exit 0
fi
