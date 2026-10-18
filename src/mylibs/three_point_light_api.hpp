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
// Rule for every preset: the rim color sits opposite the key on the color wheel, so the
// silhouette edge separates from the key-lit side (warm key -> cool rim, cool key -> warm
// rim, green key -> orange rim). Fill stays a muted cousin of the rim.
inline const Scheme SCHEMES[] = {
    {"Studio Neutral",
     {KEY_DIR, {255, 246, 232, 255}, 1.00f},   // warm white key
     {FILL_DIR, {176, 192, 220, 255}, 0.40f},
     {RIM_DIR, {190, 210, 255, 255}, 0.75f},   // cool blue rim
     {60, 64, 72, 255}, 1.0f, 0.25f, 32.0f, 3.0f},
    {"Warm Key / Cool Fill",
     {KEY_DIR, {255, 214, 170, 255}, 1.05f},   // orange key
     {FILL_DIR, {140, 170, 255, 255}, 0.45f},
     {RIM_DIR, {150, 190, 255, 255}, 0.85f},   // blue rim
     {50, 54, 70, 255}, 1.0f, 0.20f, 24.0f, 3.0f},
    {"Cool Key / Warm Fill",
     {KEY_DIR, {200, 222, 255, 255}, 1.00f},   // blue key
     {FILL_DIR, {255, 200, 160, 255}, 0.45f},
     {RIM_DIR, {255, 196, 140, 255}, 0.80f},   // orange rim
     {56, 58, 66, 255}, 1.0f, 0.22f, 28.0f, 3.0f},
    {"Golden Hour",
     {KEY_DIR, {255, 190, 120, 255}, 1.15f},   // deep orange key
     {FILL_DIR, {120, 140, 200, 255}, 0.35f},
     {RIM_DIR, {120, 165, 255, 255}, 1.00f},   // sky blue rim
     {70, 56, 48, 255}, 1.0f, 0.18f, 16.0f, 2.5f},
    {"Moonlight",
     {KEY_DIR, {170, 190, 255, 255}, 0.90f},   // blue key
     {FILL_DIR, {80, 90, 130, 255}, 0.50f},
     {RIM_DIR, {255, 205, 150, 255}, 0.90f},   // amber rim (a distant lamp)
     {28, 32, 56, 255}, 1.0f, 0.30f, 40.0f, 3.5f},
    {"Overcast Soft",
     {KEY_DIR, {236, 240, 246, 255}, 0.85f},   // faintly cool key
     {FILL_DIR, {200, 206, 216, 255}, 0.70f},
     {RIM_DIR, {255, 236, 212, 255}, 0.35f},   // faintly warm rim
     {96, 98, 104, 255}, 1.0f, 0.08f, 12.0f, 2.0f},
    {"Forest",
     {KEY_DIR, {178, 224, 140, 255}, 1.00f},   // dappled green key
     {FILL_DIR, {110, 140, 150, 255}, 0.40f},  // cool shade fill
     {RIM_DIR, {255, 150, 70, 255}, 0.95f},    // orange rim
     {38, 50, 40, 255}, 1.0f, 0.20f, 24.0f, 3.0f},
};
inline constexpr int SCHEME_COUNT = (int)(sizeof(SCHEMES) / sizeof(SCHEMES[0]));

// ---- state ----
inline Shader shader = {0};      // per-model draws (DrawMesh)
inline Shader shader_inst = {0}; // instanced draws (DrawMeshInstanced), same lighting
inline bool ready = false;
inline bool ready_inst = false;
inline int scheme_index = 0;
inline float master_intensity = 1.0f;

struct Locs {
  int viewPos, keyDir, keyColor, fillDir, fillColor, rimDir, rimColor, ambientColor, intensity,
      specStrength, specPower, rimPower, useNormalMap;
} inline locs = {}, locs_inst = {};

