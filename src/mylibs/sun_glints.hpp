#pragma once
// Sun glints: a compute shader test over the triangle particle system.
//
// Each frame the particle transforms, the sun position and the camera go to a GL 4.3 compute
// shader (assets/shaders/glint.comp). It returns, per particle, the screen position + distance,
// with strength 1.0 when the triangle's face mirrors the sun toward the camera. The game then
// draws one uniform additive sprite at every returned screen position.
//
// The same math runs on the CPU when compute is unavailable (web build is WebGL 1, or raylib
// was built without GRAPHICS_API_OPENGL_43). compare_gpu_cpu() checks the two paths agree.

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include "imgui.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if !defined(__EMSCRIPTEN__)
// rlgl has no memory-barrier wrapper; glad exports the function pointer from the raylib lib.
extern "C" {
extern void (*glad_glMemoryBarrier)(unsigned int barriers);
}
#endif

namespace SunGlints {

/// Mirrors the std430 struct written by glint.comp (16 bytes).
struct Glint {
  Vector2 screen;
  float dist;
  float strength;
};

inline constexpr unsigned GL_SHADER_STORAGE_BARRIER_BIT_ = 0x2000;
inline constexpr unsigned GL_BUFFER_UPDATE_BARRIER_BIT_ = 0x0200;

struct System {
  // tunables
  bool enabled = true;
  Vector3 sun_pos = {40.0f, 30.0f, -25.0f};
  float threshold_deg = 8.0f; // max angle between face normal and sun/camera half vector
  float sprite_px = 28.0f;
  float intensity = 1.0f;
  Color color = {255, 250, 220, 255};
  bool use_gpu = true;
  bool show_sun = true;

  // state
  unsigned prog = 0, ssbo_in = 0, ssbo_out = 0;
  int cap = 0;
  bool gpu_ready = false;
  Texture2D sprite = {0};
  std::vector<Glint> results;
  int visible = 0;
  float time = 0.0f;
  bool last_was_gpu = false;

  static bool gpu_available() {
#if defined(__EMSCRIPTEN__)
    return false;
#else
    return rlGetVersion() == RL_OPENGL_43;
#endif
  }

  void init(const char* comp_path = "assets/shaders/glint.comp") {
    sprite = make_sprite(64);
#if !defined(__EMSCRIPTEN__)
    if (gpu_available()) {
      char* code = LoadFileText(comp_path);
      if (code) {
        unsigned sh = rlCompileShader(code, RL_COMPUTE_SHADER);
        if (sh)
          prog = rlLoadComputeShaderProgram(sh);
        UnloadFileText(code);
      }
      gpu_ready = prog != 0;
      TraceLog(gpu_ready ? LOG_INFO : LOG_WARNING, "SUNGLINTS: compute program %s (%s)",
               gpu_ready ? "loaded" : "FAILED, using CPU path", comp_path);
    } else {
      TraceLog(LOG_INFO, "SUNGLINTS: no GL 4.3 compute context, using CPU path");
    }
#endif
  }

  void unload() {
#if !defined(__EMSCRIPTEN__)
    if (ssbo_in)
      rlUnloadShaderBuffer(ssbo_in);
    if (ssbo_out)
      rlUnloadShaderBuffer(ssbo_out);
    if (prog)
      rlUnloadShaderProgram(prog);
#endif
    ssbo_in = ssbo_out = prog = 0;
    cap = 0;
    gpu_ready = false;
    if (sprite.id)
      UnloadTexture(sprite);
    sprite = {0};
  }

  bool gpu_active() const { return use_gpu && gpu_ready; }

  /** @brief Fill `results` for every particle transform; GPU when active, else CPU. */
  void compute(const std::vector<Matrix>& xf, const Camera3D& cam, int w, int h) {
    time += GetFrameTime();
    results.resize(xf.size());
    if (gpu_active())
      compute_gpu(xf, cam, w, h, results);
    else
      compute_cpu(xf, cam, w, h, results);
    last_was_gpu = gpu_active();
    visible = 0;
    for (auto& g : results)
      if (g.strength > 0.0f)
        visible++;
  }

