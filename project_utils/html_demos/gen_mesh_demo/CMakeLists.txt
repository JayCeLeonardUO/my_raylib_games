cmake_minimum_required(VERSION 3.20)
project(gen_mesh_demo C CXX)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

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

add_executable(gen_mesh_demo src/main.cpp)
target_link_libraries(gen_mesh_demo PRIVATE raylib)

if (EMSCRIPTEN)
    set_target_properties(gen_mesh_demo PROPERTIES SUFFIX ".html")
    target_link_options(gen_mesh_demo PRIVATE
        -sUSE_GLFW=3
        -sALLOW_MEMORY_GROWTH=1
        -sSINGLE_FILE=1
        -sEXPORTED_FUNCTIONS=_main,_js_set_pixel,_js_rebuild_mesh,_js_get_map_size,_js_set_render_mode
        -sEXPORTED_RUNTIME_METHODS=ccall,cwrap
        --shell-file ${CMAKE_SOURCE_DIR}/src/shell.html
        --pre-js ${CMAKE_SOURCE_DIR}/src/pre.js
    )
    add_custom_command(TARGET gen_mesh_demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE_DIR:gen_mesh_demo>/gen_mesh_demo.html
            ${CMAKE_SOURCE_DIR}/gen_mesh_demo.html
    )
endif ()
