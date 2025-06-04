cmake_minimum_required(VERSION 3.15)
project(imgui_laptimer)
set(CMAKE_CXX_STANDARD 17)

file(GLOB IMGUI_SRC
        imgui/*.cpp
        imgui/backends/imgui_impl_sdl2.cpp
        imgui/backends/imgui_impl_opengl3.cpp
)

include_directories(
        imgui
        imgui/backends
        SDL2/include/SDL2
)

link_directories(${CMAKE_SOURCE_DIR}/SDL2/lib)

add_executable(imgui_laptimer
        main.cpp
        ${IMGUI_SRC}
)

set_target_properties(imgui_laptimer PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

target_link_libraries(imgui_laptimer
        mingw32
        SDL2main
        SDL2
        opengl32
)

add_custom_command(TARGET imgui_laptimer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/SDL2/bin"
        "${CMAKE_SOURCE_DIR}/bin"
)
