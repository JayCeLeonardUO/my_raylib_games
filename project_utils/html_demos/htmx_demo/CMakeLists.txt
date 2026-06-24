cmake_minimum_required(VERSION 3.20)
project(htmx_demo C CXX)

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

add_executable(htmx_demo src/main.cpp)
target_link_libraries(htmx_demo PRIVATE raylib)

if (EMSCRIPTEN)
    set_target_properties(htmx_demo PROPERTIES SUFFIX ".html")
    target_link_options(htmx_demo PRIVATE
        -sUSE_GLFW=3
        -sALLOW_MEMORY_GROWTH=1
        -sSINGLE_FILE=1
        -sEXPORTED_FUNCTIONS=_main,_js_set_color,_js_toggle_spin,_js_get_angle,_js_get_color
        -sEXPORTED_RUNTIME_METHODS=ccall,cwrap
        --shell-file ${CMAKE_SOURCE_DIR}/src/shell.html
        --pre-js ${CMAKE_SOURCE_DIR}/src/pre.js
    )
    add_custom_command(TARGET htmx_demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
            $<TARGET_FILE_DIR:htmx_demo>/htmx_demo.html
            ${CMAKE_SOURCE_DIR}/htmx_demo.html
    )
endif ()
