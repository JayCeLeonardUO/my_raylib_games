#!/bin/bash
# Double-click launcher for entity_demo.
# Builds the entity_demo target and runs it from the project root
# (the game loads assets/setup.lua relative to the project root).

PROJECT_ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
TARGET="entity_demo"

# When launched by double-click there is no terminal, so build errors would be
# invisible. Re-launch inside gnome-terminal so output stays on screen.
if [[ ! -t 1 && -z "$ENTITY_DEMO_IN_TERM" ]]; then
    if command -v gnome-terminal >/dev/null 2>&1; then
        export ENTITY_DEMO_IN_TERM=1
        exec gnome-terminal -- bash -c "\"$0\"; echo; read -rp 'Press Enter to close...'"
    fi
fi

cd "$PROJECT_ROOT" || exit 1

cmake -B build -DCMAKE_POLICY_VERSION_MINIMUM=3.5 && cmake --build build --target "$TARGET" || {
    echo "Build failed."
    exit 1
}

exec "./build/$TARGET"
