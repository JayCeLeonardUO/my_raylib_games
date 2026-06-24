#include <imgui.h>
#include <raylib.h>
#include <raymath.h>
#include <rlImGui.h>
#include <rlgl.h>
#include <cstring>
#include <cstdio>
#include <cmath>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>

// Write uploaded GLB bytes from JS global into Emscripten VFS
// Same pattern as the working glb_viewer demo
EM_JS(int, write_upload_to_vfs, (), {
    var bytes = window.__GLB_UPLOAD_BYTES;
    if (!bytes || bytes.length === 0) return 0;
    try { FS.unlink("/tmp/model.glb"); } catch(e) {}
    FS.writeFile("/tmp/model.glb", bytes);
    var len = bytes.length;
    window.__GLB_UPLOAD_BYTES = null;
    return len;
});
#endif

// --- Config ---
static constexpr int MAX_MESHES = 256;

static const Color PALETTE[] = {
    {31, 119, 180, 255}, {255, 127, 14, 255}, {44, 160, 44, 255},
    {214, 39, 40, 255},  {148, 103, 189, 255},{140, 86, 75, 255},
    {227, 119, 194, 255},{127, 127, 127, 255},{188, 189, 34, 255},
    {23, 190, 207, 255},
};
static constexpr int PALETTE_COUNT = sizeof(PALETTE) / sizeof(PALETTE[0]);

// --- Per-mesh state ---
struct MeshInfo {
    bool visible;
    Color color;
    char name[64];
};

static Model model = {0};
static bool modelLoaded = false;
static int meshCount = 0;
static MeshInfo meshInfos[MAX_MESHES] = {};
static Material meshMats[MAX_MESHES] = {};

static int soloIdx = -1;
static int hoveredMeshIdx = -1;   // from viewport raycast
static int trayHoveredIdx = -1;   // from tray hover
static bool wireframeOn = false;

// Camera
static Camera3D camera = {0};
static float camAngle = 0.8f;
static float camPitch = 0.4f;
static float camDist = 5.0f;
static Vector3 camTarget = {0};
static bool orbiting = false;
static bool panning = false;
static Vector2 lastMouse = {0};

// --- Helpers ---
static Color Brighten(Color c, float amt) {
    return {
        (unsigned char)fminf(255, c.r + amt * 255),
        (unsigned char)fminf(255, c.g + amt * 255),
        (unsigned char)fminf(255, c.b + amt * 255), c.a
    };
}

static void FitCameraToModel() {
    if (!modelLoaded) return;
    BoundingBox bb = GetModelBoundingBox(model);
    camTarget = {
        (bb.min.x + bb.max.x) * 0.5f,
        (bb.min.y + bb.max.y) * 0.5f,
        (bb.min.z + bb.max.z) * 0.5f
    };
    float maxSize = fmaxf(fmaxf(bb.max.x - bb.min.x, bb.max.y - bb.min.y), bb.max.z - bb.min.z);
    camDist = maxSize * 2.0f;
    if (camDist < 2.0f) camDist = 2.0f;
    camAngle = 0.8f;
    camPitch = 0.4f;
}

static void FitCameraToMesh(int idx) {
    if (!modelLoaded || idx < 0 || idx >= meshCount) return;
    BoundingBox bb = GetMeshBoundingBox(model.meshes[idx]);
    camTarget = {
        (bb.min.x + bb.max.x) * 0.5f,
        (bb.min.y + bb.max.y) * 0.5f,
        (bb.min.z + bb.max.z) * 0.5f
    };
    float maxSize = fmaxf(fmaxf(bb.max.x - bb.min.x, bb.max.y - bb.min.y), bb.max.z - bb.min.z);
    camDist = maxSize * 2.5f;
    if (camDist < 1.0f) camDist = 1.0f;
}

static bool IsMeshVisible(int i) {
    if (soloIdx >= 0) return i == soloIdx;
    return meshInfos[i].visible;
}

// --- Load model (called from JS upload or at startup) ---
static void DoLoadModel() {
    if (modelLoaded) {
        UnloadModel(model);
        modelLoaded = false;
        meshCount = 0;
    }

    model = LoadModel("/tmp/model.glb");
    meshCount = model.meshCount;
    if (meshCount > MAX_MESHES) meshCount = MAX_MESHES;

    for (int i = 0; i < meshCount; i++) {
        meshInfos[i].visible = true;
        meshInfos[i].color = PALETTE[i % PALETTE_COUNT];
        snprintf(meshInfos[i].name, 64, "Mesh %d", i);
        meshMats[i] = LoadMaterialDefault();
        meshMats[i].maps[MATERIAL_MAP_DIFFUSE].color = meshInfos[i].color;
    }

    modelLoaded = true;
    soloIdx = -1;
    hoveredMeshIdx = -1;
    trayHoveredIdx = -1;
    FitCameraToModel();
}

