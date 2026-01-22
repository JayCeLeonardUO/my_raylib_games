#include "../../mylibs/hexgrid_math.hpp"
#include "../../mylibs/ilist.hpp"
#include "imgui.h"
#include "raylib.h"
#include "raymath.h"
#include "rlImGui.h"

// HexTile stores game data for each hex
struct HexTile {
  Vector2 pos{};
  int row = 0;
  int col = 0;
  Color color = DARKBLUE;
  bool highlighted = false;

  // Neighbor refs (using thing_ref from ilist)
  thing_ref neighbors[6]{}; // E, NE, NW, W, SW, SE
};

using HexTileGrid = things_list<HexTile>;

// Direction helpers for offset coordinates (odd-r)
// E, NE, NW, W, SW, SE
const int dir_col_even[] = {1, 0, -1, -1, -1, 0};
const int dir_row_even[] = {0, -1, -1, 0, 1, 1};
const int dir_col_odd[] = {1, 1, 0, -1, 0, 1};
const int dir_row_odd[] = {0, -1, -1, 0, 1, 1};

const char* dir_names[] = {"E", "NE", "NW", "W", "SW", "SE"};

int main() {
  const int screenWidth = 1280;
  const int screenHeight = 720;

  InitWindow(screenWidth, screenHeight, "Hex Grid Demo - ilist + hexgrid_math");
  SetTargetFPS(60);
  rlImGuiSetup(true);

  // Create hex grid configuration
  HexGridConfig config = {8, 10, {(float)screenWidth - 200, (float)screenHeight - 100}};
  HexDims dims = hexDims_from_config(config);
  Vector2* positions = calculate_hexgrid(config, dims);

  // Create things_list to manage hex tiles
  HexTileGrid grid;
  thing_ref tile_refs[20][20]{}; // Store refs by row/col for neighbor linking

  // Add all tiles to the list
  for (int row = 0; row < config.rows; row++) {
    for (int col = 0; col < config.cols; col++) {
      int idx = hex_index(row, col, config.cols);
      HexTile tile;
      tile.pos = positions[idx];
      tile.row = row;
      tile.col = col;
      tile.color = DARKBLUE;

      thing_ref ref = grid.add(tile);
      tile_refs[row][col] = ref;
    }
  }

  // Link neighbors
  for (int row = 0; row < config.rows; row++) {
    for (int col = 0; col < config.cols; col++) {
      thing_ref ref = tile_refs[row][col];
      if (ref.kind != ilist_kind::item)
        continue;

      auto& tile = grid[ref];
      const int* dcol = (row % 2 == 0) ? dir_col_even : dir_col_odd;
      const int* drow = (row % 2 == 0) ? dir_row_even : dir_row_odd;

      for (int d = 0; d < 6; d++) {
        int nr = row + drow[d];
        int nc = col + dcol[d];
        if (nr >= 0 && nr < config.rows && nc >= 0 && nc < config.cols) {
          tile.neighbors[d] = tile_refs[nr][nc];
        }
      }
    }
  }

  // Game state
  thing_ref selected = tile_refs[config.rows / 2][config.cols / 2];
  int current_dir = 0;

  while (!WindowShouldClose()) {
    // Input handling
    if (selected.kind == ilist_kind::item) {
      auto& tile = grid[selected];

      // Navigate with keys
      thing_ref next{};
      if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT))
        next = tile.neighbors[0]; // E
      if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))
        next = tile.neighbors[3]; // W
      if (IsKeyPressed(KEY_E))
        next = tile.neighbors[1]; // NE
      if (IsKeyPressed(KEY_Q))
        next = tile.neighbors[2]; // NW
      if (IsKeyPressed(KEY_C))
        next = tile.neighbors[5]; // SE
      if (IsKeyPressed(KEY_Z))
        next = tile.neighbors[4]; // SW

      if (IsKeyPressed(KEY_TAB)) {
        current_dir = (current_dir + 1) % 6;
      }
      if (IsKeyPressed(KEY_SPACE)) {
        next = tile.neighbors[current_dir];
      }

      if (next.kind == ilist_kind::item) {
        selected = next;
      }

      // Color selected hex with number keys
      if (IsKeyPressed(KEY_ONE))
        grid[selected].color = RED;
      if (IsKeyPressed(KEY_TWO))
        grid[selected].color = GREEN;
      if (IsKeyPressed(KEY_THREE))
        grid[selected].color = BLUE;
      if (IsKeyPressed(KEY_FOUR))
        grid[selected].color = YELLOW;
      if (IsKeyPressed(KEY_FIVE))
        grid[selected].color = PURPLE;
      if (IsKeyPressed(KEY_ZERO))
        grid[selected].color = DARKBLUE;
    }

    // Mouse selection
    Vector2 mouse = GetMousePosition();
    for (auto& tile : grid) {
      float dist = Vector2Distance(mouse, tile.pos);
      tile.highlighted = (dist < dims.radius * 0.9f);
      if (tile.highlighted && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        selected = tile.this_ref();
      }
    }

    // Drawing
    BeginDrawing();
    ClearBackground(DARKGRAY);

    // Draw all tiles using iterator
    for (auto& tile : grid) {
      bool is_selected = (tile.this_ref().idx == selected.idx);

      // Draw filled hex
      Color fill = tile.color;
      if (tile.highlighted)
        fill = ColorBrightness(fill, 0.3f);
      draw_hex_filled(tile.pos, dims.radius * 0.95f, fill);

      // Draw outline
      Color outline = is_selected ? YELLOW : WHITE;
      float thickness = is_selected ? 3.0f : 1.0f;
      for (int i = 0; i < 6; i++) {
        float angle1 = (60.0f * i - 30.0f) * DEG2RAD;
        float angle2 = (60.0f * (i + 1) - 30.0f) * DEG2RAD;
        Vector2 p1 = {tile.pos.x + dims.radius * cosf(angle1),
                      tile.pos.y + dims.radius * sinf(angle1)};
        Vector2 p2 = {tile.pos.x + dims.radius * cosf(angle2),
                      tile.pos.y + dims.radius * sinf(angle2)};
        DrawLineEx(p1, p2, thickness, outline);
      }

      // Draw coordinates
      DrawText(TextFormat("%d,%d", tile.row, tile.col), (int)tile.pos.x - 12, (int)tile.pos.y - 6,
               10, WHITE);
    }

    // Draw direction indicator from selected
    if (selected.kind == ilist_kind::item) {
      auto& tile = grid[selected];
      thing_ref neighbor = tile.neighbors[current_dir];
      if (neighbor.kind == ilist_kind::item) {
        auto& ntile = grid[neighbor];
        DrawLineEx(tile.pos, ntile.pos, 3.0f, RED);
      }
    }

    // Instructions
    DrawText("Q/E: NW/NE | A/D: W/E | Z/C: SW/SE", 10, 10, 18, WHITE);
    DrawText("Tab: cycle direction | Space: move in direction", 10, 32, 18, WHITE);
    DrawText("1-5: color hex | 0: reset | Click: select", 10, 54, 18, WHITE);

    // ImGui panel
    rlImGuiBegin();
    if (ImGui::Begin("Hex Grid Info")) {
      ImGui::Text("FPS: %d", GetFPS());
      ImGui::Separator();
      ImGui::Text("Grid: %d rows x %d cols", config.rows, config.cols);
      ImGui::Text("Hex radius: %.1f", dims.radius);
      ImGui::Text("Current direction: %s", dir_names[current_dir]);

      if (selected.kind == ilist_kind::item) {
        auto& tile = grid[selected];
        ImGui::Separator();
        ImGui::Text("Selected: row %d, col %d", tile.row, tile.col);
        ImGui::Text("Position: (%.1f, %.1f)", tile.pos.x, tile.pos.y);

        ImGui::Separator();
        ImGui::Text("Neighbors:");
        for (int d = 0; d < 6; d++) {
          if (tile.neighbors[d].kind == ilist_kind::item) {
            auto& n = grid[tile.neighbors[d]];
            ImGui::Text("  %s: (%d, %d)", dir_names[d], n.row, n.col);
          } else {
            ImGui::TextDisabled("  %s: none", dir_names[d]);
          }
        }
      }
    }
    ImGui::End();
    rlImGuiEnd();

    EndDrawing();
  }

  free(positions);
  rlImGuiShutdown();
  CloseWindow();
  return 0;
}
