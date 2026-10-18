#pragma once
/**
 * @brief Camera-relative three-point lighting (key / fill / rim) for individual models.
 *
 * The three lights are directional and defined in *camera space* (x = screen right,
 * y = screen up, z = toward the viewer), so every lit object gets the same portrait
 * studio setup no matter how the camera orbits: the lit side never flips to the back.
 *
 * Usage:
 *   ThreePointLight::init();                       // after InitWindow; loads the shader
 *   ThreePointLight::begin_frame(camera);          // once per frame before drawing lit models
 *   ThreePointLight::draw_model(model, transform); // DrawMesh with the lighting shader swapped in
 *   ThreePointLight::draw_imgui();                 // scheme picker + intensity slider
 *   ThreePointLight::shutdown();
 *
 * One scheme is active globally; everything drawn through draw_model shares it.
 * Schemes are hardcoded presets below; add one by appending to SCHEMES.
 */

#include <cmath>
#include <cstring>
#include <imgui.h>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>
#include <string>
#include <vector>

namespace ThreePointLight {

struct Light {
  Vector3 dir_cam;  // direction TOWARD the light, camera space; normalized at upload
  Color color;
  float intensity;
};

struct Scheme {
  const char* name;
  Light key;
  Light fill;
  Light rim;
  Color ambient;
  float ambient_intensity;
  float spec_strength;
  float spec_power;
  float rim_power;
};

// Conventional camera-space placements. Key upper-left-front, fill right-front and
// lower, rim behind-above-right. Reused by every preset so only colors/ratios differ.
inline constexpr Vector3 KEY_DIR = {-0.55f, 0.65f, 0.55f};
inline constexpr Vector3 FILL_DIR = {0.75f, 0.15f, 0.60f};
inline constexpr Vector3 RIM_DIR = {0.35f, 0.55f, -0.75f};

// "Safe" presets: classic ratios (fill roughly a third to half of key, rim near key),
// warm/cool contrast between key and fill so form reads without looking garish.
inline const Scheme SCHEMES[] = {
    {"Studio Neutral",
     {KEY_DIR, {255, 246, 232, 255}, 1.00f},
     {FILL_DIR, {176, 192, 220, 255}, 0.40f},
     {RIM_DIR, {255, 255, 255, 255}, 0.70f},
     {60, 64, 72, 255}, 1.0f, 0.25f, 32.0f, 3.0f},
    {"Warm Key / Cool Fill",
     {KEY_DIR, {255, 214, 170, 255}, 1.05f},
     {FILL_DIR, {140, 170, 255, 255}, 0.45f},
     {RIM_DIR, {255, 240, 220, 255}, 0.80f},
     {50, 54, 70, 255}, 1.0f, 0.20f, 24.0f, 3.0f},
    {"Cool Key / Warm Fill",
     {KEY_DIR, {200, 222, 255, 255}, 1.00f},
     {FILL_DIR, {255, 200, 160, 255}, 0.45f},
     {RIM_DIR, {255, 255, 255, 255}, 0.75f},
     {56, 58, 66, 255}, 1.0f, 0.22f, 28.0f, 3.0f},
    {"Golden Hour",
     {KEY_DIR, {255, 190, 120, 255}, 1.15f},
     {FILL_DIR, {120, 140, 200, 255}, 0.35f},
     {RIM_DIR, {255, 220, 180, 255}, 1.00f},
     {70, 56, 48, 255}, 1.0f, 0.18f, 16.0f, 2.5f},
    {"Moonlight",
     {KEY_DIR, {170, 190, 255, 255}, 0.90f},
     {FILL_DIR, {80, 90, 130, 255}, 0.50f},
     {RIM_DIR, {200, 220, 255, 255}, 0.90f},
     {28, 32, 56, 255}, 1.0f, 0.30f, 40.0f, 3.5f},
    {"Overcast Soft",
     {KEY_DIR, {236, 240, 246, 255}, 0.85f},
     {FILL_DIR, {200, 206, 216, 255}, 0.70f},
     {RIM_DIR, {255, 255, 255, 255}, 0.30f},
     {96, 98, 104, 255}, 1.0f, 0.08f, 12.0f, 2.0f},
};
inline constexpr int SCHEME_COUNT = (int)(sizeof(SCHEMES) / sizeof(SCHEMES[0]));

// ---- state ----
inline Shader shader = {0};
inline bool ready = false;
inline int scheme_index = 0;
inline float master_intensity = 1.0f;

struct Locs {
  int viewPos, keyDir, keyColor, fillDir, fillColor, rimDir, rimColor, ambientColor, intensity,
      specStrength, specPower, rimPower;
} inline locs = {};

inline const Scheme& current() { return SCHEMES[scheme_index]; }

// The web build runs on WebGL 1 (GLSL ES 1.00), so it gets its own shader pair.
#if defined(__EMSCRIPTEN__)
inline constexpr const char* DEFAULT_VS = "assets/shaders/three_point_es100.vs";
inline constexpr const char* DEFAULT_FS = "assets/shaders/three_point_es100.fs";
#else
inline constexpr const char* DEFAULT_VS = "assets/shaders/three_point.vs";
inline constexpr const char* DEFAULT_FS = "assets/shaders/three_point.fs";
#endif

inline bool init(const char* vs_path = DEFAULT_VS, const char* fs_path = DEFAULT_FS) {
  shader = LoadShader(vs_path, fs_path);
  ready = shader.id != 0 && shader.id != rlGetShaderIdDefault();
  if (!ready) {
    TraceLog(LOG_WARNING, "ThreePointLight: shader failed to load (%s, %s)", vs_path, fs_path);
    return false;
  }
  locs.viewPos = GetShaderLocation(shader, "viewPos");
  locs.keyDir = GetShaderLocation(shader, "keyDir");
  locs.keyColor = GetShaderLocation(shader, "keyColor");
  locs.fillDir = GetShaderLocation(shader, "fillDir");
  locs.fillColor = GetShaderLocation(shader, "fillColor");
  locs.rimDir = GetShaderLocation(shader, "rimDir");
  locs.rimColor = GetShaderLocation(shader, "rimColor");
  locs.ambientColor = GetShaderLocation(shader, "ambientColor");
  locs.intensity = GetShaderLocation(shader, "intensity");
  locs.specStrength = GetShaderLocation(shader, "specStrength");
  locs.specPower = GetShaderLocation(shader, "specPower");
  locs.rimPower = GetShaderLocation(shader, "rimPower");
  // raylib fills matModel/matNormal/mvp/colDiffuse/texture0 via its standard locations
  shader.locs[SHADER_LOC_VECTOR_VIEW] = locs.viewPos;
  return true;
}

inline void shutdown() {
  if (ready)
    UnloadShader(shader);
  shader = {0};
  ready = false;
}

inline bool set_scheme(int index) {
  if (index < 0 || index >= SCHEME_COUNT)
    return false;
  scheme_index = index;
  return true;
}

inline bool set_scheme(const char* name) {
  for (int i = 0; i < SCHEME_COUNT; i++)
    if (strcmp(SCHEMES[i].name, name) == 0)
      return set_scheme(i);
  return false;
}

inline std::vector<std::string> scheme_names() {
  std::vector<std::string> out;
  for (const auto& s : SCHEMES)
    out.push_back(s.name);
  return out;
}

// ---- per-frame uniform upload ----

inline Vector3 scaled(Color c, float k) {
  return {c.r / 255.0f * k, c.g / 255.0f * k, c.b / 255.0f * k};
}

/// Convert a camera-space direction to world space using the camera's basis.
inline Vector3 cam_to_world(const Camera3D& cam, Vector3 d) {
  Vector3 forward = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
  Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, cam.up));
  Vector3 up = Vector3CrossProduct(right, forward);
  Vector3 toward_viewer = Vector3Negate(forward);
  Vector3 w = Vector3Add(Vector3Add(Vector3Scale(right, d.x), Vector3Scale(up, d.y)),
                         Vector3Scale(toward_viewer, d.z));
  return Vector3Normalize(w);
}

