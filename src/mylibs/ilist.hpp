/**
 * @file ilist.hpp
 * @brief Custom data structures and utilities
 */

#pragma once
#include <cstddef>

/**
 * @brief A generic container for game objects
 *
 * Use this struct to hold related game data.
 *
 * @note Add your fields and methods as needed
 */

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
};

template <typename T, size_t N> struct things_list {
  using Kinds = ilist_kind;
  using ref = thing_ref;

  struct thing : public T {
    Kinds kind = Kinds::nil;

    thing_ref prev;
    thing_ref next;

    /**
     * @brief Get a reference to this thing (computed from list)
     */
    thing_ref this_ref() const {
      int idx = static_cast<int>(this - _list->things);
      return thing_ref{kind, _list->gen_id[idx], idx};
    }

    thing& deref(thing_ref ref) { return (*this->_list)[ref]; };
    thing& operator++() { return this->deref(this->next); };

    thing& operator--() { return this->deref(this->prev); };

    explicit operator bool() const { return this->kind != Kinds::nil; }

    /**
     * @brief Insert ref before this node
     * @param ref The node to insert
     * @return true if successful, false if this node is nil
     */
    bool prepend(thing_ref ref) {
      if (kind == Kinds::nil)
        return false;

      auto& new_node = (*_list)[ref];

      // Link new node into the list
      new_node.next = this_ref();
      new_node.prev = prev;

      // Update old prev's next to point to new node
      if (prev.kind != Kinds::nil) {
        (*_list)[prev].next = ref;
      }

      // Update this node's prev
      prev = ref;

      return true;
    }

    /**
     * @brief Insert ref after this node
     * @param ref The node to insert
     * @return true if successful, false if this node is nil
     */
    bool append(thing_ref ref) {
      if (kind == Kinds::nil)
        return false;

      auto& new_node = (*_list)[ref];

      // Link new node into the list
      new_node.prev = this_ref();
      new_node.next = next;

      // Update old next's prev to point to new node
      if (next.kind != Kinds::nil) {
        (*_list)[next].prev = ref;
      }

      // Update this node's next
      next = ref;

      return true;
    }

    /**
     * @brief Remove this node from the linked list
     * Updates adjacent nodes to skip over this one
     */
    void sublist_remove() {
      // Update next node's prev to skip over this node
      if (next.kind != Kinds::nil) {
        deref(next).prev = prev;
      }

      // Update prev node's next to skip over this node
      if (prev.kind != Kinds::nil) {
        deref(prev).next = next;
      }

      // Clear this node's links
      prev = thing_ref{};
      next = thing_ref{};
    }

  private:
    friend struct things_list;
    things_list* _list;
  };

  thing& operator[](thing_ref ref) { return things[ref.idx]; };

  /**
   * @brief Iterator for iterating over active items
   */
  struct iterator {
    things_list* list;
    int idx;

    iterator(things_list* l, int i) : list(l), idx(i) {
      // Skip to first active item
      while (idx < MAX_ITEMS && !list->used[idx]) {
        idx++;
      }
    }

    thing& operator*() { return list->things[idx]; }
    thing* operator->() { return &list->things[idx]; }

    iterator& operator++() {
      idx++;
      while (idx < MAX_ITEMS && !list->used[idx]) {
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
  iterator end() { return iterator(this, MAX_ITEMS); }

  /**
   * @brief Const iterator for iterating over active items
   */
  struct const_iterator {
    const things_list* list;
    int idx;

    const_iterator(const things_list* l, int i) : list(l), idx(i) {
      while (idx < MAX_ITEMS && !list->used[idx]) {
        idx++;
      }
    }

    const thing& operator*() const { return list->things[idx]; }
    const thing* operator->() const { return &list->things[idx]; }

    const_iterator& operator++() {
      idx++;
      while (idx < MAX_ITEMS && !list->used[idx]) {
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
  const_iterator end() const { return const_iterator(this, MAX_ITEMS); }
  const_iterator cbegin() const { return const_iterator(this, 0); }
  const_iterator cend() const { return const_iterator(this, MAX_ITEMS); }

  /**
   * @brief Remove an item and return it to the free list
   */
  void pop(thing_ref ref) {
    if (ref.kind == Kinds::nil)
      return;
    if (ref.gen_id != gen_id[ref.idx])
      return; // stale ref

    auto& slot = things[ref.idx];

    // Remove from any sublist it's in
    slot.sublist_remove();

    // Mark as nil
    slot.kind = Kinds::nil;
    used[ref.idx] = false;

    // Add to free list
    if (free_head.kind != Kinds::nil) {
      things[free_head.idx].prepend(slot.this_ref());
    }
    free_head = slot.this_ref();
    free_head.kind = Kinds::nil; // it's a free slot, not an item
  }

  things_list() {
    // Set up _list pointers for each thing (this_ref is now computed)
    for (int i = 0; i < MAX_ITEMS; i++) {
      things[i]._list = this;
      things[i].kind = Kinds::nil;
      gen_id[i] = 0;
      used[i] = false;
    }

    // Build the free list chain
    for (int i = 0; i < MAX_ITEMS - 1; i++) {
      things[i].next.idx = i + 1;
      things[i].next.kind = Kinds::nil;
      things[i + 1].prev.idx = i;
      things[i + 1].prev.kind = Kinds::nil;
    }

    // Set free head to first slot
    free_head.idx = 0;
    free_head.kind = Kinds::nil;
  }

  thing& get(thing_ref ref) { return things[ref.idx]; }

  /**
   * @brief Add a new item to the list
   * @return Reference to the new item, or nil ref if full
   */
  thing_ref add(T new_thing) {
    thing_ref slot_ref = _get_next_slot();
    if (slot_ref.kind == Kinds::nil && !used[slot_ref.idx]) {
      // We got a valid free slot
      auto& slot = things[slot_ref.idx];
      static_cast<T&>(slot) = new_thing;
      slot.kind = Kinds::item;
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
  thing things[N];

  /**
   * @brief Get the next available slot from the free list
   * @return thing_ref to the slot (kind=nil if valid free slot, check used[])
   */

  thing_ref _get_next_slot() {
    if (free_head.kind == Kinds::nil && free_head.idx >= 0 && free_head.idx < MAX_ITEMS &&
        !used[free_head.idx]) {
      thing_ref result = free_head;
      auto& slot = things[free_head.idx];

      // Move free_head to next free slot
      free_head = slot.next;

      // Remove this slot from free list
      slot.sublist_remove();

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

struct Enemy {
  float x = 0, y = 0;
  int health = 100;
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

#endif
