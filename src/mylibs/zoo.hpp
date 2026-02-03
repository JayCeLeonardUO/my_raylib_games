#if 0
cd "$(dirname "$0")/../.." && cmake --build build --target mylibs_tests && ./build/mylibs_tests --no-skip -tc="image zoo visual test"
exit
#endif
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <dirent.h>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <string>
#include <sys/stat.h>
#include <vector>

struct ImageEntry {
  Texture2D texture;
  std::string filename;
  std::string fullpath;
  std::string folder; // parent folder name for grouping
  bool loaded = false;
  int width = 0;
  int height = 0;
  Vector3 position = {0, 0, 0};
  Model model;
  bool modelLoaded = false;
};

struct ImageZoo {
  std::vector<ImageEntry> images;
  int columns = 6;
  float spacing = 2.5f;
  float imageScale = 1.0f;
  int selectedIndex = -1;
  bool showInfo = true;
  int maxImages = 1000;
  int maxDepth = 3;
  int loadedCount = 0;
  bool groupByFolder = true;

  Camera3D camera = {0};
  bool cameraEnabled = false;
  float cameraSpeed = 0.1f;

  void init_camera() {
    camera.position = {0.0f, 8.0f, 12.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
  }

  void load_directory(const char* path, int depth = 0) {
    if (depth == 0) {
      loadedCount = 0;
    }
    if (loadedCount >= maxImages)
      return;
    if (depth > maxDepth)
      return;

    DIR* dir = opendir(path);
    if (!dir) {
      TraceLog(LOG_ERROR, "Failed to open directory: %s", path);
      return;
    }

    std::vector<std::string> subdirs;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && loadedCount < maxImages) {
      if (entry->d_name[0] == '.')
        continue;

      std::string fullpath = std::string(path) + "/" + entry->d_name;

      struct stat st;
      if (stat(fullpath.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
        subdirs.push_back(fullpath);
        continue;
      }

      const char* ext = strrchr(entry->d_name, '.');
      if (!ext)
        continue;

      std::string ext_lower = ext;
      std::transform(ext_lower.begin(), ext_lower.end(), ext_lower.begin(), ::tolower);

      if (ext_lower == ".png" || ext_lower == ".jpg" || ext_lower == ".jpeg" ||
          ext_lower == ".bmp" || ext_lower == ".gif") {

        ImageEntry img;
        img.filename = entry->d_name;
        img.fullpath = fullpath;

        // Extract folder name for grouping
        std::string folderPath = path;
        size_t lastSlash = folderPath.find_last_of('/');
        img.folder =
            (lastSlash != std::string::npos) ? folderPath.substr(lastSlash + 1) : folderPath;

        img.texture = LoadTexture(img.fullpath.c_str());
        if (img.texture.id != 0) {
          img.width = img.texture.width;
          img.height = img.texture.height;
          img.loaded = true;
          images.push_back(img);
          loadedCount++;
        }
      }
    }
    closedir(dir);

    for (const auto& subdir : subdirs) {
      if (loadedCount >= maxImages)
        break;
      load_directory(subdir.c_str(), depth + 1);
    }

    if (depth == 0) {
      // Sort: by folder first (if grouping), then by filename
      std::sort(images.begin(), images.end(), [this](const ImageEntry& a, const ImageEntry& b) {
        if (groupByFolder && a.folder != b.folder) {
          return a.folder < b.folder;
        }
        return a.filename < b.filename;
      });

      // Assign 3D positions and create models
      for (size_t i = 0; i < images.size(); i++) {
        int col = i % columns;
        int row = i / columns;

        float offsetX = (columns - 1) * spacing / 2.0f;

        images[i].position = {col * spacing - offsetX, 0.0f, row * spacing};

        // Create a plane mesh for each image
        float aspect = (float)images[i].width / (float)images[i].height;
        float planeWidth = imageScale * aspect;
        float planeHeight = imageScale;

        Mesh mesh = GenMeshPlane(planeWidth, planeHeight, 1, 1);
        images[i].model = LoadModelFromMesh(mesh);
        images[i].model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = images[i].texture;
        images[i].modelLoaded = true;
      }

      TraceLog(LOG_INFO, "Loaded %d images from %s (depth=%d, max=%d)", (int)images.size(), path,
               maxDepth, maxImages);
    }
  }

  void rebuild_models() {
    // Re-sort if grouping changed
    std::sort(images.begin(), images.end(), [this](const ImageEntry& a, const ImageEntry& b) {
      if (groupByFolder && a.folder != b.folder) {
        return a.folder < b.folder;
      }
      return a.filename < b.filename;
    });

    for (size_t i = 0; i < images.size(); i++) {
      if (images[i].modelLoaded) {
        UnloadModel(images[i].model);
      }

      int col = i % columns;
      int row = i / columns;
      float offsetX = (columns - 1) * spacing / 2.0f;

      images[i].position = {col * spacing - offsetX, 0.0f, row * spacing};

      float aspect = (float)images[i].width / (float)images[i].height;
      float planeWidth = imageScale * aspect;
      float planeHeight = imageScale;

      Mesh mesh = GenMeshPlane(planeWidth, planeHeight, 1, 1);
      images[i].model = LoadModelFromMesh(mesh);
      images[i].model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = images[i].texture;
      images[i].modelLoaded = true;
    }
  }

  void unload_all() {
    for (auto& img : images) {
      if (img.modelLoaded) {
        UnloadModel(img.model);
      }
      if (img.loaded) {
        UnloadTexture(img.texture);
      }
    }
    images.clear();
  }

  void update() {
    // Toggle camera control
    if (IsKeyPressed(KEY_TAB)) {
      cameraEnabled = !cameraEnabled;
      if (cameraEnabled)
        DisableCursor();
      else
        EnableCursor();
    }

    // Selection with arrow keys (always active)
    if (IsKeyPressed(KEY_LEFT) && selectedIndex > 0)
      selectedIndex--;
    if (IsKeyPressed(KEY_RIGHT) && selectedIndex < (int)images.size() - 1)
      selectedIndex++;
    if (IsKeyPressed(KEY_UP) && selectedIndex >= columns)
      selectedIndex -= columns;
    if (IsKeyPressed(KEY_DOWN) && selectedIndex + columns < (int)images.size())
      selectedIndex += columns;
    if (IsKeyPressed(KEY_HOME))
      selectedIndex = 0;
    if (IsKeyPressed(KEY_END))
      selectedIndex = images.size() - 1;

    // Keep camera centered on selected tile
    if (selectedIndex >= 0 && selectedIndex < (int)images.size()) {
      Vector3 targetPos = images[selectedIndex].position;
      camera.target = targetPos;

      // Maintain camera offset relative to target
      if (!cameraEnabled) {
        // Smooth follow when not in free camera mode
        Vector3 desiredPos = {targetPos.x, targetPos.y + 8.0f, targetPos.z + 4.0f};
        camera.position = Vector3Lerp(camera.position, desiredPos, 0.1f);
      }
    }

    if (cameraEnabled) {
      // Custom camera movement with speed control
      Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
      Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));

      float speed = cameraSpeed * GetFrameTime() * 60.0f;

      if (IsKeyDown(KEY_W)) {
        camera.position = Vector3Add(camera.position, Vector3Scale(forward, speed));
        camera.target = Vector3Add(camera.target, Vector3Scale(forward, speed));
      }
      if (IsKeyDown(KEY_S)) {
        camera.position = Vector3Subtract(camera.position, Vector3Scale(forward, speed));
        camera.target = Vector3Subtract(camera.target, Vector3Scale(forward, speed));
      }
      if (IsKeyDown(KEY_A)) {
        camera.position = Vector3Subtract(camera.position, Vector3Scale(right, speed));
        camera.target = Vector3Subtract(camera.target, Vector3Scale(right, speed));
      }
      if (IsKeyDown(KEY_D)) {
        camera.position = Vector3Add(camera.position, Vector3Scale(right, speed));
        camera.target = Vector3Add(camera.target, Vector3Scale(right, speed));
      }
      if (IsKeyDown(KEY_E) || IsKeyDown(KEY_SPACE)) {
        camera.position.y += speed;
        camera.target.y += speed;
      }
      if (IsKeyDown(KEY_Q) || IsKeyDown(KEY_LEFT_SHIFT)) {
        camera.position.y -= speed;
        camera.target.y -= speed;
      }

      // Mouse look
      Vector2 mouseDelta = GetMouseDelta();
      float sensitivity = 0.003f;

      // Yaw
      Matrix rotation = MatrixRotate(camera.up, -mouseDelta.x * sensitivity);
      forward = Vector3Transform(forward, rotation);

      // Pitch
      right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
      rotation = MatrixRotate(right, -mouseDelta.y * sensitivity);
      forward = Vector3Transform(forward, rotation);

      camera.target = Vector3Add(camera.position, forward);
    }

