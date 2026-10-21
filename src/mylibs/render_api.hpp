#pragma once
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <vector>
#include "shader_portable.hpp"
#if defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>
#endif

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
  unsigned int visible : 1 = 1;
  unsigned int highlighted : 1 = 0;
  unsigned int selected : 1 = 0;
  unsigned int wireframe : 1 = 0;
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

// ---- depth view (dev tool) ----
// Every layer's framebuffer gets a *depth texture* instead of raylib's default depth
// renderbuffer, so the depth can be sampled later (depth view, depth-of-field blur). Works on
// desktop GL and on the web under WebGL 2; depth_available() is false if the FBOs fell back.
inline bool depth_view = false;      // rasterize() shows linear depth instead of the color layers
inline float depth_vis_far = 60.0f;  // world units mapped to white in the depth view
inline Shader depth_shader = {0};
inline int depth_loc_near = -1, depth_loc_far = -1, depth_loc_vis = -1;

// ---- depth-of-field blur ----
// radius_px = |view_depth - dof_focus| * dof_strength, clamped to dof_max_radius. The color layers
// are composited into dof_scene, the layer depths MIN-combined into dof_depth (R32F, linear depth /
// zFar), then one disc-gather blur pass draws the result on the backbuffer.
inline bool dof_enabled = false;
inline float dof_focus = 8.0f;        // world units along the view axis
inline float dof_strength = 3.0f;     // blur px per world unit away from focus
inline float dof_max_radius = 16.0f;  // px clamp
inline bool dof_show_radius = false;  // debug: draw radius / max as gray instead of the blurred scene
inline RenderTexture2D dof_scene = {0}; // RGBA8: every used color layer composited
inline RenderTexture2D dof_depth = {0}; // R32F: min over layers of linear depth / zFar (1.0 = nothing drawn)
inline Shader dof_shader = {0};
inline int dof_loc_depth = -1, dof_loc_texel = -1, dof_loc_far = -1, dof_loc_focus = -1,
           dof_loc_strength = -1, dof_loc_max = -1, dof_loc_show = -1;

inline bool layers_depth_tex = false; // every layer FBO got a sampleable depth texture

inline bool depth_available() {
  return depth_shader.id != 0 && layers_depth_tex;
}

inline bool dof_available() {
  return depth_available() && dof_shader.id != 0 && dof_scene.id != 0 && dof_depth.id != 0;
}

// GLSL 330: sample the depth texture, linearize with the projection's near/far, map to gray.
// visFar <= 0 instead writes the unclamped lin / zFar (used to fill the R32F dof_depth target).
inline const char* DEPTH_VIS_FS = R"(#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D texture0;
uniform float zNear;
uniform float zFar;
uniform float visFar;
void main() {
  float d = texture(texture0, fragTexCoord).r;
  float ndc = d * 2.0 - 1.0;
  float lin = (2.0 * zNear * zFar) / (zFar + zNear - ndc * (zFar - zNear));
  float g = visFar > 0.0 ? clamp(lin / visFar, 0.0, 1.0) : lin / zFar;
  finalColor = vec4(g, g, g, 1.0);
}
)";

