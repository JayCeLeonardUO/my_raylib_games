
#ifndef TRAIT_API_HPP
#define TRAIT_API_HPP

#include <cassert>
#include <cstring>
#include <string>
#include <vector>

constexpr int MAX_TRAITS = 10;

namespace TraitAPI {

using InitFn = void (*)(void*);
using UpdateFn = void (*)(void*);

struct TraitEntry {
  const char* name;
  int slot;
  InitFn init;
  UpdateFn update;
};

struct State {
  std::vector<TraitEntry> entries;
  int next_slot = 0;
  void* PRESENT = (void*)0x1;
};

inline State state;

inline int register_trait(const char* name, InitFn init = nullptr, UpdateFn update = nullptr) {
  for (auto& e : state.entries) {
    if (strcmp(e.name, name) == 0)
      return e.slot;
  }
  assert(state.next_slot < MAX_TRAITS && "Too many traits");
  int slot = state.next_slot++;
  state.entries.push_back({name, slot, init, update});
  return slot;
}

inline int find(const char* name) {
  for (auto& e : state.entries) {
    if (strcmp(e.name, name) == 0)
      return e.slot;
  }
  return -1;
}

inline TraitEntry* entry_at(int slot) {
  for (auto& e : state.entries) {
    if (e.slot == slot)
      return &e;
  }
  return nullptr;
}

template <typename E> void apply(E& e, const char* name) {
  int slot = find(name);
  if (slot < 0)
    return;
  e.traits[slot] = state.PRESENT;
  auto* entry = entry_at(slot);
  if (entry && entry->init)
    entry->init(&e);
}

template <typename E> void remove(E& e, const char* name) {
  int slot = find(name);
  if (slot >= 0)
    e.traits[slot] = nullptr;
}

template <typename E> bool has(const E& e, const char* name) {
  int slot = find(name);
  return slot >= 0 && e.traits[slot] != nullptr;
}

template <typename EntityList> void tick_all(EntityList& ents) {
  // Update all registered traits
  for (auto& entry : state.entries) {
    if (!entry.update)
      continue;
    for (auto& e : ents) {
      if (e.traits[entry.slot])
        entry.update(&e);
    }
  }
}

inline std::string debug_registered() {
  std::string out;
  for (auto& e : state.entries) {
    out += std::string(e.name) + " [slot " + std::to_string(e.slot) + "]\n";
  }
  return out;
}

template <typename E> std::string debug_entity(const E& e) {
  std::string out;
  for (auto& entry : state.entries) {
    if (!e.traits[entry.slot])
      continue;
    if (!out.empty())
      out += " | ";
    out += entry.name;
  }
  return out;
}

} // namespace TraitAPI

#endif // TRAIT_API_HPP
