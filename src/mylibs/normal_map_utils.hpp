#pragma once
/**
 * @brief Procedural tangent-space normal maps, generated on the CPU at startup so no
 * texture asset is needed (works the same in the Emscripten build).
 */

#include "raylib.h"
#include <cmath>
#include <vector>

namespace NormalMapUtils {

/// Soft two-octave Perlin "stone/plaster" surface as a tangent-space normal map.
/// size must be a power of two (mipmaps + WebGL 1). strength ~1.0 is subtle, 3.0 is bumpy.
inline Texture2D make_noise_normal_map(int size = 1024, float strength = 1.0f, int seed = 7) {
  Image broad = GenImagePerlinNoise(size, size, seed * 13, seed * 7, 5.0f);
  Image fine = GenImagePerlinNoise(size, size, seed * 31, seed * 3, 22.0f);
  Color* pb = LoadImageColors(broad);
  Color* pf = LoadImageColors(fine);

  std::vector<float> h((size_t)size * size);
  for (int i = 0; i < size * size; i++)
    h[(size_t)i] = (pb[i].r / 255.0f) * 0.55f + (pf[i].r / 255.0f) * 0.45f;
  UnloadImageColors(pb);
  UnloadImageColors(pf);
  UnloadImage(broad);
  UnloadImage(fine);

  // central differences, wrapping so the texture tiles; slope scaled by strength
  const float k = strength * 24.0f;
  auto H = [&](int x, int y) { return h[(size_t)((y + size) % size) * size + (x + size) % size]; };
  Color* out = (Color*)RL_MALLOC((size_t)size * size * sizeof(Color));
  for (int y = 0; y < size; y++)
    for (int x = 0; x < size; x++) {
      float dx = (H(x + 1, y) - H(x - 1, y)) * k;
      float dy = (H(x, y + 1) - H(x, y - 1)) * k;
      float nx = -dx, ny = -dy, nz = 1.0f;
      float len = std::sqrt(nx * nx + ny * ny + nz * nz);
      nx /= len; ny /= len; nz /= len;
      out[(size_t)y * size + x] = Color{(unsigned char)((nx * 0.5f + 0.5f) * 255.0f),
                                        (unsigned char)((ny * 0.5f + 0.5f) * 255.0f),
                                        (unsigned char)((nz * 0.5f + 0.5f) * 255.0f), 255};
    }

  Image img = {0};
  img.data = out;
  img.width = size;
  img.height = size;
  img.mipmaps = 1;
  img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
  Texture2D tex = LoadTextureFromImage(img);
  UnloadImage(img); // frees `out`
  GenTextureMipmaps(&tex);
  SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);
  SetTextureWrap(tex, TEXTURE_WRAP_REPEAT);
  return tex;
}

} // namespace NormalMapUtils