// GLSL 330 depth-of-field: disc-gather blur whose radius comes from the per-pixel depth.
// texture0 = composited color, depthTex = linear depth / zFar. Alpha-weighted so transparent
// (undrawn) pixels do not darken edges; the result is alpha-blended over the backbuffer like the
// plain composite. A tap only contributes if its *own* radius reaches this pixel, which stops an
// in-focus object smearing over a blurred background.
inline const char* DOF_FS = R"(#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D texture0;
uniform sampler2D depthTex;
uniform vec2 texel;
uniform float zFar;
uniform float focusDist;
uniform float strength;
uniform float maxRadius;
uniform int showRadius;
float radius_at(vec2 uv) {
  float d = texture(depthTex, uv).r * zFar;
  return min(abs(d - focusDist) * strength, maxRadius);
}
void main() {
  float r = radius_at(fragTexCoord);
  if (showRadius == 1) { finalColor = vec4(vec3(r / maxRadius), 1.0); return; }
  if (r < 0.5) { finalColor = texture(texture0, fragTexCoord); return; }
  const int N = 24;
  vec4 acc = vec4(0.0);
  float wsum = 0.0;
  for (int i = 0; i < N; i++) {
    float t = (float(i) + 0.5) / float(N);
    float a = float(i) * 2.39996323;
    vec2 off = vec2(cos(a), sin(a)) * sqrt(t) * r;
    vec2 uv = fragTexCoord + off * texel;
    float w = radius_at(uv) + 0.5 >= length(off) ? 1.0 : 0.0;
    vec4 c = texture(texture0, uv);
    acc += vec4(c.rgb * c.a, c.a) * w;
    wsum += w;
  }
  vec3 rgb = acc.a > 0.0 ? acc.rgb / acc.a : vec3(0.0);
  finalColor = vec4(rgb, acc.a / max(wsum, 1.0));
}
)";

#if defined(__EMSCRIPTEN__)
inline unsigned int load_depth_texture_webgl2(int width, int height) {
  GLuint id = 0;
  glGenTextures(1, &id);
  glBindTexture(GL_TEXTURE_2D, id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT,
               GL_UNSIGNED_INT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);
  return id;
}
#endif

/// raylib's LoadRenderTexture with a chosen color format and, optionally, a sampleable depth
/// *texture* instead of the stock depth renderbuffer (pattern from the shaders_write_depth
/// example). Falls back to LoadRenderTexture if the FBO fails.
inline RenderTexture2D load_render_texture_ex(int width, int height, int color_format,
                                              bool with_depth_tex, bool* custom_ok = nullptr) {
  if (custom_ok)
    *custom_ok = false;
  RenderTexture2D target = {0};
  target.id = rlLoadFramebuffer();
  if (target.id == 0)
    return LoadRenderTexture(width, height);
  rlEnableFramebuffer(target.id);
  target.texture.id = rlLoadTexture(nullptr, width, height, color_format, 1);
  target.texture.width = width;
  target.texture.height = height;
  target.texture.format = color_format;
  target.texture.mipmaps = 1;
  rlFramebufferAttach(target.id, target.texture.id, RL_ATTACHMENT_COLOR_CHANNEL0, RL_ATTACHMENT_TEXTURE2D, 0);
  if (with_depth_tex) {
#if defined(__EMSCRIPTEN__)
    // WebGL 2 rejects the unsized GL_DEPTH_COMPONENT that rlLoadTextureDepth uses for WebGL
    // ("texImage2D: invalid internalformat"); it needs the sized DEPTH_COMPONENT24.
    target.depth.id = load_depth_texture_webgl2(width, height);
#else
    target.depth.id = rlLoadTextureDepth(width, height, false);
#endif
    target.depth.width = width;
    target.depth.height = height;
    target.depth.format = 19; // DEPTH_COMPONENT_24BIT
    target.depth.mipmaps = 1;
    rlFramebufferAttach(target.id, target.depth.id, RL_ATTACHMENT_DEPTH, RL_ATTACHMENT_TEXTURE2D, 0);
  }
  bool ok = rlFramebufferComplete(target.id);
  rlDisableFramebuffer();
  if (!ok) {
    TraceLog(LOG_WARNING, "RENDERAPI: custom FBO (format %d, depth tex %d) incomplete, using stock render texture",
             color_format, (int)with_depth_tex);
    rlUnloadTexture(target.texture.id);
    if (target.depth.id)
      rlUnloadTexture(target.depth.id);
    rlUnloadFramebuffer(target.id);
    return LoadRenderTexture(width, height);
  }
  if (custom_ok)
    *custom_ok = true;
  return target;
}