inline void begin_frame(const Camera3D& cam) {
  if (!ready)
    return;
  const Scheme& s = current();

  Vector3 key = cam_to_world(cam, s.key.dir_cam);
  Vector3 fill = cam_to_world(cam, s.fill.dir_cam);
  Vector3 rim = cam_to_world(cam, s.rim.dir_cam);
  Vector3 keyc = scaled(s.key.color, s.key.intensity);
  Vector3 fillc = scaled(s.fill.color, s.fill.intensity);
  Vector3 rimc = scaled(s.rim.color, s.rim.intensity);
  Vector3 amb = scaled(s.ambient, s.ambient_intensity);
  Vector3 view = cam.position;

  SetShaderValue(shader, locs.viewPos, &view, SHADER_UNIFORM_VEC3);
  SetShaderValue(shader, locs.keyDir, &key, SHADER_UNIFORM_VEC3);
  SetShaderValue(shader, locs.keyColor, &keyc, SHADER_UNIFORM_VEC3);
  SetShaderValue(shader, locs.fillDir, &fill, SHADER_UNIFORM_VEC3);
  SetShaderValue(shader, locs.fillColor, &fillc, SHADER_UNIFORM_VEC3);
  SetShaderValue(shader, locs.rimDir, &rim, SHADER_UNIFORM_VEC3);
  SetShaderValue(shader, locs.rimColor, &rimc, SHADER_UNIFORM_VEC3);
  SetShaderValue(shader, locs.ambientColor, &amb, SHADER_UNIFORM_VEC3);
  SetShaderValue(shader, locs.intensity, &master_intensity, SHADER_UNIFORM_FLOAT);
  SetShaderValue(shader, locs.specStrength, &s.spec_strength, SHADER_UNIFORM_FLOAT);
  SetShaderValue(shader, locs.specPower, &s.spec_power, SHADER_UNIFORM_FLOAT);
  SetShaderValue(shader, locs.rimPower, &s.rim_power, SHADER_UNIFORM_FLOAT);
}

/// Draw a model with the lighting shader swapped into each material (materials are
/// copied by value, the model is not modified). Falls back to the plain shader if the
/// lighting shader is unavailable.
inline void draw_model(const Model& model, Matrix transform) {
  for (int i = 0; i < model.meshCount; i++) {
    Material mat = model.materials[model.meshMaterial[i]];
    if (ready)
      mat.shader = shader;
    DrawMesh(model.meshes[i], mat, transform);
  }
}

// ---- dev menu ----

inline void draw_imgui() {
  if (!ready) {
    ImGui::TextDisabled("three-point shader not loaded");
    return;
  }
  if (ImGui::BeginCombo("Scheme", current().name)) {
    for (int i = 0; i < SCHEME_COUNT; i++) {
      bool sel = (i == scheme_index);
      if (ImGui::Selectable(SCHEMES[i].name, sel))
        scheme_index = i;
      if (sel)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
  ImGui::SliderFloat("Intensity", &master_intensity, 0.0f, 3.0f, "%.2f");
  const Scheme& s = current();
  ImGui::TextDisabled("key %.2f  fill %.2f  rim %.2f  ambient %.2f", s.key.intensity,
                      s.fill.intensity, s.rim.intensity, s.ambient_intensity);
}

} // namespace ThreePointLight
