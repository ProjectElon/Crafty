# Crafty
a minecraft clone game for educational purposes.

What I've Learned:
- Memory Management using Arena Allocators (Linear Allocator).
- Understaning Multithreading with std::thread and std::atomics and Building a Job System.
- The ins and outs of a Voxel Renderer with Optimizations:
  - Vertex Data Compression.
  - Frustum Culling.
  - Dynamic Lighting system with Day/Night Cycle, unlimited number of light sources and smoth lighting.
  - Voxel Mesh Generation with Back Face Culling and interior culling to reduce VRam Footprint.
  - Ambient Occlusion.
  - Persistent Mapped Buffers to Generate Voxel Meshes (Chunks) without a vertex array buffer on different threads.
  - MultidrawIndirect and Instanced Rendering with Command Buffers to reduce driver overhead.
  - Grouping Block Textures into one big array texture with mipmapping and anisotropic filtering to avoid antialiasing.
  - Order Independent Transparency. 
- Procedural content generation using Perlin Noise.
- Loading/Saving Voxel Meshes (Chunks) from/to disk with the help of the job system.
- Dropdown Console where you can type commands while playing the game.
- Basic Physics System to handle Raycasting and collisions with voxels.

![day_screenshot](https://github.com/ProjectElon/Crafty/blob/main/screenshots/day_screenshot.png)
![night_screenshot](https://github.com/ProjectElon/Crafty/blob/main/screenshots/night_screenshot.png)
