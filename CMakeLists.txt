# ============================================================================
# MY RAYLIB GAMES
# ============================================================================

cmake_minimum_required(VERSION 3.14)
project(my_raylib_games)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# ============================================================================
# DEPENDENCIES
# ============================================================================

include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)

# Raylib
FetchContent_Declare(
  raylib
  URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

# Dear ImGui (C++ version - docking branch for rlImGui compatibility)
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG docking
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
  FetchContent_Populate(imgui)
endif()

# rlImGui (Raylib + ImGui integration)
FetchContent_Declare(
  rlimgui
  GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
  GIT_TAG main
)
FetchContent_GetProperties(rlimgui)
if(NOT rlimgui_POPULATED)
  FetchContent_Populate(rlimgui)
endif()

# Google Test
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Doctest
FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG v2.4.11
)
FetchContent_MakeAvailable(doctest)

# Boost.PFR (reflection library)
FetchContent_Declare(
  pfr
  GIT_REPOSITORY https://github.com/boostorg/pfr.git
  GIT_TAG 2.2.0
)
FetchContent_MakeAvailable(pfr)

# Lua
FetchContent_Declare(
  lua
  URL https://github.com/lua/lua/archive/refs/tags/v5.4.7.tar.gz
)
FetchContent_GetProperties(lua)
if(NOT lua_POPULATED)
  FetchContent_Populate(lua)
  # Build Lua as a static library from source
  file(GLOB LUA_SOURCES "${lua_SOURCE_DIR}/*.c")
  list(REMOVE_ITEM LUA_SOURCES "${lua_SOURCE_DIR}/lua.c" "${lua_SOURCE_DIR}/luac.c" "${lua_SOURCE_DIR}/onelua.c")
  add_library(lua_lib STATIC ${LUA_SOURCES})
  target_include_directories(lua_lib PUBLIC ${lua_SOURCE_DIR})
  set_target_properties(lua_lib PROPERTIES LINKER_LANGUAGE C)
  if(UNIX)
    target_compile_definitions(lua_lib PRIVATE LUA_USE_POSIX)
    target_link_libraries(lua_lib PRIVATE m)
  endif()
endif()


# ============================================================================
# LIBRARIES
# ============================================================================

# Dear ImGui library (C++)
add_library(imgui_lib STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)
target_include_directories(imgui_lib PUBLIC ${imgui_SOURCE_DIR})

# rlImGui library
add_library(rlimgui_lib STATIC
  ${rlimgui_SOURCE_DIR}/rlImGui.cpp
)
target_include_directories(rlimgui_lib PUBLIC
  ${rlimgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}
)
target_link_libraries(rlimgui_lib PUBLIC imgui_lib raylib)

# ============================================================================
# GAME TARGETS - Add your games here
# ============================================================================

# Example game
add_executable(example_game src/scratch/example_game/main.cpp)
target_include_directories(example_game PRIVATE ${imgui_SOURCE_DIR})
target_link_libraries(example_game PRIVATE raylib imgui_lib rlimgui_lib)
if(UNIX AND NOT APPLE)
  target_link_libraries(example_game PRIVATE stdc++)
endif()

# ============================================================================
# Helper function to add new games easily
# Usage: add_game(game_name)
# ============================================================================
function(add_game GAME_NAME)
  file(GLOB_RECURSE GAME_SOURCES "src/scratch/${GAME_NAME}/*.cpp")
  if(GAME_SOURCES)
    add_executable(${GAME_NAME} ${GAME_SOURCES})
    target_include_directories(${GAME_NAME} PRIVATE ${imgui_SOURCE_DIR})
    target_link_libraries(${GAME_NAME} PRIVATE raylib imgui_lib rlimgui_lib)
    if(UNIX AND NOT APPLE)
      target_link_libraries(${GAME_NAME} PRIVATE stdc++)
    endif()
  endif()
endfunction()

# Hex grid demo
add_game(hexgrid_demo)

# Entity demo (ilist + asset_helpers)
add_game(entity_demo)
target_link_libraries(entity_demo PRIVATE Boost::pfr lua_lib)

# ImGui prototype
add_game(imgui_prototype)

# Blender GLB Zoo (experiment)
add_game(blender_glb_zoo)

# GLB Hotload Viewer
add_game(glb_hotload)


# ============================================================================
# TESTS
# ============================================================================

# Doctest runner for mylibs (with Raylib/ImGui for visual tests)
add_executable(mylibs_tests src/mylibs/tests_main.cpp)
target_include_directories(mylibs_tests PRIVATE src/mylibs ${imgui_SOURCE_DIR})
target_link_libraries(mylibs_tests PRIVATE doctest::doctest raylib imgui_lib rlimgui_lib)
if(UNIX AND NOT APPLE)
  target_link_libraries(mylibs_tests PRIVATE stdc++)
endif()

# ============================================================================
# CLI TOOLS
# ============================================================================
