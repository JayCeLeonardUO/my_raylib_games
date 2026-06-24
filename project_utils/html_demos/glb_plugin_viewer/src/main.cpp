#include <imgui.h>
#include <raylib.h>
#include <raymath.h>
#include <rlImGui.h>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>

// Write __GLB_BYTES into Emscripten VFS
EM_JS(int, load_glb_from_js, (), {
    var bytes = window.__GLB_BYTES;
    if (!bytes || bytes.length === 0) return 0;
    try { FS.unlink("/tmp/model.glb"); } catch(e) {}
    FS.writeFile("/tmp/model.glb", bytes);
    return bytes.length;
});

// JSON query functions - path is dot-separated, arrays use numeric index
// e.g. "nodes.0.name", "meshes", "asset.version"

// Get type at path: 0=missing, 1=object, 2=array, 3=string, 4=number, 5=bool, 6=null
EM_JS(int, json_type, (const char *path), {
    var p = UTF8ToString(path);
    var obj = window.__GLB_JSON;
    if (!obj) return 0;
    if (p.length > 0) {
        var parts = p.split(".");
        for (var i = 0; i < parts.length; i++) {
            if (obj == null) return 0;
            var key = parts[i];
            if (Array.isArray(obj) && /^\d+$/.test(key)) {
                obj = obj[parseInt(key)];
            } else {
                obj = obj[key];
            }
        }
    }
    if (obj === undefined || obj === null) return (obj === null) ? 6 : 0;
    if (Array.isArray(obj)) return 2;
    if (typeof obj === "object") return 1;
    if (typeof obj === "string") return 3;
    if (typeof obj === "number") return 4;
    if (typeof obj === "boolean") return 5;
    return 0;
});

// Get number of keys (object) or length (array)
EM_JS(int, json_length, (const char *path), {
    var p = UTF8ToString(path);
    var obj = window.__GLB_JSON;
    if (!obj) return 0;
    if (p.length > 0) {
        var parts = p.split(".");
        for (var i = 0; i < parts.length; i++) {
            if (obj == null) return 0;
            var key = parts[i];
            if (Array.isArray(obj) && /^\d+$/.test(key)) obj = obj[parseInt(key)];
            else obj = obj[key];
        }
    }
    if (obj === undefined || obj === null) return 0;
    if (Array.isArray(obj)) return obj.length;
    if (typeof obj === "object") return Object.keys(obj).length;
    return 0;
});

// Get key name at index for an object, or index as string for array
EM_JS(void, json_key_at, (const char *path, int index, char *buf, int bufSize), {
    var p = UTF8ToString(path);
    var obj = window.__GLB_JSON;
    if (!obj) return;
    if (p.length > 0) {
        var parts = p.split(".");
        for (var i = 0; i < parts.length; i++) {
            if (obj == null) return;
            var key = parts[i];
            if (Array.isArray(obj) && /^\d+$/.test(key)) obj = obj[parseInt(key)];
            else obj = obj[key];
        }
    }
    if (obj === undefined || obj === null) return;
    var k;
    if (Array.isArray(obj)) k = String(index);
    else k = Object.keys(obj)[index];
    if (k !== undefined) stringToUTF8(k, buf, bufSize);
});

// Get value as string (for leaf nodes)
EM_JS(void, json_value_str, (const char *path, char *buf, int bufSize), {
    var p = UTF8ToString(path);
    var obj = window.__GLB_JSON;
    if (!obj) return;
    if (p.length > 0) {
        var parts = p.split(".");
        for (var i = 0; i < parts.length; i++) {
            if (obj == null) return;
            var key = parts[i];
            if (Array.isArray(obj) && /^\d+$/.test(key)) obj = obj[parseInt(key)];
            else obj = obj[key];
        }
    }
    if (obj === undefined) return;
    var str;
    if (obj === null) str = "null";
    else if (typeof obj === "object") str = Array.isArray(obj) ? "[" + obj.length + " items]" : "{" + Object.keys(obj).length + " keys}";
    else str = String(obj);
    stringToUTF8(str, buf, bufSize);
});

// Check if GLB JSON was parsed
EM_JS(int, has_glb_json, (), {
    return window.__GLB_JSON ? 1 : 0;
});
#endif

static Model model = {0};
static bool modelLoaded = false;
static float angle = 0.0f;
static float orbitDist = 5.0f;
static float orbitHeight = 3.0f;
static Camera3D camera = {0};

