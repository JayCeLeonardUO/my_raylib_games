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
  Bloom = 3,      // bloom source: drawn here glows (not composited as color, see LayerData)
  Highlight = 4,
  Focus = 5,
  Effects = 6,
  UI_World = 7,
  Overlay = 8,
  Debug = 9,
  COUNT = 10
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
  bool used = false;  // drawn into since the last rasterize()
  bool drawn = false; // snapshot of `used` from the last rasterize(), for the buffer list
  bool bloom_source = false; // feeds the bloom pass only; draw_color_layers() skips it
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

// ---- atmospheric fog ----
// One full-screen pass after the color composite: reads the nearest linear depth (dof_depth) and
// draws fog_color with alpha = 1 - exp(-(depth - fog_start) * fog_density), capped at fog_max.
// The default alpha blend mixes it over the frame already on the backbuffer, so nothing is
// overwritten. Needs the same depth texture as the DoF pass, so it gates on dof_available().
inline bool fog_enabled = false;
inline Color fog_color = {170, 190, 215, 255};
inline float fog_start = 8.0f;     // world units: untouched closer than this
inline float fog_density = 0.05f;  // falloff per unit beyond fog_start
inline float fog_max = 0.9f;       // alpha cap so far things never vanish completely
inline bool fog_sky = true;        // tint pixels where nothing was drawn (depth = zFar) too
inline Shader fog_shader = {0};
inline int fog_loc_far = -1, fog_loc_color = -1, fog_loc_start = -1, fog_loc_density = -1,
           fog_loc_max = -1, fog_loc_sky = -1;

// ---- bloom ----
// Whatever is drawn into the Bloom layer is a light source. The layer is masked by depth
// (occluded emitters do not glow through things), downsampled to half resolution, blurred with a
// separable gaussian, and added onto the frame. There is no HDR headroom (RGBA8 everywhere), so
// the explicit layer replaces the brightness threshold of a classic HDR bloom.
inline bool bloom_enabled = false;
inline float bloom_intensity = 1.0f;  // gain on the added glow
inline float bloom_radius = 1.0f;     // blur step in half-res texels
inline int bloom_passes = 3;          // horizontal+vertical gaussian pairs
inline bool bloom_show_source = false; // debug: show the masked bloom source instead of the frame
inline RenderTexture2D bloom_a = {0}; // RGBA8, half res, ping
inline RenderTexture2D bloom_b = {0}; // RGBA8, half res, pong
inline Shader bloom_mask_shader = {0};
inline Shader bloom_blur_shader = {0};
inline int bloom_loc_depth = -1, bloom_loc_min = -1, bloom_loc_near = -1, bloom_loc_far = -1;
inline int blur_loc_texel = -1, blur_loc_dir = -1, blur_loc_radius = -1, blur_loc_gain = -1;

inline bool layers_depth_tex = false; // every layer FBO got a sampleable depth texture

// Set by a game that runs its own screen-space pass after rasterize(): forces dof_depth (nearest
// linear depth / zFar over the layers) to be built even when DoF, fog and bloom are all off.
inline bool want_depth_min = false;

// ---- buffer list (dev tool) ----
// Which of the off-screen targets the last rasterize() actually wrote. Reset at the top of
// rasterize(), set by the pass that fills the target; buffer_list() reports them.
inline bool dof_scene_drawn = false, dof_depth_drawn = false, bloom_a_drawn = false, bloom_b_drawn = false;

inline bool depth_available() {
  return depth_shader.id != 0 && layers_depth_tex;
}

inline bool dof_available() {
  return depth_available() && dof_shader.id != 0 && dof_scene.id != 0 && dof_depth.id != 0;
}

inline bool fog_available() {
  return dof_available() && fog_shader.id != 0;
}

