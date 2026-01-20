// Example Game - Raylib + ImGui template
#include "raylib.h"
#include "rlImGui.h"
#include "imgui.h"

int main()
{
  // Window setup
  const int screenWidth = 1280;
  const int screenHeight = 720;
  InitWindow(screenWidth, screenHeight, "Example Game");
  SetTargetFPS(60);

  // ImGui setup
  rlImGuiSetup(true);

  // Game state
  Vector2 ballPos = { screenWidth / 2.0f, screenHeight / 2.0f };
  float ballRadius = 20.0f;
  Color ballColor = RED;
  float speed = 200.0f;

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();

    // Input
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    ballPos.y -= speed * dt;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  ballPos.y += speed * dt;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  ballPos.x -= speed * dt;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) ballPos.x += speed * dt;

    // Drawing
    BeginDrawing();
    ClearBackground(DARKGRAY);

    // Draw game
    DrawCircleV(ballPos, ballRadius, ballColor);
    DrawText("WASD or Arrow keys to move", 10, 10, 20, WHITE);

    // ImGui
    rlImGuiBegin();

    if (ImGui::Begin("Game Settings")) {
      ImGui::Text("FPS: %d", GetFPS());
      ImGui::Separator();

      ImGui::SliderFloat("Speed", &speed, 50.0f, 500.0f, "%.0f");
      ImGui::SliderFloat("Radius", &ballRadius, 5.0f, 100.0f, "%.0f");

      float col[3] = { ballColor.r / 255.0f, ballColor.g / 255.0f, ballColor.b / 255.0f };
      if (ImGui::ColorEdit3("Color", col)) {
        ballColor = (Color){ (unsigned char)(col[0] * 255), (unsigned char)(col[1] * 255), (unsigned char)(col[2] * 255), 255 };
      }

      ImGui::Separator();
      if (ImGui::Button("Reset Position", ImVec2(120, 0))) {
        ballPos = (Vector2){ screenWidth / 2.0f, screenHeight / 2.0f };
      }
    }
    ImGui::End();

    rlImGuiEnd();
    EndDrawing();
  }

  rlImGuiShutdown();
  CloseWindow();
  return 0;
}
