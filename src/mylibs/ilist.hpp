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
  struct thing_ref {
    int gen_id;
    int idx;
  };

  struct thing {
    int prev;
    int next;

    T data;

    thing& deref(thing_ref ref) { return (*this->_list)[ref]; };

  private:
    things_list* _list;
  };

  thing& operator[](thing_ref ref) { return things[ref.idx]; };

private:
  int gen_id[N];
  bool used[N];
  int free_head;
  thing things[N];
};
