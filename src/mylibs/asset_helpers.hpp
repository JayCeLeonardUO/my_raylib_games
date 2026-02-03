#if 0
cd "$(dirname "$0")/../.." && cmake --build build --target mylibs_tests && ./build/mylibs_tests --no-skip -tc="hex grid visual test"
exit
#endif

#pragma once
#include "traits.hpp"
#include "uid_assets.hpp"
#include <cmath>
#include <raylib.h>
#include <rlgl.h>
#include <string>
#include <unordered_map>

struct AssetTraits {
  TRAIT_DEF(flip_h)
  TRAIT_DEF(flip_v)
  TRAIT_DEF(billboard)
  TRAIT_DEF(tile)
  TRAIT_DEF(sprite)
  TRAIT_DEF(has_silhouette)
};

struct Asset {
  AssetId id = AssetId::NONE;
  AssetTraits traits = {};
  Rectangle source = {0, 0, 0, 0};
  Vector2 origin = {0, 0};
  Vector3 pos = {0, 0, 0};
  float billboard_rotation = 0.0f;
  float rotation = 0.0f;
  float rot_speed = 0.0f;
  Vector2 scale = {1.0f, 1.0f};
  Color tint = WHITE;
  Vector2 bill_size = {1.0f, 1.0f};
  float silhouette_size = 1.1f;
  Color silhouette_color = BLACK;
  float vertex_radius = 0.15f;
  float line_thickness = 0.08f;

  static Asset make_2d(AssetId id) {
    Asset a;
    a.id = id;
    return a;
  }

  static Asset make_centered(AssetId id) {
    Asset a;
    a.id = id;
    a.traits.sprite = 1;
    return a;
  }

  static Asset make_tile(AssetId id) {
    Asset a;
    a.id = id;
    a.traits.tile = 1;
    return a;
  }

  static Asset make_billboard(AssetId id, Vector2 size) {
    Asset a;
    a.id = id;
    a.traits.billboard = 1;
    a.bill_size = size;
    return a;
  }

  static Asset make_with_silhouette(AssetId id, float sil_size = 1.1f, Color sil_color = BLACK) {
    Asset a;
    a.id = id;
    a.traits.has_silhouette = 1;
    a.silhouette_size = sil_size;
    a.silhouette_color = sil_color;
    return a;
  }

  static Asset make_hex_tile(AssetId id, Vector3 position, float sil_size = 1.1f,
                             Color sil_color = BLACK) {
    Asset a;
    a.id = id;
    a.pos = position;
    a.traits.tile = 1;
    a.traits.has_silhouette = 1;
    a.silhouette_size = sil_size;
    a.silhouette_color = sil_color;
    a.billboard_rotation = 30.0;
    return a;
  }
};

struct AssetCache {
  std::unordered_map<int, Texture2D> textures;
  AssetLoader loader;

  struct AssetRenderBuffersCtx {
    RenderTexture2D outlineLayer;
    RenderTexture2D mainLayer;
    bool initialized = false;
  };
  AssetRenderBuffersCtx renderBuffers;

  struct HexResources {
    Mesh mesh;
    Model model;
    Model planeModel;
    bool initialized = false;
  };
  HexResources hexResources;

  void BeginRenderingContext() {
    auto s_h = GetScreenHeight();
    auto s_w = GetScreenWidth();
    renderBuffers.outlineLayer = LoadRenderTexture(s_w, s_h);
    renderBuffers.mainLayer = LoadRenderTexture(s_w, s_h);
    renderBuffers.initialized = true;
  }

  void BeginFrame() {
    if (!renderBuffers.initialized)
      return;
    BeginTextureMode(renderBuffers.outlineLayer);
    ClearBackground(BLANK);
    EndTextureMode();
    BeginTextureMode(renderBuffers.mainLayer);
    ClearBackground(BLANK);
    EndTextureMode();
  }

  void EndFrame() {
    if (!renderBuffers.initialized)
      return;
    float w = (float)GetScreenWidth();
    float h = (float)GetScreenHeight();
    DrawTextureRec(renderBuffers.outlineLayer.texture, {0, 0, w, -h}, {0, 0}, WHITE);
    DrawTextureRec(renderBuffers.mainLayer.texture, {0, 0, w, -h}, {0, 0}, WHITE);
  }