inline bool bloom_available() {
  return dof_available() && bloom_mask_shader.id != 0 && bloom_blur_shader.id != 0 && bloom_a.id != 0 &&
         bloom_b.id != 0 && static_cast<int>(RenderLayer::Bloom) < layer_count;
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
  if (d >= 1.0) { finalColor = vec4(1.0); return; } // cleared (nothing drawn): exactly far
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

// GLSL 330 fog: texture0 = dof_depth (linear depth / zFar). Straight color with alpha = fog
// amount; drawn with the normal blend mode so the hardware does fog*a + scene*(1-a).
inline const char* FOG_FS = R"(#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D texture0;
uniform float zFar;
uniform vec3 fogColor;
uniform float fogStart;
uniform float fogDensity;
uniform float fogMax;
uniform int fogSky;
void main() {
  float raw = texture(texture0, fragTexCoord).r;
  if (fogSky == 0 && raw > 0.9999) discard;
  float d = raw * zFar;
  float f = 1.0 - exp(-max(d - fogStart, 0.0) * fogDensity);
  finalColor = vec4(fogColor, min(f, fogMax));
}
)";

// GLSL 330 bloom mask: texture0 = Bloom layer color, bloomDepth = that layer's depth texture,
// minDepth = nearest linear depth / zFar across all layers (dof_depth). A pixel only glows if the
// emitter is the nearest surface there, so bloom does not leak through occluders.
inline const char* BLOOM_MASK_FS = R"(#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D texture0;
uniform sampler2D bloomDepth;
uniform sampler2D minDepth;
uniform float zNear;
uniform float zFar;
void main() {
  vec4 c = texture(texture0, fragTexCoord);
  float d = texture(bloomDepth, fragTexCoord).r;
  float lin = 1.0;
  if (d < 1.0) {
    float ndc = d * 2.0 - 1.0;
    lin = (2.0 * zNear * zFar) / (zFar + zNear - ndc * (zFar - zNear)) / zFar;
  }
  float nearest = texture(minDepth, fragTexCoord).r;
  if (c.a <= 0.0 || lin > nearest + 0.002) { finalColor = vec4(0.0); return; }
  finalColor = vec4(c.rgb * c.a, c.a);
}
)";

