cmake_minimum_required(VERSION 3.20)
project(file_viewer_html C CXX)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

# Directory to embed — pass with -DEMBED_DIR=/path/to/dir
set(EMBED_DIR "" CACHE PATH "Directory to embed into the viewer")

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

# cgltf (single-header glTF parser)
FetchContent_Declare(
    cgltf
    GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
    GIT_TAG v1.14
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(cgltf)
if (NOT cgltf_POPULATED)
    FetchContent_Populate(cgltf)
endif ()

# Main target
add_executable(file_viewer_html src/main.cpp)
target_include_directories(file_viewer_html PRIVATE ${imgui_SOURCE_DIR} ${cgltf_SOURCE_DIR})
target_link_libraries(file_viewer_html PRIVATE raylib imgui_lib rlimgui_lib)

if (EMSCRIPTEN)
    set_target_properties(file_viewer_html PROPERTIES SUFFIX ".html")
    target_link_options(file_viewer_html PRIVATE
        -sUSE_GLFW=3
        -sALLOW_MEMORY_GROWTH=1
        -sSINGLE_FILE=1
        -sFORCE_FILESYSTEM=1
        --shell-file ${CMAKE_SOURCE_DIR}/src/shell.html
        --pre-js ${CMAKE_SOURCE_DIR}/src/pre.js
    )

    if (EMBED_DIR)
        target_link_options(file_viewer_html PRIVATE
            --embed-file ${EMBED_DIR}@/data
        )
    endif ()

    # prepend bash header so the html can self-serve, then copy to project root
    add_custom_command(TARGET file_viewer_html POST_BUILD
        COMMAND ${CMAKE_SOURCE_DIR}/src/postbuild.sh
            ${CMAKE_SOURCE_DIR}/src/header.sh
            $<TARGET_FILE_DIR:file_viewer_html>/file_viewer_html.html
            ${CMAKE_SOURCE_DIR}/file_viewer_html.html
    )
endif ()
