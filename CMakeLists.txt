# Projects Settings
cmake_minimum_required (VERSION 3.12)
project (opengl-cmake-skeleton)

file(GLOB cpp_source_files "${CMAKE_SOURCE_DIR}/src/*.cpp")
file(GLOB c_source_files "${CMAKE_SOURCE_DIR}/src/gl.c")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_executable(VisuDemo ${cpp_source_files} ${c_source_files} )
set_property(TARGET VisuDemo PROPERTY CXX_STANDARD 11)
target_compile_options(VisuDemo PRIVATE -Wall -lGL -lglfw)

find_package(OpenGL REQUIRED)
include(FetchContent)
FetchContent_Declare(GLFW GIT_REPOSITORY https://github.com/glfw/glfw.git)
fetchcontent_makeavailable(GLFW)

include_directories(${OPENGL_INCLUDE_DIR})
target_include_directories(VisuDemo PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(VisuDemo ${OPENGL_gl_LIBRARY} glfw)

