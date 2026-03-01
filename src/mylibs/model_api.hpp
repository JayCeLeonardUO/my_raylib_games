#if 0
cd "$(dirname "$0")/../.." && cmake --build build --target mylibs_tests && ./build/mylibs_tests --no-skip -tc="model store visual*"
exit
#endif
#pragma once
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

/**
 * @brief Centralized model storage — load once, instance many times.
 *
 * Namespace-style API with inline globals.
 * All loaded Model data is owned by internal storage.
 *
 * @see ModelInstance
 */
namespace ModelAPI {

inline std::unordered_map<std::string, Model> models;

/// @brief Load a model from a file path. No-op if name already loaded.
inline bool load(const std::string& name, const std::string& path) {
  if (models.find(name) != models.end())
    return true;
  Model m = LoadModel(path.c_str());
  if (m.meshCount == 0)
    return false;
  models[name] = m;
  return true;
}

/// @brief Load a model from an existing Mesh. No-op if name already loaded.
/// Applies magenta color as default "placeholder" material.
inline bool load(const std::string& name, Mesh mesh) {
  if (models.find(name) != models.end())
    return true;
  Model m = LoadModelFromMesh(mesh);
  // Apply magenta as default placeholder color
  m.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = MAGENTA;
  models[name] = m;
  return true;
}

/// @brief Check if a model is loaded.
inline bool has(const std::string& name) { return models.find(name) != models.end(); }

/// @brief Get raw model pointer, or nullptr if not found.
inline Model* get(const std::string& name) {
  auto it = models.find(name);
  return it != models.end() ? &it->second : nullptr;
}

/// @brief Create a ModelInstance with a fresh identity transform.
inline ModelInstance instance(const std::string& name) {
  auto it = models.find(name);
  if (it == models.end())
    return {Model{0}, nullptr};
  ModelInstance inst;
  inst.model = it->second;
  inst.model.transform = MatrixIdentity();
  inst.name = it->first.c_str();
  return inst;
}

/// @brief Get list of all loaded model names.
inline std::vector<std::string> names() {
  std::vector<std::string> result;
  result.reserve(models.size());
  for (auto& [name, _] : models)
    result.push_back(name);
  return result;
}

/// @brief Number of loaded models.
inline size_t count() { return models.size(); }

/// @brief Unload and remove a single model by name.
inline void unload(const std::string& name) {
  auto it = models.find(name);
  if (it != models.end()) {
    UnloadModel(it->second);
    models.erase(it);
  }
}

/// @brief Unload and remove all models.
inline void unload_all() {
  for (auto& [_, model] : models)
    UnloadModel(model);
  models.clear();
}

} // namespace ModelAPI

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
void draw_model_store(things_list<T, N>& list)
  requires HasModel<T>
{
  for (const auto& name : ModelAPI::names()) {
    Model* model = ModelAPI::get(name);
    if (!model || model->meshCount == 0)
      continue;

    std::vector<Matrix> transforms;
    for (auto& thing : list) {
      if (thing.model.valid() && strcmp(thing.model.name, name.c_str()) == 0) {
        transforms.push_back(thing.model.model.transform);
      }
    }
    if (transforms.empty())
      continue;

    Material mat = model->materials[0];

    for (int i = 0; i < model->meshCount; i++) {
      DrawMeshInstanced(model->meshes[i], mat, transforms.data(), (int)transforms.size());
    }
  }
}

// ============================================================================
// DOCTEST
// ============================================================================
#ifdef DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("model store visual test" * doctest::skip()) {
  const int screenWidth = 1280;
  const int screenHeight = 720;
  InitWindow(screenWidth, screenHeight, "Model Store Visual Test");
  SetTargetFPS(60);

#include "imgui.h"
#include "rlImGui.h"
  rlImGuiSetup(true);

  ModelAPI::load("cube", GenMeshCube(1.0f, 1.0f, 1.0f));
  ModelAPI::load("sphere", GenMeshSphere(0.5f, 16, 16));
  ModelAPI::load("cylinder", GenMeshCylinder(0.5f, 1.0f, 16));
  ModelAPI::load("torus", GenMeshTorus(0.25f, 0.5f, 16, 16));
  ModelAPI::load("knot", GenMeshKnot(0.25f, 0.5f, 16, 128));
  ModelAPI::load("plane", GenMeshPlane(2.0f, 2.0f, 1, 1));
  ModelAPI::load("cone", GenMeshCone(0.5f, 1.0f, 16));

  auto modelNames = ModelAPI::names();
  std::sort(modelNames.begin(), modelNames.end());
  int selectedIndex = 0;

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
    if (autoRotate)
      rotationY += 30.0f * GetFrameTime();

    ModelInstance inst = ModelAPI::instance(modelNames[selectedIndex]);

    BeginDrawing();
    ClearBackground(DARKGRAY);

    BeginMode3D(camera);
    DrawGrid(10, 1.0f);

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

    rlImGuiBegin();
    if (ImGui::Begin("Model Store")) {
      ImGui::Text("FPS: %d", GetFPS());
      ImGui::Text("Models loaded: %zu", ModelAPI::count());
      ImGui::Separator();

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

  ModelAPI::unload_all();
  rlImGuiShutdown();
  CloseWindow();
  CHECK(true);
}

#endif
