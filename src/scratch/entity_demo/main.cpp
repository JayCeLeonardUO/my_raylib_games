#if 0
cd "$(dirname "$0")/../../.."
cmake --build build --target entity_demo && ./build/entity_demo
exit
#endif

// ============================================================================
// TRAIT API
// ============================================================================

#include "traits_api.hpp"

// ============================================================================
// INCLUDES
// ============================================================================
#include "../../mylibs/animation_registry.hpp"
#include "../../mylibs/game_console_api.hpp"
#include "../../mylibs/ilist.hpp"     // I think I named this ilist for intrusive list
#include "../../mylibs/lua_api.hpp"   // VM core, game-agnostic
#include "../../mylibs/lua_imgui.hpp" // Lua history window
#include "../../mylibs/model_api.hpp"
#include "../../mylibs/normal_map_utils.hpp"
#include "../../mylibs/render_api.hpp"
#include "../../mylibs/sun_glints.hpp"
#include "../../mylibs/three_point_light_api.hpp"
#include "../../mylibs/triangle_particles.hpp"
#include "shaders.c" // GLSL strings for the point-light shadow capture
#include <array>
#include <bit>
#include <cmath>
#include <imgui.h>
#include <raylib.h>
#include <raymath.h>
#include <rlImGui.h>

// ============================================================================
// Unset Sentinel Helpers
// - used to detect unset values
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
// Enum for ordering draw calls
// ============================================================================
enum class RenderLayerEnum { RENDER_LAYER_0, RENDER_LAYER_1, RENDER_LAYER_2, RENDER_LAYER_3 };

// ============================================================================
// Trait Name Defines
// ============================================================================

// Traits are not listed here. They are declared with DECLARE_TRAIT(...) next to the code that
// first needed them and collected into gen/entity_traits.gen.hpp by tools/gen_traits.py.

// Model-name sentinel for entities that draw without a model (2d animations, labels).
inline constexpr const char* NO_MODEL_NAME = "no-model";

// ============================================================================
// Groups
// ============================================================================
enum GroupId { group_seed_deck, num_groups };

// debug names, indexed by GroupId. add a name per enum group.
inline const char* group_names[num_groups] = {"seed_deck"};

struct group_info {
  thing_ref parent = thing_ref::get_nil_ref();
  thing_ref next = thing_ref::get_nil_ref();
};

// ============================================================================
// Entity
// ============================================================================

/** @brief Build a fixed-size char buffer from a literal, for struct member defaults. */
template <size_t N> constexpr std::array<char, N> text_buf(const char* s) {
  std::array<char, N> a{};
  for (size_t i = 0; i + 1 < N && s[i]; i++)
    a[i] = s[i];
  return a;
}

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
  Color bloom_color = WHITE; // what a BLOOM entity emits: its texture times this (see bloom())
  float life_time = make_unset<float>();

  thing_ref spawner = make_unset<thing_ref>();
  // free-form text on the entity. shown by IS_TEXT labels and HOVER_CARD. the default is
  // deliberately loud so a card that was never given text says exactly where it came from.
  std::array<char, 128> log_text = text_buf<128>("log_text: Entity struct default (nobody set me)");

  // animation
  int animation_id = -1;
  float animation_start_time = 0.0f;
  float animation_speed = 1.0f;

  // intrusive group membership, indexed by GroupId. groups[g] is this entity's
  // ring-node for group g. orthogonal to spawner/parent_offset.
  group_info groups[num_groups] = {};

  TraitBits traits; // generated, see gen/entity_traits.gen.hpp
  /** @brief Implicit conversion to ModelInstance reference. */
  operator ModelInstance&() { return model; }

  int gx = 0, gy = 0; // grid cell (seed-deck members)
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

namespace GameCtxAPI { // TODO: this namespace is annoying. now that I look at it with fresh eyes.
// I could not find the update via the outline. it was hidden

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

  int click_2d_animation = -1; // animation id to spawn on click (-1 = none)
  float click_anim_speed = 1.0f;
  float click_anim_scale = 1.0f;

  FrameBuffer frame_buffer;

  // ---- intrusive groups: head/anchor entity per group (nil => group inactive) ----
  thing_ref group_heads[num_groups] = {};

  // ---- turn state ----
  int turn = 0;
  int seeds_per_turn = 1;
};

inline State ctx; // Im not using a namspace becuse namespaces broke my reflection scripts
// when parsing they want structs

// ---- ground platform: one flat slab lit by the three-point shader, with a procedural
// noise normal map so the key/fill/rim colors read across the surface ----
inline Model ground = {0};
inline Texture2D ground_normal_map = {0};
inline constexpr float GROUND_SIZE = 60.0f;
inline constexpr float GROUND_THICKNESS = 0.4f;
inline constexpr float GROUND_TOP_Y = -0.1f; // same height the old tiles sat at

inline void init_ground() {
  Mesh slab = GenMeshCube(GROUND_SIZE, GROUND_THICKNESS, GROUND_SIZE);
  GenMeshTangents(&slab); // the normal map needs a tangent frame
  ground = LoadModelFromMesh(slab);
  ground_normal_map = NormalMapUtils::make_noise_normal_map(1024, 0.9f);
  ground.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = Color{76, 78, 86, 255};
  ground.materials[0].maps[MATERIAL_MAP_NORMAL].texture = ground_normal_map;
}

// ---- tumbling triangle particles, one instanced draw, lit by the same shader ----
inline TriangleParticles::System particles;
inline SunGlints::System glints; // compute-shader sun glints over the particles

inline void unload_ground() {
  ground.materials[0].maps[MATERIAL_MAP_NORMAL].texture = Texture2D{0}; // owned below
  UnloadTexture(ground_normal_map);
  UnloadModel(ground);
}

#include "entity_groups.hpp"

/**
 * @brief Get the first hovered entity ref from the current frame.
 * @return The hovered entity ref, or nil if nothing is hovered.
 */
inline thing_ref get_hovered() {
  auto& hovered = ctx.frame_buffer.current().hovered;
  return hovered.empty() ? thing_ref::get_nil_ref() : hovered.front();
}

/**
 * @brief Depth of a world point along the camera's view axis (what the depth buffer stores),
 * not the Euclidean distance. Used to put the depth-of-field focus on an entity.
 */
inline float view_depth_of(Vector3 p) {
  Vector3 fwd = Vector3Normalize(Vector3Subtract(ctx.camera.target, ctx.camera.position));
  return Vector3DotProduct(Vector3Subtract(p, ctx.camera.position), fwd);
}

/**
 * @brief Draw a small screen-space card next to the mouse: entity name + its log_text.
 * raylib only (no ImGui) so it renders identically in the web build.
 */
