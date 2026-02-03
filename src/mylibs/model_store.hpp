#if 0
cd "$(dirname "$0")/../.." && cmake --build build --target mylibs_tests && ./build/mylibs_tests --no-skip -tc="model store visual*"
exit
#endif
#pragma once
#include "consol_helpers.hpp"
#include "ilist.hpp"
#include <cstring>
#include <raylib.h>
#include <raymath.h>
#include <string>
#include <unordered_map>
#include <vector>

// A lightweight handle to a model in the store
struct ModelInstance {
  Model model;
  const char* name;

  operator Model&() { return model; }
  operator const char*() { return name; }
  bool valid() const { return name != nullptr && model.meshCount > 0; }
  bool operator==(const ModelInstance& other) const {
    return name && other.name && strcmp(name, other.name) == 0;
  }
};

// Centralized model storage - load once, instance many times
struct ModelStore {
  std::unordered_map<std::string, Model> models;

  // Load model from file path
  bool load(const std::string& name, const std::string& path) {
    if (models.find(name) != models.end())
      return true; // already loaded
    Model m = LoadModel(path.c_str());
    if (m.meshCount == 0)
      return false;
    models[name] = m;
    return true;
  }

  // Load model from mesh
  bool load(const std::string& name, Mesh mesh) {
    if (models.find(name) != models.end())
      return true;
    models[name] = LoadModelFromMesh(mesh);
    return true;
  }

  // Check if a model is loaded
  bool has(const std::string& name) const { return models.find(name) != models.end(); }

  // Get raw model pointer (nullptr if not found)
  Model* get(const std::string& name) {
    auto it = models.find(name);
    return it != models.end() ? &it->second : nullptr;
  }

  // Create an instance with fresh transform
  ModelInstance instance(const std::string& name) {
    auto it = models.find(name);
    if (it == models.end())
      return {Model{0}, nullptr};
    ModelInstance inst;
    inst.model = it->second;
    inst.model.transform = MatrixIdentity();
    inst.name = it->first.c_str();
    return inst;
  }

  // Get list of all loaded model names
  std::vector<std::string> names() const {
    std::vector<std::string> result;
    result.reserve(models.size());
    for (auto& [name, _] : models)
      result.push_back(name);
    return result;
  }

  size_t count() const { return models.size(); }

  void unload(const std::string& name) {
    auto it = models.find(name);
    if (it != models.end()) {
      UnloadModel(it->second);
      models.erase(it);
    }
  }

  void unload_all() {
    for (auto& [_, model] : models)
      UnloadModel(model);
    models.clear();
  }
};

/*
 * the way that things should be drawn is that the things in the ilist would have a modelinstance
 * then for each model in the model store it would just ittorate through the ilist and draw the
 * modelinstance
 **/
template <typename T>
concept HasModel = requires(T t) {
  { t.model } -> std::convertible_to<ModelInstance&>;
};

template <typename T, size_t N>
void draw_model_store(ModelStore& store, things_list<T, N>& list)
  requires HasModel<T>
{

  //
  // collect instance data from things then draw them in a single call per model
  //

  for (const auto& name : store.names()) {
    Model* model = store.get(name);
    if (!model || model->meshCount == 0)
      continue;

    std::vector<Matrix> transforms;
    for (auto& thing : list) {
      // if the name of the model on the thing matches the target name on the store then draw it
      if (thing.model.valid() && strcmp(thing.model.name, name.c_str()) == 0) {
        transforms.push_back(thing.model.model.transform);
      }
    }
    if (transforms.empty())
      continue;

    Material mat = model->materials[0];

    //
    // draw collectied instances
    //

    for (int i = 0; i < model->meshCount; i++) {
      DrawMeshInstanced(model->meshes[i], mat, transforms.data(), (int)transforms.size());
    }
  }
}

REGISTER_CMD(list_models, "list loaded models", { return ""; });

REGISTER_CMD(spawn, "spawn an instance of model", { return ""; });
// ============================================================================
// DOCTEST
// ============================================================================
#ifdef DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("model store visual test" * doctest::skip()) {
  // Run with: ./mylibs_tests --no-skip -tc="model store visual*"

  const int screenWidth = 1280;
  const int screenHeight = 720;
  InitWindow(screenWidth, screenHeight, "Model Store Visual Test");
  SetTargetFPS(60);