  /** @brief Additive sprites at every glint. Call after RenderAPI::rasterize(), in 2D. */
  void draw_2d() const {
    if (!sprite.id || results.empty())
      return;
    Rectangle src = {0, 0, (float)sprite.width, (float)sprite.height};
    BeginBlendMode(BLEND_ADDITIVE);
    for (size_t i = 0; i < results.size(); i++) {
      const Glint& g = results[i];
      if (g.strength <= 0.0f)
        continue;
      float tw = 0.85f + 0.15f * std::sin(time * 9.0f + (float)i); // gentle twinkle
      float k = intensity * tw * g.strength;
      Color tint = {(unsigned char)Clamp(color.r * k, 0, 255), (unsigned char)Clamp(color.g * k, 0, 255),
                    (unsigned char)Clamp(color.b * k, 0, 255), 255};
      float s = sprite_px * (0.9f + 0.1f * tw);
      Rectangle dst = {g.screen.x - s * 0.5f, g.screen.y - s * 0.5f, s, s};
      DrawTexturePro(sprite, src, dst, {0, 0}, 0.0f, tint);
    }
    EndBlendMode();
  }

  /** @brief Yellow sphere at the sun. Call inside a 3D layer. */
  void draw_sun_marker() const { DrawSphere(sun_pos, 1.5f, YELLOW); }

  /**
   * @brief Run both paths on the same input and report how far apart they are.
   * @return max screen-space delta in pixels over particles either path flagged; -1 if no GPU.
   */
  float compare_gpu_cpu(const std::vector<Matrix>& xf, const Camera3D& cam, int w, int h,
                        int* mismatches = nullptr) {
    if (!gpu_ready)
      return -1.0f;
    std::vector<Glint> g(xf.size()), c(xf.size());
    compute_gpu(xf, cam, w, h, g);
    compute_cpu(xf, cam, w, h, c);
    float max_d = 0.0f;
    int bad = 0;
    for (size_t i = 0; i < xf.size(); i++) {
      bool on_g = g[i].strength > 0.0f, on_c = c[i].strength > 0.0f;
      if (on_g != on_c) {
        bad++;
        continue;
      }
      if (!on_g)
        continue;
      float d = Vector2Distance(g[i].screen, c[i].screen);
      if (d > max_d)
        max_d = d;
    }
    if (mismatches)
      *mismatches = bad;
    return max_d;
  }

  void draw_imgui() {
    ImGui::Checkbox("Enabled", &enabled);
    ImGui::SameLine();
    ImGui::Text("glints: %d / %zu", visible, results.size());
    if (gpu_ready) {
      ImGui::Text("compute: GL 4.3 program %u", prog);
      ImGui::SameLine();
      ImGui::Checkbox("Use GPU", &use_gpu);
    } else {
      ImGui::TextDisabled("compute: CPU fallback (%s)",
                          gpu_available() ? "shader failed to load" : "no GL 4.3 context");
    }
    ImGui::DragFloat3("Sun position", &sun_pos.x, 0.5f);
    ImGui::Checkbox("Show sun", &show_sun);
    ImGui::SliderFloat("Threshold (deg)", &threshold_deg, 0.5f, 30.0f, "%.1f");
    ImGui::SliderFloat("Sprite px", &sprite_px, 4.0f, 96.0f, "%.0f");
    ImGui::SliderFloat("Intensity", &intensity, 0.0f, 3.0f, "%.2f");
    float col[3] = {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f};
    if (ImGui::ColorEdit3("Glint color", col, ImGuiColorEditFlags_NoInputs))
      color = {(unsigned char)(col[0] * 255), (unsigned char)(col[1] * 255), (unsigned char)(col[2] * 255), 255};
  }

  // ---- paths ----

  float cos_threshold() const { return std::cos(threshold_deg * DEG2RAD); }

  /// Same projection GetWorldToScreenEx uses, so GPU and CPU agree to the pixel.
  static Matrix view_proj(const Camera3D& cam, int w, int h) {
    Matrix proj = MatrixPerspective(cam.fovy * DEG2RAD, (double)w / (double)h, RL_CULL_DISTANCE_NEAR,
                                    RL_CULL_DISTANCE_FAR);
    Matrix view = MatrixLookAt(cam.position, cam.target, cam.up);
    return MatrixMultiply(view, proj);
  }

  void compute_cpu(const std::vector<Matrix>& xf, const Camera3D& cam, int w, int h,
                   std::vector<Glint>& out) const {
    out.resize(xf.size());
    float ct = cos_threshold();
    for (size_t i = 0; i < xf.size(); i++) {
      const Matrix& m = xf[i];
      Vector3 pos = {m.m12, m.m13, m.m14};
      Vector3 n = Vector3Normalize({m.m8, m.m9, m.m10}); // local +Z column
      Vector3 hv = Vector3Normalize(Vector3Add(Vector3Normalize(Vector3Subtract(sun_pos, pos)),
                                               Vector3Normalize(Vector3Subtract(cam.position, pos))));
      float a = std::fabs(Vector3DotProduct(n, hv));
      Glint g = {{-1.0f, -1.0f}, Vector3Distance(cam.position, pos), 0.0f};
      // in front of the camera?
      Vector3 fwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
      if (Vector3DotProduct(Vector3Subtract(pos, cam.position), fwd) > 0.0f && a >= ct) {
        g.screen = GetWorldToScreenEx(pos, cam, w, h);
        g.strength = 1.0f;
      }
      out[i] = g;
    }
  }

