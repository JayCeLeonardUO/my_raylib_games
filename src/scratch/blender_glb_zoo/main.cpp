/**
 * Blender GLB Zoo
 *
 * Load GLB models as templates and spawn instances into a scene.
 *
 * Controls:
 * - F1: Toggle between Templates/Scene mode
 * - Arrow keys: Navigate selection
 * - ENTER: Spawn selected template (in Templates mode)
 * - DELETE: Delete selected instance (in Scene mode)
 * - TAB: Toggle free camera mode
 * - WASD/QE: Move camera (in free mode)
 * - Mouse: Look around (in free mode)
 * - ~ (grave): Toggle console
 */

#include "../../mylibs/game_console_api.hpp"
#include "../../mylibs/ilist.hpp"
#include "../../mylibs/model_api.hpp"
#include "../../mylibs/traits.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <dirent.h>
#include <imgui.h>
#include <raylib.h>
#include <raymath.h>
#include <rlImGui.h>
#include <rlgl.h>
#include <string>
#include <sys/stat.h>
#include <vector>

// Template metadata (loaded models)
struct GlbEntry {
  std::string name;
  std::string filename;
  std::string fullpath;
  std::string folder;
  BoundingBox bounds;
};

// Bit flags for entity behaviors
struct GameTraits {
  TRAIT_DEF(player_control) // WASD movement
  TRAIT_DEF(chase_player)   // move toward player
  TRAIT_DEF(wander)         // random movement
  TRAIT_DEF(orbit)          // circle around origin
  TRAIT_DEF(is_player)      // marks this as the player entity

  uint8_t as_byte() const { return *(uint8_t*)this; }
  void from_byte(uint8_t b) { *(uint8_t*)this = b; }

  std::string to_string() const {
    std::string s;
    if (player_control)
      s += "ctrl ";
    if (chase_player)
      s += "chase ";
    if (wander)
      s += "wander ";
    if (orbit)
      s += "orbit ";
    if (is_player)
      s += "player ";
    return s.empty() ? "none" : s;
  }
};

// A spawned instance - has ModelInstance for draw_model_store
struct GlbInstance {
  ModelInstance model; // contains model ref + transform
  GameTraits traits;
  float speed = 3.0f;
  float angle = 0.0f; // for orbit/wander state
};

using GlbList = things_list<GlbEntry, MAX_ITEMS>;
using InstanceList = things_list<GlbInstance, MAX_ITEMS>;

enum class ViewMode { Templates, Scene };

// Grid layout helper
struct ZooGrid {
  int columns = 6;
  float spacing = 2.5f;
  float scale = 1.0f;

  Vector3 position_for(int index) const {
    int col = index % columns;
    int row = index / columns;
    float offsetX = (columns - 1) * spacing / 2.0f;
    return {col * spacing - offsetX, 0.0f, row * spacing};
  }

  int move_left(int cur) const { return cur > 0 ? cur - 1 : cur; }
  int move_right(int cur, int total) const { return cur < total - 1 ? cur + 1 : cur; }
  int move_up(int cur) const { return cur >= columns ? cur - columns : cur; }
  int move_down(int cur, int total) const { return cur + columns < total ? cur + columns : cur; }
};

struct GlbZoo {
  GlbList entries;
  std::vector<thing_ref> entry_refs;

  InstanceList instances;
  std::vector<thing_ref> instance_refs;
  int selectedInstance = -1;

  ViewMode viewMode = ViewMode::Templates;
  ZooGrid grid;
  int selectedIndex = -1;
  bool showInfo = true;
  int maxModels = 1000;
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

  Vector3 template_position(int index) const { return grid.position_for(index); }

