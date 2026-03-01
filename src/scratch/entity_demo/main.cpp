#if 0
cd "$(dirname "$0")/../../.."
cmake --build build --target entity_demo && ./build/entity_demo
exit
#endif

// ============================================================================
// TRAIT API
// ============================================================================

#include "traits_api.cpp"

// ============================================================================
// INCLUDES
// ============================================================================
#include "../../mylibs/game_console_api.hpp"
#include "../../mylibs/ilist.hpp"
#include "../../mylibs/model_api.hpp"
#include "../../mylibs/render_api.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <imgui.h>
#include <raylib.h>
#include <raymath.h>
#include <rlImGui.h>

// ============================================================================
// Unset Sentinel Helpers
// ============================================================================

template <typename T> constexpr T make_unset() {
  std::array<unsigned char, sizeof(T)> bytes{};
  for (auto& b : bytes)
    b = 0xAB;
  return std::bit_cast<T>(bytes);
}

template <typename T> constexpr bool is_unset(const T& val) {
  auto bytes = std::bit_cast<std::array<unsigned char, sizeof(T)>>(val);
  for (auto b : bytes)
    if (b != 0xAB)
      return false;
  return true;
}

// ============================================================================
// Entity
// ============================================================================
enum class RenderLayerEnum { RENDER_LAYER_0, RENDER_LAYER_1, RENDER_LAYER_2, RENDER_LAYER_3 };
// ============================================================================
// Trait Name Defines
// ============================================================================

#define TRAIT_WSAD "wsad"
#define TRAIT_PICKUP "pickup"
#define TRAIT_CROSS_SLASH_HITBOX "cross_slash_hitbox"
#define TRAIT_IS_HITBOX "is_hitbox"
#define TRAIT_IS_TEXT "is_text"
#define TRAIT_IS_GRID_ALIGNED "is_grid_aligned"
#define TRAIT_IS_PUSHABLE "is_pushable"
#define TRAIT_NO_MODEL "no-model"
#define TRAIT_IS_BILLBOARD "is_billboard"
// ============================================================================
// Entity
// ============================================================================

struct Entity : thing_base {
  const char* _debug_name = "default_name";
  ModelInstance model;
  Vector3 position = make_unset<Vector3>();
  Vector3 parent_offset = make_unset<Vector3>();
  Vector3 rotation = make_unset<Vector3>();
  RenderLayerEnum render_layer = RenderLayerEnum::RENDER_LAYER_0;
  float scale = make_unset<float>();

  // action velocity
  float push_distance = make_unset<float>();

  // entity velocity
  Vector3 velocity = make_unset<Vector3>(); // when something hits something with an unset velocity
                                            // should stop and list the colission velocity

  struct {
    uint8_t is_collidable : 1;
    uint8_t is_highlightable : 1;
    uint8_t is_draggable : 1;
  } flags = {};

  RenderTraits render;
  float life_time = make_unset<float>();

  thing_ref spawner = make_unset<thing_ref>();
  std::array<char, 128> log_text = {};
  void* traits[MAX_TRAITS] = {};
  /** @brief Implicit conversion to ModelInstance reference. */
  operator ModelInstance&() { return model; }
};

constexpr size_t MAX_ENTITIES = 1000;
using EntityList = things_list<Entity, MAX_ENTITIES>;

#include <algorithm>
#include <unordered_set>

using Pair = std::pair<thing_ref, thing_ref>;

namespace std {
template <> struct hash<thing_ref> {
  /** @brief Hash a thing_ref by combining kind, index, and generation ID. */
  size_t operator()(const thing_ref& r) const {
    size_t h = std::hash<int>{}((int)r.kind);
    h ^= std::hash<int>{}(r.idx) << 1;
    h ^= std::hash<int>{}(r.gen_id) << 2;
    return h;
  }
};
template <> struct hash<Pair> {
  /** @brief Hash a pair of thing_refs for use in collision pair sets. */
  size_t operator()(const Pair& p) const {
    size_t h = std::hash<thing_ref>{}(p.first);
    h ^= std::hash<thing_ref>{}(p.second) << 1;
    return h;
  }
};
} // namespace std

// ============================================================================
// GameCtxAPI
// ============================================================================

