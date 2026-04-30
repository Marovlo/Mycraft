cmake_minimum_required(VERSION 3.20)
project(Mycraft VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- macOS: help dlopen find libvulkan ---
if(APPLE)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    # 支持 Apple Silicon (/opt/homebrew) 和 Intel Mac (/usr/local)
    set(CMAKE_BUILD_RPATH "/opt/homebrew/lib;/usr/local/lib")
endif()

# --- Find Vulkan ---
find_package(Vulkan REQUIRED)
if(NOT Vulkan_FOUND)
    message(FATAL_ERROR
        "Vulkan not found! Install:\n"
        "  macOS: brew install vulkan-headers vulkan-loader shaderc molten-vk\n"
        "  Linux: sudo apt install vulkan-sdk\n"
        "  Or run: ./mycraft.sh setup")
endif()

# --- Fetch dependencies ---
include(FetchContent)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    ON
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
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(glm)

# VMA (Vulkan Memory Allocator)
FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG        v3.1.0
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(vma)

# vk-bootstrap
FetchContent_Declare(
    vkbootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap.git
    GIT_TAG        v1.3.296
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(vkbootstrap)

# stb (for stb_image)
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(stb)

# FastNoiseLite
FetchContent_Declare(
    fastnoiselite
    GIT_REPOSITORY https://github.com/Auburn/FastNoiseLite.git
    GIT_TAG        v1.1.1
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(fastnoiselite)

# ENet (UDP networking library, MIT License)
FetchContent_Declare(
    enet
    GIT_REPOSITORY https://github.com/lsalzman/enet.git
    GIT_TAG        v1.3.18
    GIT_SHALLOW    ON
)
FetchContent_MakeAvailable(enet)

# --- Shader compilation ---
# 优先使用 find_package(Vulkan) 已经找到的 glslc
if(Vulkan_GLSLC_EXECUTABLE)
    set(GLSLC "${Vulkan_GLSLC_EXECUTABLE}")
else()
    find_program(GLSLC glslc
        HINTS
            $ENV{VULKAN_SDK}/bin
            /opt/homebrew/bin      # macOS Apple Silicon (Homebrew)
            /usr/local/bin         # macOS Intel (Homebrew)
    )
endif()
if(NOT GLSLC)
    message(FATAL_ERROR
        "glslc not found! Please install the Vulkan SDK:\n"
        "  macOS:  brew install vulkan-headers vulkan-loader shaderc molten-vk\n"
        "  Linux:  sudo apt install vulkan-sdk  (or see https://vulkan.lunarg.com)\n"
        "  Or run: ./mycraft.sh setup")
endif()
message(STATUS "Using glslc: ${GLSLC}")

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

# --- Common include directories ---
set(COMMON_INCLUDES
    ${CMAKE_SOURCE_DIR}/src
    ${stb_SOURCE_DIR}
    ${fastnoiselite_SOURCE_DIR}/Cpp
    ${enet_SOURCE_DIR}/include
)

# macOS 和 Linux 都需要 pthread
find_package(Threads REQUIRED)

# --- Main client executable ---
file(GLOB_RECURSE CLIENT_SOURCES
    "src/*.cpp"
    "src/*.h"
)
# 排除专用服务器入口文件（避免多个 main 函数冲突）
list(FILTER CLIENT_SOURCES EXCLUDE REGEX "src/server/dedicated_server\\.cpp")

add_executable(${PROJECT_NAME} ${CLIENT_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${COMMON_INCLUDES})

target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
    GPUOpen::VulkanMemoryAllocator
    vk-bootstrap::vk-bootstrap
    enet
    Threads::Threads
)

# macOS: miniaudio 需要 CoreAudio / AudioToolbox 框架
if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework CoreFoundation"
    )
endif()

add_dependencies(${PROJECT_NAME} shaders)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    SHADER_DIR="${SHADER_OUTPUT_DIR}"
    ASSET_DIR="${CMAKE_SOURCE_DIR}/assets"
    SOUND_DIR="${CMAKE_SOURCE_DIR}/assets/sounds"
)

# --- Dedicated Server executable (headless, no rendering) ---
# 使用所有源文件，但排除客户端 main.cpp（使用自己的入口）
file(GLOB_RECURSE SERVER_ALL_SOURCES
    "src/*.cpp"
    "src/*.h"
)
# 排除客户端入口
list(FILTER SERVER_ALL_SOURCES EXCLUDE REGEX "src/main\\.cpp")

add_executable(MycraftServer ${SERVER_ALL_SOURCES})

target_include_directories(MycraftServer PRIVATE ${COMMON_INCLUDES})

# Headless server 仍需要 Vulkan/GLFW/VMA 等头文件（共享代码间接依赖）
# 但不使用渲染功能
target_link_libraries(MycraftServer PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
    GPUOpen::VulkanMemoryAllocator
    vk-bootstrap::vk-bootstrap
    enet
    Threads::Threads
)

# macOS: miniaudio 需要 CoreAudio / AudioToolbox 框架
if(APPLE)
    target_link_libraries(MycraftServer PRIVATE
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework CoreFoundation"
    )
endif()

target_compile_definitions(MycraftServer PRIVATE
    MYCRAFT_HEADLESS_SERVER=1
    SHADER_DIR="${SHADER_OUTPUT_DIR}"
    ASSET_DIR="${CMAKE_SOURCE_DIR}/assets"
    SOUND_DIR="${CMAKE_SOURCE_DIR}/assets/sounds"
)