  std::string load_glb(const std::string& filepath) {
    struct stat st;
    if (stat(filepath.c_str(), &st) != 0) {
      return "File not found: " + filepath;
    }

    size_t lastSlash = filepath.find_last_of('/');
    std::string filename =
        (lastSlash != std::string::npos) ? filepath.substr(lastSlash + 1) : filepath;
    size_t dotPos = filename.find_last_of('.');
    std::string name = (dotPos != std::string::npos) ? filename.substr(0, dotPos) : filename;

    std::string uniqueName = name;
    int suffix = 1;
    while (ModelAPI::has(uniqueName)) {
      uniqueName = name + "_" + std::to_string(suffix++);
    }

    if (!ModelAPI::load(uniqueName, filepath)) {
      return "Failed to load: " + filepath;
    }

    GlbEntry entry;
    entry.name = uniqueName;
    entry.filename = filename;
    entry.fullpath = filepath;

    if (lastSlash != std::string::npos && lastSlash > 0) {
      size_t prevSlash = filepath.find_last_of('/', lastSlash - 1);
      entry.folder = (prevSlash != std::string::npos)
                         ? filepath.substr(prevSlash + 1, lastSlash - prevSlash - 1)
                         : filepath.substr(0, lastSlash);
    } else {
      entry.folder = ".";
    }

    Model* m = ModelAPI::get(uniqueName);
    if (m && m->meshCount > 0) {
      entry.bounds = GetMeshBoundingBox(m->meshes[0]);
    }

    thing_ref ref = entries.add(entry);
    if (ref.kind == ilist_kind::nil) {
      ModelAPI::unload(uniqueName);
      return "Storage full";
    }
    entry_refs.push_back(ref);
    loadedCount++;

    rebuild_positions();
    return "Loaded: " + uniqueName;
  }

  void load_directory(const char* path, int depth = 0) {
    if (depth == 0)
      loadedCount = 0;
    if (loadedCount >= maxModels || depth > maxDepth)
      return;

    DIR* dir = opendir(path);
    if (!dir)
      return;

    std::vector<std::string> subdirs;
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL && loadedCount < maxModels) {
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
      if (ext_lower == ".glb" || ext_lower == ".gltf") {
        load_glb(fullpath);
      }
    }
    closedir(dir);

    for (const auto& subdir : subdirs) {
      if (loadedCount >= maxModels)
        break;
      load_directory(subdir.c_str(), depth + 1);
    }

