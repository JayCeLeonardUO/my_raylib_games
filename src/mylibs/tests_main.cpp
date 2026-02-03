#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

// Raylib/ImGui for visual tests
#include "raylib.h"
#include "imgui.h"
#include "rlImGui.h"

// Include headers with embedded tests
#include "ilist.hpp"
#include "hexgrid_math.hpp"
#include "asset_helpers.hpp"
#include "zoo.hpp"
#include "game_console_api.hpp"
#include "model_api.hpp"