namespace GameCtxAPI {

struct FrameCtx {
  Vector2 mouse = {};
  Ray mouse_ray = {};
  struct RayHit {
    thing_ref ref;
    float distance;
  };
  std::vector<RayHit> under_mouse;
  std::unordered_set<Pair> collision_pairs;
  std::vector<thing_ref> hovered;
  std::unordered_set<thing_ref> dragging;
};

struct FrameBuffer {
  FrameCtx current_frame;
  FrameCtx previous_frame;

  /** @brief Rotate frames: move current to previous and reset current. */
  void advance() {
    previous_frame = std::move(current_frame);
    current_frame = FrameCtx{};
  }

  /** @brief Get the current frame context. */
  FrameCtx& current() { return current_frame; }
  /** @brief Get the previous frame context (read-only). */
  const FrameCtx& previous() const { return previous_frame; }
};

struct State {
  EntityList entities;
  Camera3D camera = {
      .position = {5.0f, 5.0f, 5.0f},
      .target = {0.0f, 0.0f, 0.0f},
      .up = {0.0f, 1.0f, 0.0f},
      .fovy = 45.0f,
      .projection = CAMERA_PERSPECTIVE,
  };
  thing_ref selected;
  Color highlight_color = YELLOW;
  Color selection_color = ORANGE;

  struct {
    float fade_time_sec = 5.0f;
  } log_layout;

  FrameBuffer frame_buffer;
};

inline State ctx; // Im not using a namspace becuse namespaces broke my reflection scripts
// when parsing they want structs

/**
 * @brief Get the first hovered entity ref from the current frame.
 * @return The hovered entity ref, or nil if nothing is hovered.
 */
inline thing_ref get_hovered() {
  auto& hovered = ctx.frame_buffer.current().hovered;
  return hovered.empty() ? thing_ref::get_nil_ref() : hovered.front();
}

/**
 * @brief Spawn a floating text label entity in the world.
 * @param text The text to display.
 * @param spawner Optional parent entity the label follows.
 */
inline void spawn_label(const char* text, thing_ref spawner = thing_ref::get_nil_ref()) {
  Entity ent = {};
  snprintf(ent.log_text.data(), ent.log_text.size(), "%s", text);
  ent.life_time = ctx.log_layout.fade_time_sec;
  ent._debug_name = "log_text";
  ent.spawner = spawner;
  ent.parent_offset = {0, 1.5f, 0};
  thing_ref ref = ctx.entities.add(ent);
  TraitAPI::apply(ctx.entities[ref], TRAIT_IS_TEXT);
}

/**
 * @brief Compute the world-space bounding box for an entity.
 * @param e The entity to compute the bounding box for.
 * @return The axis-aligned bounding box in world space.
 */
inline BoundingBox compute_world_bbox(const Entity& e) {
  BoundingBox local = {{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}};
  if (e.model.valid()) {
    Model* m = ModelAPI::get(e.model.name);
    if (m)
      local = GetModelBoundingBox(*m);
  }
  BoundingBox world;
  world.min = Vector3Add(Vector3Scale(local.min, e.scale), e.position);
  world.max = Vector3Add(Vector3Scale(local.max, e.scale), e.position);
  return world;
}

/**
 * @brief Build the world transform matrix for an entity.
 * @param e The entity.
 * @param s Override scale; uses entity scale if <= 0.
 * @return Combined scale * rotation * translation matrix.
 */
inline Matrix entity_transform(const Entity& e, float s = 0) {
  if (s <= 0)
    s = e.scale;
  return MatrixMultiply(MatrixMultiply(MatrixScale(s, s, s), MatrixRotateXYZ(e.rotation)),
                        MatrixTranslate(e.position.x, e.position.y, e.position.z));
}

/**
 * @brief Draw a model with all meshes overridden to a single color.
 * @param name Name of the model in ModelAPI.
 * @param transform World transform matrix.
 * @param color Color override for all mesh materials.
 */
inline void draw_model_colored(const char* name, Matrix transform, Color color) {
  Model* model = ModelAPI::get(name);
  if (!model)
    return;
  for (int i = 0; i < model->meshCount; i++) {
    Material mat = model->materials[model->meshMaterial[i]];
    Color original = mat.maps[MATERIAL_MAP_DIFFUSE].color;
    mat.maps[MATERIAL_MAP_DIFFUSE].color = color;
    DrawMesh(model->meshes[i], mat, transform);
    mat.maps[MATERIAL_MAP_DIFFUSE].color = original;
  }
}

/**
 * @brief DrawBillboard
 * @param name Name of the model in ModelAPI.
 * @param transform World transform matrix.
 * @param color Color override for all mesh materials.
 */

inline void draw_model_billboard(const char* name, Vector3 position, float size, Color tint) {
  Model* model = ModelAPI::get(name);
  if (!model)
    return;

  int mat_idx = model->meshMaterial[0];
  Texture2D tex = model->materials[mat_idx].maps[MATERIAL_MAP_DIFFUSE].texture;
  Rectangle source = {0, 0, (float)tex.width, (float)tex.height};
  Vector2 sz = {size, size};
  Vector2 origin = {size * 0.5f, size * 0.5f}; // center pivot

  // camera's right cross forward = true up relative to view
  Vector3 forward = Vector3Normalize(Vector3Subtract(ctx.camera.target, ctx.camera.position));
  Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, ctx.camera.up));
  Vector3 cam_up = Vector3CrossProduct(right, forward);

  DrawBillboardPro(ctx.camera, tex, source, position, cam_up, sz, origin, 0.0f, tint);
}