    if (depth == 0) {
      rebuild_positions();
    }
  }

  void rebuild_positions() {
    std::sort(entry_refs.begin(), entry_refs.end(), [this](const thing_ref& a, const thing_ref& b) {
      auto& ea = entries[a];
      auto& eb = entries[b];
      if (groupByFolder && ea.folder != eb.folder)
        return ea.folder < eb.folder;
      return ea.filename < eb.filename;
    });
  }

  void unload_all() {
    ModelAPI::unload_all();
    for (auto& ref : entry_refs)
      entries.pop(ref);
    entry_refs.clear();
    for (auto& ref : instance_refs)
      instances.pop(ref);
    instance_refs.clear();
    loadedCount = 0;
    selectedIndex = -1;
    selectedInstance = -1;
  }

  void unload(const std::string& name) {
    ModelAPI::unload(name);
    for (auto it = entry_refs.begin(); it != entry_refs.end(); ++it) {
      if (entries[*it].name == name) {
        entries.pop(*it);
        entry_refs.erase(it);
        break;
      }
    }
    // Remove instances using this model
    for (auto it = instance_refs.begin(); it != instance_refs.end();) {
      if (instances[*it].model.name && strcmp(instances[*it].model.name, name.c_str()) == 0) {
        instances.pop(*it);
        it = instance_refs.erase(it);
      } else {
        ++it;
      }
    }
    loadedCount = entry_refs.size();
    if (selectedIndex >= (int)entry_refs.size())
      selectedIndex = entry_refs.empty() ? -1 : (int)entry_refs.size() - 1;
    if (selectedInstance >= (int)instance_refs.size())
      selectedInstance = instance_refs.empty() ? -1 : (int)instance_refs.size() - 1;
    rebuild_positions();
  }

  std::string spawn(const std::string& model_name, Vector3 pos, GameTraits traits = {}) {
    ModelInstance inst = ModelAPI::instance(model_name);
    if (!inst.valid()) {
      return "Model not found: " + model_name;
    }

    // Set transform
    inst.model.transform = MatrixTranslate(pos.x, pos.y, pos.z);

    GlbInstance gi;
    gi.model = inst;
    gi.traits = traits;

    thing_ref ref = instances.add(gi);
    if (ref.kind == ilist_kind::nil) {
      return "Instance storage full";
    }
    instance_refs.push_back(ref);
    return "Spawned " + model_name + " [" + traits.to_string() + "]";
  }

  std::string spawn_selected(Vector3 pos) {
    if (selectedIndex < 0 || selectedIndex >= (int)entry_refs.size()) {
      return "No template selected";
    }
    return spawn(entries[entry_refs[selectedIndex]].name, pos);
  }

  void despawn(int idx) {
    if (idx < 0 || idx >= (int)instance_refs.size())
      return;
    instances.pop(instance_refs[idx]);
    instance_refs.erase(instance_refs.begin() + idx);
    if (selectedInstance >= (int)instance_refs.size())
      selectedInstance = instance_refs.empty() ? -1 : (int)instance_refs.size() - 1;
  }

  void clear_instances() {
    for (auto& ref : instance_refs)
      instances.pop(ref);
    instance_refs.clear();
    selectedInstance = -1;
  }

  // Find player instance
  GlbInstance* find_player() {
    for (auto& ref : instance_refs) {
      auto& inst = instances[ref];
      if (inst.traits.is_player)
        return &inst;
    }
    return nullptr;
  }

  // Update all instances based on their traits
  void update_instances(float dt) {
    for (auto& ref : instance_refs) {
      auto& inst = instances[ref];
      Matrix t = inst.model.model.transform;
      Vector3 pos = {t.m12, t.m13, t.m14};

      // Player control - WASD
      if (inst.traits.player_control && !cameraEnabled) {
        Vector3 move = {0, 0, 0};
        if (IsKeyDown(KEY_W))
          move.z -= 1;
        if (IsKeyDown(KEY_S))
          move.z += 1;
        if (IsKeyDown(KEY_A))
          move.x -= 1;
        if (IsKeyDown(KEY_D))
          move.x += 1;
        if (Vector3Length(move) > 0) {
          move = Vector3Normalize(move);
          pos = Vector3Add(pos, Vector3Scale(move, inst.speed * dt));
        }
      }

      // Chase player
      if (inst.traits.chase_player) {
        GlbInstance* player = find_player();
        if (player) {
          Matrix pt = player->model.model.transform;
          Vector3 player_pos = {pt.m12, pt.m13, pt.m14};
          Vector3 dir = Vector3Subtract(player_pos, pos);
          float dist = Vector3Length(dir);
          if (dist > 1.5f) {
            dir = Vector3Normalize(dir);
            pos = Vector3Add(pos, Vector3Scale(dir, inst.speed * dt));
          }
        }
      }

      // Wander randomly
      if (inst.traits.wander) {
        if (GetRandomValue(0, 100) < 2) {
          inst.angle = GetRandomValue(0, 628) / 100.0f;
        }
        pos.x += cosf(inst.angle) * inst.speed * dt;
        pos.z += sinf(inst.angle) * inst.speed * dt;
        pos.x = Clamp(pos.x, -20.0f, 20.0f);
        pos.z = Clamp(pos.z, -20.0f, 20.0f);
      }

      // Orbit around origin
      if (inst.traits.orbit) {
        inst.angle += inst.speed * 0.5f * dt;
        float radius = 5.0f;
        pos.x = cosf(inst.angle) * radius;
        pos.z = sinf(inst.angle) * radius;
      }

      inst.model.model.transform = MatrixTranslate(pos.x, pos.y, pos.z);
    }
  }

  // Get position from instance transform matrix
  Vector3 get_instance_position(int idx) {
    if (idx < 0 || idx >= (int)instance_refs.size())
      return {0, 0, 0};
    Matrix t = instances[instance_refs[idx]].model.model.transform;
    return {t.m12, t.m13, t.m14};
  }

  void set_instance_position(int idx, Vector3 pos) {
    if (idx < 0 || idx >= (int)instance_refs.size())
      return;
    instances[instance_refs[idx]].model.model.transform = MatrixTranslate(pos.x, pos.y, pos.z);
  }

  void update() {
    if (IsKeyPressed(KEY_TAB)) {
      cameraEnabled = !cameraEnabled;
      if (cameraEnabled)
        DisableCursor();
      else
        EnableCursor();
    }

    if (IsKeyPressed(KEY_F1)) {
      viewMode = (viewMode == ViewMode::Templates) ? ViewMode::Scene : ViewMode::Templates;
    }

    if (viewMode == ViewMode::Templates && IsKeyPressed(KEY_ENTER)) {
      if (selectedIndex >= 0 && selectedIndex < (int)entry_refs.size()) {
        Vector3 spawnPos = {instance_refs.size() * 2.0f, 0, 0};
        spawn_selected(spawnPos);
      }
    }

    if (viewMode == ViewMode::Scene && IsKeyPressed(KEY_DELETE)) {
      if (selectedInstance >= 0)
        despawn(selectedInstance);
    }

    // Update entities based on traits
    float dt = GetFrameTime();
    update_instances(dt);

    if (viewMode == ViewMode::Templates)
      update_templates();
    else
      update_scene();

    update_camera();
  }

  void update_templates() {
    int total = (int)entry_refs.size();
    if (IsKeyPressed(KEY_LEFT))
      selectedIndex = grid.move_left(selectedIndex);
    if (IsKeyPressed(KEY_RIGHT))
      selectedIndex = grid.move_right(selectedIndex, total);
    if (IsKeyPressed(KEY_UP))
      selectedIndex = grid.move_up(selectedIndex);
    if (IsKeyPressed(KEY_DOWN))
      selectedIndex = grid.move_down(selectedIndex, total);
    if (IsKeyPressed(KEY_HOME))
      selectedIndex = 0;
    if (IsKeyPressed(KEY_END))
      selectedIndex = total - 1;

    if (selectedIndex >= 0 && selectedIndex < total) {
      Vector3 targetPos = template_position(selectedIndex);
      camera.target = targetPos;
      if (!cameraEnabled) {
        Vector3 desiredPos = {targetPos.x, targetPos.y + 8.0f, targetPos.z + 4.0f};
        camera.position = Vector3Lerp(camera.position, desiredPos, 0.1f);
      }
    }
  }

  void update_scene() {
    int total = (int)instance_refs.size();
    if (IsKeyPressed(KEY_LEFT) && selectedInstance > 0)
      selectedInstance--;
    if (IsKeyPressed(KEY_RIGHT) && selectedInstance < total - 1)
      selectedInstance++;
    if (IsKeyPressed(KEY_HOME) && total > 0)
      selectedInstance = 0;
    if (IsKeyPressed(KEY_END) && total > 0)
      selectedInstance = total - 1;

    if (selectedInstance >= 0 && selectedInstance < total) {
      Vector3 targetPos = get_instance_position(selectedInstance);
      camera.target = targetPos;
      if (!cameraEnabled) {
        Vector3 desiredPos = {targetPos.x, targetPos.y + 5.0f, targetPos.z + 8.0f};
        camera.position = Vector3Lerp(camera.position, desiredPos, 0.1f);
      }
    }
  }

  void update_camera() {
    if (!cameraEnabled)
      return;

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

    Vector2 mouseDelta = GetMouseDelta();
    float sensitivity = 0.003f;
    Matrix rotation = MatrixRotate(camera.up, -mouseDelta.x * sensitivity);
    forward = Vector3Transform(forward, rotation);
    right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
    rotation = MatrixRotate(right, -mouseDelta.y * sensitivity);
    forward = Vector3Transform(forward, rotation);
    camera.target = Vector3Add(camera.position, forward);
  }

  void draw() {
    BeginMode3D(camera);
    DrawGrid(50, 1.0f);

    if (viewMode == ViewMode::Templates) {
      draw_templates();
    } else {
      // Use instanced drawing from model_store
      draw_model_store(instances);

      // Selection indicator
      if (selectedInstance >= 0 && selectedInstance < (int)instance_refs.size()) {
        Vector3 pos = get_instance_position(selectedInstance);
        DrawSphereWires(pos, 0.5f, 8, 8, YELLOW);
      }
    }

    EndMode3D();
  }

  void draw_templates() {
    Matrix preRotate = MatrixRotateX(PI / 2.0f);

    for (size_t i = 0; i < entry_refs.size(); i++) {
      auto& glb = entries[entry_refs[i]];
      Vector3 pos = template_position(i);
      pos.y = 0.01f;

      Model* model = ModelAPI::get(glb.name);
      if (!model)
        continue;

      Matrix transform = MatrixScale(grid.scale, grid.scale, grid.scale);
      transform = MatrixMultiply(transform, preRotate);
      transform = MatrixMultiply(transform, MatrixTranslate(pos.x, pos.y, pos.z));

      for (int m = 0; m < model->meshCount; m++) {
        DrawMesh(model->meshes[m], model->materials[model->meshMaterial[m]], transform);
      }

      if ((int)i == selectedIndex) {
        BoundingBox b = glb.bounds;
        float hw = (b.max.x - b.min.x) * grid.scale / 2.0f + 0.1f;
        float hh = (b.max.y - b.min.y) * grid.scale / 2.0f + 0.1f;
        DrawCubeWires(pos, hw * 2, 0.1f, hh * 2, YELLOW);
      }
    }
  }

  void draw_imgui() {
    if (ImGui::Begin("GLB Zoo Controls")) {
      const char* modeStr = (viewMode == ViewMode::Templates) ? "TEMPLATES" : "SCENE";
      ImGui::Text("Mode: %s (F1 to switch)", modeStr);
      ImGui::Text("Templates: %d | Instances: %d", (int)entry_refs.size(),
                  (int)instance_refs.size());
      ImGui::Text("TAB: free camera (%s)", cameraEnabled ? "ON" : "OFF");
      ImGui::Separator();

      if (viewMode == ViewMode::Templates) {
        ImGui::Text("ENTER: spawn | Arrows: select");
        ImGui::Separator();
        ImGui::SliderInt("Columns", &grid.columns, 1, 20);
        ImGui::SliderFloat("Spacing", &grid.spacing, 1.0f, 10.0f);
        ImGui::SliderFloat("Scale", &grid.scale, 0.5f, 5.0f);
        ImGui::Checkbox("Group by Folder", &groupByFolder);
        ImGui::Separator();
        ImGui::SliderInt("Max Models", &maxModels, 10, 5000);
        ImGui::SliderInt("Max Depth", &maxDepth, 0, 10);
      } else {
        ImGui::Text("DEL: delete | Arrows: select");
        if (ImGui::Button("Clear All Instances"))
          clear_instances();
      }

      ImGui::Separator();
      ImGui::Checkbox("Show Info Panel", &showInfo);
      ImGui::Separator();
      ImGui::Text("Camera");
      ImGui::SliderFloat("Speed", &cameraSpeed, 0.01f, 1.0f);

      if (ImGui::Button("Reset Camera"))
        init_camera();
    }
    ImGui::End();

    if (showInfo) {
      if (viewMode == ViewMode::Templates && selectedIndex >= 0 &&
          selectedIndex < (int)entry_refs.size()) {
        if (ImGui::Begin("Selected Template")) {
          auto& glb = entries[entry_refs[selectedIndex]];
          ImGui::Text("Name: %s", glb.name.c_str());
          ImGui::Text("Folder: %s", glb.folder.c_str());
          ImGui::Text("File: %s", glb.filename.c_str());
        }
        ImGui::End();
      } else if (viewMode == ViewMode::Scene && selectedInstance >= 0 &&
                 selectedInstance < (int)instance_refs.size()) {
        if (ImGui::Begin("Selected Instance")) {
          auto& inst = instances[instance_refs[selectedInstance]];
          ImGui::Text("Model: %s", inst.model.name ? inst.model.name : "?");
          Vector3 pos = get_instance_position(selectedInstance);
          if (ImGui::DragFloat3("Position", &pos.x, 0.1f)) {
            set_instance_position(selectedInstance, pos);
          }
        }
        ImGui::End();
      }
    }
  }
};