// GLSL 330 separable gaussian (9 taps). dir = (1,0) or (0,1); step = dir * texel * radius.
// gain scales the result, so the final pass can also apply the bloom intensity.
inline const char* BLOOM_BLUR_FS = R"(#version 330
in vec2 fragTexCoord;
out vec4 finalColor;
uniform sampler2D texture0;
uniform vec2 texel;
uniform vec2 dir;
uniform float radius;
uniform float gain;
void main() {
  float w[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
  vec2 step = dir * texel * radius;
  vec4 sum = texture(texture0, fragTexCoord) * w[0];
  for (int i = 1; i < 5; i++) {
    sum += texture(texture0, fragTexCoord + step * float(i)) * w[i];
    sum += texture(texture0, fragTexCoord - step * float(i)) * w[i];
  }
  finalColor = sum * gain;
}
)";

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
  if (static_cast<int>(RenderLayer::Bloom) < count)
    layers[static_cast<int>(RenderLayer::Bloom)].bloom_source = true;
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
  fog_shader = ShaderPortable::load_memory(nullptr, FOG_FS);
  if (fog_shader.id != rlGetShaderIdDefault()) {
    fog_loc_far = GetShaderLocation(fog_shader, "zFar");
    fog_loc_color = GetShaderLocation(fog_shader, "fogColor");
    fog_loc_start = GetShaderLocation(fog_shader, "fogStart");
    fog_loc_density = GetShaderLocation(fog_shader, "fogDensity");
    fog_loc_max = GetShaderLocation(fog_shader, "fogMax");
    fog_loc_sky = GetShaderLocation(fog_shader, "fogSky");
  } else {
    fog_shader = {0};
    TraceLog(LOG_WARNING, "RENDERAPI: fog shader failed to compile");
  }
  bool bloom_a_ok = false, bloom_b_ok = false;
  int bw = screen_w > 1 ? screen_w / 2 : 1, bh = screen_h > 1 ? screen_h / 2 : 1;
  bloom_a = load_render_texture_ex(bw, bh, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, false, &bloom_a_ok);
  bloom_b = load_render_texture_ex(bw, bh, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, false, &bloom_b_ok);
  if (!bloom_a_ok || !bloom_b_ok) {
    UnloadRenderTexture(bloom_a);
    UnloadRenderTexture(bloom_b);
    bloom_a = {0};
    bloom_b = {0};
  } else {
    // bilinear: the half-res blur and the upscale to the screen both lean on it
    SetTextureFilter(bloom_a.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(bloom_b.texture, TEXTURE_FILTER_BILINEAR);
  }
  bloom_mask_shader = ShaderPortable::load_memory(nullptr, BLOOM_MASK_FS);
  if (bloom_mask_shader.id != rlGetShaderIdDefault()) {
    bloom_loc_depth = GetShaderLocation(bloom_mask_shader, "bloomDepth");
    bloom_loc_min = GetShaderLocation(bloom_mask_shader, "minDepth");
    bloom_loc_near = GetShaderLocation(bloom_mask_shader, "zNear");
    bloom_loc_far = GetShaderLocation(bloom_mask_shader, "zFar");
  } else {
    bloom_mask_shader = {0};
    TraceLog(LOG_WARNING, "RENDERAPI: bloom mask shader failed to compile");
  }
  bloom_blur_shader = ShaderPortable::load_memory(nullptr, BLOOM_BLUR_FS);
  if (bloom_blur_shader.id != rlGetShaderIdDefault()) {
    blur_loc_texel = GetShaderLocation(bloom_blur_shader, "texel");
    blur_loc_dir = GetShaderLocation(bloom_blur_shader, "dir");
    blur_loc_radius = GetShaderLocation(bloom_blur_shader, "radius");
    blur_loc_gain = GetShaderLocation(bloom_blur_shader, "gain");
  } else {
    bloom_blur_shader = {0};
    TraceLog(LOG_WARNING, "RENDERAPI: bloom blur shader failed to compile");
  }
  TraceLog(LOG_INFO, "RENDERAPI: depth view %s, depth-of-field %s, fog %s, bloom %s",
           depth_available() ? "available" : "unavailable", dof_available() ? "available" : "unavailable",
           fog_available() ? "available" : "unavailable", bloom_available() ? "available" : "unavailable");
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
  if (fog_shader.id)
    UnloadShader(fog_shader);
  fog_shader = {0};
  if (bloom_mask_shader.id)
    UnloadShader(bloom_mask_shader);
  bloom_mask_shader = {0};
  if (bloom_blur_shader.id)
    UnloadShader(bloom_blur_shader);
  bloom_blur_shader = {0};
  if (bloom_a.id)
    UnloadRenderTexture(bloom_a);
  bloom_a = {0};
  if (bloom_b.id)
    UnloadRenderTexture(bloom_b);
  bloom_b = {0};
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
  for (auto& data : layers) {
    data.drawn = data.used;
    data.used = false;
  }
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
    if (!data.used || data.bloom_source)
      continue;
    DrawTextureRec(data.texture.texture,
                   {0, 0, (float)data.texture.texture.width, (float)-data.texture.texture.height},
                   {0, 0}, WHITE);
  }
}

/// Nearest depth across the used layers -> dof_depth (r = linear depth / zFar, 1.0 = nothing drawn).
inline void build_depth_min() {
  dof_depth_drawn = true;
  BeginTextureMode(dof_depth);
  ClearBackground(WHITE);
  draw_depth_min(0.0f);
  EndTextureMode();
}

/// Depth-of-field: composite color -> dof_scene, min depth -> dof_depth, blur onto the current
/// target. Leaves `used` set and dof_depth valid so fog can follow.
inline void rasterize_dof() {
  dof_scene_drawn = true;
  BeginTextureMode(dof_scene);
  ClearBackground(BLANK);
  draw_color_layers();
  EndTextureMode();

  build_depth_min();

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
}

/// Fog: tint the frame already on the current target by depth. Expects dof_depth to be filled.
inline void draw_fog() {
  float z_far = (float)RL_CULL_DISTANCE_FAR;
  Vector3 c = {fog_color.r / 255.0f, fog_color.g / 255.0f, fog_color.b / 255.0f};
  int sky = fog_sky ? 1 : 0;
  BeginShaderMode(fog_shader);
  SetShaderValue(fog_shader, fog_loc_far, &z_far, SHADER_UNIFORM_FLOAT);
  SetShaderValue(fog_shader, fog_loc_color, &c, SHADER_UNIFORM_VEC3);
  SetShaderValue(fog_shader, fog_loc_start, &fog_start, SHADER_UNIFORM_FLOAT);
  SetShaderValue(fog_shader, fog_loc_density, &fog_density, SHADER_UNIFORM_FLOAT);
  SetShaderValue(fog_shader, fog_loc_max, &fog_max, SHADER_UNIFORM_FLOAT);
  SetShaderValue(fog_shader, fog_loc_sky, &sky, SHADER_UNIFORM_INT);
  // the depth texture is texture0; the quad covers the screen; default alpha blend does the mix
  DrawTextureRec(dof_depth.texture,
                 {0, 0, (float)dof_depth.texture.width, (float)-dof_depth.texture.height}, {0, 0},
                 WHITE);
  EndShaderMode();
}