inline void draw_hover_card(const Entity& e, Vector2 mouse) {
  const int font = 18, pad = 6, gap = 4;
  const char* head = e.model.valid() ? e.model.name : e._debug_name;
  const char* body = e.log_text.data();
  int w = std::max(MeasureText(head, font), MeasureText(body, font)) + pad * 2;
  int h = font * 2 + gap + pad * 2;
  float x = mouse.x + 16, y = mouse.y + 16;
  if (x + w > GetScreenWidth())
    x = mouse.x - 16 - w;
  if (y + h > GetScreenHeight())
    y = mouse.y - 16 - h;
  Rectangle r = {x, y, (float)w, (float)h};
  DrawRectangleRec(r, Fade(BLACK, 0.75f));
  DrawRectangleLinesEx(r, 1.0f, ctx.highlight_color);
  DrawText(head, (int)x + pad, (int)y + pad, font, ctx.highlight_color);
  DrawText(body, (int)x + pad, (int)y + pad + font + gap, font, RAYWHITE);
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
  // ent.parent_offset = {0, 1.5f, 0};
  thing_ref ref = ctx.entities.add(ent);
  ADD_TRAIT(ctx.entities[ref], IS_TEXT);
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
 * @return Combined scale * rotation * translateon matrix.
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
  bool no_model = (args.model_name == NO_MODEL_NAME);
  ModelInstance inst = no_model ? ModelInstance{} : ModelAPI::instance(args.model_name);

  if (!no_model && !inst.valid())
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

  DECLARE_TRAIT(IS_GRID_ALIGNED, "position snaps to integer x/z whenever it moves");
  if (HAS_TRAIT(e, IS_GRID_ALIGNED)) {
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

  DECLARE_TRAIT(CROSS_SLASH_HITBOX,
                "short-lived hitbox spawned by a cross slash; hits what it touches");
  if (HAS_TRAIT(a, CROSS_SLASH_HITBOX) && HAS_TRAIT(b, CROSS_SLASH_HITBOX)) {
    return;
  }

  // if a has push distance and b is pushable, set b's velocity from the action
  DECLARE_TRAIT(IS_PUSHABLE, "takes velocity from pushes and from moving entities that hit it");
  if (!is_unset(a.push_distance) && HAS_TRAIT(b, IS_PUSHABLE)) {
    Vector3 dir = Vector3Subtract(b.position, a.position);
    dir.y = 0;
    float len = Vector3Length(dir);
    if (len > 0.001f) {
      dir = Vector3Scale(dir, 1.0f / len);
      b.velocity = Vector3Scale(dir, a.push_distance);
    }
  }

  DECLARE_TRAIT(WSAD, "player: moves with WASD, can pick things up and cross slash");
  DECLARE_TRAIT(PICKUP, "collected (removed) when a WSAD entity touches it");
  if (HAS_TRAIT(a, WSAD) && HAS_TRAIT(b, PICKUP)) {
    GameConsoleAPI::print(std::string("Picked up ") + (b.model.valid() ? b.model.name : "???"));
    spawn_label(TextFormat("Picked up %s", b.model.valid() ? b.model.name : "???"));
    ctx.entities.remove(b.this_ref());
  }

  if (HAS_TRAIT(a, CROSS_SLASH_HITBOX)) {
    GameConsoleAPI::print(std::string("cross slash hit ") +
                          (b.model.valid() ? b.model.name : "???"));
    spawn_label(TextFormat("cross slash hit %s", b.model.valid() ? b.model.name : "???"),
                a.this_ref());
    TraceLog(LOG_INFO, "cross slash hit %s", b._debug_name);
  }

  // moving entity hits a stationary entity
  if (!is_unset(a.velocity) && is_unset(b.velocity)) {
    float speed = Vector3Length(a.velocity);
    spawn_label(TextFormat("hit %s (%.1f)", b.model.valid() ? b.model.name : "???", speed),
                a.this_ref());

    if (HAS_TRAIT(b, IS_PUSHABLE)) {
      // transfer velocity to b
      b.velocity = a.velocity;
    }
    // a stops
    a.velocity = make_unset<Vector3>();
  }
}

/** @brief Make `anchor` the head of group `gid` as an empty ring (next -> self). */
inline void group_set_head(GroupId gid, thing_ref anchor) {
  auto& a = ctx.entities[anchor];
  if (!a)
    return;
  ctx.group_heads[gid] = anchor;
  a.groups[gid].parent = anchor; // head owns itself
  a.groups[gid].next = anchor;   // empty ring points at the head
}

/** @brief Insert `child` just after the head of group `gid`. */
inline void group_push_front(GroupId gid, thing_ref child) {
  thing_ref head = ctx.group_heads[gid];
  auto& h = ctx.entities[head];
  auto& c = ctx.entities[child];
  if (!h || !c)
    return;
  c.groups[gid].parent = head;
  c.groups[gid].next = h.groups[gid].next; // old first
  h.groups[gid].next = child;              // child is new first
}

/** @brief Remove and return the first member of group `gid`, or nil if empty. */
inline thing_ref group_pop_front(GroupId gid) {
  thing_ref head = ctx.group_heads[gid];
  auto& h = ctx.entities[head];
  if (!h)
    return thing_ref::get_nil_ref();
  thing_ref first = h.groups[gid].next;
  if (first == head)
    return thing_ref::get_nil_ref(); // empty ring
  auto& f = ctx.entities[first];
  if (!f)
    return thing_ref::get_nil_ref();
  h.groups[gid].next = f.groups[gid].next; // unlink
  f.groups[gid].next = thing_ref::get_nil_ref();
  f.groups[gid].parent = thing_ref::get_nil_ref();
  return first;
}

/** @brief Comma-joined list of groups `e` belongs to, for the debug UI ("-" if none). */
inline std::string debug_entity_groups(Entity& e) {
  std::string out;
  for (int g = 0; g < num_groups; g++) {
    if (e.groups[g].parent == thing_ref::get_nil_ref())
      continue; // not in this group
    if (!out.empty())
      out += ",";
    out += group_names[g];
    if (ctx.group_heads[g] == e.this_ref())
      out += "(head)";
  }
  return out.empty() ? "-" : out;
}

/**
 * @brief Main per-frame update: sweeps expired entities, performs mouse ray picking,
 *        handles dragging, updates positions, ticks traits, detects collisions,
 *        processes cross-slash on mouse release, and renders all entities.
 */

// point-light shadow capture (filled in the render section of update(), shown by draw_imgui())
inline constexpr int SHADOW_SIZE = 1024;       // cube face size in px
inline constexpr int SHADOW_PREVIEW_TILE = 128; // preview sheet is 3x2 tiles
inline RenderTexture2D shadow_preview = {0};   // RGBA8 sheet of the six cube faces: +X -X +Y / -Y +Z -Z
inline bool shadow_drawn = false;              // the shadow pass ran this frame
inline bool shadow_ok = false;                 // the cube + FBO were created
inline float shadow_preview_range = 10.0f;     // world units mapped to white in the preview
inline bool point_light_debug = false;         // debug: everything the point light reaches is drawn solid red
inline float point_light_debug_bias = 0.015f;  // world units of slack in the shadow-cube compare
inline float point_light_radius = 8.0f;        // effective radius: nothing farther from the light is tinted
inline float point_light_tint = 1.0f;          // tint intensity: 0 = regular color, 1 = solid red
inline bool point_light_deferred = false;      // debug tint as one screen-space pass from the camera depth
inline bool point_light_marker = true;         // show the light's small sphere; while shown it can be dragged
inline bool point_light_shading = true;        // deferred shading: the point light is added onto the frame
inline Color point_light_color = {255, 236, 200, 255};
inline float point_light_intensity = 1.5f;
inline float point_light_spec_strength = 0.25f;
inline float point_light_spec_power = 32.0f;
inline Texture2D gbuffer_normal_tex = {0};     // G-buffer targets, for the ImGui thumbnails
inline Texture2D gbuffer_albedo_tex = {0};
inline bool gbuffer_drawn = false;             // the G-buffer was filled this frame

inline constexpr int MAX_POINT_LIGHTS = 6;     // one shadow cube each; the fog shader has six cube samplers
inline int shadow_light_count = 0;             // lights whose cube was filled this frame
inline int point_light_focus = 0;              // which light the cube preview and the debug tint show

// volumetric fog: ray-marched fBm noise lit by the point lights through their shadow cubes
inline bool vfog_enabled = true;
inline bool vfog_drawn = false;                // the fog pass ran this frame
inline int vfog_steps = 65;                    // march steps per pixel
inline int vfog_downscale = 2;                 // fog is marched at 1/N of the screen size, then scaled up
inline float vfog_density = 0.15f;             // extinction per world unit at full noise
inline float vfog_max_dist = 30.0f;            // march no farther than this (view depth, units)
inline float vfog_noise_scale = 0.45f;         // fBm frequency
inline float vfog_noise_amount = 0.60f;        // 0 = even fog, 1 = full clumps
inline float vfog_wind = 0.25f;                // drift speed of the noise
inline float vfog_top = 2.5f;                  // world y where the fog ends
inline float vfog_fade = 2.0f;                 // units over which it thins out below the top
inline Color vfog_ambient = {110, 120, 140, 255};
inline float vfog_ambient_intensity = 0.15f;
inline float vfog_scatter = 3.0f;              // gain on point-light in-scattering

/// A point light entity: small draggable marker sphere, POINT_LIGHT + BLOOM (drawn unlit in the
/// light color; a light is not shaded by the point lights).
inline thing_ref spawn_point_light(Vector3 pos) {
  ModelAPI::load("point_light", GenMeshSphere(0.12f, 12, 12));
  thing_ref light = spawn({.model_name = "point_light", .pos = pos, .debug_name = "point_light"});
  if (auto& e = ctx.entities[light]) {
    e.flags.is_collidable = 0;
    e.bloom_color = point_light_color;
    ADD_TRAIT(e, POINT_LIGHT);
    ADD_TRAIT(e, BLOOM);
  }
  return light;
}

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
      // slide on the horizontal plane at the entity's own height. Skip when the mouse ray does
      // not reach that plane in front of the camera (ray going the other way, or nearly level
      // with it): otherwise a floating entity such as the point light jumps to the camera or far away.
      if (fabsf(frame.mouse_ray.direction.y) > 0.02f) {
        float t = (e.position.y - frame.mouse_ray.position.y) / frame.mouse_ray.direction.y;
        if (t > 0.0f && t < 200.0f) {
          Vector3 point =
              Vector3Add(frame.mouse_ray.position, Vector3Scale(frame.mouse_ray.direction, t));
          entity_update_position(e.this_ref(), {point.x, e.position.y, point.z});
        }
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
  // NOTE: Im pretty sure that I decited against entity wise uptdates and that If I removed this
  // nothing would change

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

    for (size_t i = 0; i < collidables.size(); i++) {
      auto& a = ctx.entities[collidables[i]];
      if (!a)
        continue;
      for (size_t j = i + 1; j < collidables.size(); j++) {
        auto& b = ctx.entities[collidables[j]];
        if (!b)
          continue;
        if (!collides(a, b))
          continue;
        // I have both directions of the collision pair
        // becuase I dont handle symmetric collisions
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
      if (!target || !HAS_TRAIT(target, WSAD))
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
        ADD_TRAIT(ent, CROSS_SLASH_HITBOX);
        ADD_TRAIT(ent, IS_HITBOX);
      }
      break;
    }
  }

  //
  // click to spawn 2d animation
  //

  if (!ImGui::GetIO().WantCaptureMouse && IsMouseButtonReleased(MOUSE_BUTTON_LEFT) &&
      ctx.click_2d_animation >= 0) {
    // raycast to y=0 ground plane
    if (frame.mouse_ray.direction.y !=
        0) { // TODO: I have code quality conernes here. Im pretty sure if I thought about it that
             // the order of checks is is causing undessary checks
      float t = -frame.mouse_ray.position.y / frame.mouse_ray.direction.y;
      if (t > 0) {
        Vector3 hit =
            Vector3Add(frame.mouse_ray.position, Vector3Scale(frame.mouse_ray.direction, t));
        auto ref = spawn({.model_name = NO_MODEL_NAME,
                          .pos = hit,
                          .scale = ctx.click_anim_scale,
                          .debug_name = "click_anim"});
        if (ref.kind !=
            ilist_kind::nil) { // the fact there is a explicit nil check here. is a bad omen
          auto& ent = ctx.entities[ref];
          ent.animation_id = ctx.click_2d_animation;
          ent.animation_speed = ctx.click_anim_speed;
          const auto* def = AnimationRegistry::get(ctx.click_2d_animation);
          if (def)
            ent.life_time =
                (def->frame_duration / ctx.click_anim_speed) * (float)def->frames.size();
          ADD_TRAIT(ent, IS_ANIMATION);
        }
      }
    }
  }

  //
  // seed deck sweep -- while the deck head is populated, walk the ring until
  // it comes back around to the head (members never point at nil)
  //

  if (auto& head = ctx.entities[ctx.group_heads[group_seed_deck]]) {
    thing_ref head_ref = head.this_ref();
    for (thing_ref cur = head.groups[group_seed_deck].next; cur != head_ref;) {
      auto& s = ctx.entities[cur];
      if (!s)
        break;
      // per-member turn logic goes here. Lua owns seed position -- do not write it
      cur = s.groups[group_seed_deck].next;
    }
  }

  //
  //  render
  //

  //
  // point-light shadow capture: every CASTS_SHADOW entity is drawn six more times from the first
  // POINT_LIGHT entity's position, into the faces of one cube map. Each texel stores
  // distance-to-light / SHADOW_FAR (1.0 = nothing there). Nothing samples the cube for shading
  // yet; the preview sheet below is only there to show that it was filled.
  //

  DECLARE_TRAIT(POINT_LIGHT, "visual: a point light at this entity's position; shadow casters are redrawn from it");
  DECLARE_TRAIT(CASTS_SHADOW, "visual: draw flag: drawn again into the shadow pass, so it casts a shadow");
  std::vector<thing_ref> point_lights;
  std::vector<thing_ref> shadow_casters;
  for (auto& e : ctx.entities) {
    if (HAS_TRAIT(e, POINT_LIGHT)) {
      point_lights.push_back(e.this_ref());
      e.bloom_color = point_light_color; // its marker sphere shows the light's color
      e.render.visible = point_light_marker; // hidden = not drawn and not pickable; still lights
      e.flags.is_draggable = point_light_marker;
      e.flags.is_highlightable = point_light_marker;
    }
    if (HAS_TRAIT(e, CASTS_SHADOW) && !HAS_TRAIT(e, IS_BILLBOARD) && e.render.visible && e.model.valid())
      shadow_casters.push_back(e.this_ref());
  }

  static const float SHADOW_NEAR = 0.1f, SHADOW_FAR = 25.0f;
  static unsigned int shadowFbo = 0, shadowDepth = 0;
  static unsigned int shadowCubes[MAX_POINT_LIGHTS] = {};      // one per light, created when first needed
  static Vector3 shadow_light_positions[MAX_POINT_LIGHTS] = {}; // where each cube was last filled from
  auto make_shadow_cube = []() { // raylib refuses an *empty* float cube map, so hand it zeros
    std::vector<float> zeros((size_t)6 * SHADOW_SIZE * SHADOW_SIZE, 0.0f);
    return rlLoadTextureCubemap(zeros.data(), SHADOW_SIZE, PIXELFORMAT_UNCOMPRESSED_R32, 1);
  };
  static Shader fogShader = {0};
  static RenderTexture2D fogTarget = {0}; // fog at reduced size (premultiplied rgb, a = 1 - transmittance)
  static struct {
    int cube[MAX_POINT_LIGHTS];
    int camPos, camForward, camRight, camUp, tanHalfFov, zFar, time, steps, density, maxDist, noiseScale,
        noiseAmount, wind, fogTop, fogFade, ambient, scatter, lightCount, lightPos, lightColor, radius, farPlane,
        bias;
  } locFog = {};
  static Material distMaterial = {0};
  static Material litDebugMaterial = {0}; // debug overlay: red where the point light reaches
  static int locDbgLightPos = -1, locDbgFar = -1, locDbgBias = -1, locDbgRadius = -1, locDbgTint = -1;
  static Shader previewShader = {0};
  // deferred passes: locations shared by the shading shader and the debug tint shader (-1 = unused)
  struct DeferredLocs {
    int cube, camPos, camForward, camRight, camUp, tanHalfFov, zNear, zFar, lightPos, farPlane, bias, radius;
    int gDepth, gNormal, gAlbedo, tint, lightColor, intensity, specStrength, specPower;
  };
  static Shader deferredShader = {0}; // debug tint from the camera depth, one full-screen pass
  static Shader shadeShader = {0};    // point-light shading from the camera depth + G-buffer
  static DeferredLocs locDef = {}, locShade = {};
  // G-buffer: ground + entities drawn once with two color outputs (world normal with the normal
  // map applied, surface color) and a depth texture that says which screen pixels belong to them
  static RenderTexture2D gbuffer = {0};
  static Shader gbufferShader = {0};
  static int locGbufUseNM = -1;
  static Texture2D white1x1 = {0};
  static int locLightPos = -1, locFar = -1, locCube = -1, locScale = -1;
  static bool shadow_tried = false;
  if (!shadow_tried) { // first use: cube map, FBO, distance material, preview sheet
    shadow_tried = true;
    unsigned int shadowCube = shadowCubes[0] = make_shadow_cube();
    shadowFbo = rlLoadFramebuffer();
    shadowDepth = rlLoadTextureDepth(SHADOW_SIZE, SHADOW_SIZE, true); // renderbuffer: z-test only
    rlEnableFramebuffer(shadowFbo);
    rlFramebufferAttach(shadowFbo, shadowCube, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_CUBEMAP_POSITIVE_X, 0);
    rlFramebufferAttach(shadowFbo, shadowDepth, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_RENDERBUFFER, 0);
    bool fbo_ok = shadowCube != 0 && rlFramebufferComplete(shadowFbo);
    rlDisableFramebuffer();

    Shader dist = ShaderPortable::load_memory(
        SHADOW_DIST_VS,
        SHADOW_DIST_FS);
    distMaterial = LoadMaterialDefault();
    distMaterial.shader = dist;
    locLightPos = GetShaderLocation(dist, "lightPos");
    locFar = GetShaderLocation(dist, "farPlane");

    // debug overlay: redrawn over the normal draw; keeps only the fragments the light reaches
    // (in range, facing the light, and not behind a shadow caster in the cube) and paints them red
    Shader lit_dbg = ShaderPortable::load_memory(
        POINT_LIGHT_DEBUG_VS,
        POINT_LIGHT_DEBUG_FS);
    litDebugMaterial = LoadMaterialDefault();
    litDebugMaterial.shader = lit_dbg;
    // DrawMesh binds maps[MATERIAL_MAP_CUBEMAP] as a cube map and points this sampler at it
    lit_dbg.locs[SHADER_LOC_MAP_CUBEMAP] = GetShaderLocation(lit_dbg, "shadowCube");
    locDbgLightPos = GetShaderLocation(lit_dbg, "lightPos");
    locDbgFar = GetShaderLocation(lit_dbg, "farPlane");
    locDbgBias = GetShaderLocation(lit_dbg, "bias");
    locDbgRadius = GetShaderLocation(lit_dbg, "radius");
    locDbgTint = GetShaderLocation(lit_dbg, "tint");

    auto deferred_locs = [](const Shader& sh) {
      auto L = [&](const char* n) { return GetShaderLocation(sh, n); };
      return DeferredLocs{L("shadowCube"), L("camPos"), L("camForward"), L("camRight"), L("camUp"),
                          L("tanHalfFov"), L("zNear"), L("zFar"), L("lightPos"), L("farPlane"), L("bias"),
                          L("radius"), L("gDepth"), L("gNormal"), L("gAlbedo"), L("tint"), L("lightColor"),
                          L("intensity"), L("specStrength"), L("specPower")};
    };
    deferredShader = ShaderPortable::load_memory(nullptr, POINT_LIGHT_DEFERRED_FS);
    locDef = deferred_locs(deferredShader);
    shadeShader = ShaderPortable::load_memory(nullptr, POINT_LIGHT_SHADE_FS);
    locShade = deferred_locs(shadeShader);

    fogShader = ShaderPortable::load_memory(nullptr, VOLUMETRIC_FOG_FS);
    {
      auto L = [&](const char* n) { return GetShaderLocation(fogShader, n); };
      for (int i = 0; i < MAX_POINT_LIGHTS; i++)
        locFog.cube[i] = L(TextFormat("shadowCube%d", i));
      locFog.camPos = L("camPos"), locFog.camForward = L("camForward"), locFog.camRight = L("camRight");
      locFog.camUp = L("camUp"), locFog.tanHalfFov = L("tanHalfFov"), locFog.zFar = L("zFar");
      locFog.time = L("time"), locFog.steps = L("steps"), locFog.density = L("density");
      locFog.maxDist = L("maxDist"), locFog.noiseScale = L("noiseScale"), locFog.noiseAmount = L("noiseAmount");
      locFog.wind = L("wind"), locFog.fogTop = L("fogTop"), locFog.fogFade = L("fogFade");
      locFog.ambient = L("ambient"), locFog.scatter = L("scatter"), locFog.lightCount = L("lightCount");
      locFog.lightPos = L("lightPos"), locFog.lightColor = L("lightColor"), locFog.radius = L("radius");
      locFog.farPlane = L("farPlane"), locFog.bias = L("bias");
    }

    int gw = GetScreenWidth(), gh = GetScreenHeight();
    gbuffer = RenderAPI::load_render_texture_depth_tex(gw, gh); // color 0 (normal) + depth texture
    unsigned int albedo_id = rlLoadTexture(nullptr, gw, gh, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, 1);
    rlFramebufferAttach(gbuffer.id, albedo_id, RL_ATTACHMENT_COLOR_CHANNEL1, RL_ATTACHMENT_TEXTURE2D, 0);
    rlEnableFramebuffer(gbuffer.id);
    rlActiveDrawBuffers(2); // stored with the framebuffer
    bool gbuffer_ok = rlFramebufferComplete(gbuffer.id);
    rlDisableFramebuffer();
    gbuffer_normal_tex = gbuffer.texture;
    gbuffer_albedo_tex = {albedo_id, gw, gh, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    gbufferShader = ShaderPortable::load_memory(GBUFFER_VS, GBUFFER_FS);
    locGbufUseNM = GetShaderLocation(gbufferShader, "useNormalMap");

    // preview: the whole 3x2 sheet is read through one samplerCube
    previewShader = ShaderPortable::load_memory(nullptr, SHADOW_PREVIEW_FS);
    locCube = GetShaderLocation(previewShader, "shadowCube");
    locScale = GetShaderLocation(previewShader, "scale");
    Image white = GenImageColor(1, 1, WHITE);
    white1x1 = LoadTextureFromImage(white); // only there to give the preview quad 0..1 UVs
    UnloadImage(white);
    shadow_preview = LoadRenderTexture(SHADOW_PREVIEW_TILE * 3, SHADOW_PREVIEW_TILE * 2);

    shadow_ok = fbo_ok && dist.id != rlGetShaderIdDefault() && previewShader.id != rlGetShaderIdDefault() &&
                lit_dbg.id != rlGetShaderIdDefault() && deferredShader.id != rlGetShaderIdDefault() &&
                shadeShader.id != rlGetShaderIdDefault() && gbufferShader.id != rlGetShaderIdDefault() &&
                gbuffer_ok && fogShader.id != rlGetShaderIdDefault();
    if (!shadow_ok)
      TraceLog(LOG_WARNING, "SHADOW: point-light shadow cube unavailable (fbo %d)", (int)fbo_ok);
  }

  shadow_drawn = false;
  shadow_light_count = 0;
  if (shadow_ok && !point_lights.empty()) {
    // standard cube-map face directions / ups, so a lookup is texture(cube, fragWorld - lightPos)
    static const Vector3 dirs[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    static const Vector3 ups[6] = {{0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0}};

    SetShaderValue(distMaterial.shader, locFar, &SHADOW_FAR, SHADER_UNIFORM_FLOAT);
    rlDrawRenderBatchActive();
    rlViewport(0, 0, SHADOW_SIZE, SHADOW_SIZE);
    rlEnableDepthTest();
    rlDisableColorBlend();
    rlClearColor(255, 255, 255, 255); // 1.0 = nothing there
    rlSetMatrixProjection(MatrixPerspective(90.0 * DEG2RAD, 1.0, SHADOW_NEAR, SHADOW_FAR));

    // the first MAX_POINT_LIGHTS lights each fill their own cube
    int light_n = (int)point_lights.size() < MAX_POINT_LIGHTS ? (int)point_lights.size() : MAX_POINT_LIGHTS;
    for (int li = 0; li < light_n; li++) {
      thing_ref light_ref = point_lights[li];
      Vector3 lp = ctx.entities[light_ref].position;
      shadow_light_positions[li] = lp;
      if (shadowCubes[li] == 0)
        shadowCubes[li] = make_shadow_cube();
      SetShaderValue(distMaterial.shader, locLightPos, &lp, SHADER_UNIFORM_VEC3);
      for (int i = 0; i < 6; i++) {
        rlFramebufferAttach(shadowFbo, shadowCubes[li], RL_ATTACHMENT_COLOR_CHANNEL0,
                            RL_ATTACHMENT_CUBEMAP_POSITIVE_X + i, 0);
        rlEnableFramebuffer(shadowFbo); // rlFramebufferAttach leaves framebuffer 0 bound
        rlClearScreenBuffers();
        rlSetMatrixModelview(MatrixLookAt(lp, Vector3Add(lp, dirs[i]), ups[i]));
        for (thing_ref ref : shadow_casters) {
          if (ref == light_ref)
            continue; // the light sits at the eye
          auto& e = ctx.entities[ref];
          Matrix t = entity_transform(e);
          for (int m = 0; m < e.model.model.meshCount; m++)
            DrawMesh(e.model.model.meshes[m], distMaterial, t);
        }
      }
    }

    rlEnableColorBlend();
    rlDisableDepthTest();
    rlDisableFramebuffer();
    rlClearColor(0, 0, 0, 0);
    shadow_drawn = true;
    shadow_light_count = light_n;
    if (point_light_focus >= light_n)
      point_light_focus = light_n - 1;
    if (point_light_focus < 0)
      point_light_focus = 0;
    unsigned int shadowCube = shadowCubes[point_light_focus]; // the preview and the debug tint show this one

    litDebugMaterial.maps[MATERIAL_MAP_CUBEMAP].texture.id = shadowCube;
    SetShaderValue(litDebugMaterial.shader, locDbgLightPos, &shadow_light_positions[point_light_focus],
                   SHADER_UNIFORM_VEC3);
    SetShaderValue(litDebugMaterial.shader, locDbgFar, &SHADOW_FAR, SHADER_UNIFORM_FLOAT);
    SetShaderValue(litDebugMaterial.shader, locDbgBias, &point_light_debug_bias, SHADER_UNIFORM_FLOAT);
    SetShaderValue(litDebugMaterial.shader, locDbgRadius, &point_light_radius, SHADER_UNIFORM_FLOAT);
    SetShaderValue(litDebugMaterial.shader, locDbgTint, &point_light_tint, SHADER_UNIFORM_FLOAT);

    // preview sheet for the ImGui panel
    float scale = SHADOW_FAR / shadow_preview_range;
    int unit = 1;
    BeginTextureMode(shadow_preview);
    ClearBackground(BLACK);
    BeginShaderMode(previewShader);
    SetShaderValue(previewShader, locScale, &scale, SHADER_UNIFORM_FLOAT);
    SetShaderValue(previewShader, locCube, &unit, SHADER_UNIFORM_INT);
    rlActiveTextureSlot(1);
    rlEnableTextureCubemap(shadowCube); // SetShaderValueTexture only binds 2D textures
    rlActiveTextureSlot(0);
    DrawTexturePro(white1x1, {0, 0, 1, 1},
                   {0, 0, (float)shadow_preview.texture.width, (float)shadow_preview.texture.height}, {0, 0},
                   0.0f, WHITE);
    EndShaderMode(); // flushes the quad while the cube is still bound to unit 1
    rlActiveTextureSlot(1);
    rlDisableTextureCubemap();
    rlActiveTextureSlot(0);
    // one bordered, labelled tile per cube face
    static const char* face_names[6] = {"+X", "-X", "+Y", "-Y", "+Z", "-Z"};
    for (int i = 0; i < 6; i++) {
      int tx = (i % 3) * SHADOW_PREVIEW_TILE, ty = (i / 3) * SHADOW_PREVIEW_TILE;
      DrawRectangleLinesEx({(float)tx, (float)ty, (float)SHADOW_PREVIEW_TILE, (float)SHADOW_PREVIEW_TILE}, 2.0f,
                           ORANGE);
      DrawText(face_names[i], tx + 7, ty + 6, 20, ORANGE);
    }
    EndTextureMode();
  }

  // G-buffer for the deferred point-light passes. Everything the light may shade is drawn once
  // with its own material (so textures and normal maps bind) and the G-buffer shader swapped in.
  // Not in it, so never point-lit: particles, BLOOM entities (they are lights), billboards.
  gbuffer_drawn = false;
  bool want_tint = point_light_debug && shadow_drawn && point_light_tint > 0.0f;
  bool want_shade = point_light_shading && shadow_drawn && point_light_intensity > 0.0f;
  if (want_shade || (want_tint && point_light_deferred)) {
    auto draw_gbuffer = [&](const Model& model, Matrix t) {
      for (int m = 0; m < model.meshCount; m++) {
        Material mat = model.materials[model.meshMaterial[m]];
        mat.shader = gbufferShader;
        int use_nm = mat.maps[MATERIAL_MAP_NORMAL].texture.id > 0 ? 1 : 0;
        SetShaderValue(gbufferShader, locGbufUseNM, &use_nm, SHADER_UNIFORM_INT);
        DrawMesh(model.meshes[m], mat, t);
      }
    };
    BeginTextureMode(gbuffer);
    ClearBackground(BLANK);
    rlDisableColorBlend(); // both targets are written as-is
    BeginMode3D(ctx.camera);
    draw_gbuffer(ground, MatrixTranslate(0, GROUND_TOP_Y - GROUND_THICKNESS * 0.5f, 0));
    for (auto& e : ctx.entities) {
      if (!e.render.visible || !e.model.valid() || HAS_TRAIT(e, BLOOM) || HAS_TRAIT(e, IS_BILLBOARD))
        continue;
      draw_gbuffer(e.model.model, entity_transform(e));
    }
    EndMode3D();
    rlEnableColorBlend();
    EndTextureMode();
    gbuffer_drawn = true;
  }

  RenderAPI::layer_start(RenderLayer::Background, ctx.camera);
  ThreePointLight::begin_frame(ctx.camera);
  ThreePointLight::draw_model(ground,
                              MatrixTranslate(0, GROUND_TOP_Y - GROUND_THICKNESS * 0.5f, 0));
  // debug: the ground again with the point-light shader (see the entity overlay below). The
  // patches the shadow casters hide from the light stay untinted, which is what shows the shadows.
  bool lit_debug = want_tint && !point_light_deferred;      // forward: redraw the meshes
  bool deferred_debug = want_tint && point_light_deferred;  // deferred: one pass after rasterize()
  if (lit_debug) {
    rlDrawRenderBatchActive();
    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ZERO, RL_ONE, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    for (int m = 0; m < ground.meshCount; m++)
      DrawMesh(ground.meshes[m], litDebugMaterial,
               MatrixTranslate(0, GROUND_TOP_Y - GROUND_THICKNESS * 0.5f, 0));
    EndBlendMode();
  }

  RenderAPI::layer_start(RenderLayer::Highlight, ctx.camera);
  if (auto& h = ctx.entities[get_hovered()]) {
    if (h.render.visible && h.model.valid()) {
      draw_model_colored(h.model.name, entity_transform(h, h.scale * 1.1f), ctx.highlight_color);
    }
  }

  // Rendering entity models

  RenderAPI::layer_start(RenderLayer::Entities, ctx.camera);
  ThreePointLight::begin_frame(ctx.camera);
  particles.update(GetFrameTime());
  particles.draw();
  if (glints.show_sun)
    glints.draw_sun_marker();
  for (auto& e : ctx.entities) {

    if (!e.render.visible || !e.model.valid()) {
      continue;
    }

    DECLARE_TRAIT(THREE_POINT_LIT, "visual: drawn with the camera-relative key/fill/rim shader");
    DECLARE_TRAIT(BLOOM, "visual: has_bloom: its texture is a light source, drawn unlit in "
                         "bloom_color and bloomed (see bloom())");
    if (HAS_TRAIT(e, BLOOM))
      draw_model_colored(e.model.name, entity_transform(e),
                         e.bloom_color); // unlit: it IS the light
    else if (HAS_TRAIT(e, THREE_POINT_LIT))
      ThreePointLight::draw_model(e.model.model, entity_transform(e));
    else
      draw_model_normal(e.model.name, entity_transform(e));
    DECLARE_TRAIT(IS_HITBOX, "debug: draws its world bounding box in red");
    if (HAS_TRAIT(e, IS_HITBOX)) {
      DrawBoundingBox(compute_world_bbox(e), RED);
    }
  }

  // debug: the entities again with the point-light shader. It passes the depth test at equal
  // depth, keeps only the fragments the light reaches and blends red over the regular color.
  // The blend leaves the layer's alpha alone, so a faint tint does not turn the entity see-through.
  if (lit_debug) {
    rlDrawRenderBatchActive();
    rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ZERO, RL_ONE, RL_FUNC_ADD, RL_FUNC_ADD);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);
    for (auto& e : ctx.entities) {
      if (!e.render.visible || !e.model.valid() || HAS_TRAIT(e, POINT_LIGHT))
        continue;
      Matrix t = entity_transform(e);
      for (int m = 0; m < e.model.model.meshCount; m++)
        DrawMesh(e.model.model.meshes[m], litDebugMaterial, t);
    }
    EndBlendMode();
  }

  // bloom sources: whatever lands in this layer glows (it is not composited as color; the BLOOM
  // entities above and the Highlight copy are what stay on screen)
  RenderAPI::layer_start(RenderLayer::Bloom, ctx.camera);
  for (auto& e : ctx.entities) {
    if (!HAS_TRAIT(e, BLOOM) || !e.render.visible || !e.model.valid())
      continue;
    if (HAS_TRAIT(e, IS_BILLBOARD))
      draw_model_billboard(e.model.name, e.position, e.scale, e.bloom_color);
    else
      draw_model_colored(e.model.name, entity_transform(e), e.bloom_color);
  }
  if (auto& h =
          ctx.entities[get_hovered()]) { // demo: the hovered item glows in the highlight color
    if (h.render.visible && h.model.valid())
      draw_model_colored(h.model.name, entity_transform(h, h.scale * 1.1f), ctx.highlight_color);
  }

  RenderAPI::layer_start(RenderLayer::Focus, ctx.camera);
  if (auto& sel = ctx.entities[ctx.selected]) {
    if (sel.render.visible && sel.model.valid())
      draw_model_colored(sel.model.name, entity_transform(sel, sel.scale * 1.15f),
                         ctx.selection_color);
  }

  RenderAPI::layer_start(RenderLayer::UI_World, ctx.camera);
  for (auto& e : ctx.entities) {
    DECLARE_TRAIT(IS_BILLBOARD, "visual: drawn as a camera-facing billboard in the UI_World layer");
    if (HAS_TRAIT(e, IS_BILLBOARD)) {
      draw_model_billboard(e.model.name, e.position, e.scale, WHITE);
    }
  }

  bool want_fog = vfog_enabled && shadow_ok && vfog_density > 0.0f;
  RenderAPI::want_depth_min = gbuffer_drawn || want_fog;
  RenderAPI::rasterize();

  // screen-space passes over the finished frame. No mesh is redrawn here: each is one full-screen
  // quad that reads the camera's depth (RenderAPI::dof_depth, nearest over all layers) and works
  // from the world position that depth gives: point-light shading (added), the debug tint, then
  // the volumetric fog on top.
  vfog_drawn = false;
  if (RenderAPI::dof_depth_drawn && (gbuffer_drawn || want_fog)) {
    Vector3 fwd = Vector3Normalize(Vector3Subtract(ctx.camera.target, ctx.camera.position));
    Vector3 right = Vector3Normalize(Vector3CrossProduct(fwd, ctx.camera.up));
    Vector3 up = Vector3CrossProduct(right, fwd);
    float tan_y = tanf(ctx.camera.fovy * DEG2RAD * 0.5f);
    Vector2 tan_half = {tan_y * (float)GetScreenWidth() / (float)GetScreenHeight(), tan_y};
    float z_near = (float)RL_CULL_DISTANCE_NEAR, z_far = (float)RL_CULL_DISTANCE_FAR;
    Vector3 light_rgb = {point_light_color.r / 255.0f, point_light_color.g / 255.0f,
                         point_light_color.b / 255.0f};
    auto& depth = RenderAPI::dof_depth.texture;
    auto screen_pass = [&](const Shader& sh, const DeferredLocs& l, int li) {
      int unit = 5; // clear of the units the batch hands out to SetShaderValueTexture (1..4)
      BeginShaderMode(sh);
      SetShaderValue(sh, l.cube, &unit, SHADER_UNIFORM_INT);
      SetShaderValue(sh, l.camPos, &ctx.camera.position, SHADER_UNIFORM_VEC3);
      SetShaderValue(sh, l.camForward, &fwd, SHADER_UNIFORM_VEC3);
      SetShaderValue(sh, l.camRight, &right, SHADER_UNIFORM_VEC3);
      SetShaderValue(sh, l.camUp, &up, SHADER_UNIFORM_VEC3);
      SetShaderValue(sh, l.tanHalfFov, &tan_half, SHADER_UNIFORM_VEC2);
      SetShaderValue(sh, l.zNear, &z_near, SHADER_UNIFORM_FLOAT);
      SetShaderValue(sh, l.zFar, &z_far, SHADER_UNIFORM_FLOAT);
      SetShaderValue(sh, l.lightPos, &shadow_light_positions[li], SHADER_UNIFORM_VEC3);
      SetShaderValue(sh, l.farPlane, &SHADOW_FAR, SHADER_UNIFORM_FLOAT);
      SetShaderValue(sh, l.bias, &point_light_debug_bias, SHADER_UNIFORM_FLOAT);
      SetShaderValue(sh, l.radius, &point_light_radius, SHADER_UNIFORM_FLOAT);
      SetShaderValueTexture(sh, l.gDepth, gbuffer.depth);
      if (l.gNormal >= 0)
        SetShaderValueTexture(sh, l.gNormal, gbuffer_normal_tex);
      if (l.gAlbedo >= 0)
        SetShaderValueTexture(sh, l.gAlbedo, gbuffer_albedo_tex);
      if (l.tint >= 0)
        SetShaderValue(sh, l.tint, &point_light_tint, SHADER_UNIFORM_FLOAT);
      if (l.lightColor >= 0) {
        SetShaderValue(sh, l.lightColor, &light_rgb, SHADER_UNIFORM_VEC3);
        SetShaderValue(sh, l.intensity, &point_light_intensity, SHADER_UNIFORM_FLOAT);
        SetShaderValue(sh, l.specStrength, &point_light_spec_strength, SHADER_UNIFORM_FLOAT);
        SetShaderValue(sh, l.specPower, &point_light_spec_power, SHADER_UNIFORM_FLOAT);
      }
      rlActiveTextureSlot(unit);
      rlEnableTextureCubemap(shadowCubes[li]);
      rlActiveTextureSlot(0);
      DrawTextureRec(depth, {0, 0, (float)depth.width, (float)-depth.height}, {0, 0}, WHITE);
      EndShaderMode(); // flushes the quad while the cube is still bound
      rlActiveTextureSlot(unit);
      rlDisableTextureCubemap();
      rlActiveTextureSlot(0);
    };
    if (gbuffer_drawn && want_shade) {
      rlSetBlendFactors(RL_ONE, RL_ONE, RL_FUNC_ADD); // light adds onto the frame
      BeginBlendMode(BLEND_CUSTOM);
      for (int li = 0; li < shadow_light_count; li++) // one pass per light, each with its own cube
        screen_pass(shadeShader, locShade, li);
      EndBlendMode();
    }
    if (gbuffer_drawn && deferred_debug)
      screen_pass(deferredShader, locDef, point_light_focus);

    // volumetric fog, after the point lighting: march each pixel's ray through fBm noise up to the
    // scene depth; every step is lit by the point lights through their shadow cubes, so the
    // shadow casters cut dark shafts through the fog.
    if (want_fog) {
      float t = (float)GetTime();
      Vector3 amb = {vfog_ambient.r / 255.0f * vfog_ambient_intensity, vfog_ambient.g / 255.0f * vfog_ambient_intensity,
                     vfog_ambient.b / 255.0f * vfog_ambient_intensity};
      Vector3 lcol = Vector3Scale(light_rgb, point_light_shading ? point_light_intensity : 0.0f);
      const Shader& sh = fogShader;
      int div = vfog_downscale < 1 ? 1 : (vfog_downscale > 4 ? 4 : vfog_downscale);
      bool small = div > 1;
      int fw = GetScreenWidth() / div, fh = GetScreenHeight() / div;
      if (small) {
        if (fogTarget.id == 0 || fogTarget.texture.width != fw || fogTarget.texture.height != fh) {
          if (fogTarget.id)
            UnloadRenderTexture(fogTarget);
          fogTarget = RenderAPI::load_render_texture_ex(fw, fh, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, false);
          SetTextureFilter(fogTarget.texture, TEXTURE_FILTER_BILINEAR); // smooth scale-up
        }
        BeginTextureMode(fogTarget);
        ClearBackground(BLANK);
      }
      BeginShaderMode(sh);
      SetShaderValue(sh, locFog.camPos, &ctx.camera.position, SHADER_UNIFORM_VEC3);
      SetShaderValue(sh, locFog.camForward, &fwd, SHADER_UNIFORM_VEC3);
      SetShaderValue(sh, locFog.camRight, &right, SHADER_UNIFORM_VEC3);
      SetShaderValue(sh, locFog.camUp, &up, SHADER_UNIFORM_VEC3);
      SetShaderValue(sh, locFog.tanHalfFov, &tan_half, SHADER_UNIFORM_VEC2);
      SetShaderValue(sh, locFog.zFar, &z_far, SHADER_UNIFORM_FLOAT);
      SetShaderValue(sh, locFog.time, &t, SHADER_UNIFORM_FLOAT);
      SetShaderValue(sh, locFog.steps, &vfog_steps, SHADER_UNIFORM_INT);
      SetShaderValue(sh, locFog.density, &vfog_density, SHADER_UNIFORM_FLOAT);
      SetShaderValue(sh, locFog.maxDist, &vfog_max_dist, SHADER_UNIFORM_FLOAT);
      SetShaderValue(sh, locFog.noiseScale, &vfog_noise_scale, SHADER_UNIFORM_FLOAT);
      SetShaderValue(sh, locFog.noiseAmount, &vfog_noise_amount, SHADER_UNIFORM_FLOAT);
      SetShaderValue(sh, locFog.wind, &vfog_wind, SHADER_UNIFORM_FLOAT);
      SetShaderValue(sh, locFog.fogTop, &vfog_top, SHADER_UNIFORM_FLOAT);
      SetShaderValue(sh, locFog.fogFade, &vfog_fade, SHADER_UNIFORM_FLOAT);
      SetShaderValue(sh, locFog.ambient, &amb, SHADER_UNIFORM_VEC3);
      SetShaderValue(sh, locFog.scatter, &vfog_scatter, SHADER_UNIFORM_FLOAT);
      SetShaderValue(sh, locFog.lightCount, &shadow_light_count, SHADER_UNIFORM_INT);
      SetShaderValueV(sh, locFog.lightPos, shadow_light_positions, SHADER_UNIFORM_VEC3, MAX_POINT_LIGHTS);
      SetShaderValue(sh, locFog.lightColor, &lcol, SHADER_UNIFORM_VEC3);
      SetShaderValue(sh, locFog.radius, &point_light_radius, SHADER_UNIFORM_FLOAT);
      SetShaderValue(sh, locFog.farPlane, &SHADOW_FAR, SHADER_UNIFORM_FLOAT);
      SetShaderValue(sh, locFog.bias, &point_light_debug_bias, SHADER_UNIFORM_FLOAT);
      // six cube samplers on units 5..10. A light slot with no cube yet gets cube 0 so every
      // sampler has a complete texture; the shader never reads past lightCount.
      for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
        int unit = 5 + i;
        SetShaderValue(sh, locFog.cube[i], &unit, SHADER_UNIFORM_INT);
        rlActiveTextureSlot(unit);
        rlEnableTextureCubemap(shadowCubes[i] ? shadowCubes[i] : shadowCubes[0]);
      }
      rlActiveTextureSlot(0);
      // the march is the expensive part (steps x noise x lights per pixel), so it runs at 1/N of
      // the screen size into fogTarget and is scaled up afterwards; N = 1 draws straight to the frame
      if (small)
        rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD); // store the result as-is
      else
        rlSetBlendFactors(RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD); // premultiplied: fog + frame * transmittance
      BeginBlendMode(BLEND_CUSTOM);
      DrawTexturePro(depth, {0, 0, (float)depth.width, (float)-depth.height}, {0, 0, (float)fw, (float)fh},
                     {0, 0}, 0.0f, WHITE);
      EndBlendMode(); // flushes the quad while the cubes are still bound
      EndShaderMode();
      for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
        rlActiveTextureSlot(5 + i);
        rlDisableTextureCubemap();
      }
      rlActiveTextureSlot(0);
      if (small) {
        EndTextureMode();
        rlSetBlendFactors(RL_ONE, RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD);
        BeginBlendMode(BLEND_CUSTOM);
        DrawTexturePro(fogTarget.texture, {0, 0, (float)fw, (float)-fh},
                       {0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()}, {0, 0}, 0.0f, WHITE);
        EndBlendMode();
      }
      vfog_drawn = true;
    }
  }

  //
  // draw animated texture entities
  //

  for (auto& e : ctx.entities) {
    DECLARE_TRAIT(IS_ANIMATION,
                  "visual: draws animation_id as a screen-space frame sequence; no model");
    if (!HAS_TRAIT(e, IS_ANIMATION) || e.animation_id < 0)
      continue;
    float elapsed = ((float)GetTime() - e.animation_start_time) * e.animation_speed;

    DECLARE_TRAIT(IS_LOOPING,
                  "the IS_ANIMATION frame sequence wraps instead of holding the last frame");
    bool looping = HAS_TRAIT(e, IS_LOOPING);

    Texture2D* tex = AnimationRegistry::get_frame(e.animation_id, elapsed, looping);
    if (!tex)
      continue;
    Vector2 screen = GetWorldToScreen(e.position, ctx.camera);
    float draw_size = e.scale * 64.0f; // scale factor to screen pixels
    Rectangle src = {0, 0, (float)tex->width, (float)tex->height};
    Rectangle dst = {screen.x - draw_size * 0.5f, screen.y - draw_size * 0.5f, draw_size,
                     draw_size};
    DrawTexturePro(*tex, src, dst, {0, 0}, 0.0f, WHITE);
  }

  for (auto& e : ctx.entities) {
    DECLARE_TRAIT(IS_TEXT, "visual: fading screen-space label from log_text (spawn_label)");
    if (!HAS_TRAIT(e, IS_TEXT))
      continue;
    Vector2 screen = GetWorldToScreen(e.position, ctx.camera);
    float alpha = Clamp(e.life_time / ctx.log_layout.fade_time_sec, 0.0f, 1.0f);
    DrawText(e.log_text.data(), (int)screen.x, (int)screen.y, 20, RED);
  }

  // sun glints: compute shader (or CPU fallback) finds particles mirroring the sun at the
  // camera and hands back screen positions; one uniform additive sprite is drawn at each.
  if (glints.enabled) {
    glints.compute(particles.transforms, ctx.camera, GetScreenWidth(), GetScreenHeight());
    glints.draw_2d();
  }

  // hover card: a trait that also reads entity data (log_text), not just a bit
  DECLARE_TRAIT(HOVER_CARD, "shows this entity's log_text in a card at the mouse while hovered");
  if (auto& h = ctx.entities[get_hovered()]) {
    if (HAS_TRAIT(h, HOVER_CARD))
      draw_hover_card(h, frame.mouse);
  }
}

