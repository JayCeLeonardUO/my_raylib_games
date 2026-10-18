#pragma once
/**
 * @brief Small tumbling triangles, drawn in one instanced call with the three-point
 * lighting shader. Each particle has its own axis and spin, so as they rotate their faces
 * sweep through the key, fill and rim directions and glint those colors.
 */

#include "imgui.h"
#include "raylib.h"
#include "raymath.h"
#include "three_point_light_api.hpp"
#include <cmath>
#include <cstdlib>
#include <vector>

namespace TriangleParticles {

struct Particle {
  Vector3 home;    // rest position
  Vector3 axis;    // spin axis, unit
  float angle;     // current rotation about axis
  float spin;      // radians per second (signed)
  float bob_phase; // vertical drift phase
  float bob_amp;
};

struct System {
  // tunables (dev panel / Lua)
  bool enabled = true;
  int count = 400;
  float size = 0.16f;      // triangle edge length
  float spin_scale = 1.0f; // multiplies every particle's spin
  float drift = 1.0f;      // multiplies the bob
  Vector3 box_min = {-14.0f, 0.4f, -14.0f};
  Vector3 box_max = {14.0f, 5.0f, 14.0f};
  Color color = {236, 236, 240, 255};
  unsigned seed = 1;

  // state
  std::vector<Particle> particles;
  std::vector<Matrix> transforms;
  Mesh mesh = {0};
  Material material = {0};
  float time = 0.0f;
  bool loaded = false;

  void init() {
    if (!loaded) {
      mesh = make_triangle(1.0f); // unit edge; size is applied in the transform
      material = LoadMaterialDefault();
      loaded = true;
    }
    material.maps[MATERIAL_MAP_DIFFUSE].color = color;
    respawn();
  }

  void unload() {
    if (!loaded)
      return;
    UnloadMesh(mesh);
    UnloadMaterial(material); // the default material owns only the default texture
    mesh = {0};
    material = {0};
    loaded = false;
  }

  /// Rebuild the particle set from `count` and `seed`.
  void respawn() {
    std::srand(seed);
    auto rnd = [](float lo, float hi) { return lo + (hi - lo) * ((float)std::rand() / (float)RAND_MAX); };
    particles.resize((size_t)count);
    for (auto& p : particles) {
      p.home = {rnd(box_min.x, box_max.x), rnd(box_min.y, box_max.y), rnd(box_min.z, box_max.z)};
      p.axis = Vector3Normalize({rnd(-1, 1), rnd(-1, 1), rnd(-1, 1)});
      p.angle = rnd(0, 2.0f * PI);
      p.spin = rnd(1.0f, 4.0f) * (std::rand() % 2 ? 1.0f : -1.0f);
      p.bob_phase = rnd(0, 2.0f * PI);
      p.bob_amp = rnd(0.1f, 0.4f);
    }
    transforms.resize((size_t)count);
  }

  void update(float dt) {
    if (!enabled)
      return;
    time += dt;
    if (particles.size() != (size_t)count)
      respawn();
    for (size_t i = 0; i < particles.size(); i++) {
      Particle& p = particles[i];
      p.angle += p.spin * spin_scale * dt;
      float y = p.home.y + std::sin(time * 0.6f + p.bob_phase) * p.bob_amp * drift;
      Matrix rot = MatrixRotate(p.axis, p.angle);
      Matrix scl = MatrixScale(size, size, size);
      transforms[i] = MatrixMultiply(MatrixMultiply(scl, rot), MatrixTranslate(p.home.x, y, p.home.z));
    }
  }

  /// Call inside a 3D mode after ThreePointLight::begin_frame.
  void draw() {
    if (!enabled || !loaded || transforms.empty())
      return;
    material.maps[MATERIAL_MAP_DIFFUSE].color = color;
    ThreePointLight::draw_mesh_instanced(mesh, material, transforms.data(), (int)transforms.size());
  }

  void draw_imgui() {
    ImGui::Checkbox("Enabled", &enabled);
    ImGui::SameLine();
    ImGui::Text("%zu drawn in one call", transforms.size());
    if (ImGui::SliderInt("Count", &count, 0, 5000))
      respawn();
    ImGui::SliderFloat("Size", &size, 0.02f, 1.0f, "%.2f");
    ImGui::SliderFloat("Spin", &spin_scale, 0.0f, 5.0f, "%.2f");
    ImGui::SliderFloat("Drift", &drift, 0.0f, 3.0f, "%.2f");
    float col[3] = {color.r / 255.0f, color.g / 255.0f, color.b / 255.0f};
    if (ImGui::ColorEdit3("Color", col, ImGuiColorEditFlags_NoInputs))
      color = {(unsigned char)(col[0] * 255), (unsigned char)(col[1] * 255), (unsigned char)(col[2] * 255), 255};
    if (ImGui::SmallButton("Reseed")) {
      seed = (unsigned)std::rand();
      respawn();
    }
  }

  /// One equilateral triangle in the XY plane, centered, facing +Z (and lit double-sided).
  static Mesh make_triangle(float edge) {
    Mesh m = {0};
    m.vertexCount = 3;
    m.triangleCount = 1;
    m.vertices = (float*)RL_CALLOC(9, sizeof(float));
    m.normals = (float*)RL_CALLOC(9, sizeof(float));
    m.texcoords = (float*)RL_CALLOC(6, sizeof(float));
    const float h = edge * 0.8660254f; // sqrt(3)/2
    const float v[9] = {-edge * 0.5f, -h / 3.0f, 0, edge * 0.5f, -h / 3.0f, 0, 0, 2.0f * h / 3.0f, 0};
    const float t[6] = {0, 1, 1, 1, 0.5f, 0};
    for (int i = 0; i < 9; i++) {
      m.vertices[i] = v[i];
      m.normals[i] = (i % 3 == 2) ? 1.0f : 0.0f;
    }
    for (int i = 0; i < 6; i++)
      m.texcoords[i] = t[i];
    UploadMesh(&m, false);
    return m;
  }
};

} // namespace TriangleParticles