/**
 * @brief Draw a model using its original materials.
 * @param name Name of the model in ModelAPI.
 * @param transform World transform matrix.
 */
inline void draw_model_normal(const char* name, Matrix transform) {
  Model* model = ModelAPI::get(name);
  if (!model)
    return;
  for (int i = 0; i < model->meshCount; i++) {
    DrawMesh(model->meshes[i], model->materials[model->meshMaterial[i]], transform);
  }
}

/**
 * @brief Count the number of live entities.
 * @return The current entity count.
 */
inline size_t entity_count() {
  size_t count = 0;
  for (auto& e : ctx.entities) {
    (void)e;
    count++;
  }
  return count;
}

// ---- spawning ----
struct SpawnArgs {
  std::string model_name;
  Vector3 pos = {0, 0, 0};
  float scale = 1.0f;
  float life_time = INFINITY;
  thing_ref spawner = thing_ref::get_nil_ref();
  const char* debug_name = "default_name";
  float push_distance = make_unset<float>();
};

/**
 * @brief Spawn a new entity from the given arguments.
 * @param args Spawn configuration (model, position, scale, etc.).
 * @return Reference to the spawned entity, or nil on failure.
 */
inline thing_ref spawn(const SpawnArgs& args) {
  ModelInstance inst = ModelAPI::instance(args.model_name);

  if (!inst.valid())
    return {};
  Entity ent = {};
  ent.model = inst;
  ent.position = args.pos;
  ent.scale = args.scale;
  ent.life_time = args.life_time;
  ent.spawner = args.spawner;
  ent._debug_name = args.debug_name;

  if (args.push_distance != make_unset<float>()) {
    ent.push_distance = args.push_distance;
  }

  if (args.spawner != thing_ref::get_nil_ref()) {
    Entity& parent = ctx.entities[args.spawner];
    if (parent)
      ent.parent_offset = Vector3Subtract(args.pos, parent.position);
  }

  ent.flags.is_highlightable = 1;
  ent.flags.is_collidable = 1;

  if (args.model_name == TRAIT_NO_MODEL) {
    // set the inst to a primitave
    TraitAPI::apply(ent, TRAIT_NO_MODEL);
  }

  thing_ref ref = ctx.entities.add(ent);

  return ref;
}

/**
 * @brief Remove all entities and clear the selection.
 */
inline void clear_entities() {
  std::vector<thing_ref> refs;
  for (auto& e : ctx.entities)
    // why would e not have this ref?
    refs.push_back(e.this_ref());
  for (auto& r : refs)
    ctx.entities.remove(r);
  ctx.selected = thing_ref::get_nil_ref();
}

// ---- collision / pair handling ----
/**
 * @brief Test AABB collision between two entities.
 * @param a First entity.
 * @param b Second entity.
 * @return True if their world bounding boxes overlap.
 */
