#include <imgui.h>
#include <raylib.h>
#include <raymath.h>
#include <rlImGui.h>
#include <cstring>
#include <cstdio>
#include <cmath>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>

// How many GLB files were pre-loaded by the shell script
EM_JS(int, js_glb_count, (), {
    if (typeof window.__GLB_LOADED === "undefined") return 0;
    return window.__GLB_LOADED.length;
});

// Get the name of a pre-loaded GLB
EM_JS(void, js_glb_name, (int index, char *buf, int bufSize), {
    var arr = window.__GLB_LOADED;
    if (!arr || index < 0 || index >= arr.length) return;
    stringToUTF8(arr[index].name, buf, bufSize);
});

// Write a pre-loaded GLB's binary data into Emscripten's virtual FS
EM_JS(int, js_glb_write_to_vfs, (int index, const char *vfsPath), {
    var arr = window.__GLB_LOADED;
    if (!arr || index < 0 || index >= arr.length) return 0;
    var entry = arr[index];
    if (!entry || !entry.data) return 0;
    var vfsp = UTF8ToString(vfsPath);
    try { FS.unlink(vfsp); } catch(e) {}
    FS.writeFile(vfsp, entry.data);
    return entry.data.length;
});
#endif

struct GlbEntry {
    char name[128];
};

static GlbEntry glbFiles[64];
static int glbCount = 0;

static int selectedIdx = -1;
static Model currentModel = {0};
static bool modelLoaded = false;
static bool loadFailed = false;
static float angle = 0.0f;
static float orbitDist = 5.0f;
static float orbitHeight = 3.0f;
static Camera3D camera = {0};

void LoadGlb(int idx) {
#ifdef __EMSCRIPTEN__
    if (modelLoaded) {
        UnloadModel(currentModel);
        modelLoaded = false;
    }
    loadFailed = false;

    int bytes = js_glb_write_to_vfs(idx, "/tmp/model.glb");
    if (bytes <= 0) {
        loadFailed = true;
        selectedIdx = idx;
        return;
    }

    currentModel = LoadModel("/tmp/model.glb");
    modelLoaded = true;
    selectedIdx = idx;

    BoundingBox bb = GetModelBoundingBox(currentModel);
    Vector3 center = {
        (bb.min.x + bb.max.x) * 0.5f,
        (bb.min.y + bb.max.y) * 0.5f,
        (bb.min.z + bb.max.z) * 0.5f
    };
    float maxSize = fmaxf(fmaxf(bb.max.x - bb.min.x, bb.max.y - bb.min.y), bb.max.z - bb.min.z);
    orbitDist = maxSize * 2.0f;
    if (orbitDist < 3.0f) orbitDist = 3.0f;
    orbitHeight = center.y + orbitDist * 0.4f;

    camera.target = center;
    camera.position = (Vector3){center.x + orbitDist, orbitHeight, center.z + orbitDist};
    angle = 0.0f;
#endif
}

void UpdateAndDraw() {
    angle += 0.01f;

    if (modelLoaded) {
        camera.position = (Vector3){
            camera.target.x + cosf(angle) * orbitDist,
            orbitHeight,
            camera.target.z + sinf(angle) * orbitDist
        };
    }

    BeginDrawing();
    ClearBackground(DARKGRAY);

    BeginMode3D(camera);
    DrawGrid(10, 1.0f);
    if (modelLoaded) {
        DrawModel(currentModel, (Vector3){0, 0, 0}, 1.0f, WHITE);
    }
    EndMode3D();

    rlImGuiBegin();

    ImGui::Begin("GLB Files");
    ImGui::Text("Models: %d", glbCount);
    ImGui::Separator();

    if (glbCount == 0) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "No .glb files loaded.");
    } else {
        ImGui::BeginChild("FileList", ImVec2(0, 0), true);
        for (int i = 0; i < glbCount; i++) {
            bool selected = (i == selectedIdx);
            if (ImGui::Selectable(glbFiles[i].name, selected)) {
                LoadGlb(i);
            }
        }
        ImGui::EndChild();
    }

    if (loadFailed) {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to load: %s",
            selectedIdx >= 0 ? glbFiles[selectedIdx].name : "?");
    }

    ImGui::End();

    rlImGuiEnd();
    DrawFPS(10, 10);
    EndDrawing();
}

int main() {
    InitWindow(800, 600, "glb viewer");
    SetTargetFPS(60);
    rlImGuiSetup(true);

    camera.position = (Vector3){5.0f, 5.0f, 5.0f};
    camera.target = (Vector3){0.0f, 0.0f, 0.0f};
    camera.up = (Vector3){0.0f, 1.0f, 0.0f};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

#ifdef __EMSCRIPTEN__
    glbCount = js_glb_count();
    if (glbCount > 64) glbCount = 64;
    for (int i = 0; i < glbCount; i++) {
        memset(glbFiles[i].name, 0, 128);
        js_glb_name(i, glbFiles[i].name, 128);
    }
    if (glbCount > 0) LoadGlb(0);
    emscripten_set_main_loop(UpdateAndDraw, 0, 1);
#else
    while (!WindowShouldClose()) {
        UpdateAndDraw();
    }
#endif

    if (modelLoaded) UnloadModel(currentModel);
    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
