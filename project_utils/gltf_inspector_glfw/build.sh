#!/bin/bash
set -e

cd "$(dirname "$0")"

cmake -B build
cmake --build build -j$(nproc)

echo ""
echo "Built: build/gltf_inspector"
echo "Usage: ./build/gltf_inspector [path/to/file.gltf|glb]"
