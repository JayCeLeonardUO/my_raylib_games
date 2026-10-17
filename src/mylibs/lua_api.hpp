#pragma once

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include "animation_registry.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

// Forward declarations — the game must define these before including lua_api.hpp
// or link them via its own headers. LuaAPI calls into GameCtxAPI, ModelAPI, TraitAPI,
// and GameConsoleAPI which must be available at the point of use.

namespace LuaAPI {

inline lua_State* L = nullptr;

// ---- Lua C functions ----

// spawn(model_name, x, y, z, [scale]) -> entity index, or -1 on failure
static int l_spawn(lua_State* L) {
  const char* model = luaL_checkstring(L, 1);
  float x = (float)luaL_optnumber(L, 2, 0);
  float y = (float)luaL_optnumber(L, 3, 0);
  float z = (float)luaL_optnumber(L, 4, 0);
  float scale = (float)luaL_optnumber(L, 5, 1.0);

  auto ref = GameCtxAPI::spawn(
      GameCtxAPI::SpawnArgs{.model_name = std::string(model), .pos = {x, y, z}, .scale = scale});

  if (ref.kind == ilist_kind::nil) {
    lua_pushinteger(L, -1);
    return 1;
  }
  // find the positional index of the new entity (same counting scheme set_flag/trait_add use)
  int idx = 0;
  for (auto& e : GameCtxAPI::ctx.entities) {
    if (e.this_ref() == ref)
      break;
    idx++;
  }
  lua_pushinteger(L, idx);
  return 1;
}
// spawn_child(parent_index, model_name, x, y, z, [scale])
static int l_spawn_child(lua_State* L) {
  int parent_idx = (int)luaL_checkinteger(L, 1);
  const char* model = luaL_checkstring(L, 2);
  float x = (float)luaL_optnumber(L, 3, 0);
  float y = (float)luaL_optnumber(L, 4, 0);
  float z = (float)luaL_optnumber(L, 5, 0);
  float scale = (float)luaL_optnumber(L, 6, 1.0);

  thing_ref parent_ref = thing_ref::get_nil_ref();
  int i = 0;
  for (auto& e : GameCtxAPI::ctx.entities) {
    if (i == parent_idx) {
      parent_ref = e.this_ref();
      break;
    }
    i++;
  }

  if (parent_ref == thing_ref::get_nil_ref()) {
    GameConsoleAPI::print("lua: spawn_child: invalid parent index");
    lua_pushboolean(L, false);
    return 1;
  }

  auto ref = GameCtxAPI::spawn(GameCtxAPI::SpawnArgs{
      .model_name = std::string(model), .pos = {x, y, z}, .scale = scale, .spawner = parent_ref});
  lua_pushboolean(L, ref.kind != ilist_kind::nil);
  return 1;
}

// load_model(path, [name])
static int l_load_model(lua_State* L) {
  const char* path = luaL_checkstring(L, 1);
  const char* name = luaL_optstring(L, 2, nullptr);

  std::string sname;
  if (name) {
    sname = name;
  } else {
    // extract name from path
    std::string spath(path);
    size_t slash = spath.find_last_of('/');
    sname = (slash != std::string::npos) ? spath.substr(slash + 1) : spath;
    size_t dot = sname.find_last_of('.');
    if (dot != std::string::npos)
      sname = sname.substr(0, dot);
  }

  bool ok = ModelAPI::load(sname, std::string(path));
  if (!ok)
    GameConsoleAPI::print("lua: failed to load model: " + std::string(path));
  lua_pushboolean(L, ok);
  return 1;
}

// load_primitive(name, type, a, b, c)
// type: "cube", "sphere", "cylinder", "plane", "torus", "knot", "cone"
static int l_load_primitive(lua_State* L) {
  const char* name = luaL_checkstring(L, 1);
  const char* type = luaL_checkstring(L, 2);
  float a = (float)luaL_optnumber(L, 3, 1.0);
  float b = (float)luaL_optnumber(L, 4, 1.0);
  float c = (float)luaL_optnumber(L, 5, 1.0);

  Mesh mesh = {0};
  if (strcmp(type, "cube") == 0)
    mesh = GenMeshCube(a, b, c);
  else if (strcmp(type, "sphere") == 0)
    mesh = GenMeshSphere(a, (int)b, (int)c);
  else if (strcmp(type, "cylinder") == 0)
    mesh = GenMeshCylinder(a, b, (int)c);
  else if (strcmp(type, "plane") == 0)
    mesh = GenMeshPlane(a, b, (int)c, (int)c);
  else if (strcmp(type, "torus") == 0)
    mesh = GenMeshTorus(a, b, (int)c, (int)c);
  else if (strcmp(type, "knot") == 0)
    mesh = GenMeshKnot(a, b, (int)c, (int)c);
  else if (strcmp(type, "cone") == 0)
    mesh = GenMeshCone(a, b, (int)c);
  else {
    GameConsoleAPI::print("lua: unknown primitive type: " + std::string(type));
    lua_pushboolean(L, false);
    return 1;
  }

  bool ok = ModelAPI::load(std::string(name), mesh);
  lua_pushboolean(L, ok);
  return 1;
}

// register_trait(name)
static int l_register_trait(lua_State* L) {
  const char* name = luaL_checkstring(L, 1);
  int slot = TraitAPI::register_trait(name);
  if (slot < 0)
    GameConsoleAPI::print("lua: register_trait: trait table full (MAX_TRAITS), could not add " +
                          std::string(name));
  lua_pushinteger(L, slot);
  return 1;
}

// console_print(msg)
static int l_console_print(lua_State* L) {
  const char* msg = luaL_checkstring(L, 1);
  GameConsoleAPI::print(std::string(msg));
  return 0;
}

// trait_add(entity_index, trait_name)
static int l_trait_add(lua_State* L) {
  int idx = (int)luaL_checkinteger(L, 1);
  const char* trait = luaL_checkstring(L, 2);

  int i = 0;
  for (auto& e : GameCtxAPI::ctx.entities) {
    if (i == idx) {
      TraitAPI::apply(e, trait);
      lua_pushboolean(L, true);
      return 1;
    }
    i++;
  }
  lua_pushboolean(L, false);
  return 1;
}

// trait_rm(entity_index, trait_name)
static int l_trait_rm(lua_State* L) {
  int idx = (int)luaL_checkinteger(L, 1);
  const char* trait = luaL_checkstring(L, 2);

  int i = 0;
  for (auto& e : GameCtxAPI::ctx.entities) {
    if (i == idx) {
      TraitAPI::remove(e, trait);
      lua_pushboolean(L, true);
      return 1;
    }
    i++;
  }
  lua_pushboolean(L, false);
  return 1;
}

// entity_count()
static int l_entity_count(lua_State* L) {
  lua_pushinteger(L, (int)GameCtxAPI::entity_count());
  return 1;
}

// model_count()
static int l_model_count(lua_State* L) {
  lua_pushinteger(L, (int)ModelAPI::count());
  return 1;
}

// color(model_name, r, g, b)
static int l_color(lua_State* L) {
  const char* name = luaL_checkstring(L, 1);
  int r = (int)luaL_checkinteger(L, 2);
  int g = (int)luaL_checkinteger(L, 3);
  int b = (int)luaL_checkinteger(L, 4);

  Model* m = ModelAPI::get(std::string(name));
  if (!m) {
    GameConsoleAPI::print("lua: unknown model: " + std::string(name));
    lua_pushboolean(L, false);
    return 1;
  }
  m->materials[0].maps[MATERIAL_MAP_DIFFUSE].color = {(unsigned char)r, (unsigned char)g,
                                                      (unsigned char)b, 255};
  lua_pushboolean(L, true);
  return 1;
}

// set_flag(entity_index, flag_name, value)
static int l_set_flag(lua_State* L) {
  int idx = (int)luaL_checkinteger(L, 1); // is this not the id? what the fuck I mea index
  const char* flag = luaL_checkstring(L, 2);
  bool val = lua_toboolean(L, 3);

  int i = 0;
  for (auto& e : GameCtxAPI::ctx.entities) {
    if (i == idx) {
      if (strcmp(flag, "is_draggable") == 0)
        e.flags.is_draggable = val;
      else if (strcmp(flag, "is_collidable") == 0)
        e.flags.is_collidable = val;
      else if (strcmp(flag, "is_highlightable") == 0)
        e.flags.is_highlightable = val;
      else {
        GameConsoleAPI::print("lua: unknown flag: " + std::string(flag));
        lua_pushboolean(L, false);
        return 1;
      }
      lua_pushboolean(L, true);
      return 1;
    }
    i++;
  }
  lua_pushboolean(L, false);
  return 1;
}

// register_animation(name, dir_path, frame_duration)
static int l_register_animation(lua_State* L) {
  const char* name = luaL_checkstring(L, 1);
  const char* dir_path = luaL_checkstring(L, 2);
  float frame_duration = (float)luaL_checknumber(L, 3);
  int id =
      AnimationRegistry::register_anim(std::string(name), std::string(dir_path), frame_duration);
  if (id < 0)
    GameConsoleAPI::print("lua: register_animation: no frames found in " + std::string(dir_path));
  lua_pushinteger(L, id);
  return 1;
}

// spawn_animation(anim_name, x, y, z, [scale], [looping])
static int l_spawn_animation(lua_State* L) {
  const char* anim_name = luaL_checkstring(L, 1);
  float x = (float)luaL_optnumber(L, 2, 0);
  float y = (float)luaL_optnumber(L, 3, 0);
  float z = (float)luaL_optnumber(L, 4, 0);
  float scale = (float)luaL_optnumber(L, 5, 1.0);
  bool looping = lua_toboolean(L, 6);

  int anim_id = AnimationRegistry::find(std::string(anim_name));
  if (anim_id < 0) {
    GameConsoleAPI::print("lua: spawn_animation: unknown animation: " + std::string(anim_name));
    lua_pushboolean(L, false);
    return 1;
  }

  // animation entities use TRAIT_NO_MODEL — they draw via texture, not model
  auto ref = GameCtxAPI::spawn(GameCtxAPI::SpawnArgs{
      .model_name = TRAIT_NO_MODEL, .pos = {x, y, z}, .scale = scale, .debug_name = "animation"});
  if (ref.kind == ilist_kind::nil) {
    lua_pushboolean(L, false);
    return 1;
  }

  auto& ent = GameCtxAPI::ctx.entities[ref];
  ent.animation_id = anim_id;
  TraitAPI::apply(ent, TRAIT_IS_ANIMATION);

  if (looping)
    TraitAPI::apply(ent, TRAIT_IS_LOOPING);

  lua_pushboolean(L, true);
  return 1;
}

// set_click_animation(anim_name) — set a 2d animation to play on click, or nil to disable
static int l_set_click_animation(lua_State* L) {
  if (lua_isnil(L, 1) || lua_isnone(L, 1)) {
    GameCtxAPI::ctx.click_2d_animation = -1;
    lua_pushboolean(L, true);
    return 1;
  }
  const char* anim_name = luaL_checkstring(L, 1);
  int anim_id = AnimationRegistry::find(std::string(anim_name));
  if (anim_id < 0) {
    GameConsoleAPI::print("lua: set_click_animation: unknown animation: " + std::string(anim_name));
    lua_pushboolean(L, false);
    return 1;
  }
  GameCtxAPI::ctx.click_2d_animation = anim_id;
  lua_pushboolean(L, true);
  return 1;
}

// clear()
static int l_clear(lua_State* L) {
  (void)L;
  GameCtxAPI::clear_entities();
  return 0;
}

// ---- print capture ----
// Lua's global print is replaced so output lands in the game console and, while an
// eval() is in flight, in a capture buffer that is returned to the caller (in-game
// console or remote client).

inline std::string* capture_buf = nullptr;

static int l_print(lua_State* L) {
  int n = lua_gettop(L);
  std::string line;
  for (int i = 1; i <= n; i++) {
    size_t len = 0;
    const char* str = luaL_tolstring(L, i, &len); // pushes the string
    if (i > 1)
      line += "\t";
    line.append(str ? str : "nil", str ? len : 3);
    lua_pop(L, 1);
  }
  GameConsoleAPI::print(line);
  if (capture_buf) {
    if (!capture_buf->empty())
      *capture_buf += "\n";
    *capture_buf += line;
  }
  return 0;
}

// ---- lifecycle ----

// group bindings are defined below init() -- forward declare so they can register
static int l_init_group(lua_State* L);
static int l_add_to_group(lua_State* L);
static int l_help(lua_State* L);
static int l_traits(lua_State* L);
static int l_models(lua_State* L);
static int l_entities(lua_State* L);

// ---- binding table ----
// Single source of truth for what Lua can call. help() prints it, init() registers it.
struct Binding {
  const char* name;
  lua_CFunction fn;
  const char* sig;  // argument list as the user types it
  const char* doc;  // one line
};

inline const Binding BINDINGS[] = {
    // --- discovery ---
    {"help", l_help, "help([name])", "this list, or details for one function"},
    {"entities", l_entities, "entities()", "list live entities: index, model, position, traits"},
    {"traits", l_traits, "traits()", "list registered trait names"},
    {"models", l_models, "models()", "list loaded model names"},
    {"entity_count", l_entity_count, "entity_count()", "number of live entities"},
    {"model_count", l_model_count, "model_count()", "number of loaded models"},
    // --- models ---
    {"load_model", l_load_model, "load_model(path, [name])", "load a .glb/.obj; name defaults to the file stem"},
    {"load_primitive", l_load_primitive, "load_primitive(name, type, [a], [b], [c])",
     "type: cube sphere cylinder plane torus knot cone; a b c are dims/segments"},
    {"color", l_color, "color(model_name, r, g, b)", "tint a model's diffuse color (0-255)"},
    // --- entities ---
    {"spawn", l_spawn, "spawn(model_name, [x], [y], [z], [scale]) -> index",
     "spawn an entity; the returned index is what every other call takes"},
    {"spawn_child", l_spawn_child, "spawn_child(parent_index, model_name, [x], [y], [z], [scale])",
     "spawn attached to a parent entity"},
    {"clear", l_clear, "clear()", "remove all entities (indices reset)"},
    {"set_flag", l_set_flag, "set_flag(index, flag, bool)",
     "flags: is_draggable is_collidable is_highlightable"},
    // --- traits ---
    {"trait_add", l_trait_add, "trait_add(index, trait_name) -> bool",
     "attach a registered trait; false if the index is bad (unknown names are ignored)"},
    {"trait_rm", l_trait_rm, "trait_rm(index, trait_name) -> bool", "detach a trait"},
    {"register_trait", l_register_trait, "register_trait(name) -> slot",
     "add a new tag-only trait (no C++ behavior); -1 if the table is full"},
    // --- animation ---
    {"register_animation", l_register_animation, "register_animation(name, dir_path, frame_duration)",
     "register a frame-sequence animation from a directory of images"},
    {"spawn_animation", l_spawn_animation, "spawn_animation(anim_name, x, y, z, [scale], [looping])",
     "spawn a 2d animation entity at a position"},
    {"set_click_animation", l_set_click_animation, "set_click_animation(anim_name | nil)",
     "play this animation wherever the mouse clicks"},
    // --- groups ---
    {"init_group", l_init_group, "init_group(group_name, head_index)",
     "make an entity the head of a group (groups: seed_deck)"},
    {"add_to_group", l_add_to_group, "add_to_group(index, group_name)", "link an entity into a group"},
    // --- output ---
    {"print", l_print, "print(...)", "print to the game console and to the remote console"},
    {"console_print", l_console_print, "console_print(msg)", "print a single string to the game console"},
};

// help([name]) -> string
static int l_help(lua_State* L) {
  const char* want = luaL_optstring(L, 1, nullptr);
  std::string out;
  if (want) {
    for (const auto& b : BINDINGS) {
      if (strcmp(b.name, want) == 0) {
        out = std::string(b.sig) + "\n  " + b.doc;
        lua_pushstring(L, out.c_str());
        return 1;
      }
    }
    lua_pushstring(L, ("no such function: " + std::string(want)).c_str());
    return 1;
  }
  out += "Lua API (entity indices are 0-based spawn order; `entities()` shows them)\n";
  size_t w = 0;
  for (const auto& b : BINDINGS)
    w = std::max(w, strlen(b.sig));
  for (const auto& b : BINDINGS) {
    out += "  " + std::string(b.sig);
    out.append(w - strlen(b.sig) + 2, ' ');
    out += b.doc;
    out += "\n";
  }
  out += "Also: any registered console command (`help` alone lists them in-game), and plain Lua.\n";
  out += "Try: traits()  models()  entities()  help(\"spawn\")";
  lua_pushstring(L, out.c_str());
  return 1;
}

// traits() -> string
static int l_traits(lua_State* L) {
  std::string out = TraitAPI::debug_registered();
  if (!out.empty() && out.back() == '\n')
    out.pop_back();
  lua_pushstring(L, out.c_str());
  return 1;
}

// models() -> string
static int l_models(lua_State* L) {
  std::string out;
  for (const auto& n : ModelAPI::names()) {
    if (!out.empty())
      out += "\n";
    out += n;
  }
  lua_pushstring(L, out.empty() ? "(no models loaded)" : out.c_str());
  return 1;
}

// entities() -> string
static int l_entities(lua_State* L) {
  std::string out;
  int i = 0;
  for (auto& e : GameCtxAPI::ctx.entities) {
    char line[256];
    snprintf(line, sizeof(line), "[%d] %-16s (%.1f, %.1f, %.1f)", i,
             e.model.valid() ? e.model.name : e._debug_name, e.position.x, e.position.y,
             e.position.z);
    std::string tr = TraitAPI::debug_entity(e);
    if (!out.empty())
      out += "\n";
    out += line;
    if (!tr.empty())
      out += "  traits: " + tr;
    i++;
  }
  lua_pushstring(L, out.empty() ? "(no entities)" : out.c_str());
  return 1;
}

inline void init() {
  L = luaL_newstate();
  luaL_openlibs(L);

  for (const auto& b : BINDINGS)
    lua_register(L, b.name, b.fn);
}

inline void shutdown() {
  if (L) {
    lua_close(L);
    L = nullptr;
  }
}

inline bool run_file(const std::string& path) {
  if (!L) {
    GameConsoleAPI::print("lua: not initialized");
    return false;
  }
  int result = luaL_dofile(L, path.c_str());
  if (result != LUA_OK) {
    const char* err = lua_tostring(L, -1);
    GameConsoleAPI::print("lua error: " + std::string(err ? err : "unknown"));
    lua_pop(L, 1);
    return false;
  }
  return true;
}

inline bool run_string(const std::string& code) {
  if (!L) {
    GameConsoleAPI::print("lua: not initialized");
    return false;
  }
  int result = luaL_dostring(L, code.c_str());
  if (result != LUA_OK) {
    const char* err = lua_tostring(L, -1);
    GameConsoleAPI::print("lua error: " + std::string(err ? err : "unknown"));
    lua_pop(L, 1);
    return false;
  }
  return true;
}

// Evaluate a chunk REPL-style and return everything it produced as text:
// captured print() output, then the chunk's return values (one per line), or
// "lua error: ..." on failure. Like lua.c, "return <code>" is tried first so a
// bare expression such as `entity_count()` or `1+1` shows its value.
inline std::string eval(const std::string& code) {
  if (!L)
    return "lua: not initialized";

  std::string out;
  std::string* prev_capture = capture_buf;
  capture_buf = &out;

  int base = lua_gettop(L);
  int status = luaL_loadstring(L, ("return " + code).c_str());
  if (status != LUA_OK) {
    lua_pop(L, 1); // discard syntax error from the "return" attempt
    status = luaL_loadstring(L, code.c_str());
  }
  if (status == LUA_OK)
    status = lua_pcall(L, 0, LUA_MULTRET, 0);

  if (status != LUA_OK) {
    const char* err = lua_tostring(L, -1);
    std::string msg = "lua error: " + std::string(err ? err : "unknown");
    GameConsoleAPI::print(msg);
    if (!out.empty())
      out += "\n";
    out += msg;
    lua_pop(L, 1);
  } else {
    int nresults = lua_gettop(L) - base;
    for (int i = 1; i <= nresults; i++) {
      size_t len = 0;
      const char* str = luaL_tolstring(L, base + i, &len);
      std::string val(str ? str : "nil", str ? len : 3);
      lua_pop(L, 1);
      GameConsoleAPI::print(val);
      if (!out.empty())
        out += "\n";
      out += val;
    }
  }
  lua_settop(L, base);
  capture_buf = prev_capture;
  return out;
}
// ---- groups ----

// resolve a group name string to its GroupId enum. add a line per enum group.
static int group_id_from_name(const char* name) {
  if (strcmp(name, "seed_deck") == 0)
    return group_seed_deck;
  return -1;
}

// resolve a positional entity index to a thing_ref (same scheme as set_flag/trait_add)
static thing_ref entity_ref_from_index(
    int idx) { // this does not use the suger of the array of thing TODO: dont use this function
  int i = 0;
  for (auto& e : GameCtxAPI::ctx.entities) {
    if (i == idx)
      return e.this_ref();
    i++;
  }
  return thing_ref::get_nil_ref();
}

// init_group(group_name, head_index) -- make entity[head_index] the head/anchor of the group
static int l_init_group(lua_State* L) {
  const char* gname = luaL_checkstring(L, 1);
  int head_idx = (int)luaL_checkinteger(L, 2);

  int gid = group_id_from_name(gname);
  if (gid < 0) {
    GameConsoleAPI::print("lua: init_group: unknown group: " + std::string(gname));
    lua_pushboolean(L, false);
    return 1;
  }
  thing_ref head = entity_ref_from_index(head_idx);
  if (head == thing_ref::get_nil_ref()) {
    GameConsoleAPI::print("lua: init_group: invalid head index");
    lua_pushboolean(L, false);
    return 1;
  }
  GameCtxAPI::group_set_head((GroupId)gid, head);
  lua_pushboolean(L, true);
  return 1;
}

// add_to_group(entity_index, group_name) -- link entity into the group's list
static int l_add_to_group(lua_State* L) {
  int idx = (int)luaL_checkinteger(L, 1);
  const char* gname = luaL_checkstring(L, 2);

  int gid = group_id_from_name(gname);
  if (gid < 0) {
    GameConsoleAPI::print("lua: add_to_group: unknown group: " + std::string(gname));
    lua_pushboolean(L, false);
    return 1;
  }
  thing_ref ref = entity_ref_from_index(idx);
  if (ref == thing_ref::get_nil_ref()) {
    GameConsoleAPI::print("lua: add_to_group: invalid entity index");
    lua_pushboolean(L, false);
    return 1;
  }
  GameCtxAPI::group_push_front((GroupId)gid, ref);
  lua_pushboolean(L, true);
  return 1;
}

} // namespace LuaAPI