    // Mouse picking
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !cameraEnabled) {
      Ray ray = GetScreenToWorldRay(GetMousePosition(), camera);

      float closestDist = 1000000.0f;
      int closestIdx = -1;

      for (size_t i = 0; i < images.size(); i++) {
        // Simple bounding box check for flat images
        Vector3 pos = images[i].position;
        float aspect = (float)images[i].width / (float)images[i].height;
        float hw = imageScale * aspect / 2.0f;
        float hh = imageScale / 2.0f;

        BoundingBox box = {{pos.x - hw, pos.y - 0.1f, pos.z - hh},
                           {pos.x + hw, pos.y + 0.1f, pos.z + hh}};

        RayCollision collision = GetRayCollisionBox(ray, box);
        if (collision.hit && collision.distance < closestDist) {
          closestDist = collision.distance;
          closestIdx = i;
        }
      }

      if (closestIdx >= 0) {
        selectedIndex = closestIdx;
      }
    }
  }

  void draw() {
    BeginMode3D(camera);

    // Draw ground grid
    DrawGrid(50, 1.0f);

    // Draw all images
    for (size_t i = 0; i < images.size(); i++) {
      ImageEntry& img = images[i];

      Vector3 pos = img.position;
      pos.y = 0.01f; // slightly above ground

      Color tint = WHITE;
      if ((int)i == selectedIndex) {
        tint = {255, 255, 200, 255};
      }

      DrawModel(img.model, pos, 1.0f, tint);

      // Draw selection box
      if ((int)i == selectedIndex) {
        float aspect = (float)img.width / (float)img.height;
        float hw = imageScale * aspect / 2.0f + 0.05f;
        float hh = imageScale / 2.0f + 0.05f;

        DrawCubeWires(pos, hw * 2, 0.1f, hh * 2, YELLOW);
      }
    }

    EndMode3D();
  }

  void draw_imgui() {
    if (ImGui::Begin("Image Zoo Controls")) {
      ImGui::Text("Images: %d / %d max", (int)images.size(), maxImages);
      ImGui::Text("TAB to toggle free camera (currently %s)", cameraEnabled ? "ON" : "OFF");
      ImGui::Separator();

      bool needsRebuild = false;
      if (ImGui::SliderInt("Columns", &columns, 1, 20))
        needsRebuild = true;
      if (ImGui::SliderFloat("Spacing", &spacing, 1.0f, 10.0f))
        needsRebuild = true;
      if (ImGui::SliderFloat("Image Scale", &imageScale, 0.5f, 5.0f))
        needsRebuild = true;
      if (ImGui::Checkbox("Group by Folder", &groupByFolder))
        needsRebuild = true;

      if (needsRebuild) {
        rebuild_models();
      }

      ImGui::Separator();
      ImGui::SliderInt("Max Images", &maxImages, 10, 5000);
      ImGui::SliderInt("Max Depth", &maxDepth, 0, 10);
      ImGui::Separator();
      ImGui::Checkbox("Show Info Panel", &showInfo);

      ImGui::Separator();
      ImGui::Text("Camera");
      ImGui::SliderFloat("Camera Speed", &cameraSpeed, 0.01f, 1.0f);
      ImGui::Text("Pos: %.1f, %.1f, %.1f", camera.position.x, camera.position.y, camera.position.z);
      ImGui::Text("Target: %.1f, %.1f, %.1f", camera.target.x, camera.target.y, camera.target.z);

      if (ImGui::Button("Reset Camera")) {
        init_camera();
      }
    }
    ImGui::End();

    if (showInfo && selectedIndex >= 0 && selectedIndex < (int)images.size()) {
      if (ImGui::Begin("Selected Image")) {
        ImageEntry& img = images[selectedIndex];
        ImGui::Text("Index: %d", selectedIndex);
        ImGui::Text("Folder: %s", img.folder.c_str());
        ImGui::Text("File: %s", img.filename.c_str());
        ImGui::Text("Path: %s", img.fullpath.c_str());
        ImGui::Text("Size: %d x %d", img.width, img.height);
        ImGui::Text("Pos: %.1f, %.1f, %.1f", img.position.x, img.position.y, img.position.z);

        float previewSize = 256;
        float scale = previewSize / fmaxf(img.width, img.height);
        ImGui::Image((ImTextureID)(intptr_t)img.texture.id,
                     ImVec2(img.width * scale, img.height * scale));
      }
      ImGui::End();
    }
  }
};

