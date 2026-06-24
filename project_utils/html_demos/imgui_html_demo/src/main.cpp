#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

static float sliderVal = 0.5f;
static float clearColor[3] = {0.2f, 0.2f, 0.2f};
static bool showDemo = false;
static char textBuf[128] = "Hello from ImGui!";
static int counter = 0;

void UpdateAndDraw() {
    BeginDrawing();
    ClearBackground({
        (unsigned char)(clearColor[0] * 255),
        (unsigned char)(clearColor[1] * 255),
        (unsigned char)(clearColor[2] * 255),
        255
    });

    rlImGuiBegin();

    ImGui::Begin("Controls");
    ImGui::Text("ImGui + Raylib + Emscripten");
    ImGui::Separator();
    ImGui::InputText("Text", textBuf, sizeof(textBuf));
    ImGui::SliderFloat("Slider", &sliderVal, 0.0f, 1.0f);
    ImGui::ColorEdit3("Background", clearColor);
    if (ImGui::Button("Click me")) counter++;
    ImGui::SameLine();
    ImGui::Text("Count: %d", counter);
    ImGui::Separator();
    ImGui::Checkbox("Show Demo Window", &showDemo);
    ImGui::Text("FPS: %d", GetFPS());
    ImGui::End();

    if (showDemo) ImGui::ShowDemoWindow(&showDemo);

    rlImGuiEnd();

    // Draw something with Raylib behind ImGui
    DrawCircle(GetScreenWidth() / 2, GetScreenHeight() / 2,
               sliderVal * 200.0f, RED);
    DrawText("Raylib draws behind ImGui", 10, GetScreenHeight() - 30, 16, WHITE);

    EndDrawing();
}

int main() {
    InitWindow(800, 600, "imgui demo");
    SetTargetFPS(60);
    rlImGuiSetup(true);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(UpdateAndDraw, 0, 1);
#else
    while (!WindowShouldClose()) {
        UpdateAndDraw();
    }
#endif

    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
