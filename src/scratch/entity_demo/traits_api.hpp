#ifndef TRAIT_API_HPP
#define TRAIT_API_HPP

// Trait runtime for entity_demo. The trait *set* is generated: tools/gen_traits.py scans this
// game's sources for the tags below and writes gen/entity_traits.gen.hpp (TraitId, TraitBits,
// TRAIT_INFO, TRAIT_SITES). CMake reruns it whenever a source in this folder changes.
//
//   DECLARE_TRAIT(IDENT, "doc")   once per trait, right where the trait was first needed
//   HAS_TRAIT(e, IDENT)           bool
//   ADD_TRAIT(e, IDENT)           sets the bit and runs the init hook, if one is wired
//   RM_TRAIT(e, IDENT)            clears the bit
//
// Per-trait init/update hooks stay hand-wired: TraitAPI::set_hooks(TraitId::X, init, update).

#include "gen/entity_traits.gen.hpp"

#include <string>

namespace TraitAPI {

using HookFn = void (*)(void*);

struct Hooks {
  HookFn init = nullptr;
  HookFn update = nullptr;
};

inline Hooks hooks[TRAIT_COUNT];

inline void set_hooks(TraitId id, HookFn init, HookFn update = nullptr) {
  hooks[(int)id] = {init, update};
}

template <typename E> bool has(const E& e, TraitId id) { return e.traits.get(id); }

template <typename E> void apply(E& e, TraitId id) {
  e.traits.set(id, true);
  if (hooks[(int)id].init)
    hooks[(int)id].init(&e);
}

template <typename E> void remove(E& e, TraitId id) { e.traits.set(id, false); }

/** @brief Lua/console name -> index into TraitId, or -1. */
inline int find(const char* name) { return trait_from_name(name); }

/** @brief Name-based apply for Lua/console. Returns false if the name is unknown. */
template <typename E> bool apply(E& e, const char* name) {
  int i = find(name);
  if (i < 0)
    return false;
  apply(e, (TraitId)i);
  return true;
}

template <typename E> bool remove(E& e, const char* name) {
  int i = find(name);
  if (i < 0)
    return false;
  remove(e, (TraitId)i);
  return true;
}

/** @brief Run every wired update hook over every entity carrying that trait. */
template <typename EntityList> void tick_all(EntityList& ents) {
  for (int i = 0; i < TRAIT_COUNT; i++) {
    if (!hooks[i].update)
      continue;
    for (auto& e : ents) {
      if (e.traits.get((TraitId)i))
        hooks[i].update(&e);
    }
  }
}

/** @brief One line per trait: name, doc, where it was declared, how many sites use it. */
inline std::string debug_registered() {
  std::string out;
  for (const auto& t : TRAIT_INFO) {
    int uses = 0;
    for (int s = 0; s < TRAIT_SITE_COUNT; s++)
      if (TRAIT_SITES[s].id == t.id)
        uses++;
    out += std::string(t.name) + " — " + t.doc + "  [" + t.file + ":" + std::to_string(t.line) +
           ", " + std::to_string(uses) + " sites]\n";
  }
  return out;
}

template <typename E> std::string debug_entity(const E& e) {
  std::string out;
  for (const auto& t : TRAIT_INFO) {
    if (!e.traits.get(t.id))
      continue;
    if (!out.empty())
      out += " | ";
    out += t.name;
  }
  return out;
}

} // namespace TraitAPI

// Marker for tools/gen_traits.py; expands to nothing meaningful so it is legal at any scope.
#define DECLARE_TRAIT(ID, DOC) static_assert(true, "")
#define HAS_TRAIT(e, ID) TraitAPI::has((e), TraitId::ID)
#define ADD_TRAIT(e, ID) TraitAPI::apply((e), TraitId::ID)
#define RM_TRAIT(e, ID) TraitAPI::remove((e), TraitId::ID)

#endif // TRAIT_API_HPP
