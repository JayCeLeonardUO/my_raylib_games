cmake_minimum_required(VERSION 3.20)
project(vault_viewer C CXX)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

# Raylib
FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
if (EMSCRIPTEN)
    set(PLATFORM "Web" CACHE STRING "" FORCE)
endif ()
FetchContent_MakeAvailable(raylib)

# Dear ImGui (docking branch)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)
FetchContent_GetProperties(imgui)
if (NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif ()

add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
)
target_include_directories(imgui_lib PUBLIC ${imgui_SOURCE_DIR})

# rlImGui
FetchContent_Declare(
    rlimgui
    GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
    GIT_TAG main
)
FetchContent_GetProperties(rlimgui)
if (NOT rlimgui_POPULATED)
    FetchContent_Populate(rlimgui)
endif ()

add_library(rlimgui_lib STATIC
    ${rlimgui_SOURCE_DIR}/rlImGui.cpp
)
target_include_directories(rlimgui_lib PUBLIC
    ${rlimgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
)
target_link_libraries(rlimgui_lib PUBLIC imgui_lib raylib)

# Main target
add_executable(vault_viewer src/main.cpp)
target_include_directories(vault_viewer PRIVATE ${imgui_SOURCE_DIR})
target_link_libraries(vault_viewer PRIVATE raylib imgui_lib rlimgui_lib)

if (EMSCRIPTEN)
    set_target_properties(vault_viewer PROPERTIES SUFFIX ".html")
    target_link_options(vault_viewer PRIVATE
        -sUSE_GLFW=3
        -sALLOW_MEMORY_GROWTH=1
        -sSINGLE_FILE=1
        --shell-file ${CMAKE_SOURCE_DIR}/src/shell.html
        --pre-js ${CMAKE_SOURCE_DIR}/src/pre.js
    )
    add_custom_command(TARGET vault_viewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE_DIR:vault_viewer>/vault_viewer.html
            ${CMAKE_SOURCE_DIR}/vault_viewer.html
    )
endif ()
