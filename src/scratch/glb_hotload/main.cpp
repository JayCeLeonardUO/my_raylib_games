/**
 * GLB Hotload Viewer
 *
 * Simple Raylib app that watches a GLB file and reloads when it changes.
 * Use with the Blender addon to export and see changes live.
 *
 * Usage: ./glb_hotload [path_to_glb]
 * Default: ./assets/hotload.glb
 *
 * Controls:
 * - Mouse drag: Orbit camera
 * - Scroll: Zoom
 * - R: Force reload
 * - Space: Reset camera
 */

#include <cstring>
#include <imgui.h>
#include <raylib.h>
#include <raymath.h>
#include <rlImGui.h>
#include <string>
#include <sys/stat.h>

struct HotloadViewer {
  std::string glb_path = "./assets/hotload.glb";
  Model model = {0};
  bool model_loaded = false;
  time_t last_modified = 0;
  float check_interval = 0.5f;
  float check_timer = 0.0f;

  Camera3D camera = {0};
  float orbit_angle = 0.0f;
  float orbit_height = 2.0f;
  float orbit_distance = 5.0f;

  int reload_count = 0;
  bool auto_reload = true;

  void init(const char* path) {
    if (path)
      glb_path = path;

    camera.position = {0.0f, 2.0f, 5.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    try_load();
  }

  time_t get_file_mtime() {
    struct stat st;
    if (stat(glb_path.c_str(), &st) == 0) {
      return st.st_mtime;
    }
    return 0;
  }

  bool file_exists() {
    struct stat st;
    return stat(glb_path.c_str(), &st) == 0;
  }

  void try_load() {
    if (!file_exists()) {
      TraceLog(LOG_WARNING, "GLB file not found: %s", glb_path.c_str());
      return;
    }

    if (model_loaded) {
      UnloadModel(model);
      model_loaded = false;
    }

    model = LoadModel(glb_path.c_str());
    if (model.meshCount > 0) {
      model_loaded = true;
      last_modified = get_file_mtime();
      reload_count++;
      TraceLog(LOG_INFO, "Loaded GLB: %s (meshes: %d, reload #%d)", glb_path.c_str(),
               model.meshCount, reload_count);
    } else {
      TraceLog(LOG_ERROR, "Failed to load GLB: %s", glb_path.c_str());
    }
  }

  void check_reload() {
    if (!auto_reload)
      return;

    time_t current_mtime = get_file_mtime();
    if (current_mtime > 0 && current_mtime != last_modified) {
      TraceLog(LOG_INFO, "File changed, reloading...");
      try_load();
    }
  }

  void update() {
    // Check for file changes
    check_timer += GetFrameTime();
    if (check_timer >= check_interval) {
      check_timer = 0.0f;
      check_reload();
    }

    // Force reload with R
    if (IsKeyPressed(KEY_R)) {
      try_load();
    }

    // Reset camera with Space
    if (IsKeyPressed(KEY_SPACE)) {
      orbit_angle = 0.0f;
      orbit_height = 2.0f;
      orbit_distance = 5.0f;
    }

    // Orbit camera with mouse drag
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
      Vector2 delta = GetMouseDelta();
      orbit_angle += delta.x * 0.01f;
      orbit_height -= delta.y * 0.05f;
      orbit_height = Clamp(orbit_height, -5.0f, 10.0f);
    }

    // Zoom with scroll
    float scroll = GetMouseWheelMove();
    orbit_distance -= scroll * 0.5f;
    orbit_distance = Clamp(orbit_distance, 1.0f, 20.0f);

    // Update camera position
    camera.position.x = sinf(orbit_angle) * orbit_distance;
    camera.position.z = cosf(orbit_angle) * orbit_distance;
    camera.position.y = orbit_height;
  }

  void draw() {
    BeginMode3D(camera);

    DrawGrid(10, 1.0f);

    if (model_loaded) {
      DrawModel(model, {0, 0, 0}, 1.0f, WHITE);
      DrawModelWires(model, {0, 0, 0}, 1.0f, DARKGRAY);
    }

    // Draw axis
    DrawLine3D({0, 0, 0}, {2, 0, 0}, RED);
    DrawLine3D({0, 0, 0}, {0, 2, 0}, GREEN);
    DrawLine3D({0, 0, 0}, {0, 0, 2}, BLUE);

    EndMode3D();
  }

  void draw_imgui() {
    if (ImGui::Begin("GLB Hotload")) {
      ImGui::Text("File: %s", glb_path.c_str());
      ImGui::Text("Status: %s", model_loaded ? "Loaded" : "Not loaded");
      if (model_loaded) {
        ImGui::Text("Meshes: %d", model.meshCount);
        ImGui::Text("Materials: %d", model.materialCount);
      }
      ImGui::Text("Reload count: %d", reload_count);

      ImGui::Separator();
      ImGui::Checkbox("Auto reload", &auto_reload);
      ImGui::SliderFloat("Check interval", &check_interval, 0.1f, 2.0f, "%.1f s");

      if (ImGui::Button("Force Reload (R)")) {
        try_load();
      }

      ImGui::Separator();
      ImGui::Text("Controls:");
      ImGui::BulletText("Mouse drag: Orbit");
      ImGui::BulletText("Scroll: Zoom");
      ImGui::BulletText("Space: Reset camera");
    }
    ImGui::End();
  }

  void cleanup() {
    if (model_loaded) {
      UnloadModel(model);
    }
  }
};

int main(int argc, char* argv[]) {
  const int screenWidth = 1024;
  const int screenHeight = 768;

  InitWindow(screenWidth, screenHeight, "GLB Hotload Viewer");
  SetTargetFPS(60);
  rlImGuiSetup(true);

  HotloadViewer viewer;
  viewer.init(argc > 1 ? argv[1] : nullptr);

  while (!WindowShouldClose()) {
    viewer.update();

    BeginDrawing();
    ClearBackground({40, 44, 52, 255});

    viewer.draw();

    rlImGuiBegin();
    viewer.draw_imgui();
    rlImGuiEnd();

    // Status bar
    DrawRectangle(0, screenHeight - 25, screenWidth, 25, {30, 30, 30, 255});
    const char* status =
        viewer.model_loaded
            ? TextFormat("Watching: %s | Reloads: %d", viewer.glb_path.c_str(), viewer.reload_count)
            : TextFormat("Waiting for: %s", viewer.glb_path.c_str());
    DrawText(status, 10, screenHeight - 20, 14, LIGHTGRAY);

    DrawFPS(screenWidth - 100, 10);
    EndDrawing();
  }

  viewer.cleanup();
  rlImGuiShutdown();
  CloseWindow();

  return 0;
}
