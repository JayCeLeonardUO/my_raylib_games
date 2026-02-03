#if 0
cd "$(dirname "$0")/../.." && cmake --build build --target mylibs_tests && ./build/mylibs_tests -tc="uid_assets*"
exit
#endif

/**
 * @file uid_assets.hpp
 * @brief File-path based asset system with runtime loading
 *
 * Unlike embedded_assets.hpp which embeds binary data at compile time,
 * this system stores file paths and loads assets at runtime.
 */

#pragma once
#include <raylib.h>

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

/// @brief Identifiers for assets
enum class AssetId { NONE, GRASSLANDDENSE2_PNG, GRASSPATCH59_PNG, TESTBACKGROUND_PNG };

/// @brief Information about an asset
struct AssetInfo {
  const char* filepath; ///< Path to the asset file
  const char* name;     ///< Human-readable name
};

/// @brief Get asset info for a given AssetId
inline AssetInfo get_asset_info(AssetId id) {
  switch (id) {
    case AssetId::NONE:
      return {nullptr, "none"};
    case AssetId::GRASSLANDDENSE2_PNG:
      return {"assets/grassland_dense_2.png", "grassland_dense_2"};
    case AssetId::GRASSPATCH59_PNG:
      return {"assets/grass_patch_59.png", "grass_patch_59"};
    case AssetId::TESTBACKGROUND_PNG:
      return {"assets/test_background.png", "test_background"};
  }
  return {nullptr, "unknown"};
}

/// @brief Loaded binary data from file
struct LoadedBinary {
  std::vector<uint8_t> data; ///< Binary data
  const char* name;          ///< Asset name

  const uint8_t* ptr() const { return data.data(); }
  size_t size() const { return data.size(); }
};

/// @brief Asset loader that caches loaded binary data
struct AssetLoader {
  std::unordered_map<int, LoadedBinary> cache;

  /// @brief Load or get cached binary data for an asset
  LoadedBinary& get(AssetId id) {
    int key = static_cast<int>(id);
    auto it = cache.find(key);
    if (it != cache.end()) {
      return it->second;
    }

    AssetInfo info = get_asset_info(id);
    LoadedBinary bin;
    bin.name = info.name;

    if (info.filepath != nullptr) {
      int size = 0;
      unsigned char* data = LoadFileData(info.filepath, &size);
      if (data != nullptr) {
        bin.data.assign(data, data + size);
        UnloadFileData(data);
      }
    }

    cache[key] = std::move(bin);
    return cache[key];
  }

  /// @brief Preload all assets into cache
  void preload_all() {
    get(AssetId::GRASSLANDDENSE2_PNG);
    get(AssetId::GRASSPATCH59_PNG);
    get(AssetId::TESTBACKGROUND_PNG);
  }

  /// @brief Clear all cached data
  void clear() { cache.clear(); }
};

// ============================================================================
// DOCTEST
// ============================================================================
#ifdef DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

TEST_CASE("uid_assets get_asset_info") {
  AssetInfo none = get_asset_info(AssetId::NONE);
  CHECK(none.filepath == nullptr);

  AssetInfo grass = get_asset_info(AssetId::GRASSLANDDENSE2_PNG);
  CHECK(grass.filepath != nullptr);
  CHECK(std::string(grass.filepath) == "assets/grassland_dense_2.png");
}

TEST_CASE("uid_assets loader") {
  InitWindow(100, 100, "test");

  AssetLoader loader;
  LoadedBinary& bin = loader.get(AssetId::GRASSLANDDENSE2_PNG);

  CHECK(bin.size() > 0);
  CHECK(bin.ptr() != nullptr);

  // Second call should return cached
  LoadedBinary& bin2 = loader.get(AssetId::GRASSLANDDENSE2_PNG);
  CHECK(bin.ptr() == bin2.ptr());

  loader.clear();
  CloseWindow();
}

#endif
