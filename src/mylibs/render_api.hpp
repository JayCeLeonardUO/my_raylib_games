#pragma once
#include <raylib.h>
#include <raymath.h>
#include <vector>

// ============================================================================
// Render Layers
// ============================================================================

enum class RenderLayer : int {
  Background = 0,
  World = 1,
  Entities = 2,
  Highlight = 3,
  Focus = 4,
  Effects = 5,
  UI_World = 6,
  Overlay = 7,
  Debug = 8,
  COUNT = 9
};

// ============================================================================
// Render Traits - per-entity render state
// ============================================================================

struct RenderTraits {
  RenderLayer layer = RenderLayer::Entities;
  uint visible : 1 = 1;
  uint highlighted : 1 = 0;
  uint selected : 1 = 0;
  uint wireframe : 1 = 0;
};

// ============================================================================
// RenderAPI - namespace-style render layer management
// ============================================================================

namespace RenderAPI {

struct LayerData {
  RenderTexture2D texture = {0};
  bool used = false;
};

inline std::vector<LayerData> layers;
inline int open_layer_id = -1;
inline int screen_w = 0;
inline int screen_h = 0;
inline int layer_count = 0;

inline void close_layer() {
  if (open_layer_id >= 0) {
    EndMode3D();
    EndTextureMode();
    open_layer_id = -1;
  }
}

inline void init(int count = static_cast<int>(RenderLayer::COUNT)) {
  screen_w = GetScreenWidth();
  screen_h = GetScreenHeight();
  layer_count = count;
  layers.resize(count);
  for (int i = 0; i < count; i++) {
    layers[i].texture = LoadRenderTexture(screen_w, screen_h);
    layers[i].used = false;
  }
}

inline void shutdown() {
  close_layer();
  for (auto& data : layers) {
    UnloadRenderTexture(data.texture);
  }
  layers.clear();
  layer_count = 0;
}

inline void layer_start(RenderLayer layer, Camera3D& cam) {
  close_layer();
  int idx = static_cast<int>(layer);
  if (idx < 0 || idx >= layer_count)
    return;

  auto& data = layers[idx];
  BeginTextureMode(data.texture);
  ClearBackground(BLANK);
  BeginMode3D(cam);
  data.used = true;
  open_layer_id = idx;
}

inline void layer_start(int layer, Camera3D& cam) {
  close_layer();
  if (layer < 0 || layer >= layer_count)
    return;

  auto& data = layers[layer];
  BeginTextureMode(data.texture);
  ClearBackground(BLANK);
  BeginMode3D(cam);
  data.used = true;
  open_layer_id = layer;
}

// Start a layer in 2D mode (no BeginMode3D) for screen-space overlays
inline void layer_start_2d(RenderLayer layer) {
  close_layer();
  int idx = static_cast<int>(layer);
  if (idx < 0 || idx >= layer_count)
    return;

  auto& data = layers[idx];
  BeginTextureMode(data.texture);
  ClearBackground(BLANK);
  data.used = true;
  open_layer_id = -1; // not a 3D layer, so close_layer won't call EndMode3D
}

inline void end_2d_layer() {
  EndTextureMode();
}

inline void rasterize() {
  close_layer();
  for (int i = 0; i < layer_count; i++) {
    auto& data = layers[i];
    if (!data.used)
      continue;
    DrawTextureRec(data.texture.texture,
                   {0, 0, (float)data.texture.texture.width, (float)-data.texture.texture.height},
                   {0, 0}, WHITE);
    data.used = false;
  }
}

} // namespace RenderAPI
