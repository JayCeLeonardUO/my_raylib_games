#!/bin/bash
# Build a game for the browser with Emscripten. Output: build_web/<game>.html (single file).
# Usage: ./build_web.sh [game]   (default: entity_demo)
set -e
cd "$(dirname "$0")"
GAME="${1:-entity_demo}"
if ! command -v emcmake >/dev/null 2>&1; then
  # local dev: activate the home-directory emsdk; CI puts emcc on PATH already
  source ~/emsdk/emsdk_env.sh >/dev/null
fi
[ -f build_web/CMakeCache.txt ] || emcmake cmake -B build_web -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build_web --target "$GAME"
echo "built: build_web/$GAME.html  (serve it: python3 -m http.server -d build_web 8080)"