// Console commands
REGISTER_CMD(load, "load <path> - load a GLB file", {
  CMD_CTX(GlbZoo, zoo);
  if (args.empty())
    return std::string("Usage: load <path>");
  return zoo.load_glb(args[0]);
});

REGISTER_CMD(loaddir, "loaddir <path> - load directory of GLBs", {
  CMD_CTX(GlbZoo, zoo);
  if (args.empty())
    return std::string("Usage: loaddir <path>");
  int before = zoo.entry_refs.size();
  zoo.load_directory(args[0].c_str());
  return "Loaded " + std::to_string(zoo.entry_refs.size() - before) + " models";
});

REGISTER_CMD(list, "list loaded models", {
  CMD_CTX(GlbZoo, zoo);
  (void)args;
  if (zoo.entry_refs.empty())
    return std::string("No models loaded");
  std::string out;
  for (size_t i = 0; i < zoo.entry_refs.size(); i++) {
    out += std::to_string(i) + ": " + zoo.entries[zoo.entry_refs[i]].name;
    if ((int)i == zoo.selectedIndex)
      out += " *";
    out += "\n";
  }
  return out;
});

REGISTER_CMD(select, "select <index>", {
  CMD_CTX(GlbZoo, zoo);
  if (args.empty())
    return std::string("Usage: select <index>");
  int idx = std::stoi(args[0]);
  if (idx < 0 || idx >= (int)zoo.entry_refs.size())
    return "Invalid index";
  zoo.selectedIndex = idx;
  return "Selected: " + zoo.entries[zoo.entry_refs[idx]].name;
});

