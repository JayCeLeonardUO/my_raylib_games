
// ============================================================================
// Console commands
// ============================================================================

/** @brief Spawn a player entity with WSAD movement at an optional position. */
REGISTER_CMD(spawn_player, "spawn_player <model> [x y z]", {
  if (args.empty())
    return std::string("Usage: spawn_player <model> [x y z]");
  std::string name = args[0];
  if (!ModelAPI::has(name))
    return "Unknown model: " + name;
  Vector3 pos = {0, 0, 0};
  if (args.size() >= 4) {
    pos.x = std::stof(args[1]);
    pos.y = std::stof(args[2]);
    pos.z = std::stof(args[3]);
  }
  auto ref = GameCtxAPI::spawn(GameCtxAPI::SpawnArgs{.model_name = name, .pos = pos});
  if (ref.kind == ilist_kind::nil)
    return std::string("Failed");
  auto& e = GameCtxAPI::ctx.entities[ref];
  if (e)
    TraitAPI::apply(e, TRAIT_WSAD);
  return "Spawned player " + name;
});

/** @brief Spawn a collectable pickup entity at an optional position. */
REGISTER_CMD(spawn_pickup, "spawn_pickup <model> [x y z]", {
  if (args.empty())
    return std::string("Usage: spawn_pickup <model> [x y z]");
  std::string name = args[0];
  if (!ModelAPI::has(name))
    return "Unknown model: " + name;
  Vector3 pos = {0, 0, 0};
  if (args.size() >= 4) {
    pos.x = std::stof(args[1]);
    pos.y = std::stof(args[2]);
    pos.z = std::stof(args[3]);
  }
  auto ref = GameCtxAPI::spawn(GameCtxAPI::SpawnArgs{.model_name = name, .pos = pos, .scale = 0.5f});
  if (ref.kind == ilist_kind::nil)
    return std::string("Failed");
  auto& e = GameCtxAPI::ctx.entities[ref];
  if (e)
    TraitAPI::apply(e, TRAIT_PICKUP);
  return "Spawned pickup " + name;
});

/** @brief Load a model from a file path, optionally giving it a custom name. */
REGISTER_CMD(load, "load <path> [name]", {
  if (args.empty())
    return std::string("Usage: load <path> [name]");
  std::string path = args[0];
  std::string name = (args.size() > 1) ? args[1] : path;
  if (args.size() == 1) {
    size_t slash = path.find_last_of('/');
    size_t dot = path.find_last_of('.');
    if (slash != std::string::npos)
      name = path.substr(slash + 1);
    if (dot != std::string::npos && dot > (slash == std::string::npos ? 0 : slash))
      name = name.substr(0, name.find_last_of('.'));
  }
  if (ModelAPI::load(name, path))
    return "Loaded: " + name;
  return "Failed to load: " + path;
});

/** @brief Spawn a generic entity from a loaded model at an optional position and scale. */
REGISTER_CMD(spawn, "spawn <model> [x y z] [scale]", {
  if (args.empty())
    return std::string("Usage: spawn <model> [x y z] [scale]");
  std::string name = args[0];
  if (!ModelAPI::has(name))
    return "Unknown model: " + name;
  Vector3 pos = {0, 0, 0};
  float scale = 1.0f;
  if (args.size() >= 4) {
    pos.x = std::stof(args[1]);
    pos.y = std::stof(args[2]);
    pos.z = std::stof(args[3]);
  }
  if (args.size() >= 5)
    scale = std::stof(args[4]);
  auto ref = GameCtxAPI::spawn(GameCtxAPI::SpawnArgs{.model_name = name, .pos = pos, .scale = scale});
  if (ref.kind == ilist_kind::nil)
    return std::string("Failed");
  return "Spawned " + name;
});

/** @brief List all live entities with their index, model name, and traits. */
REGISTER_CMD(list, "list — show all entities", {
  (void)args;
  std::string out;
  int i = 0;
  for (auto& e : GameCtxAPI::ctx.entities) {
    std::string traits = TraitAPI::debug_entity(e);
    out += std::to_string(i++) + ": " + (e.model.valid() ? std::string(e.model.name) : "---") +
           " [" + traits + "]\n";
  }
  return out.empty() ? "No entities" : out;
});

