# My Raylib Games

Collection of games built with Raylib + ImGui.

## Build

```bash
cmake -B build
cmake --build build
```

## Run

```bash
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

## Dependencies (fetched automatically)

- Raylib 5.5
- Dear ImGui (via cimgui)
- rlImGui
