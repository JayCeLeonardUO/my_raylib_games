#include "../../mylibs/hexgrid_math.hpp"
#include "imgui.h"
#include "raylib.h"
#include "rlImGui.h"

int main() {
  const int screenWidth = 1280;
  const int screenHeight = 720;
  InitWindow(screenWidth, screenHeight, "Hexgrid Demo");
  SetTargetFPS(60);

  rlImGuiSetup(true);

  // Hex grid settings
  float hexSize = 30.0f;
  int gridRadius = 5;
  Vector2 origin = {screenWidth / 2.0f, screenHeight / 2.0f};

  while (!WindowShouldClose()) {
    Layout layout(layout_pointy, Point{hexSize, hexSize}, origin);

    BeginDrawing();
    ClearBackground(DARKGRAY);

    // Draw hex grid
    auto hexes = grid_hexagon(gridRadius);
    for (auto& h : hexes) {
      draw_hex(layout, h, LIGHTGRAY);
    }

    // Highlight hex under mouse
    Point mouse = GetMousePosition();
    FractionalHex fh = pixel_to_hex_fractional(layout, mouse);
    Hex hovered((int)round(fh.q), (int)round(fh.r));
    draw_hex_filled(layout, hovered, ColorAlpha(YELLOW, 0.3f));

    DrawText("Hexgrid Demo", 10, 10, 20, WHITE);

    // ImGui panel
    rlImGuiBegin();
    if (ImGui::Begin("Hex Settings")) {
      ImGui::Text("FPS: %d", GetFPS());
      ImGui::SliderFloat("Hex Size", &hexSize, 10.0f, 60.0f);
      ImGui::SliderInt("Grid Radius", &gridRadius, 1, 10);
      ImGui::Text("Hovered: (%d, %d)", hovered.q, hovered.r);
    }
    ImGui::End();
    rlImGuiEnd();

    EndDrawing();
  }

  rlImGuiShutdown();
  CloseWindow();
  return 0;
}
