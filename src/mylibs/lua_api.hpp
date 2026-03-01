#pragma once

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <string>

// Forward declarations — the game must define these before including lua_api.hpp
// or link them via its own headers. LuaAPI calls into GameCtxAPI, ModelAPI, TraitAPI,
// and GameConsoleAPI which must be available at the point of use.

namespace LuaAPI {

inline lua_State* L = nullptr;

// ---- Lua C functions ----

// spawn(model_name, x, y, z, [scale])
static int l_spawn(lua_State* L) {
  const char* model = luaL_checkstring(L, 1);
  float x = (float)luaL_optnumber(L, 2, 0);
  float y = (float)luaL_optnumber(L, 3, 0);
  float z = (float)luaL_optnumber(L, 4, 0);
  float scale = (float)luaL_optnumber(L, 5, 1.0);

  auto ref = GameCtxAPI::spawn(
      GameCtxAPI::SpawnArgs{.model_name = std::string(model), .pos = {x, y, z}, .scale = scale});
  lua_pushboolean(L, ref.kind != ilist_kind::nil);
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
  int idx = (int)luaL_checkinteger(L, 1);
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

// clear()
static int l_clear(lua_State* L) {
  (void)L;
  GameCtxAPI::clear_entities();
  return 0;
}

// ---- lifecycle ----

inline void init() {
  L = luaL_newstate();
  luaL_openlibs(L);

  lua_register(L, "spawn", l_spawn);
  lua_register(L, "spawn_child", l_spawn_child);
  lua_register(L, "load_model", l_load_model);
  lua_register(L, "load_primitive", l_load_primitive);
  lua_register(L, "register_trait", l_register_trait);
  lua_register(L, "console_print", l_console_print);
  lua_register(L, "trait_add", l_trait_add);
  lua_register(L, "trait_rm", l_trait_rm);
  lua_register(L, "entity_count", l_entity_count);
  lua_register(L, "model_count", l_model_count);
  lua_register(L, "color", l_color);
  lua_register(L, "set_flag", l_set_flag);
  lua_register(L, "clear", l_clear);
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

} // namespace LuaAPI
