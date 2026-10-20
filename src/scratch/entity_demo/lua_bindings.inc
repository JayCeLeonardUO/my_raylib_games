
// ============================================================================
// Lua bindings for entity_demo
// ============================================================================
// Game-specific C functions exposed to Lua, plus the table that documents them.
// Included from main.cpp after GameCtxAPI, TraitAPI, ModelAPI and lua_api.hpp exist.
// main() calls EntityDemoLua::register_lua_bindings() after LuaAPI::init().
//
// Every function here follows the Lua C convention: read arguments off the stack with
// luaL_check*/luaL_opt*, push results, return how many results were pushed.

namespace EntityDemoLua {

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
    LuaAPI::log("lua: spawn_child: invalid parent index");
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
    LuaAPI::log("lua: failed to load model: " + std::string(path));
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
    LuaAPI::log("lua: unknown primitive type: " + std::string(type));
    lua_pushboolean(L, false);
    return 1;
  }

  bool ok = ModelAPI::load(std::string(name), mesh);
  lua_pushboolean(L, ok);
  return 1;
}

// console_print(msg)
static int l_console_print(lua_State* L) {
  const char* msg = luaL_checkstring(L, 1);
  LuaAPI::log(std::string(msg));
  return 0;
}

// trait_add(entity_index, trait_name)
static int l_trait_add(lua_State* L) {
  int idx = (int)luaL_checkinteger(L, 1);
  const char* trait = luaL_checkstring(L, 2);

  int i = 0;
  for (auto& e : GameCtxAPI::ctx.entities) {
    if (i == idx) {
      bool ok = TraitAPI::apply(e, trait);
      if (!ok)
        LuaAPI::log("lua: trait_add: unknown trait '" + std::string(trait) + "'");
      lua_pushboolean(L, ok);
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
      bool ok = TraitAPI::remove(e, trait);
      if (!ok)
        LuaAPI::log("lua: trait_rm: unknown trait '" + std::string(trait) + "'");
      lua_pushboolean(L, ok);
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
    LuaAPI::log("lua: unknown model: " + std::string(name));
    lua_pushboolean(L, false);
    return 1;
  }
  m->materials[0].maps[MATERIAL_MAP_DIFFUSE].color = {(unsigned char)r, (unsigned char)g,
                                                      (unsigned char)b, 255};
  lua_pushboolean(L, true);
  return 1;
}

// set_text(entity_index, text) -> bool
static int l_set_text(lua_State* L) {
  int idx = (int)luaL_checkinteger(L, 1);
  const char* text = luaL_checkstring(L, 2);

  int i = 0;
  for (auto& e : GameCtxAPI::ctx.entities) {
    if (i == idx) {
      snprintf(e.log_text.data(), e.log_text.size(), "%s", text);
      lua_pushboolean(L, true);
      return 1;
    }
    i++;
  }
  lua_pushboolean(L, false);
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
        LuaAPI::log("lua: unknown flag: " + std::string(flag));
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
    LuaAPI::log("lua: register_animation: no frames found in " + std::string(dir_path));
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
    LuaAPI::log("lua: spawn_animation: unknown animation: " + std::string(anim_name));
    lua_pushboolean(L, false);
    return 1;
  }

  // animation entities have no model (NO_MODEL_NAME); they draw via texture
  auto ref = GameCtxAPI::spawn(GameCtxAPI::SpawnArgs{
      .model_name = NO_MODEL_NAME, .pos = {x, y, z}, .scale = scale, .debug_name = "animation"});
  if (ref.kind == ilist_kind::nil) {
    lua_pushboolean(L, false);
    return 1;
  }

  auto& ent = GameCtxAPI::ctx.entities[ref];
  ent.animation_id = anim_id;
  ADD_TRAIT(ent, IS_ANIMATION);

  if (looping)
    ADD_TRAIT(ent, IS_LOOPING);

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
    LuaAPI::log("lua: set_click_animation: unknown animation: " + std::string(anim_name));
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

// light_scheme([name]) -> current scheme name; with a name, switches to it
static int l_light_scheme(lua_State* L) {
  const char* name = luaL_optstring(L, 1, nullptr);
  if (name && !ThreePointLight::set_scheme(name)) {
    std::string known;
    for (const auto& n : ThreePointLight::scheme_names())
      known += (known.empty() ? "" : ", ") + n;
    LuaAPI::log("lua: unknown light scheme: " + std::string(name) + " (known: " + known + ")");
    lua_pushboolean(L, false);
    return 1;
  }
  lua_pushstring(L, ThreePointLight::current().name);
  return 1;
}

// light_intensity([x]) -> master intensity; with a number, sets it.
// light_intensity(light, [x]) -> that light's intensity; light is key, fill, rim or ambient.
static int l_light_intensity(lua_State* L) {
  if (lua_type(L, 1) == LUA_TSTRING) {
    const char* name = lua_tostring(L, 1);
    float* v = ThreePointLight::intensity_of(name);
    if (!v) {
      LuaAPI::log("lua: light_intensity: unknown light: " + std::string(name) +
                  " (key, fill, rim, ambient)");
      lua_pushboolean(L, false);
      return 1;
    }
    if (lua_gettop(L) >= 2)
      *v = (float)luaL_checknumber(L, 2);
    lua_pushnumber(L, *v);
    return 1;
  }
  if (lua_gettop(L) >= 1)
    ThreePointLight::master_intensity = (float)luaL_checknumber(L, 1);
  lua_pushnumber(L, ThreePointLight::master_intensity);
  return 1;
}

// screenshot([path]) -> path. Deferred: the main loop saves it at the end of the current
// frame (see LuaAPI::take_pending_screenshot) so the framebuffer is complete.
inline std::string pending_screenshot;
static int l_screenshot(lua_State* L) {
  const char* path = luaL_optstring(L, 1, "screenshot.png");
  pending_screenshot = path;
  lua_pushstring(L, path);
  return 1;
}
/// Call right before EndDrawing().
inline void take_pending_screenshot() {
  if (pending_screenshot.empty())
    return;
  // raylib saves GetFileName(path) into its storage base path (the working directory);
  // move the file to where the caller actually asked for it when a directory was given.
  std::string requested = pending_screenshot;
  pending_screenshot.clear();
  TakeScreenshot(requested.c_str());
  std::string written = std::string(GetWorkingDirectory()) + "/" + GetFileName(requested.c_str());
  if (requested.find('/') != std::string::npos && written != requested) {
    if (std::rename(written.c_str(), requested.c_str()) != 0)
      LuaAPI::log("lua: screenshot saved to " + written + " (could not move to " + requested + ")");
  }
}

// camera([x, y, z, [tx, ty, tz]]) -> x, y, z, tx, ty, tz. Get or set camera position/target.
static int l_camera(lua_State* L) {
  Camera3D& cam = GameCtxAPI::ctx.camera;
  if (lua_gettop(L) >= 3) {
    cam.position = {(float)luaL_checknumber(L, 1), (float)luaL_checknumber(L, 2),
                    (float)luaL_checknumber(L, 3)};
    if (lua_gettop(L) >= 6)
      cam.target = {(float)luaL_checknumber(L, 4), (float)luaL_checknumber(L, 5),
                    (float)luaL_checknumber(L, 6)};
  }
  lua_pushnumber(L, cam.position.x);
  lua_pushnumber(L, cam.position.y);
  lua_pushnumber(L, cam.position.z);
  lua_pushnumber(L, cam.target.x);
  lua_pushnumber(L, cam.target.y);
  lua_pushnumber(L, cam.target.z);
  return 6;
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
    LuaAPI::log("lua: init_group: unknown group: " + std::string(gname));
    lua_pushboolean(L, false);
    return 1;
  }
  thing_ref head = entity_ref_from_index(head_idx);
  if (head == thing_ref::get_nil_ref()) {
    LuaAPI::log("lua: init_group: invalid head index");
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
    LuaAPI::log("lua: add_to_group: unknown group: " + std::string(gname));
    lua_pushboolean(L, false);
    return 1;
  }
  thing_ref ref = entity_ref_from_index(idx);
  if (ref == thing_ref::get_nil_ref()) {
    LuaAPI::log("lua: add_to_group: invalid entity index");
    lua_pushboolean(L, false);
    return 1;
  }
  GameCtxAPI::group_push_front((GroupId)gid, ref);
  lua_pushboolean(L, true);
  return 1;
}