/// One gaussian pass of `src` into the current target. `dst_w/h` size the quad (half res for the
/// ping-pong, screen size for the final upscale).
inline void bloom_blur_pass(const RenderTexture2D& src, Vector2 dir, float gain, int dst_w, int dst_h) {
  Vector2 texel = {1.0f / (float)src.texture.width, 1.0f / (float)src.texture.height};
  SetShaderValue(bloom_blur_shader, blur_loc_texel, &texel, SHADER_UNIFORM_VEC2);
  SetShaderValue(bloom_blur_shader, blur_loc_dir, &dir, SHADER_UNIFORM_VEC2);
  SetShaderValue(bloom_blur_shader, blur_loc_radius, &bloom_radius, SHADER_UNIFORM_FLOAT);
  SetShaderValue(bloom_blur_shader, blur_loc_gain, &gain, SHADER_UNIFORM_FLOAT);
  BeginShaderMode(bloom_blur_shader);
  DrawTexturePro(src.texture, {0, 0, (float)src.texture.width, (float)-src.texture.height},
                 {0, 0, (float)dst_w, (float)dst_h}, {0, 0}, 0.0f, WHITE);
  EndShaderMode();
}

/// Bloom: mask the Bloom layer by depth into bloom_a (half res), blur a<->b, add onto the current
/// target. Expects dof_depth to be filled. Leaves the blend mode as it found it.
inline void draw_bloom() {
  auto& src = layers[static_cast<int>(RenderLayer::Bloom)].texture;
  int bw = bloom_a.texture.width, bh = bloom_a.texture.height;
  float z_near = (float)RL_CULL_DISTANCE_NEAR, z_far = (float)RL_CULL_DISTANCE_FAR;

  // 1. masked, premultiplied source at half res. The layer is drawn opaque (BLEND_ALPHA would
  //    re-multiply by alpha); the mask shader already folds alpha into rgb.
  bloom_a_drawn = true;
  BeginTextureMode(bloom_a);
  ClearBackground(BLANK);
  rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);
  BeginBlendMode(BLEND_CUSTOM);
  BeginShaderMode(bloom_mask_shader);
  SetShaderValue(bloom_mask_shader, bloom_loc_near, &z_near, SHADER_UNIFORM_FLOAT);
  SetShaderValue(bloom_mask_shader, bloom_loc_far, &z_far, SHADER_UNIFORM_FLOAT);
  SetShaderValueTexture(bloom_mask_shader, bloom_loc_depth, src.depth);
  SetShaderValueTexture(bloom_mask_shader, bloom_loc_min, dof_depth.texture);
  DrawTexturePro(src.texture, {0, 0, (float)src.texture.width, (float)-src.texture.height},
                 {0, 0, (float)bw, (float)bh}, {0, 0}, 0.0f, WHITE);
  EndShaderMode();
  EndBlendMode();
  EndTextureMode();

  if (bloom_show_source) {
    DrawTexturePro(bloom_a.texture, {0, 0, (float)bw, (float)-bh}, {0, 0, (float)screen_w, (float)screen_h},
                   {0, 0}, 0.0f, WHITE);
    return;
  }

  // 2. separable gaussian ping-pong (premultiplied data: copy, do not alpha-blend)
  bloom_b_drawn = true;
  rlSetBlendFactors(RL_ONE, RL_ZERO, RL_FUNC_ADD);
  BeginBlendMode(BLEND_CUSTOM);
  int passes = bloom_passes < 1 ? 1 : bloom_passes;
  for (int i = 0; i < passes - 1; i++) {
    BeginTextureMode(bloom_b);
    bloom_blur_pass(bloom_a, {1, 0}, 1.0f, bw, bh);
    EndTextureMode();
    BeginTextureMode(bloom_a);
    bloom_blur_pass(bloom_b, {0, 1}, 1.0f, bw, bh);
    EndTextureMode();
  }
  BeginTextureMode(bloom_b);
  bloom_blur_pass(bloom_a, {1, 0}, 1.0f, bw, bh);
  EndTextureMode();
  EndBlendMode();

  // 3. last vertical pass straight onto the frame, scaled by the intensity. Plain ONE+ONE add:
  //    the data is premultiplied, so raylib's BLEND_ADDITIVE (SRC_ALPHA, ONE) would dim the edges.
  rlSetBlendFactors(RL_ONE, RL_ONE, RL_FUNC_ADD);
  BeginBlendMode(BLEND_CUSTOM);
  bloom_blur_pass(bloom_b, {0, 1}, bloom_intensity, screen_w, screen_h);
  EndBlendMode();
}

