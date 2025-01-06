# Projects Settings
cmake_minimum_required (VERSION 3.1)
project (opengl-cmake-skeleton)

file(GLOB cpp_source_files "${CMAKE_SOURCE_DIR}/src/*.cpp")
file(GLOB c_source_files "${CMAKE_SOURCE_DIR}/src/gl.c")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_executable(VisuDemo ${cpp_source_files} ${c_source_files} )
set_property(TARGET VisuDemo PROPERTY CXX_STANDARD 11)
target_compile_options(VisuDemo PRIVATE -Wall -lGL -lglfw)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

include_directories(${OPENGL_INCLUDE_DIR})
target_include_directories(VisuDemo PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(VisuDemo ${OPENGL_gl_LIBRARY} glfw)

