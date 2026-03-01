#include <compare>
#if 0
cd "$(dirname "$0")/../.."
cmake --build build --target mylibs_tests && ./build/mylibs_tests -tc="things_list*"
exit
#endif
/**
 * @file ilist.hpp
 * @brief Custom data structures and utilities
 */

#pragma once
#include <cstddef>

#define MAX_ITEMS 1000

// Defined outside template so types can use thing_ref without circular dependency
enum class ilist_kind {
  nil,
  item,
};

struct thing_ref {
  ilist_kind kind = ilist_kind::nil;
  int gen_id = 0;
  int idx = 0;
  auto operator<=>(const thing_ref&) const = default;
  bool operator==(const thing_ref&) const = default;

  static thing_ref get_nil_ref() { return {ilist_kind::nil, 0, 0}; }

  bool operator!=(const thing_ref& other) const { return !(*this == other); }
};

/**
 * @brief Base class for list elements. Inherit from this to use things_list.
 */
struct thing_base {
  ilist_kind kind = ilist_kind::nil;
  thing_ref prev;
  thing_ref next;

  explicit operator bool() const { return kind != ilist_kind::nil; }

  thing_ref this_ref() const { return {kind, _gen_id ? *_gen_id : 0, _index}; }

  template <typename, size_t> struct things_list;
  int _index = 0;
  int* _gen_id = nullptr;
};