  void UnloadRenderingContext() {
    if (!renderBuffers.initialized)
      return;
    UnloadRenderTexture(renderBuffers.outlineLayer);
    UnloadRenderTexture(renderBuffers.mainLayer);
    renderBuffers.initialized = false;
  }

  HexResources& get_hex_resources() {
    if (!hexResources.initialized) {
      hexResources.mesh = GenMeshPoly(6, 1.0f);
      hexResources.model = LoadModelFromMesh(hexResources.mesh);
      Mesh planeMesh = GenMeshPlane(2.0f, 2.0f, 1, 1);
      hexResources.planeModel = LoadModelFromMesh(planeMesh);
      hexResources.initialized = true;
    }
    return hexResources;
  }

  Texture2D& get_texture(AssetId id) {
    int key = static_cast<int>(id);
    auto it = textures.find(key);
    if (it != textures.end()) {
      return it->second;
    }
    // Handle NONE - create a 1x1 white texture
    if (id == AssetId::NONE) {
      Image img = GenImageColor(1, 1, WHITE);
      textures[key] = LoadTextureFromImage(img);
      UnloadImage(img);
      return textures[key];
    }
    LoadedBinary& bin = loader.get(id);
    Image img = LoadImageFromMemory(".png", bin.ptr(), (int)bin.size());
    Texture2D tex = LoadTextureFromImage(img);
    UnloadImage(img);
    textures[key] = tex;
    return textures[key];
  }

  void unload_all() {
    UnloadRenderingContext();
    if (hexResources.initialized) {
      UnloadModel(hexResources.model);
      UnloadModel(hexResources.planeModel);
      hexResources.initialized = false;
    }
    for (auto& [key, tex] : textures) {
      UnloadTexture(tex);
    }
    textures.clear();
    loader.clear();
  }
};

// 2D draw_asset - basic
inline void draw_asset(AssetCache& cache, Asset& asset, float x, float y) {
  if (asset.rot_speed != 0.0f) {
    asset.rotation += asset.rot_speed * GetFrameTime();
  }
  Texture2D& tex = cache.get_texture(asset.id);
  Rectangle src = asset.source;
  if (src.width == 0 && src.height == 0) {
    src = {0, 0, (float)tex.width, (float)tex.height};
  }
  if (asset.traits.flip_h)
    src.width = -src.width;
  if (asset.traits.flip_v)
    src.height = -src.height;
  float abs_w = src.width < 0 ? -src.width : src.width;
  float abs_h = src.height < 0 ? -src.height : src.height;
  float dst_w = abs_w * asset.scale.x;
  float dst_h = abs_h * asset.scale.y;
  Vector2 origin = asset.origin;
  if ((asset.traits.sprite || asset.traits.tile) && origin.x == 0 && origin.y == 0) {
    origin = {dst_w / 2.0f, dst_h / 2.0f};
  }
  Rectangle dst = {x, y, dst_w, dst_h};
  DrawTexturePro(tex, src, dst, origin, asset.rotation, asset.tint);
}

// 2D draw_asset - with size
inline void draw_asset(AssetCache& cache, Asset& asset, float x, float y, Vector2 size) {
  if (asset.rot_speed != 0.0f) {
    asset.rotation += asset.rot_speed * GetFrameTime();
  }
  Texture2D& tex = cache.get_texture(asset.id);
  Rectangle src = asset.source;
  if (src.width == 0 && src.height == 0) {
    src = {0, 0, (float)tex.width, (float)tex.height};
  }
  if (asset.traits.flip_h)
    src.width = -src.width;
  if (asset.traits.flip_v)
    src.height = -src.height;
  float dst_w = size.x * asset.scale.x;
  float dst_h = size.y * asset.scale.y;
  Vector2 origin = asset.origin;
  if ((asset.traits.sprite || asset.traits.tile) && origin.x == 0 && origin.y == 0) {
    origin = {dst_w / 2.0f, dst_h / 2.0f};
  }
  Rectangle dst = {x, y, dst_w, dst_h};
  DrawTexturePro(tex, src, dst, origin, asset.rotation, asset.tint);
}

