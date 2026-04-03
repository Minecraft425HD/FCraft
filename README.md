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

Open `vkCraft.vcxproj` in Visual Studio (Windows).
