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
- Dear ImGui (C++, docking branch)
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

## Blender MCP
A Blender MCP server is configured for this project. When the user asks about Blender:
1. **First check if the MCP is working** - look for blender tools in your available tools
2. If no blender tools are available, remind the user to:
   - Make sure Blender is running
   - Install the `blender-mcp` addon in Blender (Edit > Preferences > Add-ons)
   - Restart Claude Code session
3. The MCP allows direct control of Blender (creating objects, modifying scenes, etc.)