// 3D draw_asset - hex tile with camera
inline void draw_asset(AssetCache& cache, Asset& asset, Camera3D& camera) {
  if (asset.rot_speed != 0.0f) {
    asset.rotation += asset.rot_speed * GetFrameTime();
  }

  auto& hex = cache.get_hex_resources();
  Texture2D& tex = cache.get_texture(asset.id);
  hex.planeModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = tex;

  float scale = asset.scale.x;
  Vector3 billPos = {asset.pos.x, asset.pos.y + 0.01f, asset.pos.z};

  if (asset.traits.has_silhouette && cache.renderBuffers.initialized) {
    float silScale = scale * asset.silhouette_size;

    // Draw silhouette to outline layer
    BeginTextureMode(cache.renderBuffers.outlineLayer);
    BeginMode3D(camera);
    rlDisableBackfaceCulling();
    DrawModelEx(hex.model, asset.pos, {0, 1, 0}, asset.rotation, {silScale, silScale, silScale},
                asset.silhouette_color);
    rlEnableBackfaceCulling();
    EndMode3D();
    EndTextureMode();

    // Draw main model to main layer
    BeginTextureMode(cache.renderBuffers.mainLayer);
    BeginMode3D(camera);
    rlDisableBackfaceCulling();
    DrawModelEx(hex.model, asset.pos, {0, 1, 0}, asset.rotation, {scale, scale, scale}, asset.tint);
    DrawModelEx(hex.planeModel, billPos, {0, 1, 0}, asset.rotation + asset.billboard_rotation,
                {asset.bill_size.x, 1.0f, asset.bill_size.y}, WHITE);
    rlEnableBackfaceCulling();
    EndMode3D();
    EndTextureMode();

  } else {
    BeginMode3D(camera);
    rlDisableBackfaceCulling();
    DrawModelEx(hex.model, asset.pos, {0, 1, 0}, asset.rotation, {scale, scale, scale}, asset.tint);
    DrawModelEx(hex.planeModel, billPos, {0, 1, 0}, asset.rotation + asset.billboard_rotation,
                {asset.bill_size.x, 1.0f, asset.bill_size.y}, WHITE);
    rlEnableBackfaceCulling();
    EndMode3D();
  }
}

// ============================================================================
// DOCTEST - Unit and visual tests
// ============================================================================
#ifdef DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("Asset make_2d defaults") {
  Asset a = Asset::make_2d(AssetId::GRASSPATCH59_PNG);
  CHECK((int)a.traits.flip_h == 0);
  CHECK((int)a.traits.flip_v == 0);
  CHECK((int)a.traits.billboard == 0);
  CHECK((int)a.traits.tile == 0);
  CHECK((int)a.traits.sprite == 0);
  CHECK(a.rotation == doctest::Approx(0.0f));
  CHECK(a.scale.x == doctest::Approx(1.0f));
  CHECK(a.scale.y == doctest::Approx(1.0f));
  CHECK(a.origin.x == doctest::Approx(0.0f));
  CHECK(a.origin.y == doctest::Approx(0.0f));
}

TEST_CASE("Asset make_centered") {
  Asset a = Asset::make_centered(AssetId::GRASSPATCH59_PNG);
  CHECK((int)a.traits.sprite == 1);
}

TEST_CASE("Asset make_tile") {
  Asset a = Asset::make_tile(AssetId::GRASSPATCH59_PNG);
  CHECK((int)a.traits.tile == 1);
}

TEST_CASE("Asset make_billboard") {
  Asset a = Asset::make_billboard(AssetId::GRASSPATCH59_PNG, {2.0f, 3.0f});
  CHECK((int)a.traits.billboard == 1);
  CHECK(a.bill_size.x == doctest::Approx(2.0f));
  CHECK(a.bill_size.y == doctest::Approx(3.0f));
}

