#!/bin/bash
# Regenerate embedded_assets.hpp from all files in assets/

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# Build the tool if needed
if [ ! -f build/embed_binary ]; then
  echo "Building embed_binary..."
  cmake --build build --target embed_binary
fi

# Find all files in assets/
ASSETS=(assets/*)

if [ ${#ASSETS[@]} -eq 0 ] || [ ! -e "${ASSETS[0]}" ]; then
  echo "No assets found in assets/ directory"
  echo "Add files to assets/ and run this script again"
  exit 0
fi

echo "Embedding ${#ASSETS[@]} asset(s)..."
./build/embed_binary src/mylibs/embedded_assets.hpp "${ASSETS[@]}"