/**
 * @brief Draw the ImGui debug panels: scene inspector, entity list,
 *        selected entity properties, trait registry, and game console.
 */
inline bool show_lua_history = false; // toggled from the Scene panel's dev tools

inline void draw_imgui() {
  rlImGuiBegin();
  if (ImGui::Begin("Scene")) {

    //
    // scene counts header
    //

    ImGui::Text("Entities: %zu", entity_count());
    ImGui::Text("Models: %zu", ModelAPI::count());
    ImGui::Separator();

    //
    // dev tools
    //

    if (ImGui::Button(show_lua_history ? "Hide Lua history" : "Show Lua history"))
      show_lua_history = !show_lua_history;
    ImGui::SameLine();
    if (ImGui::Button(GameConsoleAPI::visible() ? "Hide console" : "Show console (~)"))
      GameConsoleAPI::toggle_visible();
    if (RenderAPI::depth_available()) {
      ImGui::Checkbox("Depth view", &RenderAPI::depth_view);
      if (RenderAPI::depth_view) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160);
        ImGui::SliderFloat("far (white)", &RenderAPI::depth_vis_far, 2.0f, 200.0f, "%.0f");
      }
    } else {
      ImGui::TextDisabled("Depth view: not available (needs desktop depth textures)");
    }
    if (RenderAPI::dof_available()) {
      ImGui::Checkbox("Depth blur", &RenderAPI::dof_enabled);
      if (RenderAPI::dof_enabled) {
        ImGui::SameLine();
        ImGui::Checkbox("show radius", &RenderAPI::dof_show_radius);
        ImGui::SameLine();
        if (ImGui::Button("Focus selected")) {
          if (auto& sel = ctx.entities[ctx.selected])
            RenderAPI::dof_focus = view_depth_of(sel.position);
        }
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("focus (units)", &RenderAPI::dof_focus, 0.5f, 100.0f, "%.1f");
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("strength (px/unit)", &RenderAPI::dof_strength, 0.0f, 20.0f, "%.1f");
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("max radius (px)", &RenderAPI::dof_max_radius, 1.0f, 32.0f, "%.0f");
      }
    }
    if (RenderAPI::fog_available()) {
      ImGui::Checkbox("Fog", &RenderAPI::fog_enabled);
      if (RenderAPI::fog_enabled) {
        ImGui::SameLine();
        ImGui::Checkbox("tint sky", &RenderAPI::fog_sky);
        float fc[3] = {RenderAPI::fog_color.r / 255.0f, RenderAPI::fog_color.g / 255.0f,
                       RenderAPI::fog_color.b / 255.0f};
        ImGui::SetNextItemWidth(200);
        if (ImGui::ColorEdit3("fog color", fc))
          RenderAPI::fog_color = {(unsigned char)(fc[0] * 255.0f), (unsigned char)(fc[1] * 255.0f),
                                  (unsigned char)(fc[2] * 255.0f), 255};
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("fog start (units)", &RenderAPI::fog_start, 0.0f, 100.0f, "%.1f");
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("fog density", &RenderAPI::fog_density, 0.0f, 0.5f, "%.3f");
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("fog max", &RenderAPI::fog_max, 0.0f, 1.0f, "%.2f");
      }
    }
    if (RenderAPI::bloom_available()) {
      ImGui::Checkbox("Bloom", &RenderAPI::bloom_enabled);
      if (RenderAPI::bloom_enabled) {
        ImGui::SameLine();
        ImGui::Checkbox("show source", &RenderAPI::bloom_show_source);
        ImGui::SameLine();
        ImGui::TextDisabled("(hovered item + entities with the bloom trait)");
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("bloom intensity", &RenderAPI::bloom_intensity, 0.0f, 4.0f, "%.2f");
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("bloom radius", &RenderAPI::bloom_radius, 0.5f, 4.0f, "%.1f");
        ImGui::SetNextItemWidth(200);
        ImGui::SliderInt("blur passes", &RenderAPI::bloom_passes, 1, 6);
      }
    }
    ImGui::Separator();

    //
    // point light: deferred shading from the first POINT_LIGHT entity, plus the debug tint
    //

    if (ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen)) {
      if (!shadow_ok) {
        ImGui::TextDisabled("unavailable");
      } else if (!shadow_drawn) {
        ImGui::TextDisabled("no point light (add the point_light trait to an entity)");
      } else {
        int light_i = 0;
        for (auto& e : ctx.entities) { // the first MAX_POINT_LIGHTS point lights are the ones in use
          if (!HAS_TRAIT(e, POINT_LIGHT))
            continue;
          ImGui::PushID(light_i);
          ImGui::SetNextItemWidth(200);
          ImGui::DragFloat3(TextFormat("light %d position%s", light_i, light_i >= MAX_POINT_LIGHTS ? " (unused)" : ""),
                            &e.position.x, 0.05f);
          ImGui::PopID();
          light_i++;
        }
        if (light_i < MAX_POINT_LIGHTS) {
          if (ImGui::Button(TextFormat("add point light (%d / %d)", light_i, MAX_POINT_LIGHTS))) {
            float ang = (float)light_i * 2.4f; // spread new lights around the middle
            spawn_point_light({cosf(ang) * 3.0f, 2.0f, sinf(ang) * 3.0f});
          }
        } else {
          ImGui::TextDisabled("%d / %d point lights", light_i, MAX_POINT_LIGHTS);
        }
        if (shadow_light_count > 1) {
          ImGui::SetNextItemWidth(200);
          ImGui::SliderInt("light shown in cube preview / debug tint", &point_light_focus, 0, shadow_light_count - 1);
        }
        ImGui::Checkbox("show light marker (viewable + draggable)", &point_light_marker);
        ImGui::Checkbox("shading (deferred)", &point_light_shading);
        float lc[3] = {point_light_color.r / 255.0f, point_light_color.g / 255.0f, point_light_color.b / 255.0f};
        ImGui::SetNextItemWidth(200);
        if (ImGui::ColorEdit3("light color", lc))
          point_light_color = {(unsigned char)(lc[0] * 255.0f), (unsigned char)(lc[1] * 255.0f),
                               (unsigned char)(lc[2] * 255.0f), 255};
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("light intensity", &point_light_intensity, 0.0f, 6.0f, "%.2f");
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("effective radius (units)", &point_light_radius, 0.5f, 25.0f, "%.1f");
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("shadow bias (units)", &point_light_debug_bias, 0.0f, 0.2f, "%.3f");
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("highlight strength", &point_light_spec_strength, 0.0f, 2.0f, "%.2f");
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("highlight tightness", &point_light_spec_power, 2.0f, 128.0f, "%.0f");
        ImGui::Checkbox("debug: tint point-lit red", &point_light_debug);
        if (point_light_debug) {
          ImGui::SameLine();
          ImGui::Checkbox("deferred (from camera depth)", &point_light_deferred);
          ImGui::SetNextItemWidth(200);
          ImGui::SliderFloat("tint intensity", &point_light_tint, 0.0f, 1.0f, "%.2f");
        }
      }
    }

    //
    // volumetric fog: marched fBm noise, lit by the point lights through their shadow cubes
    //

    if (ImGui::CollapsingHeader("Volumetric Fog", ImGuiTreeNodeFlags_DefaultOpen)) {
      if (!shadow_ok) {
        ImGui::TextDisabled("unavailable");
      } else {
        ImGui::Checkbox("enabled", &vfog_enabled);
        ImGui::SameLine();
        ImGui::TextDisabled(vfog_drawn ? "(drawn, %d light(s))" : "(not drawn)", shadow_light_count);
        ImGui::SetNextItemWidth(200);
        ImGui::SliderInt("march steps", &vfog_steps, 8, 128);
        ImGui::SetNextItemWidth(200);
        ImGui::SliderInt("resolution divisor (GPU load)", &vfog_downscale, 1, 4);
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("density", &vfog_density, 0.0f, 0.6f, "%.3f");
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("light scattering", &vfog_scatter, 0.0f, 8.0f, "%.2f");
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("noise amount", &vfog_noise_amount, 0.0f, 1.0f, "%.2f");
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("noise scale", &vfog_noise_scale, 0.05f, 2.0f, "%.2f");
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("wind", &vfog_wind, 0.0f, 2.0f, "%.2f");
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("fog top (y)", &vfog_top, 0.0f, 12.0f, "%.1f");
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("fade below top (units)", &vfog_fade, 0.1f, 8.0f, "%.1f");
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("max distance (units)", &vfog_max_dist, 5.0f, 100.0f, "%.0f");
        float ac[3] = {vfog_ambient.r / 255.0f, vfog_ambient.g / 255.0f, vfog_ambient.b / 255.0f};
        ImGui::SetNextItemWidth(200);
        if (ImGui::ColorEdit3("ambient color", ac))
          vfog_ambient = {(unsigned char)(ac[0] * 255.0f), (unsigned char)(ac[1] * 255.0f),
                          (unsigned char)(ac[2] * 255.0f), 255};
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("ambient intensity", &vfog_ambient_intensity, 0.0f, 2.0f, "%.2f");
      }
    }

    //
    // render buffers: every off-screen target, and whether the last frame drew into it
    //

    if (ImGui::CollapsingHeader("Render Buffers", ImGuiTreeNodeFlags_DefaultOpen)) {
      if (!shadow_ok) {
        ImGui::TextDisabled("point_shadow cube: unavailable");
      } else if (!shadow_drawn) {
        ImGui::TextDisabled("point_shadow cube: no point light (add the point_light trait to an entity)");
      } else {
        ImGui::TextUnformatted("point_shadow cube (dark = near the light, white = nothing)");
        rlImGuiImageRenderTexture(&shadow_preview);
        ImGui::SetNextItemWidth(200);
        ImGui::SliderFloat("preview range (units)", &shadow_preview_range, 1.0f, 25.0f, "%.0f");
        if (gbuffer_drawn) {
          ImGui::TextUnformatted("G-buffer: normal (normal map applied) | surface color");
          int tw = 300, th = tw * gbuffer_normal_tex.height / gbuffer_normal_tex.width;
          Rectangle flip = {0, 0, (float)gbuffer_normal_tex.width, (float)-gbuffer_normal_tex.height};
          rlImGuiImageRect(&gbuffer_normal_tex, tw, th, flip);
          ImGui::SameLine();
          rlImGuiImageRect(&gbuffer_albedo_tex, tw, th, flip);
        }
      }
      auto buffers = RenderAPI::buffer_list();
      buffers.push_back({TextFormat("point_shadow x%d", shadow_light_count > 0 ? shadow_light_count : 1),
                         "one cube map (6 faces) per point light: distance from the light to the shadow casters",
                         shadow_ok ? 1u : 0u, SHADOW_SIZE, SHADOW_SIZE, "R32F cube x6", false, shadow_drawn});
      buffers.push_back({"gbuffer_normal", "deferred point light: world normal (normal map applied) + depth",
                         shadow_ok ? 1u : 0u, GetScreenWidth(), GetScreenHeight(), "RGBA8", true, gbuffer_drawn});
      buffers.push_back({"gbuffer_albedo", "deferred point light: surface color", shadow_ok ? 1u : 0u,
                         GetScreenWidth(), GetScreenHeight(), "RGBA8", false, gbuffer_drawn});
      int drawn_count = 0;
      for (auto& b : buffers)
        drawn_count += b.drawn ? 1 : 0;
      ImGui::Text("Drawn this frame: %d / %zu", drawn_count, buffers.size());
      if (ImGui::BeginTable("render_buffers", 5,
                            ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("#");
        ImGui::TableSetupColumn("buffer");
        ImGui::TableSetupColumn("drawn");
        ImGui::TableSetupColumn("size");
        ImGui::TableSetupColumn("format");
        ImGui::TableHeadersRow();
        for (int i = 0; i < (int)buffers.size(); i++) {
          auto& b = buffers[i];
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::Text("%d", i);
          ImGui::TableNextColumn();
          if (b.drawn)
            ImGui::TextUnformatted(b.name);
          else
            ImGui::TextDisabled("%s", b.name);
          if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s (fbo %u)", b.role, b.fbo);
          ImGui::TableNextColumn();
          if (b.fbo == 0)
            ImGui::TextDisabled("n/a");
          else if (b.drawn)
            ImGui::TextColored({0.4f, 1.0f, 0.4f, 1.0f}, "yes");
          else
            ImGui::TextDisabled("-");
          ImGui::TableNextColumn();
          if (b.fbo)
            ImGui::Text("%dx%d", b.width, b.height);
          ImGui::TableNextColumn();
          if (b.fbo)
            ImGui::Text("%s%s", b.format, b.depth_tex ? " + depth" : "");
        }
        ImGui::EndTable();
      }
    }

    //
    // model spawn list
    //

    if (ImGui::CollapsingHeader("Models", ImGuiTreeNodeFlags_DefaultOpen)) {
      for (const auto& name : ModelAPI::names()) {
        if (ImGui::Button(name.c_str()))
          spawn(SpawnArgs{.model_name = name});
        ImGui::SameLine();
        ImGui::Text("(click to spawn)");
      }
    }

    //
    // entity selectable list
    //

    if (ImGui::CollapsingHeader("Entities", ImGuiTreeNodeFlags_DefaultOpen)) {
      int idx = 0;
      for (auto& e : ctx.entities) {
        ImGui::PushID(idx);
        std::string traits_str = TraitAPI::debug_entity(e);
        std::string groups_str = debug_entity_groups(e);
        bool is_selected = (e.this_ref() == ctx.selected);
        if (ImGui::Selectable(TextFormat("%d: %s [%s] {%s}", idx,
                                         e.model.valid() ? e.model.name : "---", traits_str.c_str(),
                                         groups_str.c_str()),
                              is_selected)) {
          ctx.selected = e.this_ref();
        }
        ImGui::PopID();
        idx++;
      }
    }

    //
    // selected entity properties
    //

    if (auto& sel = ctx.entities[ctx.selected]) {
      ImGui::Separator();
      ImGui::Text("Selected: %s", sel.model.valid() ? sel.model.name : "---");
      ImGui::Text("Traits: %s", TraitAPI::debug_entity(sel).c_str());
      ImGui::Text("Groups: %s", debug_entity_groups(sel).c_str());
      ImGui::DragFloat3("Position", &sel.position.x, 0.1f);
      ImGui::DragFloat3("Rotation", &sel.rotation.x, 0.01f);
      ImGui::DragFloat("Scale", &sel.scale, 0.1f, 0.1f, 10.0f);
      ImGui::Separator();
      bool visible = sel.render.visible;
      if (ImGui::Checkbox("Visible", &visible))
        sel.render.visible = visible;
      ImGui::InputText("log_text", sel.log_text.data(), sel.log_text.size());
      if (HAS_TRAIT(sel, BLOOM)) {
        float bc[3] = {sel.bloom_color.r / 255.0f, sel.bloom_color.g / 255.0f,
                       sel.bloom_color.b / 255.0f};
        ImGui::SetNextItemWidth(200);
        if (ImGui::ColorEdit3("bloom color", bc))
          sel.bloom_color = {(unsigned char)(bc[0] * 255.0f), (unsigned char)(bc[1] * 255.0f),
                             (unsigned char)(bc[2] * 255.0f), 255};
      }
      if (ImGui::TreeNodeEx("Traits", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (const auto& t : TRAIT_INFO) {
          bool on = TraitAPI::has(sel, t.id);
          if (ImGui::Checkbox(t.name, &on)) {
            if (on)
              TraitAPI::apply(sel, t.id);
            else
              TraitAPI::remove(sel, t.id);
          }
          if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s\n%s:%d", t.doc, t.file, t.line);
        }
        ImGui::TreePop();
      }
      ImGui::Separator();
      if (ImGui::Button("Delete")) {
        ctx.entities.remove(ctx.selected);
        ctx.selected = thing_ref::get_nil_ref();
      }
    }

    //
    // three-point lighting (global scheme shared by every entity with the trait)
    //

    if (ImGui::CollapsingHeader("Particles")) {
      particles.draw_imgui();
    }

    if (ImGui::CollapsingHeader("Sun Glints", ImGuiTreeNodeFlags_DefaultOpen)) {
      glints.draw_imgui();
    }

    if (ImGui::CollapsingHeader("Three-Point Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
      ThreePointLight::draw_imgui();
      int lit_count = 0;
      for (auto& e : ctx.entities)
        if (HAS_TRAIT(e, THREE_POINT_LIT))
          lit_count++;
      ImGui::Text("Lit entities: %d / %zu", lit_count, entity_count());
      if (ImGui::Button("Apply to all")) {
        for (auto& e : ctx.entities)
          ADD_TRAIT(e, THREE_POINT_LIT);
      }
      ImGui::SameLine();
      if (ImGui::Button("Remove from all")) {
        for (auto& e : ctx.entities)
          RM_TRAIT(e, THREE_POINT_LIT);
      }
      int light_count = 0;
      for (auto& e : ctx.entities)
        if (HAS_TRAIT(e, POINT_LIGHT))
          light_count++;
      ImGui::Text("Point lights: %d (up to 6 are used)", light_count);
      int caster_count = 0;
      for (auto& e : ctx.entities)
        if (HAS_TRAIT(e, CASTS_SHADOW))
          caster_count++;
      ImGui::Text("Shadow casters: %d / %zu", caster_count, entity_count());
      if (ImGui::Button("All cast shadows")) {
        for (auto& e : ctx.entities)
          ADD_TRAIT(e, CASTS_SHADOW);
      }
      ImGui::SameLine();
      if (ImGui::Button("None cast shadows")) {
        for (auto& e : ctx.entities)
          RM_TRAIT(e, CASTS_SHADOW);
      }
    }

    //
    // trait registry view
    //

    if (ImGui::CollapsingHeader("Trait Registry")) {
      ImGui::TextUnformatted(TraitAPI::debug_registered().c_str());
    }

    //
    // click animation picker
    // NOTE: generated coments here are wrong, there is only one animation

    if (ImGui::CollapsingHeader("Click Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
      if (ctx.click_2d_animation >= 0) {
        const auto* def = AnimationRegistry::get(ctx.click_2d_animation);
        ImGui::Text("Active: %s", def ? def->name.c_str() : "???");
        if (ImGui::Button("Disable"))
          ctx.click_2d_animation = -1;
      } else {
        ImGui::Text("Active: none");
        // list registered animations to pick from
        for (int i = 0; i < (int)AnimationRegistry::animations.size(); i++) {
          if (ImGui::Button(AnimationRegistry::animations[i].name.c_str()))
            ctx.click_2d_animation = i;
          ImGui::SameLine();
          ImGui::Text("(%d frames)", (int)AnimationRegistry::animations[i].frames.size());
        }
      }
      ImGui::DragFloat("Speed", &ctx.click_anim_speed, 0.1f, 0.1f, 10.0f);
      ImGui::DragFloat("Size", &ctx.click_anim_scale, 0.1f, 0.1f, 10.0f);
    }
  }
  ImGui::End();

  //
  // game console + frame end
  //

  GameConsoleAPI::draw_imgui();
  LuaImGui::draw_history(&show_lua_history);
  rlImGuiEnd();
}

// ============================================================================
// CRPG camera -- WASD pans along the ground, right-drag orbits the target
// (Baldur's Gate-style). Drop-in replacement for UpdateCamera(.., CAMERA_FREE).
// ============================================================================
inline void update_crpg_camera(Camera3D& cam) {
  float dt = GetFrameTime();

  // tweakables
  const float pan_speed = 10.0f;   // world units / second
  const float orbit_sens = 0.005f; // radians / pixel of mouse drag

  //
  // WASD pan: slide camera + target together across the ground plane
  //
  Vector3 forward = Vector3Subtract(cam.target, cam.position);
  forward.y = 0.0f;
  forward = Vector3Normalize(forward);
  Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, {0.0f, 1.0f, 0.0f}));

  Vector3 move = {0.0f, 0.0f, 0.0f};
  if (IsKeyDown(KEY_W))
    move = Vector3Add(move, forward);
  if (IsKeyDown(KEY_S))
    move = Vector3Subtract(move, forward);
  if (IsKeyDown(KEY_D))
    move = Vector3Add(move, right);
  if (IsKeyDown(KEY_A))
    move = Vector3Subtract(move, right);
  if (Vector3Length(move) > 0.0f) {
    move = Vector3Scale(Vector3Normalize(move), pan_speed * dt);
    cam.position = Vector3Add(cam.position, move);
    cam.target = Vector3Add(cam.target, move);
  }

  //
  // right-drag orbit: rotate camera around the target (yaw + clamped pitch)
  //
  if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && !ImGui::GetIO().WantCaptureMouse) {
    Vector2 d = GetMouseDelta();
    Vector3 offset = Vector3Subtract(cam.position, cam.target);
    float radius = Vector3Length(offset);

    float yaw = atan2f(offset.x, offset.z) - d.x * orbit_sens;
    float pitch = asinf(offset.y / radius) - d.y * orbit_sens;

    // clamp pitch: stay above ground, below straight-down (no pole flip)
    const float pitch_min = 0.05f; // ~3 deg above horizon
    const float pitch_max = 1.45f; // ~83 deg, near top-down
    if (pitch < pitch_min)
      pitch = pitch_min;
    if (pitch > pitch_max)
      pitch = pitch_max;

    offset.x = radius * cosf(pitch) * sinf(yaw);
    offset.y = radius * sinf(pitch);
    offset.z = radius * cosf(pitch) * cosf(yaw);
    cam.position = Vector3Add(cam.target, offset);
  }
}

} // namespace GameCtxAPI

