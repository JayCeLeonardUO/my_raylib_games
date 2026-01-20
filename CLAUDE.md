# Claude Code Notes

## Project Origin
This project was created from `/home/jpleona/CLionProjects/my-raylib-tools` as a clean slate for Raylib games.

## Building
```bash
./build_run.sh
```

Or manually:
```bash
cmake -B build
cmake --build build --target <game_name>
./build/<game_name>
```

## Project Structure
- `src/scratch/` - Game projects live here
- Each game is a subfolder with its own `main.cpp`
- Add new games to CMakeLists.txt with `add_game(game_name)`

## Dependencies (auto-fetched)
- Raylib 5.5
- cimgui (C bindings for Dear ImGui)
- rlImGui (Raylib + ImGui integration)
- Google Test

## Adding a New Game
1. Create `src/scratch/my_game/main.cpp`
2. Add to CMakeLists.txt:
   ```cmake
   add_game(my_game)
   ```
3. Run `./build_run.sh` or `cmake --build build --target my_game`

## Template
Use `src/scratch/example_game/main.cpp` as a starting template - it has Raylib window setup, ImGui panel, and basic input handling.
