#if 0
cd "$(dirname "$0")/../../.."
if [[ "$1" == "gdb" ]]; then
    cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build --target entity_demo || exit 1
    gdb -x .gdbinit ./build/entity_demo
else
    cmake --build build --target entity_demo || exit 1
    ./build/entity_demo
fi
exit
#endif
#include "../../mylibs/consol_helpers.hpp"
#include "../../mylibs/hexgrid_math.hpp"
#include "../../mylibs/ilist.hpp"
#include "../../mylibs/model_store.hpp"
#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>
#include <type_traits>
// Forward declare
template <typename T> void imgui_widget(const char* name, T& v);
struct GameTraits {
  uint is_hex : 1 = 0;
  uint is_dot : 1 = 0;
  uint is_link : 1 = 0;
};
struct EntityConfig {
  GameTraits traits;
};
struct EntityData {
  ModelInstance model;
  uint hex_id;
  GameTraits traits;
  Vector3 pos;
  BoundingBox bbox;
  std::function<void(const EntityData&)> update_impl = nullptr;
  void update() { update_impl(*this); }

  bool is_hit() { return false; }; // this is a placeholder
  // Implicit conversion - just use it where a Model is expected
  operator ModelInstance&() { return model; }
};

struct GameControls {
  float camera_speed = 1.0f;
};
struct GameRenderTextureBuffers {
  RenderTexture2D buffer_1;
  RenderTexture2D buffer_2;
};

