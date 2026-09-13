# FCraft

A Minecraft clone written in **C++** using the **Vulkan** graphics API and **GLFW** as the window/input backend.

## Architecture

```
main.cpp
  └── VkCraft            ← Main Vulkan engine (init, swap chain, pipeline, render loop)
        ├── ChunkWorld    ← World management
        │     └── ChunkNode  ← Chunk tree (recursive, 6 neighbors)
        │           ├── Chunk         ← Raw block data
        │           └── ChunkGeometry ← Mesh for this chunk
        ├── FirstPersonCamera  ← FPS camera (WASD + mouse look)
        ├── Geometry (base class)
        │     ├── BoxGeometry
        │     └── PlaneGeometry
        └── Vulkan Utils
              ├── BufferUtils
              └── CommandBufferUtils
```

## Controls

| Key | Action |
|-----|--------|
| W/A/S/D | Move |
| Shift | Sprint (3× speed) |
| Space | Fly up |
| C | Fly down |
| Left Mouse + Drag | Mouse look |
| Arrow Keys | Keyboard look |

## Dependencies

- [Vulkan SDK](https://vulkan.lunarg.com/)
- [GLFW](https://www.glfw.org/)
- [GLM](https://github.com/g-truc/glm)

## Coordinate System

- X = right
- Y = up  
- Z = back

## Build

### Windows

Open `vkCraft.vcxproj` in Visual Studio.

### macOS

Vulkan isn't native on macOS — it runs through [MoltenVK](https://github.com/KhronosGroup/MoltenVK), which ships inside the official Vulkan SDK. The project already detects Homebrew and adds the right include/lib paths for Apple Silicon and Intel.

1. Install the [Vulkan SDK for macOS](https://vulkan.lunarg.com/sdk/home#mac) (the `.dmg` installer) and run its `install_vulkan.py` (or use the installer's setup step) so `glslangValidator`, the validation layers and MoltenVK are on your system.
2. Install the remaining dependencies with [Homebrew](https://brew.sh):
   ```sh
   brew install cmake glfw glm tinyobjloader
   ```
3. Configure and build:
   ```sh
   mkdir build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release
   cmake --build . -j
   ```
4. Run it from the `build` directory (the executable expects `shader/` and `texture/` next to it, which the build already copies there):
   ```sh
   ./vkCraft
   ```

If CMake can't find Vulkan, make sure you opened a fresh terminal after installing the SDK (or `source` the SDK's `setup-env.sh`) so `VULKAN_SDK` is set.
