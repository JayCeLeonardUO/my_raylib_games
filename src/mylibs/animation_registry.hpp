#pragma once
#include <raylib.h>
#include <string>
#include <vector>

namespace AnimationRegistry {

struct AnimationDef {
  std::string name;
  std::vector<Texture2D> frames;
  float frame_duration; // seconds per frame
};

inline std::vector<AnimationDef> animations;

// Register an animation by loading numbered PNGs from a directory.
// Expects files named frame_0000.png, frame_0001.png, etc.
inline int register_anim(const std::string& name, const std::string& dir_path,
                          float frame_duration) {
  for (int i = 0; i < (int)animations.size(); i++) {
    if (animations[i].name == name)
      return i;
  }
  AnimationDef def;
  def.name = name;
  def.frame_duration = frame_duration;
  for (int i = 0;; i++) {
    char path[512];
    snprintf(path, sizeof(path), "%s/frame_%04d.png", dir_path.c_str(), i);
    if (!FileExists(path))
      break;
    def.frames.push_back(LoadTexture(path));
  }
  if (def.frames.empty())
    return -1;
  animations.push_back(std::move(def));
  return (int)animations.size() - 1;
}

inline int find(const std::string& name) {
  for (int i = 0; i < (int)animations.size(); i++) {
    if (animations[i].name == name)
      return i;
  }
  return -1;
}

inline const AnimationDef* get(int id) {
  if (id < 0 || id >= (int)animations.size())
    return nullptr;
  return &animations[id];
}

inline Texture2D* get_frame(int anim_id, float elapsed, bool looping) {
  if (anim_id < 0 || anim_id >= (int)animations.size())
    return nullptr;
  auto& def = animations[anim_id];
  if (def.frames.empty())
    return nullptr;
  int total = (int)def.frames.size();
  int idx = (int)(elapsed / def.frame_duration);
  if (looping) {
    idx = idx % total;
  } else {
    if (idx >= total)
      idx = total - 1;
  }
  return &def.frames[idx];
}

inline void unload_all() {
  for (auto& def : animations)
    for (auto& tex : def.frames)
      UnloadTexture(tex);
  animations.clear();
}

} // namespace AnimationRegistry