template <size_t N> struct game_ctx {
  Layout hex_layout;
  GameTraits game_traits[N];
  GameControls game_controls;
  GameRenderTextureBuffers render_texture_buffers;
  ModelStore models;
  using EntityList = things_list<EntityData, N>;
  EntityList things;
  thing_ref player_ref;
  thing_ref higlighted_ref;
  Camera3D camera = {
      .position = {10.0f, 10.0f, 10.0f},
      .target = {0.0f, 0.0f, 0.0f},
      .up = {0.0f, 1.0f, 0.0f},
      .fovy = 45.0f,
      .projection = CAMERA_ORTHOGRAPHIC,
  };
  struct hex_textures {
    Texture2D grass;
    Texture2D water;
  };
  void update_game_ctx() {
    float radius = this->hex_layout.hex_size.x;
    float height = radius * 0.1f;

    // ---- Mouse picking ----
    uint hovered_id = mouseray_hex(this->hex_layout, camera);
    if (hovered_id != UINT_MAX && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      for (auto& thing : things) {
        if (thing.hex_id == hovered_id) {
          higlighted_ref = thing.this_ref();
          break;
        }
      }
    }

    BeginMode3D(camera);

    //
    // draw hex
    //

    for (uint i = 0; i < hex_layout.n_hex; i++) {
      Hex h = hex_layout[i];
      Vector3 center = hex_to_world(this->hex_layout, h);
      Color fill = DARKBLUE;
      Color wire = WHITE;
      if (i == hovered_id) {
        fill = ColorBrightness(DARKBLUE, 0.3f);
        wire = YELLOW;
      }
      DrawCylinder(center, radius, radius, height, 6, fill);
      DrawCylinderWires(center, radius, radius, height, 6, wire);
    }

    //
    // draw entities using instanced rendering
    //

    float scale = radius / 5.0f;
    for (const auto& name : models.names()) {
      draw_instanced(name.c_str(), scale);
    }

    //
    // draw highlight
    //

    if (auto& higlighted_thing = things[higlighted_ref]) {
      Vector3 center = hex_to_world(this->hex_layout, hex_layout[higlighted_thing.hex_id]);
      center.y += height / 2.0f;

      BeginTextureMode(render_texture_buffers.buffer_1);
      ClearBackground(BLANK);
      BeginMode3D(camera);
      DrawSphere(center, radius / 8.0f, YELLOW);
      EndMode3D();
      EndTextureMode();

      BeginTextureMode(render_texture_buffers.buffer_2);
      ClearBackground(BLANK);
      BeginMode3D(camera);
      DrawSphere(center, radius / 10.0f, RED);
      EndMode3D();
      EndTextureMode();
    }
    EndMode3D();

    //
    // draw hover tooltip
    //

    if (hovered_id != UINT_MAX) {
      Hex h = hex_layout[hovered_id];
      Vector2 mouse = GetMousePosition();
      DrawText(TextFormat("hex %u (%d,%d,%d)", hovered_id, h.q, h.r, h.s), (int)mouse.x + 15,
               (int)mouse.y - 20, 16, YELLOW);
    }

    //
    // draw render texture
    //

    DrawTextureRec(render_texture_buffers.buffer_1.texture,
                   {0, 0, (float)render_texture_buffers.buffer_1.texture.width,
                    (float)-render_texture_buffers.buffer_1.texture.height},
                   {0, 0}, WHITE);

    DrawTextureRec(render_texture_buffers.buffer_2.texture,
                   {0, 0, (float)render_texture_buffers.buffer_2.texture.width,
                    (float)-render_texture_buffers.buffer_2.texture.height},
                   {0, 0}, WHITE);
  }

  void imgui_game_ctx() {
    rlImGuiBegin();
    // ---- Grid Settings ----
    auto ctx = *this;
    static float hex_size_min = 0.1f, hex_size_max = 10.0f;
    static float origin_min = -100.0f, origin_max = 100.0f;
    static int n_hex_min = 1, n_hex_max = 200;
    static int params_min = 0, params_max = 20;
    static float cam_pos_min = -50.0f, cam_pos_max = 50.0f;
    static float fovy_min = 1.0f, fovy_max = 120.0f;
    if (ImGui::Begin("Grid Settings")) {
      if (ImGui::CollapsingHeader("Layout", ImGuiTreeNodeFlags_DefaultOpen)) {
        // hex_size
        ImGui::SliderFloat2("hex_size", &ctx.hex_layout.hex_size.x, hex_size_min, hex_size_max);
        ImGui::PushItemWidth(80);
        ImGui::InputFloat("##hex_size_min", &hex_size_min);
        ImGui::SameLine();
        ImGui::InputFloat("##hex_size_max", &hex_size_max);
        ImGui::PopItemWidth();
        // origin
        ImGui::SliderFloat2("origin", &ctx.hex_layout.origin.x, origin_min, origin_max);
        ImGui::PushItemWidth(80);
        ImGui::InputFloat("##origin_min", &origin_min);
        ImGui::SameLine();
        ImGui::InputFloat("##origin_max", &origin_max);
        ImGui::PopItemWidth();
        // n_hex
        int n = ctx.hex_layout.n_hex;
        if (ImGui::SliderInt("n_hex", &n, n_hex_min, n_hex_max))
          ctx.hex_layout.n_hex = n;
        ImGui::PushItemWidth(80);
        ImGui::InputInt("##n_hex_min", &n_hex_min);
        ImGui::SameLine();
        ImGui::InputInt("##n_hex_max", &n_hex_max);
        ImGui::PopItemWidth();
        // shape
        const char* shapes[] = {"Parallelogram", "TriangleDown",    "TriangleUp",
                                "Hexagon",       "RectanglePointy", "RectangleFlat"};
        int shape = static_cast<int>(ctx.hex_layout.shape);
        if (ImGui::Combo("shape", &shape, shapes, 6))
          ctx.hex_layout.shape = static_cast<GridShape>(shape);
        // params.a
        ImGui::SliderInt("params.a", &ctx.hex_layout.params.a, params_min, params_max);
        ImGui::PushItemWidth(80);
        ImGui::InputInt("##params_a_min", &params_min);
        ImGui::SameLine();
        ImGui::InputInt("##params_a_max", &params_max);
        ImGui::PopItemWidth();
        // params.b
        ImGui::SliderInt("params.b", &ctx.hex_layout.params.b, params_min, params_max);
        // params.c
        ImGui::SliderInt("params.c", &ctx.hex_layout.params.c, params_min, params_max);
        // params.d
        ImGui::SliderInt("params.d", &ctx.hex_layout.params.d, params_min, params_max);
      }
      if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
        // position
        ImGui::SliderFloat3("position", &ctx.camera.position.x, cam_pos_min, cam_pos_max);
        ImGui::PushItemWidth(80);
        ImGui::InputFloat("##pos_min", &cam_pos_min);
        ImGui::SameLine();
        ImGui::InputFloat("##pos_max", &cam_pos_max);
        ImGui::PopItemWidth();
        // target
        ImGui::SliderFloat3("target", &ctx.camera.target.x, cam_pos_min, cam_pos_max);
        // up
        ImGui::SliderFloat3("up", &ctx.camera.up.x, -1.0f, 1.0f);
        // fovy
        ImGui::SliderFloat("fovy", &ctx.camera.fovy, fovy_min, fovy_max);
        ImGui::PushItemWidth(80);
        ImGui::InputFloat("##fovy_min", &fovy_min);
        ImGui::SameLine();
        ImGui::InputFloat("##fovy_max", &fovy_max);
        ImGui::PopItemWidth();
        // projection
        const char* projections[] = {"PERSPECTIVE", "ORTHOGRAPHIC"};
        int proj = ctx.camera.projection;
        if (ImGui::Combo("projection", &proj, projections, 2))
          ctx.camera.projection = proj;
      }
    }
    ImGui::End();
    // ---- Console ----
    Console::get().draw_imgui();
    rlImGuiEnd();
  }
  // Collect transforms for entities with matching model, then draw instanced
  void draw_instanced(const char* model_name, float scale = 1.0f) {
    Model* model = models.get(model_name);
    if (!model || model->meshCount == 0)
      return;
    // Collect transforms for all entities using this model
    std::vector<Matrix> transforms;
    float radius = hex_layout.hex_size.x;
    float height = radius * 0.1f;
    for (auto& e : things) {
      if (e.model.valid() && strcmp(e.model.name, model_name) == 0) {
        Vector3 center = hex_to_world(hex_layout, hex_layout[e.hex_id]);
        center.y += height;
        // T * S: scale first (around origin), then translate to position
        Matrix s = MatrixScale(scale, scale, scale);
        Matrix t = MatrixTranslate(center.x, center.y, center.z);
        transforms.push_back(MatrixMultiply(t, s));
      }
    }
    if (transforms.empty())
      return;
    // Get the material with proper color
    Material mat = model->materials[0];
    // Draw all instances of each mesh in the model
    for (int i = 0; i < model->meshCount; i++) {
      DrawMeshInstanced(model->meshes[i], mat, transforms.data(), (int)transforms.size());
    }
  }
  void add_placement(uint hex_id, EntityConfig config, const std::string& model_name = "") {
    ModelInstance inst = models.instance(model_name);
    things.add({inst, hex_id, config.traits, {0, 0, 0}, {}});
  }
};