// particles([count]) -> count. Get or set how many tumbling triangles are drawn (0 hides them).
static int l_particles(lua_State* L) {
  auto& ps = GameCtxAPI::particles;
  if (lua_gettop(L) >= 1) {
    ps.count = (int)luaL_checkinteger(L, 1);
    if (ps.count < 0)
      ps.count = 0;
    ps.respawn();
  }
  lua_pushinteger(L, ps.count);
  return 1;
}

// ---- binding table ----
// Single source of truth for what this game exposes. help() prints it; register_lua_bindings()
// registers it with the VM. `help` and `print` are provided by LuaAPI itself.
inline const LuaAPI::Binding BINDINGS[] = {
    // --- discovery ---
    {"entities", l_entities, "entities()", "list live entities: index, model, position, traits"},
    {"traits", l_traits, "traits()", "list traits: name, doc, where declared"},
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
    {"set_text", l_set_text, "set_text(index, text) -> bool",
     "set the entity's log_text (shown by hover_card and is_text)"},
    {"set_flag", l_set_flag, "set_flag(index, flag, bool)",
     "flags: is_draggable is_collidable is_highlightable"},
    // --- traits ---
    {"trait_add", l_trait_add, "trait_add(index, trait_name) -> bool",
     "attach a trait; false if the index or the name is unknown (see traits())"},
    {"trait_rm", l_trait_rm, "trait_rm(index, trait_name) -> bool", "detach a trait"},
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
    // --- lighting (entities need the "three_point_lit" trait) ---
    {"light_scheme", l_light_scheme, "light_scheme([name]) -> name",
     "get/set the three-point scheme: Studio Neutral, Warm Key / Cool Fill, Cool Key / Warm Fill, Golden Hour, Moonlight, Overcast Soft, Forest"},
    {"light_intensity", l_light_intensity, "light_intensity([x]) | light_intensity(light, [x]) -> x",
     "get/set master intensity, or one light's: key fill rim ambient"},
    // --- particles ---
    {"particles", l_particles, "particles([count]) -> count",
     "how many tumbling triangle particles are drawn (one instanced call); 0 hides them"},
    // --- camera ---
    {"camera", l_camera, "camera([x, y, z, [tx, ty, tz]]) -> pos, target",
     "get or set the camera position and look-at target"},
    // --- output ---
    {"screenshot", l_screenshot, "screenshot([path]) -> path", "save a PNG of the next frame (default screenshot.png)"},
    {"console_print", l_console_print, "console_print(msg)", "print a single string to the game console"},
};

/// Call after LuaAPI::init(). Registers every binding and the help text around the list.
inline void register_lua_bindings() {
  LuaAPI::help_header = "Lua API (entity indices are 0-based spawn order; `entities()` shows them)";
  LuaAPI::help_footer =
      "Also: any registered console command (`help` alone lists them in-game), and plain Lua.\n"
      "Try: traits()  models()  entities()  help(\"spawn\")";
  for (const auto& b : BINDINGS)
    LuaAPI::register_fn(b.name, b.fn, b.sig, b.doc);
}

} // namespace EntityDemoLua