inline bool collides(const Entity& a, const Entity& b) {
  BoundingBox ba = compute_world_bbox(a);
  BoundingBox bb = compute_world_bbox(b);
  return CheckCollisionBoxes(ba, bb);
}

/**
 * @brief Update an entity's position, respecting grid alignment and parent offset.
 * @param ref Reference to the entity.
 * @param pos New desired world position.
 */
inline void entity_update_position(thing_ref ref, Vector3 pos) {
  auto& e = ctx.entities[ref];
  if (!e)
    return;

  if (TraitAPI::has(e, TRAIT_IS_GRID_ALIGNED)) {
    pos.x = roundf(pos.x);
    pos.z = roundf(pos.z);
  }

  if (e.spawner != thing_ref::get_nil_ref()) {
    auto& parent = ctx.entities[e.spawner];
    if (parent)
      e.parent_offset = Vector3Subtract(pos, parent.position);
  }

  e.position = pos;
}

/**
 * @brief Handle a newly detected collision between two entities.
 *
 * Processes pickup collection and cross-slash hit reactions.
 * Ignores pairs where one entity spawned the other.
 *
 * @param a First colliding entity.
 * @param b Second colliding entity.
 */
inline void handle_pair(Entity& a, Entity& b) {

  // chidren can't collide with their parents
  if (a.spawner == b.this_ref() || b.spawner == a.this_ref()) {
    return;
  }

  if (TraitAPI::has(a, TRAIT_CROSS_SLASH_HITBOX) && TraitAPI::has(b, TRAIT_CROSS_SLASH_HITBOX)) {
    return;
  }

  // if a has push distance and b is pushable, set b's velocity from the action
  if (!is_unset(a.push_distance) && TraitAPI::has(b, TRAIT_IS_PUSHABLE)) {
    Vector3 dir = Vector3Subtract(b.position, a.position);
    dir.y = 0;
    float len = Vector3Length(dir);
    if (len > 0.001f) {
      dir = Vector3Scale(dir, 1.0f / len);
      b.velocity = Vector3Scale(dir, a.push_distance);
    }
  }

  if (TraitAPI::has(a, TRAIT_WSAD) && TraitAPI::has(b, TRAIT_PICKUP)) {
    GameConsoleAPI::print(std::string("Picked up ") + (b.model.valid() ? b.model.name : "???"));
    spawn_label(TextFormat("Picked up %s", b.model.valid() ? b.model.name : "???"));
    ctx.entities.remove(b.this_ref());
  }

  if (TraitAPI::has(a, TRAIT_CROSS_SLASH_HITBOX)) {
    GameConsoleAPI::print(std::string("cross slash hit ") +
                          (b.model.valid() ? b.model.name : "???"));
    spawn_label(TextFormat("cross slash hit %s", b.model.valid() ? b.model.name : "???"),
                b.this_ref());
    TraceLog(LOG_INFO, "cross slash hit %s", b._debug_name);
  }

  // moving entity hits a stationary entity
  if (!is_unset(a.velocity) && is_unset(b.velocity)) {
    float speed = Vector3Length(a.velocity);
    spawn_label(TextFormat("hit %s (%.1f)", b.model.valid() ? b.model.name : "???", speed),
                a.this_ref());

    if (TraitAPI::has(b, TRAIT_IS_PUSHABLE)) {
      // transfer velocity to b
      b.velocity = a.velocity;
    }
    // a stops
    a.velocity = make_unset<Vector3>();
  }
}

/**
 * @brief Main per-frame update: sweeps expired entities, performs mouse ray picking,
 *        handles dragging, updates positions, ticks traits, detects collisions,
 *        processes cross-slash on mouse release, and renders all entities.
 */