/** @brief Remove all entities from the scene. */
REGISTER_CMD(clear, "clear — remove all entities", {
  (void)args;
  size_t count = GameCtxAPI::entity_count();
  GameCtxAPI::clear_entities();
  return "Cleared " + std::to_string(count);
});

/** @brief List all currently loaded model names. */
REGISTER_CMD(models, "models — list loaded models", {
  (void)args;
  std::string out;
  for (const auto& name : ModelAPI::names())
    out += name + "\n";
  return out.empty() ? "No models" : out;
});

/** @brief Select an entity by its list index for inspection/editing. */
REGISTER_CMD(select, "select <index>", {
  if (args.empty())
    return std::string("Usage: select <index>");
  int idx = std::stoi(args[0]);
  int i = 0;
  for (auto& e : GameCtxAPI::ctx.entities) {
    if (i == idx) {
      GameCtxAPI::ctx.selected = e.this_ref();
      return "Selected " + args[0];
    }
    i++;
  }
  return "Invalid index";
});

/** @brief List all registered trait names. */
REGISTER_CMD(traits, "traits — list registered traits", {
  (void)args;
  return TraitAPI::debug_registered();
});

/** @brief Apply a named trait to the currently selected entity. */
REGISTER_CMD(trait_add, "trait_add <n> — apply trait to selected", {
  if (args.empty())
    return std::string("Usage: trait_add <n>");
  auto& sel = GameCtxAPI::ctx.entities[GameCtxAPI::ctx.selected];
  if (!sel)
    return std::string("Nothing selected");
  if (TraitAPI::find(args[0].c_str()) < 0)
    return "Unknown trait: " + args[0];
  TraitAPI::apply(sel, args[0].c_str());
  return "Applied " + args[0];
});

/** @brief Remove a named trait from the currently selected entity. */
REGISTER_CMD(trait_rm, "trait_rm <n> — remove trait from selected", {
  if (args.empty())
    return std::string("Usage: trait_rm <n>");
  auto& sel = GameCtxAPI::ctx.entities[GameCtxAPI::ctx.selected];
  if (!sel)
    return std::string("Nothing selected");
  TraitAPI::remove(sel, args[0].c_str());
  return "Removed " + args[0];
});

/** @brief Set the diffuse color of a loaded model's first material. */
REGISTER_CMD(color, "color <model> <r> <g> <b>", {
  if (args.size() < 4)
    return std::string("Usage: color <model> <r> <g> <b>");
  Model* m = ModelAPI::get(args[0]);
  if (!m)
    return "Unknown model: " + args[0];
  unsigned char r = std::stoi(args[1]);
  unsigned char g = std::stoi(args[2]);
  unsigned char b = std::stoi(args[3]);
  m->materials[0].maps[MATERIAL_MAP_DIFFUSE].color = {r, g, b, 255};
  return "Set " + args[0] + " color";
});

/** @brief Set the camera position to the given coordinates. */
REGISTER_CMD(cam, "cam <x> <y> <z>", {
  if (args.size() < 3)
    return std::string("Usage: cam <x> <y> <z>");
  GameCtxAPI::ctx.camera.position = {std::stof(args[0]), std::stof(args[1]), std::stof(args[2])};
  return "OK";
});

/** @brief Set the camera look-at target to the given coordinates. */
REGISTER_CMD(target, "target <x> <y> <z>", {
  if (args.size() < 3)
    return std::string("Usage: target <x> <y> <z>");
  GameCtxAPI::ctx.camera.target = {std::stof(args[0]), std::stof(args[1]), std::stof(args[2])};
  return "OK";
});

/** @brief Execute a Lua script file by path. */
REGISTER_CMD(lua, "lua <file> — run a lua script", {
  if (args.empty())
    return std::string("Usage: lua <file>");
  bool ok = LuaAPI::run_file(args[0]);
  return ok ? "OK" : "Failed";
});