// Parse traits from string like "ctrl,chase,player"
GameTraits parse_traits(const std::string& s) {
  GameTraits t = {};
  if (s.find("ctrl") != std::string::npos)
    t.player_control = 1;
  if (s.find("chase") != std::string::npos)
    t.chase_player = 1;
  if (s.find("wander") != std::string::npos)
    t.wander = 1;
  if (s.find("orbit") != std::string::npos)
    t.orbit = 1;
  if (s.find("player") != std::string::npos)
    t.is_player = 1;
  return t;
}

REGISTER_CMD(spawn, "spawn <name> <traits> [x y z]", {
  CMD_CTX(GlbZoo, zoo);
  if (args.size() < 2)
    return std::string(
        "Usage: spawn <name> <traits> [x y z]\nTraits: ctrl,chase,wander,orbit,player");
  Vector3 pos = {0, 0, 0};
  if (args.size() >= 5) {
    pos.x = std::stof(args[2]);
    pos.y = std::stof(args[3]);
    pos.z = std::stof(args[4]);
  }
  GameTraits traits = parse_traits(args[1]);
  return zoo.spawn(args[0], pos, traits);
});

REGISTER_CMD(spawni, "spawni [x y z] - spawn selected", {
  CMD_CTX(GlbZoo, zoo);
  Vector3 pos = {0, 0, 0};
  if (args.size() >= 3) {
    pos.x = std::stof(args[0]);
    pos.y = std::stof(args[1]);
    pos.z = std::stof(args[2]);
  }
  return zoo.spawn_selected(pos);
});

