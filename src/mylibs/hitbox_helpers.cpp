

#include "ilist.hpp"
#include "raylib.h"
#include <vector>

namespace HitBoxAPI {

struct CollisionTable {
  struct HitboxEntry {
    thing_ref spawner;
    BoundingBox hitbox;
    int number_frames = 0;
  };
  std::vector<HitboxEntry> entries;
};

void spawn_hitbox(thing_ref thing, std::vector<BoundingBox> bboxes) {
  //
}

} // namespace HitBoxAPI