using game_ctx_1k = game_ctx<1000>;

// ============================================================================
// Console commands — registered before main(), ctx bound later
// ============================================================================

REGISTER_CMD(spawn, "spawn <model> <hex_id>", {
  CMD_CTX(game_ctx_1k, ctx);
  if (args.size() < 2)
    return std::string("Usage: spawn <model> <hex_id>");
  std::string model_name = args[0];
  uint hex_id = std::stoi(args[1]);
  if (hex_id >= ctx.hex_layout.n_hex)
    return "Invalid hex_id: " + args[1] + " (max " + std::to_string(ctx.hex_layout.n_hex - 1) + ")";
  if (!ctx.models.get(model_name))
    return "Unknown model: " + model_name;
  EntityConfig config = {.traits = {0, 0, 0}};
  ctx.add_placement(hex_id, config, model_name);
  return "Spawned " + model_name + " at hex " + args[1];
});

REGISTER_CMD(list, "list entities", {
  CMD_CTX(game_ctx_1k, ctx);
  (void)args;
  std::string out;
  int i = 0;
  for (auto& thing : ctx.things) {
    out += std::to_string(i++) + ": " +
           (thing.model.valid() ? std::string(thing.model.name) : "???") +
           " hex=" + std::to_string(thing.hex_id) + "\n";
  }
  return out.empty() ? "No entities" : out;
});

REGISTER_CMD(fill, "fill <model> - place on every hex", {
  CMD_CTX(game_ctx_1k, ctx);
  if (args.empty())
    return std::string("Usage: fill <model>");
  if (!ctx.models.get(args[0]))
    return "Unknown model: " + args[0];
  EntityConfig config = {.traits = {0, 0, 0}};
  for (uint i = 0; i < ctx.hex_layout.n_hex; i++) {
    ctx.add_placement(i, config, args[0]);
  }
  return "Filled " + std::to_string(ctx.hex_layout.n_hex) + " hexes with " + args[0];
});

REGISTER_CMD(highlight, "highlight <hex_id>", {
  CMD_CTX(game_ctx_1k, ctx);
  if (args.empty())
    return std::string("Usage: highlight <hex_id>");
  uint hex_id = std::stoi(args[0]);
  for (auto& thing : ctx.things) {
    if (thing.hex_id == hex_id) {
      ctx.higlighted_ref = thing.this_ref();
      return "Highlighting hex " + args[0];
    }
  }
  return "No entity at hex " + args[0];
});

REGISTER_CMD(cam, "cam <x> <y> <z>", {
  CMD_CTX(game_ctx_1k, ctx);
  if (args.size() < 3)
    return std::string("Usage: cam <x> <y> <z>");
  ctx.camera.position = {std::stof(args[0]), std::stof(args[1]), std::stof(args[2])};
  return "Camera at " + args[0] + " " + args[1] + " " + args[2];
});

REGISTER_CMD(target, "target <x> <y> <z>", {
  CMD_CTX(game_ctx_1k, ctx);
  if (args.size() < 3)
    return std::string("Usage: target <x> <y> <z>");
  ctx.camera.target = {std::stof(args[0]), std::stof(args[1]), std::stof(args[2])};
  return "Target at " + args[0] + " " + args[1] + " " + args[2];
});

REGISTER_CMD(fov, "fov [degrees]", {
  CMD_CTX(game_ctx_1k, ctx);
  if (args.empty())
    return "FOV: " + std::to_string(ctx.camera.fovy);
  ctx.camera.fovy = std::stof(args[0]);
  return "FOV set to " + args[0];
});