REGISTER_CMD(despawn, "despawn [index]", {
  CMD_CTX(GlbZoo, zoo);
  int idx = zoo.selectedInstance;
  if (!args.empty())
    idx = std::stoi(args[0]);
  if (idx < 0 || idx >= (int)zoo.instance_refs.size())
    return std::string("Invalid index");
  std::string name = zoo.instances[zoo.instance_refs[idx]].model.name
                         ? zoo.instances[zoo.instance_refs[idx]].model.name
                         : "?";
  zoo.despawn(idx);
  return "Despawned " + name;
});

REGISTER_CMD(instances, "instances - list spawned", {
  CMD_CTX(GlbZoo, zoo);
  (void)args;
  if (zoo.instance_refs.empty())
    return std::string("No instances");
  std::string out;
  for (size_t i = 0; i < zoo.instance_refs.size(); i++) {
    auto& inst = zoo.instances[zoo.instance_refs[i]];
    Vector3 pos = zoo.get_instance_position(i);
    out += std::to_string(i) + ": " + (inst.model.name ? inst.model.name : "?");
    out += " [" + inst.traits.to_string() + "]";
    out += " @ (" + std::to_string((int)pos.x) + "," + std::to_string((int)pos.z) + ")";
    if ((int)i == zoo.selectedInstance)
      out += " *";
    out += "\n";
  }
  return out;
});

