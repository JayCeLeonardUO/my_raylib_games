#pragma once

// has_traits concept

template <typename T>
concept has_traits = requires(T t) { t.traits; };

// should I make them dynamic?

#define set_trait(entity, member) entity.member = 1;

#define TRAIT_DEF(name) uint8_t name : 1 = 0;