/// Every render layer uses this: RGBA8 color plus a depth texture the depth view / DoF can sample.
inline RenderTexture2D load_render_texture_depth_tex(int width, int height, bool* ok = nullptr) {
  return load_render_texture_ex(width, height, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, true, ok);
}

inline void unload_render_texture_depth_tex(RenderTexture2D target) {
  // UnloadRenderTexture handles both: it deletes the color texture and the framebuffer, and
  // rlUnloadFramebuffer queries + deletes the depth attachment whether texture or renderbuffer.
  UnloadRenderTexture(target);
}

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
  layers_depth_tex = count > 0;
  for (int i = 0; i < count; i++) {
    bool ok = false;
    layers[i].texture = load_render_texture_depth_tex(screen_w, screen_h, &ok);
    layers[i].used = false;
    layers_depth_tex = layers_depth_tex && ok;
  }
  depth_shader = ShaderPortable::load_memory(nullptr, DEPTH_VIS_FS);
  if (depth_shader.id != rlGetShaderIdDefault()) {
    depth_loc_near = GetShaderLocation(depth_shader, "zNear");
    depth_loc_far = GetShaderLocation(depth_shader, "zFar");
    depth_loc_vis = GetShaderLocation(depth_shader, "visFar");
  } else {
    depth_shader = {0};
    TraceLog(LOG_WARNING, "RENDERAPI: depth view shader failed to compile");
  }
  bool scene_ok = false, depth_ok = false;
  dof_scene = load_render_texture_ex(screen_w, screen_h, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, true, &scene_ok);
  dof_depth = load_render_texture_ex(screen_w, screen_h, PIXELFORMAT_UNCOMPRESSED_R32, false, &depth_ok);
  if (!scene_ok || !depth_ok) { // a stock RGBA8 fallback would quantize the depth; disable instead
    UnloadRenderTexture(dof_scene);
    UnloadRenderTexture(dof_depth);
    dof_scene = {0};
    dof_depth = {0};
  }
  dof_shader = ShaderPortable::load_memory(nullptr, DOF_FS);
  if (dof_shader.id != rlGetShaderIdDefault()) {
    dof_loc_depth = GetShaderLocation(dof_shader, "depthTex");
    dof_loc_texel = GetShaderLocation(dof_shader, "texel");
    dof_loc_far = GetShaderLocation(dof_shader, "zFar");
    dof_loc_focus = GetShaderLocation(dof_shader, "focusDist");
    dof_loc_strength = GetShaderLocation(dof_shader, "strength");
    dof_loc_max = GetShaderLocation(dof_shader, "maxRadius");
    dof_loc_show = GetShaderLocation(dof_shader, "showRadius");
  } else {
    dof_shader = {0};
    TraceLog(LOG_WARNING, "RENDERAPI: depth-of-field shader failed to compile");
  }
  TraceLog(LOG_INFO, "RENDERAPI: depth view %s, depth-of-field %s", depth_available() ? "available" : "unavailable",
           dof_available() ? "available" : "unavailable");
}

