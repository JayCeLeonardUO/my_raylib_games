# project_utils Build Notes

Every util in this directory is an **Emscripten project** that compiles to a **single self-contained HTML file**.

## How to build any util

```bash
source ~/emsdk/emsdk_env.sh
cd <util_folder>
emcmake cmake -B build
cmake --build build --target <util_name>
```

The CMakeLists.txt post-build step copies the output HTML to the util's root folder (e.g. `file_viewer_html/file_viewer_html.html`).

## Emsdk location

```
~/emsdk/upstream/emscripten/emcc
```

Activate with `source ~/emsdk/emsdk_env.sh` before building.

## Key CMake flags (Emscripten)

All utils use:
- `-sUSE_GLFW=3` — Raylib needs GLFW
- `-sALLOW_MEMORY_GROWTH=1` — dynamic memory
- `-sSINGLE_FILE=1` — everything baked into one HTML

Some utils add:
- `-sASYNCIFY -sFORCE_FILESYSTEM=1` — for file I/O (glb_viewer)
- `-sEXPORTED_FUNCTIONS` / `-sEXPORTED_RUNTIME_METHODS` — for C<->JS calls (htmx_demo)

## Utils overview

| Util | ImGui? | Notes |
|------|--------|-------|
| raylib_html_demo | No | Simplest — just a 3D cube |
| htmx_demo | No | HTML buttons control Raylib via exported C functions |
| imgui_html_demo | Yes | ImGui widgets over Raylib |
| vault_viewer | Yes | File list from Obsidian vault (injected via `window.__VAULT_FILES`) |
| glb_viewer | Yes | GLB models from Blender plugin |
| glb_plugin_viewer | Yes | GLB + JSON inspector from Blender plugin |
| file_viewer_html | Yes | Standalone file viewer — browser file picker, displays raw text |

## NOT for Obsidian

`file_viewer_html` is standalone — it runs directly in the browser with its own file picker. The others (vault_viewer, glb_viewer, glb_plugin_viewer) expect data injected by Obsidian/Blender plugins.
