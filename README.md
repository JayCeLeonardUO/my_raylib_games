# My Raylib Games

Collection of games built with Raylib + ImGui.

## Quick Start

```bash
./build_run.sh
```

## Manual Build

```bash
cmake -B build
cmake --build build
./build/example_game
```

## Add a New Game

1. Create a folder in `src/scratch/your_game_name/`
2. Add `main.cpp` (or multiple .cpp files)
3. Add to CMakeLists.txt:
   ```cmake
   add_game(your_game_name)
   ```
4. Rebuild

## Tools

### `tools/mov_to_frames.py` — Video to Sprite Frames

Extracts frames from a video file (`.mov`, `.mp4`, etc.) into numbered RGBA PNGs with automatic color keying for transparency.

**Install dependencies:**

```bash
pip install imageio imageio[pyav] numpy Pillow
```

**Basic usage:**

```bash
python3 tools/mov_to_frames.py path/to/animation.mov
```

By default, **white is keyed out as transparent** — pixels matching white become fully transparent, and pixels far from white become opaque. Frames are saved to `assets/animations/<video_name>/`.

**Options:**

| Flag | Description |
|---|---|
| `-o`, `--output DIR` | Output directory (default: `assets/animations/<stem>`) |
| `--skip N` | Keep every Nth frame (default: 1 = keep all) |
| `--key-color R,G,B` | Color to key out as transparent (default: `255,255,255` = white). Pixels matching this color become transparent; pixels far from it become opaque. |
| `--mask PATH` | Use an external mask image instead of color keying. Mask brightness maps to opacity: **white = opaque**, **black = transparent**. |
| `--no-key` | Disable color keying entirely. Output fully opaque frames. |

**Examples:**

```bash
# Default: extract frames, key out white background
python3 tools/mov_to_frames.py clip.mov

# Key out black instead of white (e.g. VFX on black background)
python3 tools/mov_to_frames.py clip.mov --key-color 0,0,0

# Use an external mask image for alpha
python3 tools/mov_to_frames.py clip.mov --mask masks/circle.png

# Extract every 2nd frame, no keying (fully opaque)
python3 tools/mov_to_frames.py clip.mov --skip 2 --no-key

# Custom output directory
python3 tools/mov_to_frames.py clip.mov -o sprites/explosion
```

## Dependencies (fetched automatically)

- Raylib 5.5
- Dear ImGui (C++, docking branch)
- rlImGui