#include "traits.inc"

#include "my_gltf_utils.hpp"

// ============================================================================
// Lua bindings (game-specific; the VM core and viewer are included at the top)
// ============================================================================
#include "../../mylibs/lua_remote_api.hpp"
#include "lua_bindings.inc"
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif
#include <cstdlib>

#include "cmds.inc"

// ============================================================================
// Main
// ============================================================================
/**
 * @brief Application entry point. Initializes window, ImGui, rendering, Lua,
 *        registers traits, runs the main loop, and cleans up on exit.
 */
/// One iteration of the game loop. Desktop calls it from a while loop; the web build
/// hands it to the browser via emscripten_set_main_loop.
static void frame() {
  LuaRemote::poll(); // run any Lua sent from an external console, on the main thread

  if (IsKeyPressed(KEY_GRAVE))
    GameConsoleAPI::toggle_visible();

  if (!GameConsoleAPI::visible())
    GameCtxAPI::update_crpg_camera(GameCtxAPI::ctx.camera);

  BeginDrawing();
  ClearBackground(DARKGRAY);
  GameCtxAPI::update();
  GameCtxAPI::draw_imgui();
  DrawFPS(10, 10);
  EntityDemoLua::take_pending_screenshot();
  EndDrawing();
}

int main() {
  InitWindow(1280, 720, "Entity Spawner");
  SetTargetFPS(60);
  rlImGuiSetup(true);
  RenderAPI::init();
  ThreePointLight::init();
  GameCtxAPI::init_ground();
  GameCtxAPI::particles.init();
  GameCtxAPI::glints.init();

  LuaAPI::set_log([](const std::string& m) { GameConsoleAPI::print(m); });
  LuaAPI::init();
  EntityDemoLua::register_lua_bindings();

  // in-game console: anything that is not a registered command is evaluated as Lua,
  // and "lua <code>" forces Lua evaluation explicitly
  GameConsoleAPI::fallback() = [](const std::string& code) { return LuaAPI::eval(code); };

  // remote Lua console on localhost (override port with ENTITY_DEMO_LUA_PORT, 0 disables)
  {
    int port = LuaRemote::DEFAULT_PORT;
    if (const char* env = std::getenv("ENTITY_DEMO_LUA_PORT"))
      port = std::atoi(env);
    if (port > 0)
      LuaRemote::start([](const std::string& code) { return LuaAPI::eval(code); },
                       [](const std::string& msg) { GameConsoleAPI::print(msg); }, port);
  }

  // Traits themselves are generated (gen/entity_traits.gen.hpp); only behaviour hooks are wired
  // here.
  TraitAPI::set_hooks(TraitId::WSAD, wsad_init, wsad_update);
  TraitAPI::set_hooks(TraitId::PICKUP, pickup_init, pickup_update);
  TraitAPI::set_hooks(TraitId::CROSS_SLASH_HITBOX, pickup_init, pickup_update);
  TraitAPI::set_hooks(TraitId::IS_ANIMATION, animation_init, animation_update);

  GameConsoleAPI::print("Press ~ for console.");

  // this should become an arg the to main loop,
  // then in each lua file I could just run the lua file

  LuaAPI::run_file("assets/setup.lua");

  // hard-coded point light above the middle of the world: fills the shadow cube and drives the
  // deferred shading (see the render section of update()). It has a small sphere so you can see
  // where it is, drawn unlit in the light's color (BLOOM: it is a light, so the point light does
  // not shade it), and it can be dragged with the mouse like any is_draggable entity.
  GameCtxAPI::spawn_point_light({0.0f, 0.35f, 0.0f});

#if defined(__EMSCRIPTEN__)
  // the browser owns the loop: it calls frame() once per animation frame and never
  // returns here, so the teardown below only runs on desktop
  emscripten_set_main_loop(frame, 0, 1);
#else
  while (!WindowShouldClose())
    frame();
#endif

  LuaRemote::stop();
  LuaAPI::shutdown();
  GameCtxAPI::clear_entities();
  AnimationRegistry::unload_all();
  GameCtxAPI::glints.unload();
  GameCtxAPI::particles.unload();
  GameCtxAPI::unload_ground();
  ThreePointLight::shutdown();
  RenderAPI::shutdown();
  ModelAPI::unload_all();
  rlImGuiShutdown();
  CloseWindow();
  return 0;
}