#ifdef __EMSCRIPTEN__
// Recursive ImGui tree for the GLB JSON
void DrawJsonTree(const char *path, const char *label) {
    int type = json_type(path);

    if (type == 1 || type == 2) {
        // Object or array
        int len = json_length(path);
        char header[256];
        if (type == 2) {
            snprintf(header, sizeof(header), "%s [%d]", label, len);
        } else {
            snprintf(header, sizeof(header), "%s {%d}", label, len);
        }

        if (ImGui::TreeNode(header)) {
            for (int i = 0; i < len; i++) {
                char key[128] = {0};
                json_key_at(path, i, key, sizeof(key));

                char childPath[512];
                if (path[0] == '\0') {
                    snprintf(childPath, sizeof(childPath), "%s", key);
                } else {
                    snprintf(childPath, sizeof(childPath), "%s.%s", path, key);
                }

                int childType = json_type(childPath);
                if (childType == 1 || childType == 2) {
                    DrawJsonTree(childPath, key);
                } else {
                    char val[256] = {0};
                    json_value_str(childPath, val, sizeof(val));
                    ImGui::Text("%s: %s", key, val);
                }
            }
            ImGui::TreePop();
        }
    } else {
        char val[256] = {0};
        json_value_str(path, val, sizeof(val));
        ImGui::Text("%s: %s", label, val);
    }
}

void DrawInstanceInfo() {
    if (!has_glb_json()) return;

    // Count how many nodes reference each mesh
    int nodeCount = json_length("nodes");
    int meshCount = json_length("meshes");

    if (meshCount == 0) return;

    if (ImGui::CollapsingHeader("Instances", ImGuiTreeNodeFlags_DefaultOpen)) {
        // For each mesh, count referencing nodes
        for (int m = 0; m < meshCount; m++) {
            char meshPath[64];
            snprintf(meshPath, sizeof(meshPath), "meshes.%d.name", m);
            char meshName[128] = {0};
            json_value_str(meshPath, meshName, sizeof(meshName));
            if (meshName[0] == '\0') snprintf(meshName, sizeof(meshName), "Mesh %d", m);

            int refCount = 0;
            for (int n = 0; n < nodeCount; n++) {
                char nodeMeshPath[64];
                snprintf(nodeMeshPath, sizeof(nodeMeshPath), "nodes.%d.mesh", n);
                int t = json_type(nodeMeshPath);
                if (t == 4) { // number
                    char val[32] = {0};
                    json_value_str(nodeMeshPath, val, sizeof(val));
                    if (atoi(val) == m) refCount++;
                }
            }

            ImGui::BulletText("%s: %d instance(s)", meshName, refCount);
        }
    }
}
#endif

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
    ClearBackground((Color){34, 34, 34, 255});

    BeginMode3D(camera);
    DrawGrid(10, 1.0f);
    if (modelLoaded) {
        DrawModel(model, (Vector3){0, 0, 0}, 1.0f, WHITE);
    }
    EndMode3D();

    rlImGuiBegin();

#ifdef __EMSCRIPTEN__
    ImGui::Begin("Inspector");

    if (modelLoaded) {
        ImGui::Text("Meshes: %d", model.meshCount);
        ImGui::Text("Materials: %d", model.materialCount);
        ImGui::Text("Bones: %d", model.boneCount);

        // Per-mesh vertex/triangle info
        if (ImGui::CollapsingHeader("Mesh Details")) {
            for (int i = 0; i < model.meshCount; i++) {
                ImGui::BulletText("Mesh %d: %d verts, %d tris",
                    i, model.meshes[i].vertexCount, model.meshes[i].triangleCount);
            }
        }

        // Bounding box
        BoundingBox bb = GetModelBoundingBox(model);
        if (ImGui::CollapsingHeader("Bounds")) {
            ImGui::Text("Min: %.2f, %.2f, %.2f", bb.min.x, bb.min.y, bb.min.z);
            ImGui::Text("Max: %.2f, %.2f, %.2f", bb.max.x, bb.max.y, bb.max.z);
            ImGui::Text("Size: %.2f x %.2f x %.2f",
                bb.max.x - bb.min.x, bb.max.y - bb.min.y, bb.max.z - bb.min.z);
        }
    }

    ImGui::Separator();

    if (has_glb_json()) {
        DrawInstanceInfo();
        ImGui::Separator();

        if (ImGui::CollapsingHeader("GLTF JSON")) {
            DrawJsonTree("", "root");
        }
    } else {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "No GLTF JSON found");
    }

    ImGui::End();
#endif

    rlImGuiEnd();

    if (!modelLoaded) {
        DrawText("No model loaded", 10, 10, 20, RED);
    }
    DrawFPS(10, GetScreenHeight() - 24);

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
    int bytes = load_glb_from_js();
    if (bytes > 0) {
        model = LoadModel("/tmp/model.glb");
        modelLoaded = true;

        BoundingBox bb = GetModelBoundingBox(model);
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
    }
    emscripten_set_main_loop(UpdateAndDraw, 0, 1);
#else
    while (!WindowShouldClose()) {
        UpdateAndDraw();
    }
#endif

    if (modelLoaded) UnloadModel(model);
    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