inline void update() {
  ctx.frame_buffer.advance();
  auto& frame = ctx.frame_buffer.current();
  const auto& last = ctx.frame_buffer.previous();

  //
  // sweep expired entities
  //

  {
    float dt = GetFrameTime();
    std::vector<thing_ref> expired;
    for (auto& e : ctx.entities) {
      e.life_time -= dt;
      if (e.life_time <= 0.0f)
        expired.push_back(e.this_ref());
    }
    for (auto& r : expired) {
      if (r == ctx.selected)
        ctx.selected = thing_ref::get_nil_ref();
      ctx.entities.remove(r);
    }
  }

  //
  //  mouse ray collision
  //

  frame.mouse = GetMousePosition();
  frame.mouse_ray = GetScreenToWorldRay(frame.mouse, ctx.camera);
  for (auto& e : ctx.entities) {
    if (!e.render.visible || !e.model.valid())
      continue;
    BoundingBox wb = compute_world_bbox(e);
    RayCollision col = GetRayCollisionBox(frame.mouse_ray, wb);
    if (col.hit)
      frame.under_mouse.push_back({e.this_ref(), col.distance});
  }
  std::sort(
      frame.under_mouse.begin(), frame.under_mouse.end(),
      [](const FrameCtx::RayHit& a, const FrameCtx::RayHit& b) { return a.distance < b.distance; });

  if (!ImGui::GetIO().WantCaptureMouse) {
    for (auto& hit : frame.under_mouse) {
      auto& e = ctx.entities[hit.ref];
      if (e.flags.is_highlightable) {
        frame.hovered.push_back(hit.ref);
        break;
      }
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !frame.hovered.empty())
      ctx.selected = frame.hovered.front();
  }

  //
  // entity dragging logic
  //

  if (!ImGui::GetIO().WantCaptureMouse && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
    for (auto& ref : last.dragging) {
      auto& e = ctx.entities[ref];
      if (e)
        frame.dragging.insert(ref);
    }
    for (auto& hit : frame.under_mouse) {
      auto& e = ctx.entities[hit.ref];
      if (e && e.flags.is_draggable && !frame.dragging.count(hit.ref))
        frame.dragging.insert(hit.ref);
    }
  }

  //
  //  entity position update
  //

  for (auto& e : ctx.entities) {
    if (frame.dragging.count(e.this_ref())) {
      if (frame.mouse_ray.direction.y != 0) {
        float t = (e.position.y - frame.mouse_ray.position.y) / frame.mouse_ray.direction.y;
        Vector3 point =
            Vector3Add(frame.mouse_ray.position, Vector3Scale(frame.mouse_ray.direction, t));
        entity_update_position(e.this_ref(), {point.x, e.position.y, point.z});
      }
    } else if (e.spawner != thing_ref::get_nil_ref()) {
      auto& parent = ctx.entities[e.spawner];
      if (parent) {
        e.position = Vector3Add(parent.position, e.parent_offset);
        e.rotation = parent.rotation;
      }
    }
    // velocity update with friction
    if (!is_unset(e.velocity)) {
      float dt = GetFrameTime();
      e.position = Vector3Add(e.position, Vector3Scale(e.velocity, dt));
      constexpr float friction = 3.0f;
      float decay = expf(-friction * dt);
      e.velocity = Vector3Scale(e.velocity, decay);
      if (Vector3Length(e.velocity) < 0.05f)
        e.velocity = make_unset<Vector3>();
    }
  }

  //
  //   traits update
  //

  TraitAPI::tick_all(ctx.entities);

  //
  // entity collision update
  //

  {
    std::vector<thing_ref> collidables;
    for (auto& e : ctx.entities) {
      if (e.flags.is_collidable)
        collidables.push_back(e.this_ref());
    }

    for (size_t i = 0; i < collidables.size(); i++) { // pre filter collisions
      auto& a = ctx.entities[collidables[i]];
      if (!a)
        continue;
      for (size_t j = i + 1; j < collidables.size(); j++) {
        auto& b = ctx.entities[collidables[j]];
        if (!b)
          continue;
        if (!collides(a, b))
          continue;
        frame.collision_pairs.insert({a.this_ref(), b.this_ref()});
        frame.collision_pairs.insert({b.this_ref(), a.this_ref()});
      }
    }
    for (auto& pair : frame.collision_pairs) {
      if (last.collision_pairs.count(pair))
        continue;
      auto& a = ctx.entities[pair.first];
      auto& b = ctx.entities[pair.second];
      if (a && b)
        handle_pair(a, b);
    }
  }

  //
  // mouse up logic
  //

  if (!ImGui::GetIO().WantCaptureMouse && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
      !last.dragging.empty()) {
    for (auto& hit : frame.under_mouse) {
      auto& target = ctx.entities[hit.ref];
      if (!target || !TraitAPI::has(target, TRAIT_WSAD))
        continue;

      TraceLog(LOG_INFO, "cross slash on: %s", target.model.name);
      spawn_label(TextFormat("cross slash at (%.1f, %.1f, %.1f)", target.position.x,
                             target.position.y, target.position.z),
                  target.this_ref());

      thing_ref spawner_ref = target.this_ref();
      struct {
        float dx, dz;
        const char* name;
      } dirs[] = {
          {0, 1, "north slash hitbox"},      {0, -1, "south slash hitbox"},
          {1, 0, "east slash hitbox"},       {-1, 0, "west slash hitbox"},
          {-2, 0, "west west slash hitbox"},
      };

      for (auto& d : dirs) {
        thing_ref ref = spawn({
            .model_name = target.model.name,
            .pos = {target.position.x + d.dx, target.position.y, target.position.z + d.dz},
            .life_time = 1.0f,
            .spawner = spawner_ref,
            .debug_name = d.name,
            .push_distance = 1.0f,
        });
        auto& ent = ctx.entities[ref];
        TraitAPI::apply(ent, TRAIT_CROSS_SLASH_HITBOX);
        TraitAPI::apply(ent, TRAIT_IS_HITBOX);
      }
      break;
    }
  }

  //
  //  render
  //

  RenderAPI::layer_start(RenderLayer::Background, ctx.camera);
  for (int x = -10; x <= 10; x++)
    for (int z = -10; z <= 10; z++) {
      Color tile = ((x + z) % 2 == 0) ? Color{60, 60, 60, 255} : Color{40, 40, 40, 255};
      DrawPlane({(float)x, -.1f, (float)z}, {1, 1}, tile);
    }

  RenderAPI::layer_start(RenderLayer::Highlight, ctx.camera);
  if (auto& h = ctx.entities[get_hovered()]) {
    if (h.render.visible && h.model.valid()) {
      draw_model_colored(h.model.name, entity_transform(h, h.scale * 1.1f), ctx.highlight_color);
    }
  }

  RenderAPI::layer_start(RenderLayer::Entities, ctx.camera);
  for (auto& e : ctx.entities) {

    if (!e.render.visible || !e.model.valid()) {
      continue;
    }

    draw_model_normal(e.model.name, entity_transform(e));

    if (TraitAPI::has(e, TRAIT_IS_HITBOX)) {
      DrawBoundingBox(compute_world_bbox(e), RED);
    }
  }

  RenderAPI::layer_start(RenderLayer::Focus, ctx.camera);
  if (auto& sel = ctx.entities[ctx.selected]) {
    if (sel.render.visible && sel.model.valid())
      draw_model_colored(sel.model.name, entity_transform(sel, sel.scale * 1.15f),
                         ctx.selection_color);
  }

  RenderAPI::layer_start(RenderLayer::UI_World, ctx.camera);
  for (auto& e : ctx.entities) {
    if (TraitAPI::has(e, TRAIT_IS_BILLBOARD)) {
      draw_model_billboard(e.model.name, e.position, e.scale, WHITE);
    }
  }

  RenderAPI::rasterize();

  for (auto& e : ctx.entities) {
    if (!TraitAPI::has(e, TRAIT_IS_TEXT))
      continue;
    Vector2 screen = GetWorldToScreen(e.position, ctx.camera);
    float alpha = Clamp(e.life_time / ctx.log_layout.fade_time_sec, 0.0f, 1.0f);
    DrawText(e.log_text.data(), (int)screen.x, (int)screen.y, 20, RED);
  }
}

