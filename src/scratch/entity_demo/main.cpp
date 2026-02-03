#if 0
cd "$(dirname "$0")/../../.."
cmake --build build --target entity_demo && ./build/entity_demo
exit
#endif
#include "../../mylibs/game_console_api.hpp"
#include "../../mylibs/ilist.hpp"
#include "../../mylibs/model_api.hpp"
#include <imgui.h>
#include <raylib.h>
#include <raymath.h>
#include <rlImGui.h>

// ============================================================================
// Entity data
// ============================================================================

struct Entity {
  ModelInstance model;
  Vector3 position;
  Vector3 rotation;
  float scale;

  operator ModelInstance&() { return model; }
};

// ============================================================================
// GameCtxAPI - namespace-style game context
// ============================================================================

struct GameCtxAPI {
  static constexpr size_t MAX_ENTITIES = 1000;
  using EntityList = things_list<Entity, MAX_ENTITIES>;

private:
  static EntityList& entities_ref() {
    static EntityList e;
    return e;
  }

  static Camera3D& camera_ref() {
    static Camera3D c = {
        .position = {5.0f, 5.0f, 5.0f},
        .target = {0.0f, 0.0f, 0.0f},
        .up = {0.0f, 1.0f, 0.0f},
        .fovy = 45.0f,
        .projection = CAMERA_PERSPECTIVE,
    };
    return c;
  }

  static thing_ref& selected_ref() {
    static thing_ref r;
    return r;
  }

public:
  static EntityList& entities() { return entities_ref(); }
  static Camera3D& camera() { return camera_ref(); }
  static thing_ref& selected() { return selected_ref(); }

  // ---- Entity management ----
  static thing_ref spawn(const std::string& model_name, Vector3 pos = {0, 0, 0}, float scale = 1.0f) {
    ModelInstance inst = ModelAPI::instance(model_name);
    if (!inst.valid())
      return {};
    return entities().add({inst, pos, {0, 0, 0}, scale});
  }

  static void clear_entities() {
    std::vector<thing_ref> refs;
    for (auto& e : entities())
      refs.push_back(e.this_ref());
    for (auto& r : refs)
      entities().pop(r);
    selected() = {};
  }

  static size_t entity_count() {
    size_t count = 0;
    for (auto& e : entities()) {
      (void)e;
      count++;
    }
    return count;
  }

  // ---- Rendering ----
  static void draw_entities() {
    for (auto& e : entities()) {
      if (!e.model.valid())
        continue;

      Model* m = ModelAPI::get(e.model.name);
      if (!m)
        continue;

      Matrix transform = MatrixMultiply(
          MatrixMultiply(
              MatrixScale(e.scale, e.scale, e.scale),
              MatrixRotateXYZ(e.rotation)),
          MatrixTranslate(e.position.x, e.position.y, e.position.z));

      for (int i = 0; i < m->meshCount; i++) {
        DrawMesh(m->meshes[i], m->materials[m->meshMaterial[i]], transform);
      }

      // Draw selection box
      if (e.this_ref() == selected()) {
        DrawCubeWires(e.position, e.scale * 1.2f, e.scale * 1.2f, e.scale * 1.2f, YELLOW);
      }
    }
  }

  static void update() {
    BeginMode3D(camera());
    DrawGrid(20, 1.0f);
    draw_entities();
    EndMode3D();
  }

  static void draw_imgui() {
    rlImGuiBegin();

    if (ImGui::Begin("Scene")) {
      ImGui::Text("Entities: %zu", entity_count());
      ImGui::Text("Models: %zu", ModelAPI::count());
      ImGui::Separator();

      // List loaded models
      if (ImGui::CollapsingHeader("Models", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& name : ModelAPI::names()) {
          if (ImGui::Button(name.c_str())) {
            spawn(name, {0, 0, 0}, 1.0f);
          }
          ImGui::SameLine();
          ImGui::Text("(click to spawn)");
        }
      }

      // List entities
      if (ImGui::CollapsingHeader("Entities", ImGuiTreeNodeFlags_DefaultOpen)) {
        int idx = 0;
        for (auto& e : entities()) {
          ImGui::PushID(idx);
          bool is_selected = (e.this_ref() == selected());
          if (ImGui::Selectable(
                  TextFormat("%d: %s", idx, e.model.valid() ? e.model.name : "???"),
                  is_selected)) {
            selected() = e.this_ref();
          }
          ImGui::PopID();
          idx++;
        }
      }

      // Selected entity properties
      if (auto& sel = entities()[selected()]) {
        ImGui::Separator();
        ImGui::Text("Selected: %s", sel.model.name);
        ImGui::DragFloat3("Position", &sel.position.x, 0.1f);
        ImGui::DragFloat3("Rotation", &sel.rotation.x, 0.01f);
        ImGui::DragFloat("Scale", &sel.scale, 0.1f, 0.1f, 10.0f);
        if (ImGui::Button("Delete")) {
          entities().pop(selected());
          selected() = {};
        }
      }
    }
    ImGui::End();

    GameConsoleAPI::draw_imgui();
    rlImGuiEnd();
  }
};