// --- Exported: JS calls this after setting window.__GLB_UPLOAD_BYTES ---
#ifdef __EMSCRIPTEN__
extern "C" {
EMSCRIPTEN_KEEPALIVE
void js_upload_glb() {
    int bytes = write_upload_to_vfs();
    if (bytes > 0) DoLoadModel();
}
}
#endif

// --- Main loop ---
static void UpdateAndDraw() {
    ImGuiIO &io = ImGui::GetIO();
    bool imguiMouse = io.WantCaptureMouse;

    // Camera controls (only when ImGui doesn't want mouse)
    if (!imguiMouse) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !panning) {
            orbiting = true;
            lastMouse = GetMousePosition();
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && !orbiting) {
            panning = true;
            lastMouse = GetMousePosition();
        }
        float wheel = GetMouseWheelMove();
        if (wheel != 0) camDist = Clamp(camDist - wheel * camDist * 0.1f, 0.2f, 200.0f);
    }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) orbiting = false;
    if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT)) panning = false;

    if (orbiting) {
        Vector2 cur = GetMousePosition();
        camAngle -= (cur.x - lastMouse.x) * 0.005f;
        camPitch += (cur.y - lastMouse.y) * 0.005f;
        camPitch = Clamp(camPitch, -1.4f, 1.4f);
        lastMouse = cur;
    }
    if (panning) {
        Vector2 cur = GetMousePosition();
        float dx = (cur.x - lastMouse.x) * camDist * 0.002f;
        float dy = (cur.y - lastMouse.y) * camDist * 0.002f;
        Vector3 fwd = Vector3Normalize(Vector3Subtract(camTarget, camera.position));
        Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, camera.up));
        Vector3 up = Vector3Normalize(Vector3CrossProduct(right, fwd));
        camTarget = Vector3Add(camTarget, Vector3Scale(right, -dx));
        camTarget = Vector3Add(camTarget, Vector3Scale(up, dy));
        lastMouse = cur;
    }

    camera.position = {
        camTarget.x + cosf(camAngle) * cosf(camPitch) * camDist,
        camTarget.y + sinf(camPitch) * camDist,
        camTarget.z + sinf(camAngle) * cosf(camPitch) * camDist
    };
    camera.target = camTarget;

    // --- Viewport hover picking ---
    hoveredMeshIdx = -1;
    if (!imguiMouse && modelLoaded && !orbiting && !panning) {
        Ray ray = GetScreenToWorldRay(GetMousePosition(), camera);
        float closest = 9999.0f;
        for (int i = 0; i < meshCount; i++) {
            if (!IsMeshVisible(i)) continue;
            RayCollision rc = GetRayCollisionMesh(ray, model.meshes[i], model.transform);
            if (rc.hit && rc.distance < closest) {
                closest = rc.distance;
                hoveredMeshIdx = i;
            }
        }
    }

    // --- Draw ---
    BeginDrawing();
    ClearBackground((Color){25, 25, 35, 255});

    BeginMode3D(camera);
    DrawGrid(20, 1.0f);

    if (modelLoaded) {
        for (int i = 0; i < meshCount; i++) {
            if (!IsMeshVisible(i)) continue;

            bool highlight = (i == hoveredMeshIdx) || (i == trayHoveredIdx);
            Color col = highlight ? Brighten(meshInfos[i].color, 0.35f) : meshInfos[i].color;
            meshMats[i].maps[MATERIAL_MAP_DIFFUSE].color = col;
            DrawMesh(model.meshes[i], meshMats[i], model.transform);

            if (wireframeOn) {
                rlEnableWireMode();
                Material wireMat = LoadMaterialDefault();
                wireMat.maps[MATERIAL_MAP_DIFFUSE].color = {255, 255, 255, 30};
                DrawMesh(model.meshes[i], wireMat, model.transform);
                rlDisableWireMode();
            }
        }
    }

    EndMode3D();

    // --- ImGui ---
    rlImGuiBegin();

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 500), ImGuiCond_FirstUseEver);
    ImGui::Begin("Mesh Inspector");

    // Toolbar
    if (ImGui::Button("Show All")) {
        soloIdx = -1;
        for (int i = 0; i < meshCount; i++) meshInfos[i].visible = true;
        FitCameraToModel();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Cam")) FitCameraToModel();
    ImGui::SameLine();
    ImGui::Checkbox("Wire", &wireframeOn);

    if (!modelLoaded) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Upload a .glb file above.");
    } else {
        ImGui::Separator();
        ImGui::Text("Meshes: %d", meshCount);

        int totalVerts = 0;
        for (int i = 0; i < meshCount; i++) totalVerts += model.meshes[i].vertexCount;
        ImGui::SameLine();
        ImGui::TextDisabled("(%d verts total)", totalVerts);

        ImGui::Separator();

        // Mesh list
        ImGui::BeginChild("MeshList", ImVec2(0, 0), true);
        trayHoveredIdx = -1;

        for (int i = 0; i < meshCount; i++) {
            ImGui::PushID(i);

            bool isVisible = IsMeshVisible(i);
            bool isSolo = (soloIdx == i);
            bool isHighlighted = (i == hoveredMeshIdx);

            // Highlight background if viewport-hovered
            if (isHighlighted) {
                ImVec2 p = ImGui::GetCursorScreenPos();
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImVec2(p.x - 4, p.y - 2),
                    ImVec2(p.x + ImGui::GetContentRegionAvail().x + 4, p.y + ImGui::GetTextLineHeight() + 6),
                    IM_COL32(60, 60, 120, 180), 3.0f);
                // Scroll to this item
                ImGui::SetScrollHereY();
            }

            // Color swatch
            ImVec4 col = ImVec4(meshInfos[i].color.r / 255.f, meshInfos[i].color.g / 255.f,
                                meshInfos[i].color.b / 255.f, 1.0f);
            ImGui::ColorButton("##c", col, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker,
                               ImVec2(12, 12));
            ImGui::SameLine();

            // Visibility toggle
            if (!isVisible) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
            bool vis = meshInfos[i].visible;
            if (ImGui::SmallButton(vis ? "E" : "H")) {
                meshInfos[i].visible = !meshInfos[i].visible;
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(vis ? "Hide" : "Show");
            if (!isVisible) ImGui::PopStyleColor();
            ImGui::SameLine();

            // Solo button
            if (isSolo) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.35f, 0.1f, 1.0f));
            if (ImGui::SmallButton("S")) {
                if (soloIdx == i) soloIdx = -1;
                else { soloIdx = i; FitCameraToMesh(i); }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(isSolo ? "Unsolo" : "Solo");
            if (isSolo) ImGui::PopStyleColor();
            ImGui::SameLine();

            // Name + vert count
            ImGui::Text("%s", meshInfos[i].name);
            ImGui::SameLine();
            ImGui::TextDisabled("(%dv %dt)", model.meshes[i].vertexCount, model.meshes[i].triangleCount);

            // Tray hover detection
            if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
                trayHoveredIdx = i;
            }

            ImGui::PopID();
        }

        ImGui::EndChild();
    }

    ImGui::End();

    // Viewport tooltip for hovered mesh
    if (hoveredMeshIdx >= 0 && !imguiMouse) {
        Vector2 mouse = GetMousePosition();
        ImGui::SetNextWindowPos(ImVec2(mouse.x + 16, mouse.y - 8));
        ImGui::SetNextWindowBgAlpha(0.85f);
        ImGui::Begin("##tooltip", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);
        ImVec4 col = ImVec4(meshInfos[hoveredMeshIdx].color.r / 255.f,
                            meshInfos[hoveredMeshIdx].color.g / 255.f,
                            meshInfos[hoveredMeshIdx].color.b / 255.f, 1.0f);
        ImGui::ColorButton("##tc", col, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker,
                           ImVec2(10, 10));
        ImGui::SameLine();
        ImGui::Text("%s  %dv %dt", meshInfos[hoveredMeshIdx].name,
            model.meshes[hoveredMeshIdx].vertexCount,
            model.meshes[hoveredMeshIdx].triangleCount);
        ImGui::End();
    }

    rlImGuiEnd();
    DrawFPS(GetScreenWidth() - 90, 8);
    EndDrawing();
}

int main() {
    InitWindow(1024, 700, "GLB Mesh Inspector");
    SetTargetFPS(60);
    rlImGuiSetup(true);

    camera.position = {5, 5, 5};
    camera.target = {0, 0, 0};
    camera.up = {0, 1, 0};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(UpdateAndDraw, 0, 1);
#else
    while (!WindowShouldClose()) UpdateAndDraw();
#endif

    if (modelLoaded) UnloadModel(model);
    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