// ImGui setup
#include "imgui.h"
#include "rlImGui.h"
  rlImGuiSetup(true);

  // Create store and load primitive meshes
  ModelStore store;
  store.load("cube", GenMeshCube(1.0f, 1.0f, 1.0f));
  store.load("sphere", GenMeshSphere(0.5f, 16, 16));
  store.load("cylinder", GenMeshCylinder(0.5f, 1.0f, 16));
  store.load("torus", GenMeshTorus(0.25f, 0.5f, 16, 16));
  store.load("knot", GenMeshKnot(0.25f, 0.5f, 16, 128));
  store.load("plane", GenMeshPlane(2.0f, 2.0f, 1, 1));
  store.load("cone", GenMeshCone(0.5f, 1.0f, 16));

  auto modelNames = store.names();
  std::sort(modelNames.begin(), modelNames.end());
  int selectedIndex = 0;

  // Camera
  Camera3D camera = {0};
  camera.position = {3.0f, 3.0f, 3.0f};
  camera.target = {0.0f, 0.0f, 0.0f};
  camera.up = {0.0f, 1.0f, 0.0f};
  camera.fovy = 45.0f;
  camera.projection = CAMERA_PERSPECTIVE;

  float rotationY = 0.0f;
  bool autoRotate = true;
  bool wireframe = false;
  Color modelColor = BLUE;

  while (!WindowShouldClose()) {
    // Update
    if (autoRotate)
      rotationY += 30.0f * GetFrameTime();

    // Get current model instance
    ModelInstance inst = store.instance(modelNames[selectedIndex]);

    BeginDrawing();
    ClearBackground(DARKGRAY);

    BeginMode3D(camera);

    // Draw grid
    DrawGrid(10, 1.0f);

    // Draw model
    if (inst.valid()) {
      Vector3 pos = {0.0f, 0.5f, 0.0f};
      if (wireframe) {
        DrawModelWiresEx(inst.model, pos, {0, 1, 0}, rotationY, {1, 1, 1}, modelColor);
      } else {
        DrawModelEx(inst.model, pos, {0, 1, 0}, rotationY, {1, 1, 1}, modelColor);
        DrawModelWiresEx(inst.model, pos, {0, 1, 0}, rotationY, {1, 1, 1}, BLACK);
      }
    }

    EndMode3D();

    // ImGui panel
    rlImGuiBegin();
    if (ImGui::Begin("Model Store")) {
      ImGui::Text("FPS: %d", GetFPS());
      ImGui::Text("Models loaded: %zu", store.count());
      ImGui::Separator();

      // Model selector
      if (ImGui::BeginListBox("Models", ImVec2(-1, 150))) {
        for (int i = 0; i < (int)modelNames.size(); i++) {
          bool selected = (i == selectedIndex);
          if (ImGui::Selectable(modelNames[i].c_str(), selected))
            selectedIndex = i;
          if (selected)
            ImGui::SetItemDefaultFocus();
        }
        ImGui::EndListBox();
      }

      ImGui::Separator();
      ImGui::Checkbox("Auto Rotate", &autoRotate);
      if (!autoRotate)
        ImGui::SliderFloat("Rotation", &rotationY, 0.0f, 360.0f);
      ImGui::Checkbox("Wireframe", &wireframe);

      float col[3] = {modelColor.r / 255.0f, modelColor.g / 255.0f, modelColor.b / 255.0f};
      if (ImGui::ColorEdit3("Color", col)) {
        modelColor = {(unsigned char)(col[0] * 255), (unsigned char)(col[1] * 255),
                      (unsigned char)(col[2] * 255), 255};
      }

      ImGui::Separator();
      if (inst.valid()) {
        ImGui::Text("Current: %s", inst.name);
        ImGui::Text("Meshes: %d", inst.model.meshCount);
        ImGui::Text("Materials: %d", inst.model.materialCount);
      }
    }
    ImGui::End();
    rlImGuiEnd();

    EndDrawing();
  }

  store.unload_all();
  rlImGuiShutdown();
  CloseWindow();
  CHECK(true);
}

#endif
