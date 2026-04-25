cmake_minimum_required(VERSION 3.20)
project(Mycraft VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- macOS: help dlopen find libvulkan ---
if(APPLE)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    set(CMAKE_BUILD_RPATH "/opt/homebrew/lib")
endif()

# --- Find Vulkan ---
find_package(Vulkan REQUIRED)

# --- Fetch dependencies ---
include(FetchContent)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
if(APPLE)
    set(GLFW_VULKAN_STATIC OFF CACHE BOOL "" FORCE)
    add_compile_definitions(_GLFW_VULKAN_LIBRARY="${Vulkan_LIBRARY}")
endif()
FetchContent_MakeAvailable(glfw)

# GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
)
FetchContent_MakeAvailable(glm)

# VMA (Vulkan Memory Allocator)
FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG        v3.1.0
)
FetchContent_MakeAvailable(vma)

# vk-bootstrap
FetchContent_Declare(
    vkbootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
    GIT_TAG        v1.3.296
)
FetchContent_MakeAvailable(vkbootstrap)

# stb (for stb_image)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
)
FetchContent_MakeAvailable(stb)

# FastNoiseLite
FetchContent_Declare(
    fastnoiselite
    GIT_REPOSITORY https://github.com/Auburn/FastNoiseLite.git
    GIT_TAG        v1.1.1
)
FetchContent_MakeAvailable(fastnoiselite)

# --- Shader compilation ---
find_program(GLSLC glslc HINTS ${Vulkan_GLSLC_EXECUTABLE} $ENV{VULKAN_SDK}/bin)
if(NOT GLSLC)
    message(FATAL_ERROR "glslc not found! Install the Vulkan SDK.")
endif()

file(GLOB_RECURSE SHADER_SOURCES
    "${CMAKE_SOURCE_DIR}/src/shaders/*.vert"
    "${CMAKE_SOURCE_DIR}/src/shaders/*.frag"
    "${CMAKE_SOURCE_DIR}/src/shaders/*.comp"
)

set(SHADER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

set(SHADER_SPIRV_FILES "")
foreach(SHADER_SRC ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER_SRC} NAME)
    set(SHADER_SPV "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")
    add_custom_command(
        OUTPUT ${SHADER_SPV}
        COMMAND ${GLSLC} ${SHADER_SRC} -o ${SHADER_SPV}
        DEPENDS ${SHADER_SRC}
        COMMENT "Compiling shader: ${SHADER_NAME}"
    )
    list(APPEND SHADER_SPIRV_FILES ${SHADER_SPV})
endforeach()

add_custom_target(shaders ALL DEPENDS ${SHADER_SPIRV_FILES})

# --- Main executable ---
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/*.h"
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${stb_SOURCE_DIR}
    ${fastnoiselite_SOURCE_DIR}/Cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
    GPUOpen::VulkanMemoryAllocator
    vk-bootstrap::vk-bootstrap
)

# macOS 和 Linux 都需要 pthread
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

add_dependencies(${PROJECT_NAME} shaders)

# Pass shader directory to code
target_compile_definitions(${PROJECT_NAME} PRIVATE
    SHADER_DIR="${SHADER_OUTPUT_DIR}"
    ASSET_DIR="${CMAKE_SOURCE_DIR}/assets"
)
