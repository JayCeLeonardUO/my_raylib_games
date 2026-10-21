---
description: Build the current project (or a specified demo) to WebAssembly using Emscripten. Use when the user asks to build for web, export to HTML, or run an Emscripten build.
user_invocable: true
---

# Emscripten Build

Build a project_utils demo (or any CMake target in this repo) to a single-file HTML using Emscripten.

## Environment

- emsdk is installed at `~/emsdk`
- Always source the environment first: `source ~/emsdk/emsdk_env.sh`
- The CMakeLists.txt for each demo already handles the `EMSCRIPTEN` flag, including:
  - Setting `PLATFORM=Web` for raylib
  - Link options: `-sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1 -sSINGLE_FILE=1`
  - Custom `--shell-file` and `--pre-js` from `src/`
  - Post-build copy of the `.html` to the project source directory

## Build Steps

Run these commands from the demo's directory (the one containing `CMakeLists.txt`):

```bash
source ~/emsdk/emsdk_env.sh
emcmake cmake -B build_web -DCMAKE_BUILD_TYPE=Release
cmake --build build_web -j$(nproc)
```

## Output

- The build produces a single `.html` file (everything embedded via `SINGLE_FILE=1`)
- The post-build step copies it to the project source directory (next to `CMakeLists.txt`)
- Open it in a browser or serve with `python3 -m http.server`

## Shaders

The web build targets WebGL 2 (raylib `OPENGL_VERSION "ES 3.0"`, set in CMakeLists.txt). GLSL ES 3.00 is GLSL 330 with a different header, so shaders are written once as `#version 330` files and `ShaderPortable::load` / `load_memory` (`src/mylibs/shader_portable.hpp`) swaps the header for `#version 300 es` + default precisions on Emscripten. Do not keep `_es100` copies. Things WebGL 2 still lacks:
- No compute shaders or SSBOs: keep a CPU fallback behind `rlGetVersion() == RL_OPENGL_43`.
- Float color attachments (R32F) need the EXT_color_buffer_float extension; Emscripten enables it automatically when the browser has it, and rlgl's `rlFramebufferComplete` tells you if it didn't.
- `#version` must stay the first line of the source.

## Prerequisites for a Demo

Each web-buildable demo needs in its `src/` directory:
- `shell.html` — minimal HTML template with `{{{ SCRIPT }}}` placeholder and a `<canvas id="canvas">`
- `pre.js` — at minimum: `var Module = { canvas: document.getElementById('canvas') };`
- The main loop must use `emscripten_set_main_loop()` instead of a `while` loop (guard with `#ifdef __EMSCRIPTEN__`)
