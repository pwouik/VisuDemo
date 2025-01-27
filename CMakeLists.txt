# Projects Settings
cmake_minimum_required (VERSION 3.12)
project (opengl-cmake-skeleton)

#Suppressed warning :
# C4710 C4711 purely informational (source : internet)
# C4365 int to GLuint
# C4191 idk
# C4244 conversion double to float
# C4100 idk 
# C5039 idk
# C5045 C5219 C4514 idk
#ideally suppressed warning only comming from given path
if(MSVC)
    add_compile_options(/wd4710 /wd4711 /wd4191 /wd4244 /wd4100 /wd4514 /wd4464 /wd4820 /wd4365 /wd5039 /wd4505)
endif()


file(GLOB cpp_source_files "${CMAKE_SOURCE_DIR}/src/*.cpp")
file(GLOB c_source_files "${CMAKE_SOURCE_DIR}/src/gl.c")
file(GLOB IMGUI_source_files "${CMAKE_SOURCE_DIR}/include/imgui/*.cpp")

#disable all warning for imgui src
if(MSVC)
    foreach(src_file ${IMGUI_source_files})
        set_source_files_properties(${src_file} PROPERTIES COMPILE_FLAGS "/W0")
    endforeach()
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_executable(VisuDemo ${cpp_source_files} ${c_source_files} ${IMGUI_source_files})
set_property(TARGET VisuDemo PROPERTY CXX_STANDARD 14)
target_compile_options(VisuDemo PRIVATE -Wall)

find_package(OpenGL REQUIRED)

# GLFW
find_package(glfw3 QUIET)
if(NOT glfw3_FOUND)
    include(FetchContent)
    FetchContent_Declare(GLFW GIT_REPOSITORY https://github.com/glfw/glfw.git)
    FetchContent_MakeAvailable(GLFW)
endif()

# GLM
find_package(glm CONFIG QUIET)
if (NOT glm_FOUND)
    include(FetchContent)
    FetchContent_Declare(GLM GIT_REPOSITORY https://github.com/icaven/glm.git)
    FetchContent_MakeAvailable(GLM)
endif ()


# Leap motion SDK
if (WIN32)
    set(ULTRALEAP_PATH_ROOT "$ENV{ProgramFiles}/Ultraleap")
elseif (APPLE)
    set(ULTRALEAP_PATH_ROOT "/Applications/Ultraleap\ Hand\ Tracking.app/Contents/LeapSDK/lib/cmake/LeapSDK")
else()
    # Linux and other OSs
    set(ULTRALEAP_PATH_ROOT "/usr/lib/ultraleap-hand-tracking-service")
endif()

find_package(LeapSDK
        5
        REQUIRED
        PATHS
        "${ULTRALEAP_PATH_ROOT}")
if (NOT LeapSDK_FOUND)
    message("aaaaaaaaaaaa")
endif ()


include_directories(${OPENGL_INCLUDE_DIR})
target_include_directories(VisuDemo PRIVATE "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(VisuDemo ${OPENGL_gl_LIBRARY} glfw glm::glm-header-only LeapSDK::LeapC)

get_target_property(
        LEAPC_IMPORTED_CONFIG
        LeapSDK::LeapC
        IMPORTED_CONFIGURATIONS
)

get_target_property(
        LEAPC_SHARED_LIB_PATH
        LeapSDK::LeapC
        IMPORTED_LOCATION_${LEAPC_IMPORTED_CONFIG}
)

add_custom_command(
        TARGET
        VisuDemo
        POST_BUILD
        COMMAND
        ${CMAKE_COMMAND} -E copy
        ${LEAPC_SHARED_LIB_PATH}
        $<TARGET_FILE_DIR:VisuDemo>)