REGISTER_CMD(ortho, "toggle ortho/perspective", {
  CMD_CTX(game_ctx_1k, ctx);
  (void)args;
  ctx.camera.projection =
      (ctx.camera.projection == CAMERA_ORTHOGRAPHIC) ? CAMERA_PERSPECTIVE : CAMERA_ORTHOGRAPHIC;
  return (ctx.camera.projection == CAMERA_ORTHOGRAPHIC) ? "Orthographic" : "Perspective";
});

REGISTER_CMD(hex_size, "hex_size [size]", {
  CMD_CTX(game_ctx_1k, ctx);
  if (args.empty())
    return "Hex size: " + std::to_string(ctx.hex_layout.hex_size.x);
  float s = std::stof(args[0]);
  ctx.hex_layout.hex_size = {s, s};
  return "Hex size set to " + args[0];
});

REGISTER_CMD(model_list, "list loaded models", {
  CMD_CTX(game_ctx_1k, ctx);
  (void)args;
  std::string out;
  for (const auto& name : ctx.models.names())
    out += name + "\n";
  return out.empty() ? "No models loaded" : out;
});

// ============================================================================

int main() {
  const int screenWidth = 1920;
  const int screenHeight = 1080;
  InitWindow(screenWidth, screenHeight, "Hex Grid - Simple blahblahblah");
  SetTargetFPS(60);
  game_ctx_1k ctx = {
      .hex_layout =
          {
              .orientation = layout_pointy,
              .hex_size = {1.0f, 1.0f},
              .origin = {0, 0},
              .shape = GridShape::Hexagon,
              .params = {.a = 5},
              .n_hex = 91,
          },
  };
  // Bind game context so console commands can access it
  Console::get().bind<game_ctx_1k>(&ctx);
  ctx.render_texture_buffers.buffer_1 = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
  ctx.render_texture_buffers.buffer_2 = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
  // Load instancing shader
  Shader instancingShader =
      LoadShader("assets/shaders/instancing.vs", "assets/shaders/instancing.fs");
  instancingShader.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(instancingShader, "mvp");
  instancingShader.locs[SHADER_LOC_MATRIX_MODEL] =
      GetShaderLocationAttrib(instancingShader, "instanceTransform");
  instancingShader.locs[SHADER_LOC_MATRIX_NORMAL] =
      GetShaderLocation(instancingShader, "matNormal");
  instancingShader.locs[SHADER_LOC_COLOR_DIFFUSE] =
      GetShaderLocation(instancingShader, "colDiffuse");
  // Load models at unit size - scaling happens in draw_instanced
  ctx.models.load("sphere", GenMeshSphere(0.5f, 16, 16));
  ctx.models.load("cube", GenMeshCube(1.0f, 1.0f, 1.0f));
  ctx.models.load("cylinder", GenMeshCylinder(0.5f, 1.0f, 16));
  // Set instancing shader and colors for each model
  ctx.models.get("sphere")->materials[0].shader = instancingShader;
  ctx.models.get("sphere")->materials[0].maps[MATERIAL_MAP_DIFFUSE].color = RED;
  ctx.models.get("cube")->materials[0].shader = instancingShader;
  ctx.models.get("cube")->materials[0].maps[MATERIAL_MAP_DIFFUSE].color = GREEN;
  ctx.models.get("cylinder")->materials[0].shader = instancingShader;
  ctx.models.get("cylinder")->materials[0].maps[MATERIAL_MAP_DIFFUSE].color = BLUE;
  rlImGuiSetup(true);
  EntityConfig config;
  const char* model_names[] = {"sphere", "cube", "cylinder"};
  for (int i = 0; i < (int)ctx.hex_layout.n_hex; i++) {
    config.traits = {0, 0, 0};
    ctx.add_placement(i, config, model_names[i % 3]);
  }
  for (auto& thing : ctx.things) {
    if (thing.hex_id == 1) {
      ctx.higlighted_ref = thing.this_ref();
    }
  }
  Console::get().print("Press ~ to toggle console. Type 'help' for commands.");
  while (!WindowShouldClose()) {
    if (IsKeyPressed(KEY_GRAVE))
      Console::get().toggle_visible();
    BeginDrawing();
    if (!Console::get().visible) {
      UpdateCamera(&ctx.camera, CAMERA_ORBITAL);
    }
    ClearBackground(WHITE);
    ctx.update_game_ctx();
    ctx.imgui_game_ctx();
    DrawFPS(10, 10);
    EndDrawing();
  }
  Console::get().unbind<game_ctx_1k>();
  UnloadShader(instancingShader);
  ctx.models.unload_all();
  CloseWindow();
  return 0;
}
