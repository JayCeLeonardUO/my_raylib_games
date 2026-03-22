#include <imgui.h>
#include <raylib.h>
#include <raymath.h>
#include <rlImGui.h>
#include <cmath>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

// --- Follower shape ---
struct Follower {
    const char *name;
    Color color;
    float influence;
    Vector3 pos;
};

// --- State ---
static Camera3D camera = {0};
static Vector3 cubePos = {0, 0.5f, 0};
static bool isDragging = false;
static float dragPlaneY = 0.0f; // Y plane for drag

static Follower followers[3] = {
    {"Sphere",     {50, 200, 255, 255},  1.0f, {0, 0.5f, 0}},
    {"Cone",       {255, 120, 50, 255},  0.5f, {0, 0.5f, 0}},
    {"Small Cube", {180, 50, 255, 255},  0.2f, {0, 0.5f, 0}},
};

// --- Camera orbit state (manual, so we can disable during drag) ---
static float camAngle = 0.8f;
static float camPitch = 0.6f;
static float camDist = 12.0f;
static bool camOrbiting = false;
static Vector2 camLastMouse = {0};

// --- Ray-plane intersection for dragging on XZ plane ---
static bool RayPlaneXZ(Ray ray, float planeY, Vector3 *hit) {
    if (fabsf(ray.direction.y) < 0.0001f) return false;
    float t = (planeY - ray.position.y) / ray.direction.y;
    if (t < 0) return false;
    *hit = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    return true;
}

// --- Check if mouse ray hits the cube (simple sphere test) ---
static bool MouseHitsCube(Vector3 cubeCenter, float radius) {
    Ray ray = GetScreenToWorldRay(GetMousePosition(), camera);
    Vector3 oc = Vector3Subtract(ray.position, cubeCenter);
    float b = Vector3DotProduct(oc, ray.direction);
    float c = Vector3DotProduct(oc, oc) - radius * radius;
    return (b * b - c) >= 0;
}

// --- Draw a cone (approximated with cylinder) ---
static void DrawConeAt(Vector3 pos, float radius, float height, Color color) {
    DrawCylinder(pos, radius, 0.0f, height, 12, color);
    DrawCylinderWires(pos, radius, 0.0f, height, 12,
        (Color){(unsigned char)(color.r/2), (unsigned char)(color.g/2), (unsigned char)(color.b/2), 200});
}

// --- Draw a torus approximation (ring of small spheres) ---
static void DrawTorusAt(Vector3 center, float majorR, float minorR, Color color) {
    int segments = 16;
    for (int i = 0; i < segments; i++) {
        float a = (float)i / segments * 2.0f * PI;
        Vector3 p = {
            center.x + cosf(a) * majorR,
            center.y,
            center.z + sinf(a) * majorR
        };
        DrawSphere(p, minorR, color);
    }
}

