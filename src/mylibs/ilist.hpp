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

template <typename T, size_t N> struct things_list {
  enum class Kinds {
    nil,
    item,
  };

  struct thing_ref {
    Kinds kind = Kinds::nil;
    int gen_id = 0;
    int idx = 0;
  };

  struct thing {
    Kinds kind = Kinds::nil;

    thing_ref prev;
    thing_ref next;

    T data;

    thing& deref(thing_ref ref) { return (*this->_list)[ref]; };
    thing& operator++() { return this->deref(this->next); };

    thing& operator--() { return this->deref(this->prev); };

    explicit operator bool() const { return this->kind == Kinds::nil; }

  private:
    things_list* _list;
  };

  thing& operator[](thing_ref ref) { return things[ref.idx]; };

  void pop(thing_ref ref) {
    // in this you would check valid ref
    // then we would look at the free head.
    // if the free head is nil then we make this item the new free head
    // if there are items then you would lick the prev head to the list
  }
  void add(T new_thing) {
    // pop the free head
    // use the poped slot
  }

private:
  int gen_id[N];
  bool used[N];
  int free_head;
  thing things[N];
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
  things_list<Enemy, 10> enemies;
  CHECK(true);  // List created without crash
}

TEST_CASE("things_list thing_ref default state") {
  things_list<Enemy, 10>::thing_ref ref;
  CHECK(ref.kind == things_list<Enemy, 10>::Kinds::nil);
  CHECK(ref.gen_id == 0);
  CHECK(ref.idx == 0);
}

TEST_CASE("things_list access by index") {
  things_list<Enemy, 10> enemies;

  things_list<Enemy, 10>::thing_ref ref;
  ref.idx = 0;

  auto& thing = enemies[ref];
  thing.data.health = 50;

  CHECK(enemies[ref].data.health == 50);
}

#endif
