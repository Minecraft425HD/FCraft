cmake_minimum_required(VERSION 3.10...3.27)

project(vkCraft)

find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)

# Auto-download stb_image.h (header-only, no build needed)
include(FetchContent)
FetchContent_Declare(
    stb
    URL https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
    DOWNLOAD_NO_EXTRACT TRUE
    DOWNLOAD_DIR ${CMAKE_SOURCE_DIR}/src/include
)
FetchContent_MakeAvailable(stb)

# ---------------------------------------------------------------------------
# Homebrew prefix detection (macOS Apple Silicon & Intel)
# ---------------------------------------------------------------------------
if(APPLE)
  # Prefer an explicit HOMEBREW_PREFIX env-var; fall back to arm64 then x86 default
  if(DEFINED ENV{HOMEBREW_PREFIX})
    set(BREW_PREFIX "$ENV{HOMEBREW_PREFIX}")
  elseif(EXISTS "/opt/homebrew")
    set(BREW_PREFIX "/opt/homebrew")
  else()
    set(BREW_PREFIX "/usr/local")
  endif()

  # glfw
  list(APPEND CMAKE_PREFIX_PATH "${BREW_PREFIX}/opt/glfw")
  # glm  (header-only — just needs the include dir)
  set(GLM_INCLUDE_DIR "${BREW_PREFIX}/include")
  # tinyobjloader
  set(TINYOBJ_INCLUDE_DIR "${BREW_PREFIX}/include")
  set(TINYOBJ_LIB_DIR     "${BREW_PREFIX}/lib")
endif()

set(SOURCE_FILES
    src/vkCraft/BoxGeometry.cpp
    src/vkCraft/BufferUtils.cpp
    src/vkCraft/Camera.cpp
    src/vkCraft/Chunk.cpp
    src/vkCraft/ChunkGeometry.cpp
    src/vkCraft/ChunkNode.cpp
    src/vkCraft/ChunkWorld.cpp
    src/vkCraft/CommandBufferUtils.cpp
    src/vkCraft/Device.cpp
    src/vkCraft/FileUtils.cpp
    src/vkCraft/FirstPersonCamera.cpp
    src/vkCraft/Geometry.cpp
    src/vkCraft/main.cpp
    src/vkCraft/Object3D.cpp
    src/vkCraft/PlaneGeometry.cpp
    src/vkCraft/QueueFamilyIndices.cpp
    src/vkCraft/Texture.cpp
    src/vkCraft/Vertex.cpp
    src/vkCraft/VkCraft.cpp
)

set(HEADER_FILES
    src/vkCraft/BoxGeometry.h
    src/vkCraft/BufferUtils.h
    src/vkCraft/Camera.h
    src/vkCraft/Chunk.h
    src/vkCraft/ChunkGeometry.h
    src/vkCraft/ChunkNode.h
    src/vkCraft/ChunkWorld.h
    src/vkCraft/CommandBufferUtils.h
    src/vkCraft/Device.h
    src/vkCraft/FileUtils.h
    src/vkCraft/FirstPersonCamera.h
    src/vkCraft/Geometry.h
    src/vkCraft/Object3D.h
    src/vkCraft/PlaneGeometry.h
    src/vkCraft/QueueFamilyIndices.h
    src/vkCraft/SwapChainSupportDetails.h
    src/vkCraft/UniformBufferObject.h
    src/vkCraft/Vertex.h
    src/vkCraft/VkCraft.h
)

include_directories(
    src/include
    ${GLFW_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIR}
    ${TINYOBJ_INCLUDE_DIR}
    ${Vulkan_INCLUDE_DIRS}
)

add_executable(vkCraft ${SOURCE_FILES})
add_library(libvkCraft SHARED ${SOURCE_FILES})
set_target_properties(libvkCraft PROPERTIES OUTPUT_NAME "vkCraft")

# tinyobjloader: try find_package first, fall back to raw link path
find_library(TINYOBJ_LIB tinyobjloader HINTS ${TINYOBJ_LIB_DIR})
if(TINYOBJ_LIB)
  set(TINYOBJ_LINK ${TINYOBJ_LIB})
else()
  set(TINYOBJ_LINK tinyobjloader)
endif()

target_link_libraries(vkCraft
    glfw
    ${TINYOBJ_LINK}
    ${Vulkan_LIBRARIES}
)

target_link_libraries(libvkCraft
    glfw
    ${TINYOBJ_LINK}
    ${Vulkan_LIBRARIES}
)

# Compile shaders: block.vert -> shader/vert.spv, block.frag -> shader/frag.spv
# Output goes into build/shader/ so the executable finds them at runtime
add_custom_command(TARGET vkCraft POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shader
    COMMAND Vulkan::glslangValidator
        --target-env vulkan1.0
        -o ${CMAKE_BINARY_DIR}/shader/vert.spv
        ${CMAKE_SOURCE_DIR}/src/shaders/block.vert
    COMMAND Vulkan::glslangValidator
        --target-env vulkan1.0
        -o ${CMAKE_BINARY_DIR}/shader/frag.spv
        ${CMAKE_SOURCE_DIR}/src/shaders/block.frag
    COMMENT "Compiling shaders"
)

add_custom_command(TARGET vkCraft
    POST_BUILD
    COMMAND cp -r ${CMAKE_SOURCE_DIR}/src/vkCraft/texture ${CMAKE_CURRENT_BINARY_DIR}/texture
)

install(TARGETS vkCraft      DESTINATION bin)
install(TARGETS libvkCraft   DESTINATION lib)
install(FILES ${HEADER_FILES} DESTINATION include/vkCraft)