static void UpdateAndDraw() {
    ImGuiIO &io = ImGui::GetIO();
    bool imguiWantsMouse = io.WantCaptureMouse;

    // --- Camera orbit (right mouse button, only when not over ImGui) ---
    if (!imguiWantsMouse && !isDragging) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
            camOrbiting = true;
            camLastMouse = GetMousePosition();
        }
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) camOrbiting = false;
    if (camOrbiting) {
        Vector2 cur = GetMousePosition();
        camAngle -= (cur.x - camLastMouse.x) * 0.005f;
        camPitch += (cur.y - camLastMouse.y) * 0.005f;
        camPitch = Clamp(camPitch, 0.1f, 1.4f);
        camLastMouse = cur;
    }

    // Zoom
    if (!imguiWantsMouse) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0) camDist = Clamp(camDist - wheel * 1.0f, 3.0f, 30.0f);
    }

    camera.position = (Vector3){
        cosf(camAngle) * cosf(camPitch) * camDist,
        sinf(camPitch) * camDist,
        sinf(camAngle) * cosf(camPitch) * camDist
    };
    camera.target = (Vector3){0, 0, 0};

    // --- Cube dragging (left mouse button) ---
    if (!imguiWantsMouse && !camOrbiting) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (MouseHitsCube(cubePos, 0.7f)) {
                isDragging = true;
                dragPlaneY = cubePos.y;
            }
        }
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) isDragging = false;

    if (isDragging) {
        Ray ray = GetScreenToWorldRay(GetMousePosition(), camera);
        Vector3 hit;
        if (RayPlaneXZ(ray, dragPlaneY, &hit)) {
            cubePos.x = hit.x;
            cubePos.z = hit.z;
        }
    }

    // --- Update followers (lerp toward cube based on influence) ---
    float dt = GetFrameTime();
    float speed = 5.0f; // base chase speed
    for (int i = 0; i < 3; i++) {
        Vector3 target = cubePos;
        float t = 1.0f - expf(-speed * followers[i].influence * dt);
        followers[i].pos = Vector3Lerp(followers[i].pos, target, t);
    }

    // --- Draw ---
    BeginDrawing();
    ClearBackground((Color){20, 20, 30, 255});

    BeginMode3D(camera);

    // Ground grid
    DrawGrid(20, 1.0f);

    // Ground plane (subtle)
    DrawPlane((Vector3){0, -0.01f, 0}, (Vector2){20, 20}, (Color){30, 30, 45, 255});

    // Lines from followers to cube
    for (int i = 0; i < 3; i++) {
        DrawLine3D(followers[i].pos, cubePos,
            (Color){followers[i].color.r, followers[i].color.g, followers[i].color.b, 150});
    }

    // Draw followers
    // Sphere (follower 0)
    DrawSphere(followers[0].pos, 0.4f, followers[0].color);
    DrawSphereWires(followers[0].pos, 0.41f, 8, 8,
        (Color){(unsigned char)(followers[0].color.r/2), (unsigned char)(followers[0].color.g/2),
                (unsigned char)(followers[0].color.b/2), 180});

    // Cone (follower 1)
    Vector3 coneBase = followers[1].pos;
    coneBase.y -= 0.3f;
    DrawConeAt(coneBase, 0.35f, 0.8f, followers[1].color);

    // Torus (follower 2)
    DrawTorusAt(followers[2].pos, 0.35f, 0.08f, followers[2].color);

    // Draw the yellow controller cube
    Color cubeColor = isDragging ? GOLD : YELLOW;
    DrawCube(cubePos, 0.8f, 0.8f, 0.8f, cubeColor);
    DrawCubeWires(cubePos, 0.82f, 0.82f, 0.82f, (Color){180, 180, 0, 200});

    // Label above cube
    EndMode3D();

    // HUD
    DrawText("Left-click cube to drag | Right-drag to orbit | Scroll to zoom",
             8, GetScreenHeight() - 24, 14, (Color){150, 150, 150, 180});

    // --- ImGui ---
    rlImGuiBegin();

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(310, 0), ImGuiCond_FirstUseEver);
    ImGui::Begin("Influence Controls");

    ImGui::TextWrapped("Drag the yellow cube. Each shape follows based on its influence value.");
    ImGui::Spacing();
    ImGui::SeparatorText("Followers");

    for (int i = 0; i < 3; i++) {
        ImGui::PushID(i);

        // Colored bullet
        ImVec4 col = ImVec4(
            followers[i].color.r / 255.0f,
            followers[i].color.g / 255.0f,
            followers[i].color.b / 255.0f, 1.0f);
        ImGui::ColorButton("##col", col, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, ImVec2(14, 14));
        ImGui::SameLine();
        ImGui::SliderFloat(followers[i].name, &followers[i].influence, 0.0f, 1.0f, "%.2f");

        // Distance info
        float dist = Vector3Distance(followers[i].pos, cubePos);
        ImGui::SameLine();
        ImGui::TextDisabled("(%.1f)", dist);

        ImGui::PopID();
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Info");
    ImGui::BulletText("Influence 1.0 = follows exactly");
    ImGui::BulletText("Influence 0.0 = stays still");
    ImGui::BulletText("Values in between = trails behind");

    ImGui::Spacing();
    if (ImGui::Button("Reset Positions")) {
        cubePos = {0, 0.5f, 0};
        for (int i = 0; i < 3; i++)
            followers[i].pos = cubePos;
    }
    ImGui::SameLine();
    ImGui::Text("FPS: %d", GetFPS());

    ImGui::End();

    rlImGuiEnd();

    EndDrawing();
}

int main() {
    InitWindow(1024, 700, "Influence Demo");
    SetTargetFPS(60);
    rlImGuiSetup(true);

    camera.up = (Vector3){0, 1, 0};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(UpdateAndDraw, 0, 1);
#else
    while (!WindowShouldClose()) UpdateAndDraw();
#endif

    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