REGISTER_CMD(traits, "list available traits", {
  (void)args;
  return "Traits: ctrl (WASD), chase (follow player), wander (random), orbit (circle), player "
         "(mark as player)";
});

REGISTER_CMD(clear_instances, "clear all instances", {
  CMD_CTX(GlbZoo, zoo);
  (void)args;
  int count = zoo.instance_refs.size();
  zoo.clear_instances();
  return "Cleared " + std::to_string(count) + " instances";
});

REGISTER_CMD(mode, "mode [t|s]", {
  CMD_CTX(GlbZoo, zoo);
  if (args.empty())
    return std::string(zoo.viewMode == ViewMode::Templates ? "templates" : "scene");
  if (args[0] == "t" || args[0] == "templates") {
    zoo.viewMode = ViewMode::Templates;
    return "templates";
  }
  if (args[0] == "s" || args[0] == "scene") {
    zoo.viewMode = ViewMode::Scene;
    return "scene";
  }
  return "Unknown mode";
});

int main(int argc, char* argv[]) {
  const int screenWidth = 1280;
  const int screenHeight = 800;

  InitWindow(screenWidth, screenHeight, "GLB Zoo");
  SetTargetFPS(60);
  rlImGuiSetup(true);

  GlbZoo zoo;
  zoo.init_camera();

  // Load primitive models for testing
  ModelAPI::load("cube", GenMeshCube(1, 1, 1));
  ModelAPI::load("sphere", GenMeshSphere(0.5f, 16, 16));
  ModelAPI::load("cylinder", GenMeshCylinder(0.3f, 1, 16));
  ModelAPI::load("cone", GenMeshCone(0.5f, 1, 16));

  // Set colors
  if (Model* m = ModelAPI::get("cube"))
    m->materials[0].maps[MATERIAL_MAP_DIFFUSE].color = BLUE;
  if (Model* m = ModelAPI::get("sphere"))
    m->materials[0].maps[MATERIAL_MAP_DIFFUSE].color = RED;
  if (Model* m = ModelAPI::get("cylinder"))
    m->materials[0].maps[MATERIAL_MAP_DIFFUSE].color = GREEN;
  if (Model* m = ModelAPI::get("cone"))
    m->materials[0].maps[MATERIAL_MAP_DIFFUSE].color = ORANGE;

  GameConsoleAPI::bind<GlbZoo>(&zoo);
  GameConsoleAPI::print("~ for console. 'help' for commands.");

  const char* path = (argc > 1) ? argv[1] : getenv("GLB_ZOO_PATH");
  if (path)
    zoo.load_directory(path);

  if (!zoo.entry_refs.empty()) {
    zoo.selectedIndex = 0;
    Vector3 pos = zoo.template_position(0);
    zoo.camera.target = pos;
    zoo.camera.position = {pos.x, pos.y + 8.0f, pos.z + 4.0f};
  }

  while (!WindowShouldClose()) {
    if (IsKeyPressed(KEY_GRAVE))
      GameConsoleAPI::toggle_visible();
    if (!GameConsoleAPI::visible())
      zoo.update();

    BeginDrawing();
    ClearBackground({30, 30, 30, 255});

    zoo.draw();

    rlImGuiBegin();
    zoo.draw_imgui();
    GameConsoleAPI::draw_imgui();
    rlImGuiEnd();

    DrawRectangle(0, screenHeight - 25, screenWidth, 25, {20, 20, 20, 255});
    const char* modeStr = (zoo.viewMode == ViewMode::Templates) ? "TEMPLATES" : "SCENE";
    int sel = (zoo.viewMode == ViewMode::Templates) ? zoo.selectedIndex : zoo.selectedInstance;
    int count = (zoo.viewMode == ViewMode::Templates) ? (int)zoo.entry_refs.size()
                                                      : (int)zoo.instance_refs.size();
    DrawText(TextFormat("[%s] %d | Sel: %d | F1: mode | ~: console", modeStr, count, sel), 10,
             screenHeight - 20, 14, LIGHTGRAY);

    DrawFPS(screenWidth - 100, 10);
    EndDrawing();
  }

  GameConsoleAPI::unbind<GlbZoo>();
  zoo.unload_all();
  rlImGuiShutdown();
  CloseWindow();

  return 0;
}