// ============================================================================
// Console commands
// ============================================================================

REGISTER_CMD(load, "load <path> [name]", {
  if (args.empty())
    return std::string("Usage: load <path> [name]");

  std::string path = args[0];
  std::string name = (args.size() > 1) ? args[1] : path;

  // Extract filename if no name given
  if (args.size() == 1) {
    size_t slash = path.find_last_of('/');
    size_t dot = path.find_last_of('.');
    if (slash != std::string::npos)
      name = path.substr(slash + 1);
    if (dot != std::string::npos && dot > slash)
      name = name.substr(0, dot - slash - 1);
  }

  if (ModelAPI::load(name, path))
    return "Loaded: " + name;
  return "Failed to load: " + path;
});

REGISTER_CMD(spawn, "spawn <model> [x y z] [scale]", {
  if (args.empty())
    return std::string("Usage: spawn <model> [x y z] [scale]");

  std::string model_name = args[0];
  if (!ModelAPI::has(model_name))
    return "Unknown model: " + model_name;

  Vector3 pos = {0, 0, 0};
  float scale = 1.0f;

  if (args.size() >= 4) {
    pos.x = std::stof(args[1]);
    pos.y = std::stof(args[2]);
    pos.z = std::stof(args[3]);
  }
  if (args.size() >= 5) {
    scale = std::stof(args[4]);
  }

  thing_ref ref = GameCtxAPI::spawn(model_name, pos, scale);
  if (ref.kind == ilist_kind::nil)
    return "Failed to spawn";

  return "Spawned " + model_name + " at (" +
         std::to_string(pos.x) + ", " + std::to_string(pos.y) + ", " + std::to_string(pos.z) + ")";
});

REGISTER_CMD(list, "list entities", {
  (void)args;
  std::string out;
  int i = 0;
  for (auto& e : GameCtxAPI::entities()) {
    out += std::to_string(i++) + ": " +
           (e.model.valid() ? std::string(e.model.name) : "???") +
           " pos=(" + std::to_string((int)e.position.x) + "," +
           std::to_string((int)e.position.y) + "," +
           std::to_string((int)e.position.z) + ")\n";
  }
  return out.empty() ? "No entities" : out;
});

REGISTER_CMD(clear, "clear all entities", {
  (void)args;
  size_t count = GameCtxAPI::entity_count();
  GameCtxAPI::clear_entities();
  return "Cleared " + std::to_string(count) + " entities";
});

REGISTER_CMD(models, "list loaded models", {
  (void)args;
  std::string out;
  for (const auto& name : ModelAPI::names())
    out += name + "\n";
  return out.empty() ? "No models" : out;
});

REGISTER_CMD(unload, "unload <model>", {
  if (args.empty())
    return std::string("Usage: unload <model>");
  ModelAPI::unload(args[0]);
  return "Unloaded: " + args[0];
});

REGISTER_CMD(cam, "cam <x> <y> <z>", {
  if (args.size() < 3)
    return std::string("Usage: cam <x> <y> <z>");
  GameCtxAPI::camera().position = {std::stof(args[0]), std::stof(args[1]), std::stof(args[2])};
  return "OK";
});

REGISTER_CMD(target, "target <x> <y> <z>", {
  if (args.size() < 3)
    return std::string("Usage: target <x> <y> <z>");
  GameCtxAPI::camera().target = {std::stof(args[0]), std::stof(args[1]), std::stof(args[2])};
  return "OK";
});

// ============================================================================
// Main
// ============================================================================

int main() {
  InitWindow(1280, 720, "Entity Spawner");
  SetTargetFPS(60);
  rlImGuiSetup(true);

  // Load some default primitives
  ModelAPI::load("cube", GenMeshCube(1, 1, 1));
  ModelAPI::load("sphere", GenMeshSphere(0.5f, 16, 16));
  ModelAPI::load("cylinder", GenMeshCylinder(0.5f, 1, 16));

  GameConsoleAPI::print("Commands: load <file.glb>, spawn <model>, list, clear, models");
  GameConsoleAPI::print("Press ~ for console");

  while (!WindowShouldClose()) {
    if (IsKeyPressed(KEY_GRAVE))
      GameConsoleAPI::toggle_visible();

    if (!GameConsoleAPI::visible())
      UpdateCamera(&GameCtxAPI::camera(), CAMERA_ORBITAL);

    BeginDrawing();
    ClearBackground(DARKGRAY);

    GameCtxAPI::update();
    GameCtxAPI::draw_imgui();

    DrawFPS(10, 10);
    EndDrawing();
  }

  ModelAPI::unload_all();
  rlImGuiShutdown();
  CloseWindow();
  return 0;
}