/**
 * @brief Draw the ImGui debug panels: scene inspector, entity list,
 *        selected entity properties, trait registry, and game console.
 */
inline void draw_imgui() {
  rlImGuiBegin();
  if (ImGui::Begin("Scene")) {
    ImGui::Text("Entities: %zu", entity_count());
    ImGui::Text("Models: %zu", ModelAPI::count());
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Models", ImGuiTreeNodeFlags_DefaultOpen)) {
      for (const auto& name : ModelAPI::names()) {
        if (ImGui::Button(name.c_str()))
          spawn(SpawnArgs{.model_name = name});
        ImGui::SameLine();
        ImGui::Text("(click to spawn)");
      }
    }

    if (ImGui::CollapsingHeader("Entities", ImGuiTreeNodeFlags_DefaultOpen)) {
      int idx = 0;
      for (auto& e : ctx.entities) {
        ImGui::PushID(idx);
        std::string traits_str = TraitAPI::debug_entity(e);
        bool is_selected = (e.this_ref() == ctx.selected);
        if (ImGui::Selectable(TextFormat("%d: %s [%s]", idx, e.model.valid() ? e.model.name : "---",
                                         traits_str.c_str()),
                              is_selected)) {
          ctx.selected = e.this_ref();
        }
        ImGui::PopID();
        idx++;
      }
    }

    if (auto& sel = ctx.entities[ctx.selected]) {
      ImGui::Separator();
      ImGui::Text("Selected: %s", sel.model.valid() ? sel.model.name : "---");
      ImGui::Text("Traits: %s", TraitAPI::debug_entity(sel).c_str());
      ImGui::DragFloat3("Position", &sel.position.x, 0.1f);
      ImGui::DragFloat3("Rotation", &sel.rotation.x, 0.01f);
      ImGui::DragFloat("Scale", &sel.scale, 0.1f, 0.1f, 10.0f);

      ImGui::Separator();
      bool visible = sel.render.visible;
      if (ImGui::Checkbox("Visible", &visible))
        sel.render.visible = visible;

      ImGui::Separator();
      if (ImGui::Button("Delete")) {
        ctx.entities.remove(ctx.selected);
        ctx.selected = thing_ref::get_nil_ref();
      }
    }

    if (ImGui::CollapsingHeader("Trait Registry")) {
      ImGui::TextUnformatted(TraitAPI::debug_registered().c_str());
    }
  }
  ImGui::End();
  GameConsoleAPI::draw_imgui();
  rlImGuiEnd();
}

} // namespace GameCtxAPI

