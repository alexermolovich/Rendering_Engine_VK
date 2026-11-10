cmake_minimum_required(VERSION 3.20)
project(VkRenderer LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(glfw3 REQUIRED)
find_package(meshoptimizer CONFIG REQUIRED)
find_package(Threads REQUIRED)
add_subdirectory(external/spdlog)

set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE BOOL "Build tinygltf example" FORCE)
set(TINYGLTF_INSTALL OFF CACHE BOOL "Install tinygltf" FORCE)
add_subdirectory(external/tinygltf)
set(TRACY_ENABLE ON CACHE BOOL "Enable Tracy Profiler")
add_subdirectory(external/tracy)

add_library(renderer_core STATIC
    src/core/camera.cpp
    src/core/resources.cpp
    src/core/rendering_technique.cpp
    src/core/renderer.cpp
    src/core/renderer_resources.cpp
    src/core/renderer_scene.cpp
    src/core/renderer_pipelines.cpp
)
target_include_directories(renderer_core PUBLIC src PRIVATE external)
target_link_libraries(renderer_core PUBLIC
    Vulkan::Vulkan
    glm::glm
    glfw
    tinygltf
    spdlog::spdlog
    Threads::Threads
    Tracy::TracyClient
    PRIVATE meshoptimizer::meshoptimizer
)
# Resolve runtime files independently of the executable's working directory.
target_compile_definitions(renderer_core PRIVATE
    VKML_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    VKML_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
)

add_executable(vkml src/demo/main.cpp)
target_link_libraries(vkml PRIVATE renderer_core)

set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SPIRV_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
if(NOT Vulkan_GLSLC_EXECUTABLE)
    find_program(Vulkan_GLSLC_EXECUTABLE NAMES glslc REQUIRED)
endif()
file(MAKE_DIRECTORY "${SPIRV_DIR}")
file(GLOB SHADERS CONFIGURE_DEPENDS
    "${SHADER_DIR}/*.vert"
    "${SHADER_DIR}/*.frag"
    "${SHADER_DIR}/*.comp"
)
set(SPIRV_SHADERS "")
foreach(SHADER IN LISTS SHADERS)
    get_filename_component(FILE_NAME "${SHADER}" NAME)
    set(SPIRV "${SPIRV_DIR}/${FILE_NAME}.spv")
    add_custom_command(
        OUTPUT "${SPIRV}"
        COMMAND "${Vulkan_GLSLC_EXECUTABLE}" -I "${SHADER_DIR}" "${SHADER}" -o "${SPIRV}"
        DEPENDS "${SHADER}" "${SHADER_DIR}/config.h"
        COMMENT "Compiling shader ${FILE_NAME}"
        VERBATIM
    )
    list(APPEND SPIRV_SHADERS "${SPIRV}")
endforeach()
add_custom_target(shaders ALL DEPENDS ${SPIRV_SHADERS})
add_dependencies(vkml shaders)