template <typename T, size_t N> struct things_list {
  using Kinds = ilist_kind;
  using ref = thing_ref;
  using thing = T;

  T& operator[](thing_ref ref) { return things[ref.idx]; };

  /**
   * @brief Iterator for iterating over active items
   */
  struct iterator {
    things_list* list;
    int idx;

    iterator(things_list* l, int i) : list(l), idx(i) {
      // Skip to first active item
      while (idx < (int)N && !list->used[idx]) {
        idx++;
      }
    }

    T& operator*() { return list->things[idx]; }
    T* operator->() { return &list->things[idx]; }

    iterator& operator++() {
      idx++;
      while (idx < (int)N && !list->used[idx]) {
        idx++;
      }
      return *this;
    }

    iterator operator++(int) {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    bool operator==(const iterator& other) const { return idx == other.idx; }
    bool operator!=(const iterator& other) const { return idx != other.idx; }
  };

  iterator begin() { return iterator(this, 0); }
  iterator end() { return iterator(this, (int)N); }

  /**
   * @brief Const iterator for iterating over active items
   */
  struct const_iterator {
    const things_list* list;
    int idx;

    const_iterator(const things_list* l, int i) : list(l), idx(i) {
      while (idx < (int)N && !list->used[idx]) {
        idx++;
      }
    }

    const T& operator*() const { return list->things[idx]; }
    const T* operator->() const { return &list->things[idx]; }

    const_iterator& operator++() {
      idx++;
      while (idx < (int)N && !list->used[idx]) {
        idx++;
      }
      return *this;
    }

    const_iterator operator++(int) {
      const_iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    bool operator==(const const_iterator& other) const { return idx == other.idx; }
    bool operator!=(const const_iterator& other) const { return idx != other.idx; }
  };

  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator end() const { return const_iterator(this, (int)N); }
  const_iterator cbegin() const { return const_iterator(this, 0); }
  const_iterator cend() const { return const_iterator(this, (int)N); }

  /**
   * @brief Remove an item and return it to the free list
   */
  void remove(thing_ref ref) {
    if (ref.kind == Kinds::nil)
      return;
    if (ref.gen_id != gen_id[ref.idx])
      return; // stale ref

    auto& slot = things[ref.idx];

    // Remove from any sublist it's in
    _sublist_remove(slot);

    // Mark as nil
    slot.kind = Kinds::nil;
    used[ref.idx] = false;

    // Prepend to free list (link directly since free list nodes are kind=nil)
    slot.next = free_head;
    slot.prev = thing_ref{};
    free_head = thing_ref{Kinds::nil, 0, ref.idx};
  }

  things_list() {
    for (int i = 0; i < (int)N; i++) {
      things[i]._index = i;
      things[i]._gen_id = &gen_id[i];
      things[i].kind = Kinds::nil;
      gen_id[i] = 0;
      used[i] = false;
    }

    // Build the free list chain
    for (int i = 0; i < (int)N - 1; i++) {
      things[i].next.idx = i + 1;
      things[i].next.kind = Kinds::nil;
      things[i + 1].prev.idx = i;
      things[i + 1].prev.kind = Kinds::nil;
    }

    // Set free head to first slot
    free_head.idx = 0;
    free_head.kind = Kinds::nil;
  }

  T& get(thing_ref ref) { return things[ref.idx]; }

  /**
   * @brief Add a new item to the list
   * @return Reference to the new item, or nil ref if full
   */
  thing_ref add(T new_thing) {
    thing_ref slot_ref = _get_next_slot();
    if (slot_ref.kind == Kinds::nil && !used[slot_ref.idx]) {
      // We got a valid free slot — save bookkeeping before overwrite
      auto& slot = things[slot_ref.idx];
      int saved_index = slot._index;
      int* saved_gen_id = slot._gen_id;

      // Copy user data
      slot = new_thing;

      // Restore bookkeeping
      slot._index = saved_index;
      slot._gen_id = saved_gen_id;
      slot.kind = Kinds::item;
      slot.prev = thing_ref{};
      slot.next = thing_ref{};
      slot_ref.kind = Kinds::item;
      used[slot_ref.idx] = true;
      gen_id[slot_ref.idx]++;
      slot_ref.gen_id = gen_id[slot_ref.idx];
      return slot_ref;
    }
    // List is full
    return thing_ref{};
  }

private:
  int gen_id[N];
  bool used[N];
  thing_ref free_head;
  T things[N];

  void _sublist_remove(T& slot) {
    if (slot.next.kind != Kinds::nil)
      things[slot.next.idx].prev = slot.prev;
    if (slot.prev.kind != Kinds::nil)
      things[slot.prev.idx].next = slot.next;
    slot.prev = thing_ref{};
    slot.next = thing_ref{};
  }

  /**
   * @brief Get the next available slot from the free list
   * @return thing_ref to the slot (kind=nil if valid free slot, check used[])
   */
  thing_ref _get_next_slot() {
    if (free_head.kind == Kinds::nil && free_head.idx >= 0 && free_head.idx < (int)N &&
        !used[free_head.idx]) {
      thing_ref result = free_head;
      auto& slot = things[free_head.idx];

      // Move free_head to next free slot
      free_head = slot.next;

      // Remove this slot from free list
      _sublist_remove(slot);

      return result;
    }
    // No free slots
    thing_ref empty{};
    empty.idx = -1; // Mark as invalid
    return empty;
  }
};

// ============================================================================
// DOCTEST - Tests run when compiled with tests_main.cpp
// ============================================================================
#ifdef DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

struct Enemy : thing_base {
  float x = 0, y = 0;
  int health = 100;

  Enemy() = default;
  Enemy(float x, float y, int health) : x(x), y(y), health(health) {}
};

TEST_CASE("things_list basic creation") {
  things_list<Enemy, 100> enemies;
  CHECK(true);
}

TEST_CASE("things_list thing_ref default state") {
  thing_ref ref;
  CHECK(ref.kind == ilist_kind::nil);
  CHECK(ref.gen_id == 0);
  CHECK(ref.idx == 0);
}

TEST_CASE("things_list access by index") {
  things_list<Enemy, 100> enemies;

  thing_ref ref;
  ref.idx = 0;

  auto& thing = enemies[ref];
  thing.health = 50;

  CHECK(enemies[ref].health == 50);
}

TEST_CASE("things_list remove") {
  things_list<Enemy, 100> enemies;

  // Helper to count active items via iterator
  auto count = [&]() {
    int n = 0;
    for (auto& e : enemies) {
      (void)e;
      n++;
    }
    return n;
  };

  // Add 3 enemies
  Enemy a{1.0f, 2.0f, 10};
  Enemy b{3.0f, 4.0f, 20};
  Enemy c{5.0f, 6.0f, 30};

  thing_ref ra = enemies.add(a);
  thing_ref rb = enemies.add(b);
  thing_ref rc = enemies.add(c);

  CHECK(ra.kind == ilist_kind::item);
  CHECK(rb.kind == ilist_kind::item);
  CHECK(rc.kind == ilist_kind::item);
  CHECK(count() == 3);

  // Verify data
  CHECK(enemies[ra].health == 10);
  CHECK(enemies[rb].health == 20);
  CHECK(enemies[rc].health == 30);

  // Remove middle element
  enemies.remove(rb);
  CHECK(count() == 2);

  // Remaining elements still accessible
  CHECK(enemies[ra].health == 10);
  CHECK(enemies[rc].health == 30);

  // Stale ref should be harmless (gen_id mismatch)
  enemies.remove(rb); // double remove is a no-op
  CHECK(count() == 2);

  // Remove first
  enemies.remove(ra);
  CHECK(count() == 1);
  CHECK(enemies[rc].health == 30);

  // Remove last
  enemies.remove(rc);
  CHECK(count() == 0);

  // Removing nil ref is a no-op
  enemies.remove(thing_ref::get_nil_ref());
  CHECK(count() == 0);

  // Slots are reusable after removal — add back as many as we removed
  Enemy d{7.0f, 8.0f, 40};
  Enemy e{9.0f, 10.0f, 50};
  Enemy f{11.0f, 12.0f, 60};

  thing_ref rd = enemies.add(d);
  thing_ref re = enemies.add(e);
  thing_ref rf = enemies.add(f);

  CHECK(rd.kind == ilist_kind::item);
  CHECK(re.kind == ilist_kind::item);
  CHECK(rf.kind == ilist_kind::item);
  CHECK(count() == 3);
  CHECK(enemies[rd].health == 40);
  CHECK(enemies[re].health == 50);
  CHECK(enemies[rf].health == 60);
}

TEST_CASE("things_list clear and re-add") {
  things_list<Enemy, 100> enemies;

  auto count = [&]() {
    int n = 0;
    for (auto& e : enemies) {
      (void)e;
      n++;
    }
    return n;
  };

  // Fill some entities
  for (int i = 0; i < 10; i++)
    enemies.add(Enemy{(float)i, 0, i * 10});
  CHECK(count() == 10);

  // Collect refs and remove all (same pattern as clear_entities)
  std::vector<thing_ref> refs;
  for (auto& e : enemies)
    refs.push_back(e.this_ref());
  for (auto& r : refs)
    enemies.remove(r);
  CHECK(count() == 0);

  // Re-add the same amount — this failed before the fix
  for (int i = 0; i < 10; i++) {
    thing_ref r = enemies.add(Enemy{(float)i, 0, i * 100});
    CHECK(r.kind == ilist_kind::item);
  }
  CHECK(count() == 10);
}

#endif
