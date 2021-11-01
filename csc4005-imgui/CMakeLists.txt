cmake_minimum_required(VERSION 3.2)
project(csc4005_imgui)

set(CMAKE_CXX_STANDARD 17)

find_package(SDL2 REQUIRED)
find_package(Freetype REQUIRED)
find_package(MPI REQUIRED)
find_package(Threads REQUIRED)
set(CMAKE_CXX_STANDARD 20)
set(OpenGL_GL_PREFERENCE "GLVND")
find_package(OpenGL REQUIRED)

include_directories(
        include
        imgui
        imgui/backends
        ${SDL2_INCLUDE_DIRS}
        ${FREETYPE_INCLUDE_DIRS}
        ${MPI_CXX_INCLUDE_DIRS})

file(GLOB IMGUI_SRC
        imgui/*.cpp
        imgui/backends/imgui_impl_sdl.cpp
        imgui/backends/imgui_impl_opengl2.cpp
        imgui/misc/freetype/imgui_freetype.cpp
        imgui/misc/fonts/binary_to_compressed_c.cpp
        imgui/misc/cpp/imgui_stdlib.cpp
        )

add_library(core STATIC ${IMGUI_SRC})
file(GLOB CSC4005_PROJECT_SRC src/main.cpp src/graphic.cpp)
file(GLOB CSC4005_PROJECT_SRCD src/main_d.cpp src/graphic.cpp)
file(GLOB CSC4005_PROJECT_SRCP src/main_p.cpp src/graphic.cpp)
file(GLOB CSC4005_PROJECT_SRCPD src/main_p_dynamic.cpp src/graphic.cpp)
add_executable(csc4005_imgui ${CSC4005_PROJECT_SRC})
add_executable(csc4005_imguid ${CSC4005_PROJECT_SRCD})
add_executable(csc4005_imguip ${CSC4005_PROJECT_SRCP})
add_executable(csc4005_imguipd ${CSC4005_PROJECT_SRCPD})
get_filename_component(FONT_PATH imgui/misc/fonts/DroidSans.ttf ABSOLUTE)
target_link_libraries(core PUBLIC
        Freetype::Freetype SDL2::SDL2 OpenGL::GL ${CMAKE_DL_LIBS} Threads::Threads ${MPI_CXX_LIBRARIES})
target_link_libraries(csc4005_imgui core)
target_compile_options(csc4005_imgui PRIVATE -Werror -Wall -Wextra -Wpedantic)
target_link_libraries(csc4005_imguid core)
target_compile_options(csc4005_imguid PRIVATE -Werror -Wall -Wextra -Wpedantic)
target_link_libraries(csc4005_imguip core)
target_compile_options(csc4005_imguip PRIVATE -Werror -Wall -Wextra -Wpedantic)
target_link_libraries(csc4005_imguipd core)
target_compile_options(csc4005_imguipd PRIVATE -Werror -Wall -Wextra -Wpedantic)
target_compile_definitions(core PUBLIC -DImDrawIdx=unsigned)
target_compile_definitions(csc4005_imgui PRIVATE -DFONT_PATH=\"${FONT_PATH}\")
target_compile_definitions(csc4005_imguid PRIVATE -DFONT_PATH=\"${FONT_PATH}\")
target_compile_definitions(csc4005_imguip PRIVATE -DFONT_PATH=\"${FONT_PATH}\")
target_compile_definitions(csc4005_imguipd PRIVATE -DFONT_PATH=\"${FONT_PATH}\")