cmake_minimum_required(VERSION 3.20)
project(glb_anim_demo C)

set(CMAKE_C_STANDARD 99)

include(FetchContent)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(raylib)

add_executable(glb_anim_demo
    src/main.c
)
target_include_directories(glb_anim_demo PRIVATE src)
target_link_libraries(glb_anim_demo PRIVATE raylib)

# Copy the glb next to the binary so it can find it
add_custom_command(TARGET glb_anim_demo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/test_cubes.glb
        $<TARGET_FILE_DIR:glb_anim_demo>/test_cubes.glb
)