inline Locs load_locs(const Shader& sh) {
  Locs l;
  l.viewPos = GetShaderLocation(sh, "viewPos");
  l.keyDir = GetShaderLocation(sh, "keyDir");
  l.keyColor = GetShaderLocation(sh, "keyColor");
  l.fillDir = GetShaderLocation(sh, "fillDir");
  l.fillColor = GetShaderLocation(sh, "fillColor");
  l.rimDir = GetShaderLocation(sh, "rimDir");
  l.rimColor = GetShaderLocation(sh, "rimColor");
  l.ambientColor = GetShaderLocation(sh, "ambientColor");
  l.intensity = GetShaderLocation(sh, "intensity");
  l.specStrength = GetShaderLocation(sh, "specStrength");
  l.specPower = GetShaderLocation(sh, "specPower");
  l.rimPower = GetShaderLocation(sh, "rimPower");
  l.useNormalMap = GetShaderLocation(sh, "useNormalMap");
  return l;
}

// The live scheme is a mutable copy of the selected preset: the dev panel and Lua can
// change its per-light intensities without touching the preset table. reset_scheme()
// copies the preset back.
inline Scheme active = SCHEMES[0];
inline const Scheme& current() { return active; }

// The web build runs on WebGL 1 (GLSL ES 1.00), so it gets its own shader pair.
#if defined(__EMSCRIPTEN__)
inline constexpr const char* DEFAULT_VS = "assets/shaders/three_point_es100.vs";
inline constexpr const char* DEFAULT_FS = "assets/shaders/three_point_es100.fs";
inline constexpr const char* DEFAULT_VS_INST = "assets/shaders/three_point_instanced_es100.vs";
#else
inline constexpr const char* DEFAULT_VS = "assets/shaders/three_point.vs";
inline constexpr const char* DEFAULT_FS = "assets/shaders/three_point.fs";
inline constexpr const char* DEFAULT_VS_INST = "assets/shaders/three_point_instanced.vs";
#endif

inline bool init(const char* vs_path = DEFAULT_VS, const char* fs_path = DEFAULT_FS,
                 const char* vs_inst_path = DEFAULT_VS_INST) {
  shader = LoadShader(vs_path, fs_path);
  ready = shader.id != 0 && shader.id != rlGetShaderIdDefault();
  if (!ready) {
    TraceLog(LOG_WARNING, "ThreePointLight: shader failed to load (%s, %s)", vs_path, fs_path);
    return false;
  }
  locs = load_locs(shader);
  shader.locs[SHADER_LOC_VECTOR_VIEW] = locs.viewPos;

  // instanced variant: same fragment shader, model matrix from the instanceTransform attribute
  shader_inst = LoadShader(vs_inst_path, fs_path);
  ready_inst = shader_inst.id != 0 && shader_inst.id != rlGetShaderIdDefault();
  if (ready_inst) {
    locs_inst = load_locs(shader_inst);
    shader_inst.locs[SHADER_LOC_VECTOR_VIEW] = locs_inst.viewPos;
    shader_inst.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocationAttrib(shader_inst, "instanceTransform");
  } else {
    TraceLog(LOG_WARNING, "ThreePointLight: instanced shader failed to load (%s)", vs_inst_path);
  }
  return true;
}

inline void shutdown() {
  if (ready_inst) {
    UnloadShader(shader_inst);
    shader_inst = {0};
    ready_inst = false;
  }
  if (ready)
    UnloadShader(shader);
  shader = {0};
  ready = false;
}

inline bool set_scheme(int index) {
  if (index < 0 || index >= SCHEME_COUNT)
    return false;
  scheme_index = index;
  active = SCHEMES[index];
  return true;
}

inline void reset_scheme() { active = SCHEMES[scheme_index]; }