// ============================================================================
// DOCTEST
// ============================================================================
#ifdef DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("ImageEntry defaults") {
  ImageEntry entry;
  CHECK(entry.loaded == false);
  CHECK(entry.width == 0);
  CHECK(entry.height == 0);
  CHECK(entry.filename.empty());
}

TEST_CASE("ImageZoo defaults") {
  ImageZoo zoo;
  CHECK(zoo.columns == 6);
  CHECK(zoo.spacing == doctest::Approx(2.5f));
  CHECK(zoo.selectedIndex == -1);
  CHECK(zoo.groupByFolder == true);
  CHECK(zoo.cameraSpeed == doctest::Approx(0.1f));
  CHECK(zoo.images.empty());
}

TEST_CASE("image zoo visual test" * doctest::skip()) {
  // Run with: ./mylibs_tests --no-skip -tc="image zoo visual test"

  const int screenWidth = 1280;
  const int screenHeight = 800;

  InitWindow(screenWidth, screenHeight, "Image Zoo 3D");
  SetTargetFPS(60);
  rlImGuiSetup(true);

  ImageZoo zoo;
  zoo.init_camera();

  const char* path = getenv("IMAGE_ZOO_PATH");
  if (!path)
    path = "/home/jpleona/Documents/itch/isle-of-lore-2-hex-tiles-regular-borderless/Isle of Lore "
           "2 - Borderless";
  zoo.load_directory(path);

  // Select first image and position camera
  if (!zoo.images.empty()) {
    zoo.selectedIndex = 0;
    Vector3 pos = zoo.images[0].position;
    zoo.camera.target = pos;
    zoo.camera.position = {pos.x, pos.y + 8.0f, pos.z + 4.0f};
  }

  while (!WindowShouldClose()) {
    zoo.update();

    BeginDrawing();
    ClearBackground({30, 30, 30, 255});

    zoo.draw();

    rlImGuiBegin();
    zoo.draw_imgui();
    rlImGuiEnd();

    // Status bar
    DrawRectangle(0, screenHeight - 25, screenWidth, 25, {20, 20, 20, 255});
    DrawText(TextFormat("Images: %d | Selected: %d | TAB: free camera | Arrows: select",
                        (int)zoo.images.size(), zoo.selectedIndex),
             10, screenHeight - 20, 14, LIGHTGRAY);

    DrawFPS(screenWidth - 100, 10);
    EndDrawing();
  }

  zoo.unload_all();
  rlImGuiShutdown();
  CloseWindow();

  CHECK(true);
}

#endif
