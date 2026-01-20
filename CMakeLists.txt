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

# cimgui (C bindings for Dear ImGui)
FetchContent_Declare(
  cimgui
  GIT_REPOSITORY https://github.com/cimgui/cimgui.git
  GIT_TAG master
  GIT_SUBMODULES "imgui"
)
FetchContent_MakeAvailable(cimgui)

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

# ============================================================================
# LIBRARIES
# ============================================================================

# cimgui library
add_library(cimgui_lib STATIC
  ${cimgui_SOURCE_DIR}/cimgui.cpp
  ${cimgui_SOURCE_DIR}/imgui/imgui.cpp
  ${cimgui_SOURCE_DIR}/imgui/imgui_demo.cpp
  ${cimgui_SOURCE_DIR}/imgui/imgui_draw.cpp
  ${cimgui_SOURCE_DIR}/imgui/imgui_tables.cpp
  ${cimgui_SOURCE_DIR}/imgui/imgui_widgets.cpp
)
target_include_directories(cimgui_lib PUBLIC
  ${cimgui_SOURCE_DIR}
  ${cimgui_SOURCE_DIR}/imgui
)

# rlImGui library
add_library(rlimgui_lib STATIC
  ${rlimgui_SOURCE_DIR}/rlImGui.cpp
)
target_include_directories(rlimgui_lib PUBLIC
  ${rlimgui_SOURCE_DIR}
  ${cimgui_SOURCE_DIR}
  ${cimgui_SOURCE_DIR}/imgui
)
target_link_libraries(rlimgui_lib PUBLIC cimgui_lib raylib)

# ============================================================================
# GAME TARGETS - Add your games here
# ============================================================================

# Example game
add_executable(example_game src/scratch/example_game/main.cpp)
target_link_libraries(example_game PRIVATE raylib cimgui_lib rlimgui_lib)
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
    target_link_libraries(${GAME_NAME} PRIVATE raylib cimgui_lib rlimgui_lib)
    if(UNIX AND NOT APPLE)
      target_link_libraries(${GAME_NAME} PRIVATE stdc++)
    endif()
  endif()
endfunction()