  void compute_gpu(const std::vector<Matrix>& xf, const Camera3D& cam, int w, int h,
                   std::vector<Glint>& out) {
    out.resize(xf.size());
#if !defined(__EMSCRIPTEN__)
    int n = (int)xf.size();
    if (n == 0)
      return;
    if (n > cap) {
      if (ssbo_in)
        rlUnloadShaderBuffer(ssbo_in);
      if (ssbo_out)
        rlUnloadShaderBuffer(ssbo_out);
      cap = n + n / 2 + 64;
      ssbo_in = rlLoadShaderBuffer((unsigned)(cap * sizeof(Matrix)), nullptr, RL_DYNAMIC_COPY);
      ssbo_out = rlLoadShaderBuffer((unsigned)(cap * sizeof(Glint)), nullptr, RL_DYNAMIC_COPY);
    }
    rlUpdateShaderBuffer(ssbo_in, xf.data(), (unsigned)(n * sizeof(Matrix)), 0);

    rlEnableShader(prog);
    Vector3 cp = cam.position;
    Vector2 ss = {(float)w, (float)h};
    float ct = cos_threshold();
    rlSetUniform(rlGetLocationUniform(prog, "sun_pos"), &sun_pos, RL_SHADER_UNIFORM_VEC3, 1);
    rlSetUniform(rlGetLocationUniform(prog, "cam_pos"), &cp, RL_SHADER_UNIFORM_VEC3, 1);
    rlSetUniformMatrix(rlGetLocationUniform(prog, "view_proj"), view_proj(cam, w, h));
    rlSetUniform(rlGetLocationUniform(prog, "screen_size"), &ss, RL_SHADER_UNIFORM_VEC2, 1);
    rlSetUniform(rlGetLocationUniform(prog, "cos_threshold"), &ct, RL_SHADER_UNIFORM_FLOAT, 1);
    rlSetUniform(rlGetLocationUniform(prog, "count"), &n, RL_SHADER_UNIFORM_INT, 1);
    rlBindShaderBuffer(ssbo_in, 1);
    rlBindShaderBuffer(ssbo_out, 2);
    rlComputeShaderDispatch((unsigned)((n + 63) / 64), 1, 1);
    rlDisableShader();

    if (glad_glMemoryBarrier)
      glad_glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT_ | GL_BUFFER_UPDATE_BARRIER_BIT_);
    rlReadShaderBuffer(ssbo_out, out.data(), (unsigned)(n * sizeof(Glint)), 0);
#else
    (void)cam;
    (void)w;
    (void)h;
#endif
  }

  /// White sprite: soft radial glow plus a thin 4-point star, alpha only. Power-of-two size.
  static Texture2D make_sprite(int size) {
    Color* px = (Color*)RL_MALLOC((size_t)size * size * sizeof(Color));
    for (int y = 0; y < size; y++) {
      for (int x = 0; x < size; x++) {
        float dx = ((x + 0.5f) / size) * 2.0f - 1.0f;
        float dy = ((y + 0.5f) / size) * 2.0f - 1.0f;
        float r = std::sqrt(dx * dx + dy * dy);
        float glow = std::pow(fmaxf(0.0f, 1.0f - r), 3.0f);
        float sx = std::pow(fmaxf(0.0f, 1.0f - std::fabs(dy) * 10.0f), 2.0f) * fmaxf(0.0f, 1.0f - std::fabs(dx));
        float sy = std::pow(fmaxf(0.0f, 1.0f - std::fabs(dx) * 10.0f), 2.0f) * fmaxf(0.0f, 1.0f - std::fabs(dy));
        float a = fminf(1.0f, glow * 1.2f + (sx + sy) * 0.7f);
        px[y * size + x] = {255, 255, 255, (unsigned char)(a * 255.0f)};
      }
    }
    Image img = {px, size, size, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    Texture2D t = LoadTextureFromImage(img);
    UnloadImage(img);
    SetTextureFilter(t, TEXTURE_FILTER_BILINEAR);
    return t;
  }
};

} // namespace SunGlints