// Per-light intensity by name: "key", "fill", "rim", "ambient". nullptr if unknown.
inline float* intensity_of(const char* light) {
  if (strcmp(light, "key") == 0)
    return &active.key.intensity;
  if (strcmp(light, "fill") == 0)
    return &active.fill.intensity;
  if (strcmp(light, "rim") == 0)
    return &active.rim.intensity;
  if (strcmp(light, "ambient") == 0)
    return &active.ambient_intensity;
  return nullptr;
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

  auto upload = [&](const Shader& sh, const Locs& l) {
    SetShaderValue(sh, l.viewPos, &view, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, l.keyDir, &key, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, l.keyColor, &keyc, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, l.fillDir, &fill, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, l.fillColor, &fillc, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, l.rimDir, &rim, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, l.rimColor, &rimc, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, l.ambientColor, &amb, SHADER_UNIFORM_VEC3);
    SetShaderValue(sh, l.intensity, &master_intensity, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, l.specStrength, &s.spec_strength, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, l.specPower, &s.spec_power, SHADER_UNIFORM_FLOAT);
    SetShaderValue(sh, l.rimPower, &s.rim_power, SHADER_UNIFORM_FLOAT);
  };
  upload(shader, locs);
  if (ready_inst)
    upload(shader_inst, locs_inst);
}

/// Draw a model with the lighting shader swapped into each material (materials are
/// copied by value, the model is not modified). Falls back to the plain shader if the
/// lighting shader is unavailable.
inline void draw_model(const Model& model, Matrix transform) {
  for (int i = 0; i < model.meshCount; i++) {
    Material mat = model.materials[model.meshMaterial[i]];
    if (ready) {
      mat.shader = shader;
      // raylib binds maps[MATERIAL_MAP_NORMAL] to texture1 when present; tell the shader
      int use_nm = mat.maps[MATERIAL_MAP_NORMAL].texture.id > 0 ? 1 : 0;
      SetShaderValue(shader, locs.useNormalMap, &use_nm, SHADER_UNIFORM_INT);
    }
    DrawMesh(model.meshes[i], mat, transform);
  }
}

/// Draw many copies of one mesh in a single call, each with its own transform, lit the
/// same way as draw_model. Drawn double-sided so thin shapes (particles) read from any
/// angle. Falls back to the material's own shader if the instanced shader is unavailable.
inline void draw_mesh_instanced(const Mesh& mesh, Material mat, const Matrix* transforms,
                                int count) {
  if (count <= 0)
    return;
  if (ready_inst) {
    mat.shader = shader_inst;
    int use_nm = 0;
    SetShaderValue(shader_inst, locs_inst.useNormalMap, &use_nm, SHADER_UNIFORM_INT);
  }
  rlDisableBackfaceCulling();
  DrawMeshInstanced(mesh, mat, transforms, count);
  rlEnableBackfaceCulling();
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
        set_scheme(i);
      if (sel)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
  ImGui::SliderFloat("Master", &master_intensity, 0.0f, 3.0f, "%.2f");

  // per-light intensities, with the light's color as a swatch so the slider reads at a glance
  auto light_slider = [](const char* label, Color c, float* intensity, float max) {
    ImGui::ColorButton(label, ImVec4(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, 1.0f),
                       ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoAlpha,
                       ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
    ImGui::SameLine();
    ImGui::SliderFloat(label, intensity, 0.0f, max, "%.2f");
  };
  light_slider("Key", active.key.color, &active.key.intensity, 3.0f);
  light_slider("Fill", active.fill.color, &active.fill.intensity, 2.0f);
  light_slider("Rim", active.rim.color, &active.rim.intensity, 3.0f);
  light_slider("Ambient", active.ambient, &active.ambient_intensity, 2.0f);

  const Scheme& preset = SCHEMES[scheme_index];
  bool dirty = active.key.intensity != preset.key.intensity ||
               active.fill.intensity != preset.fill.intensity ||
               active.rim.intensity != preset.rim.intensity ||
               active.ambient_intensity != preset.ambient_intensity;
  if (dirty) {
    if (ImGui::SmallButton("Reset to preset"))
      reset_scheme();
  } else {
    ImGui::TextDisabled("preset values");
  }
}

} // namespace ThreePointLight