#include "traits.inc"

// ============================================================================
// Lua API
// ============================================================================
#include "../../mylibs/lua_api.hpp"

#include "cmds.inc"

// ============================================================================
// Main
// ============================================================================
/**
 * @brief Application entry point. Initializes window, ImGui, rendering, Lua,
 *        registers traits, runs the main loop, and cleans up on exit.
 */
int main() {
  InitWindow(1280, 720, "Entity Spawner");
  SetTargetFPS(60);
  rlImGuiSetup(true);
  RenderAPI::init();

  LuaAPI::init();

  TraitAPI::register_trait(TRAIT_WSAD, wsad_init, wsad_update);

  TraitAPI::register_trait(TRAIT_PICKUP, pickup_init, pickup_update);

  TraitAPI::register_trait(TRAIT_CROSS_SLASH_HITBOX, pickup_init, pickup_update);

  TraitAPI::register_trait(TRAIT_IS_HITBOX);

  TraitAPI::register_trait(TRAIT_IS_TEXT);

  TraitAPI::register_trait(TRAIT_IS_GRID_ALIGNED);

  TraitAPI::register_trait(TRAIT_IS_PUSHABLE);

  TraitAPI::register_trait("is_billboard");

  // how do I make a frame around all the items in game?

  GameConsoleAPI::print("Press ~ for console.");
  LuaAPI::run_file("assets/setup.lua");

  while (!WindowShouldClose()) {
    if (IsKeyPressed(KEY_GRAVE))
      GameConsoleAPI::toggle_visible();
    if (!GameConsoleAPI::visible())
      UpdateCamera(&GameCtxAPI::ctx.camera, CAMERA_FREE);

    BeginDrawing();
    ClearBackground(DARKGRAY);
    GameCtxAPI::update();
    GameCtxAPI::draw_imgui();
    DrawFPS(10, 10);
    EndDrawing();
  }

  LuaAPI::shutdown();
  GameCtxAPI::clear_entities();
  RenderAPI::shutdown();
  ModelAPI::unload_all();
  rlImGuiShutdown();
  CloseWindow();
  return 0;
}
