#include "raylib.h"
#include "raymath.h"
#include <math.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

static Camera3D camera = { 0 };
static float angle = 0.0f;

void UpdateAndDraw() {
    angle += 0.02f;
    camera.position = (Vector3){ cosf(angle) * 7.0f, 5.0f, sinf(angle) * 7.0f };

    BeginDrawing();
    ClearBackground(RAYWHITE);

    BeginMode3D(camera);
    DrawCube((Vector3){ 0.0f, 0.0f, 0.0f }, 2.0f, 2.0f, 2.0f, RED);
    DrawCubeWires((Vector3){ 0.0f, 0.0f, 0.0f }, 2.0f, 2.0f, 2.0f, DARKGRAY);
    DrawGrid(10, 1.0f);
    EndMode3D();

    DrawText("Raylib + Emscripten", 10, 10, 20, DARKGRAY);
    EndDrawing();
}

int main() {
    InitWindow(800, 600, "cube demo");
    SetTargetFPS(60);

    camera.position = (Vector3){ 5.0f, 5.0f, 5.0f };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(UpdateAndDraw, 0, 1);
#else
    while (!WindowShouldClose()) {
        UpdateAndDraw();
    }
#endif

    CloseWindow();
    return 0;
}