inline void rasterize() {
  close_layer();
  dof_scene_drawn = dof_depth_drawn = bloom_a_drawn = bloom_b_drawn = false;
  if (depth_view && depth_available()) {
    rasterize_depth();
    return;
  }
  bool fog = fog_enabled && fog_available();
  bool bloom = bloom_enabled && bloom_available() && layers[static_cast<int>(RenderLayer::Bloom)].used;
  if (dof_enabled && dof_available()) {
    rasterize_dof(); // fills dof_depth on the way
  } else {
    draw_color_layers();
    if (fog || bloom || (want_depth_min && dof_available()))
      build_depth_min();
  }
  if (bloom)
    draw_bloom(); // before fog, so the glow is fogged by depth like everything else
  if (fog)
    draw_fog();
  clear_used();
}

// ============================================================================
// Buffer list (dev tool): every off-screen target and whether the last frame wrote it
// ============================================================================

inline const char* layer_name(int layer) {
  static const char* names[] = {"Background", "World", "Entities", "Bloom", "Highlight",
                                "Focus", "Effects", "UI_World", "Overlay", "Debug"};
  if (layer < 0 || layer >= static_cast<int>(RenderLayer::COUNT))
    return "?";
  return names[layer];
}

inline const char* pixel_format_name(int format) {
  switch (format) {
  case PIXELFORMAT_UNCOMPRESSED_R8G8B8A8: return "RGBA8";
  case PIXELFORMAT_UNCOMPRESSED_R32: return "R32F";
  case PIXELFORMAT_UNCOMPRESSED_R32G32B32A32: return "RGBA32F";
  case PIXELFORMAT_UNCOMPRESSED_R16G16B16A16: return "RGBA16F";
  default: return "other";
  }
}

struct BufferInfo {
  const char* name;
  const char* role;      // what the target holds
  unsigned int fbo = 0;  // 0 = not allocated
  int width = 0, height = 0;
  const char* format = "";
  bool depth_tex = false; // has a sampleable depth texture attached
  bool drawn = false;     // written during the last rasterize()d frame
};

inline BufferInfo buffer_info(const char* name, const char* role, const RenderTexture2D& rt, bool depth_tex,
                              bool drawn) {
  return {name, role, rt.id, rt.texture.width, rt.texture.height,
          rt.id ? pixel_format_name(rt.texture.format) : "", rt.id != 0 && depth_tex, rt.id != 0 && drawn};
}

/// Every off-screen target in pipeline order: the layers (composite order), then the post targets.
inline std::vector<BufferInfo> buffer_list() {
  std::vector<BufferInfo> out;
  for (int i = 0; i < layer_count; i++) {
    auto& data = layers[i];
    out.push_back(buffer_info(layer_name(i), data.bloom_source ? "layer (bloom source, not composited)" : "layer",
                              data.texture, layers_depth_tex, data.drawn));
  }
  out.push_back(buffer_info("dof_depth", "nearest linear depth over the layers (DoF, fog, bloom mask)", dof_depth,
                            false, dof_depth_drawn));
  out.push_back(buffer_info("dof_scene", "composited color, input to the DoF blur", dof_scene, true,
                            dof_scene_drawn));
  out.push_back(buffer_info("bloom_a", "half res: masked bloom source / blur ping", bloom_a, false, bloom_a_drawn));
  out.push_back(buffer_info("bloom_b", "half res: blur pong", bloom_b, false, bloom_b_drawn));
  return out;
}

} // namespace RenderAPI