TEST_CASE("Asset source rect and transforms") {
  Asset a = Asset::make_2d(AssetId::GRASSPATCH59_PNG);

  // Spritesheet region
  a.source = {32, 0, 64, 64};
  CHECK(a.source.x == doctest::Approx(32.0f));
  CHECK(a.source.width == doctest::Approx(64.0f));

  // Custom origin, rotation, scale
  a.origin = {32, 32};
  a.rotation = 45.0f;
  a.scale = {0.5f, 2.0f};
  a.tint = RED;

  CHECK(a.origin.x == doctest::Approx(32.0f));
  CHECK(a.rotation == doctest::Approx(45.0f));
  CHECK(a.scale.x == doctest::Approx(0.5f));
  CHECK(a.scale.y == doctest::Approx(2.0f));
}

TEST_CASE("hex grid visual test" * doctest::skip()) {
  // Run with: ./mylibs_tests --no-skip -tc="hex grid visual test"
  const int screenWidth = 800;
  const int screenHeight = 600;
  InitWindow(screenWidth, screenHeight, "Hex Grid - Simple");
  SetTargetFPS(60);
  rlImGuiSetup(true);

  AssetCache cache;
  cache.BeginRenderingContext();

  // Create a grid of hex assets - just like the single hex but multiple
  // Spacing in world units between hex centers
  float spacing = 1.8f;
  int gridSize = 3; // -3 to +3

  std::vector<Asset> hexes;
  for (int x = -gridSize; x <= gridSize; x++) {
    for (int z = -gridSize; z <= gridSize; z++) {
      // Offset every other row for hex pattern
      float offsetX = (z % 2) * (spacing * 0.5f);
      Vector3 pos = {x * spacing + offsetX, 0.0f, z * spacing * 0.866f};

      Asset hex = Asset::make_hex_tile(AssetId::GRASSLANDDENSE2_PNG, pos, 1.1f, BLACK);
      hex.scale = {1.0f, 1.0f};
      hex.bill_size = {1.0f, 1.0f};
      hex.tint = GREEN;
      hexes.push_back(hex);
    }
  }

  Camera3D camera = {0};
  camera.position = {0.0f, 12.0f, 8.0f};
  camera.target = {0.0f, 0.0f, 0.0f};
  camera.up = {0.0f, 1.0f, 0.0f};
  camera.fovy = 45.0f;
  camera.projection = CAMERA_PERSPECTIVE;

  bool cameraEnabled = false;
  float hexScale = 1.0f;

  while (!WindowShouldClose()) {
    if (IsKeyPressed(KEY_TAB)) {
      cameraEnabled = !cameraEnabled;
      if (cameraEnabled)
        DisableCursor();
      else
        EnableCursor();
    }

    if (cameraEnabled) {
      UpdateCamera(&camera, CAMERA_FREE);
    }

    // Update all hex scales
    for (auto& hex : hexes) {
      hex.scale = {hexScale, hexScale};
      hex.bill_size = {hexScale, hexScale};
    }

    BeginDrawing();
    ClearBackground(DARKGRAY);

    cache.BeginFrame();
    for (auto& hex : hexes) {
      draw_asset(cache, hex, camera);
    }
    cache.EndFrame();

    rlImGuiBegin();
    if (ImGui::Begin("Settings")) {
      ImGui::Text("FPS: %d", GetFPS());
      ImGui::Text("Hexes: %d", (int)hexes.size());
      ImGui::Text("TAB to toggle camera (currently %s)", cameraEnabled ? "ON" : "OFF");
      ImGui::Separator();
      ImGui::SliderFloat("Hex Scale", &hexScale, 0.5f, 2.0f);
      ImGui::SliderFloat("Vertex Radius", &hexes[0].vertex_radius, 0.01f, 0.5f);
      ImGui::SliderFloat("Line Thickness", &hexes[0].line_thickness, 0.005f, 0.2f);
      ImGui::SliderFloat("Silhouette Size", &hexes[0].silhouette_size, 1.0f, 1.5f);
      // Apply to all
      for (size_t i = 1; i < hexes.size(); i++) {
        hexes[i].vertex_radius = hexes[0].vertex_radius;
        hexes[i].line_thickness = hexes[0].line_thickness;
        hexes[i].silhouette_size = hexes[0].silhouette_size;
      }
    }
    ImGui::End();
    rlImGuiEnd();

    DrawFPS(screenWidth - 100, 10);
    EndDrawing();
  }

  rlImGuiShutdown();
  cache.unload_all();
  CloseWindow();
  CHECK(true);
}
#endif