inline void shutdown() {
  close_layer();
  for (auto& data : layers) {
    unload_render_texture_depth_tex(data.texture);
  }
  layers.clear();
  layer_count = 0;
  layers_depth_tex = false;
  if (depth_shader.id)
    UnloadShader(depth_shader);
  depth_shader = {0};
  if (dof_shader.id)
    UnloadShader(dof_shader);
  dof_shader = {0};
  if (dof_scene.id)
    UnloadRenderTexture(dof_scene);
  dof_scene = {0};
  if (dof_depth.id)
    UnloadRenderTexture(dof_depth);
  dof_depth = {0};
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

/// MIN-blend every used layer's depth texture through depth_shader onto the current target:
/// each layer has its own depth buffer, so the nearest surface across all layers wins.
/// vis_far > 0 maps depth to gray (depth view); vis_far <= 0 writes linear depth / zFar (DoF).
/// Leaves the layers' `used` flags alone.
inline void draw_depth_min(float vis_far) {
  float z_near = (float)RL_CULL_DISTANCE_NEAR, z_far = (float)RL_CULL_DISTANCE_FAR;
  SetShaderValue(depth_shader, depth_loc_near, &z_near, SHADER_UNIFORM_FLOAT);
  SetShaderValue(depth_shader, depth_loc_far, &z_far, SHADER_UNIFORM_FLOAT);
  SetShaderValue(depth_shader, depth_loc_vis, &vis_far, SHADER_UNIFORM_FLOAT);
  rlSetBlendFactors(RL_ONE, RL_ONE, RL_MIN);
  BeginBlendMode(BLEND_CUSTOM);
  BeginShaderMode(depth_shader);
  for (int i = 0; i < layer_count; i++) {
    auto& data = layers[i];
    if (!data.used)
      continue;
    DrawTextureRec(data.texture.depth,
                   {0, 0, (float)data.texture.depth.width, (float)-data.texture.depth.height}, {0, 0},
                   WHITE);
  }
  EndShaderMode();
  EndBlendMode();
}

inline void clear_used() {
  for (auto& data : layers)
    data.used = false;
}

/// Depth view: white = far, black = near.
inline void rasterize_depth() {
  DrawRectangle(0, 0, screen_w, screen_h, WHITE);
  draw_depth_min(depth_vis_far);
  clear_used();
}

/// Draw every used color layer, in order, onto the current target.
inline void draw_color_layers() {
  for (int i = 0; i < layer_count; i++) {
    auto& data = layers[i];
    if (!data.used)
      continue;
    DrawTextureRec(data.texture.texture,
                   {0, 0, (float)data.texture.texture.width, (float)-data.texture.texture.height},
                   {0, 0}, WHITE);
  }
}

/// Depth-of-field: composite color -> dof_scene, min depth -> dof_depth, blur onto the backbuffer.
inline void rasterize_dof() {
  BeginTextureMode(dof_scene);
  ClearBackground(BLANK);
  draw_color_layers();
  EndTextureMode();

  BeginTextureMode(dof_depth);
  ClearBackground(WHITE); // r = 1.0 = zFar where nothing was drawn
  draw_depth_min(0.0f);
  EndTextureMode();

  float z_far = (float)RL_CULL_DISTANCE_FAR;
  Vector2 texel = {1.0f / (float)screen_w, 1.0f / (float)screen_h};
  int show = dof_show_radius ? 1 : 0;
  BeginShaderMode(dof_shader);
  SetShaderValue(dof_shader, dof_loc_texel, &texel, SHADER_UNIFORM_VEC2);
  SetShaderValue(dof_shader, dof_loc_far, &z_far, SHADER_UNIFORM_FLOAT);
  SetShaderValue(dof_shader, dof_loc_focus, &dof_focus, SHADER_UNIFORM_FLOAT);
  SetShaderValue(dof_shader, dof_loc_strength, &dof_strength, SHADER_UNIFORM_FLOAT);
  SetShaderValue(dof_shader, dof_loc_max, &dof_max_radius, SHADER_UNIFORM_FLOAT);
  SetShaderValue(dof_shader, dof_loc_show, &show, SHADER_UNIFORM_INT);
  // extra sampler: registered in the batch and bound to unit 1 on the flush this draw causes
  SetShaderValueTexture(dof_shader, dof_loc_depth, dof_depth.texture);
  DrawTextureRec(dof_scene.texture,
                 {0, 0, (float)dof_scene.texture.width, (float)-dof_scene.texture.height}, {0, 0},
                 WHITE);
  EndShaderMode();
  clear_used();
}

inline void rasterize() {
  close_layer();
  if (depth_view && depth_available()) {
    rasterize_depth();
    return;
  }
  if (dof_enabled && dof_available()) {
    rasterize_dof();
    return;
  }
  draw_color_layers();
  clear_used();
}


} // namespace RenderAPI
