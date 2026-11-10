# Renderer

The original Vulkan passes now live in `src/core/`, and `src/demo/main.cpp` initializes and runs `Renderer`.

## Architecture

- `Renderer` owns the window, Vulkan device, queues, resources, and pipeline map. `init()` creates each technique as a named instance. `DrawFrame()` explicitly orders every pass and its resource transitions.
- `GraphicsPipeline` manages graphics and compute handles. The map key hashes the pipeline type together with its supported `TechniqueFlags`, including depth buffering and FXAA.
- Each `RenderingTechnique` implements `draw(CommandBuffer&)` and holds its supporting pipeline and identifying flag. The existing order is cascade filtering, camera filtering, shadow depth, G-buffer, SSAO, SSAO blur, reflections, composition, and FXAA.
- `Buffer`, `Image`, `RenderTarget`, `Shader`, `Sampler`, and `CommandBuffer` require explicit `destroy()` calls. Their destructors log and throw `std::logic_error` if a Vulkan object remains live. They are never responsible for releasing Vulkan objects during destruction. Destroy live wrappers before C++ deletion; an exception during stack unwinding cannot be recovered safely.
- Resource creation rolls back failed Vulkan allocations. `Renderer::shutdown()` waits for pending preparation and device work, then releases resources in dependency order. It also handles partial initialization and repeated calls.
- `FramePreparationQueue` prepares copied camera and lighting snapshots asynchronously. Its bounded FIFO preserves frame order and propagates worker exceptions. GLFW input, command recording, submission, and FIFO presentation stay on the main thread.
- Shared offscreen targets and filtering output are synchronized between submissions. Indirect counts reset in the command buffer, and presentation semaphores belong to swapchain images. Swapchain recreation rebuilds targets and updates descriptors.
- `BufferType` identifies uniform or storage data; `BufferUsage` identifies vertex, index, rendering, compute, and staging roles. Existing Vulkan usage flags remain available for allocation.

Classes use PascalCase, functions and source filenames use snake_case, and private fields start with `b_`. `DrawFrame` keeps the name explicitly requested in the architecture specification. CMake's required filenames (`CMakeLists.txt`, `CMakePresets.json`, and `CMakeSettings.json`) retain their tool-defined spelling.

The shader algorithms and your composition edit are preserved. The existing SSAO blur shader is still a placeholder; the quality work below remains separate from this cleanup.

## Roadmap

## Implemented Techniques

- [x] Vulkan swapchain and presentation
- [x] GLFW window and Vulkan surface
- [x] Dynamic rendering with `VK_KHR_dynamic_rendering`
- [x] Render-target abstraction
- [x] Sponza glTF scene loading
- [x] Texture loading and upload
- [x] Mesh remapping with `meshoptimizer`
- [x] Fly camera controls
- [x] G-buffer rendering
- [x] SSAO
- [x] SSAO blur pass
- [x] Cascaded shadow mapping
- [x] Shadow depth rendering
- [x] GPU triangle filtering
- [x] GPU frustum culling
- [x] Per-cascade triangle filtering
- [x] Composition pass with direct-light shading and cascade shadows
- [x] Screen-space reflections (SSR) pass blended in compose
- [x] FXAA
- [x] CMake shader compilation to SPIR-V

## Active work

- [ ] SSAO blur improvements
- [ ] SSR quality improvements (edge fades, temporal stability)

## Future Roadmap

- [ ] GPU occlusion culling with a depth pyramid
- [ ] Meshlet generation with `meshoptimizer`
- [ ] Meshlet culling pass
- [ ] Runtime LOD selection
- [ ] Particle system
- [ ] Particle collision against depth
- [ ] Point light shadow cubemaps
- [ ] Deferred lighting pass
- [ ] Tiled or clustered lighting
- [ ] PBR material shader
- [ ] Alpha-tested material pass
- [ ] Transparent material pass
- [ ] Bilateral SSAO blur
- [ ] Temporal anti-aliasing pass
- [ ] Skybox pass
- [ ] Image-based lighting
- [ ] Screen-space reflections
- [ ] Volumetric fog pass
- [ ] Render graph for pass ordering and resource tracking
- [ ] Shader hot reload
- [ ] ImGui debug panel
- [ ] GPU timestamp profiler
- [ ] Asset manifest for scenes, meshes, materials, and textures
- [ ] Material cache
- [ ] Texture cache
- [ ] Async asset loading jobs
- [ ] Screenshot capture
- [ ] README screenshots

## Build And Run

### Requirements

- CMake 3.20 or newer
- C++20 compiler
- Vulkan SDK or Vulkan development packages
- `glslc`
- Development packages for `glm` and `glfw3`
- An installed static `meshoptimizer` library with its CMake package configuration
- `spdlog` sources in `external/spdlog` (built with `add_subdirectory`)
- Vulkan-capable GPU and driver
- Local `external/` assets and third-party folders (ignored by git)

### External Folder Notes

This repo now treats `external/` as local-only. Populate it with the dependencies/assets required by `CMakeLists.txt`:

- `external/tracy`
- `external/tinygltf`
- `external/spdlog`
- `external/gltf-sample-assets` (Sponza is loaded from this tree)

The build uses these existing local dependencies without modifying their source trees. `foundation/` has been removed; Camera is part of `renderer_core`.

### Configure And Build

Install [meshoptimizer](https://github.com/zeux/meshoptimizer) separately as a static library
if it is not already available. From a meshoptimizer source checkout:

```bash
cmake -S . -B build-static -DCMAKE_BUILD_TYPE=Release \
    -DMESHOPT_BUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX=/path/to/meshoptimizer-install
cmake --build build-static --config Release -j
cmake --install build-static --config Release
```

The renderer uses `find_package(meshoptimizer CONFIG REQUIRED)` and links
`meshoptimizer::meshoptimizer`. For an installation outside CMake's default search
paths, add `-DCMAKE_PREFIX_PATH=/path/to/meshoptimizer-install` to the configure
command below. If an existing build cached another installation, set
`-Dmeshoptimizer_DIR=/path/to/meshoptimizer-install/lib/cmake/meshoptimizer`
(adjust `lib` to your installation's library directory). Shared-only installations
are rejected.

From the renderer repository:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
```

The build creates `vkml` and compiles shaders into `build/shaders/*.spv`.

### Run

The executable can be launched from any working directory. For example, from the repository root:

```bash
./build/vkml
```

Shaders are resolved from the configured build directory via `VKML_BINARY_DIR`, and assets are resolved from the source tree via `VKML_SOURCE_DIR`. The working directory does not affect loading; keep the build tree and `external/` populated.

### Release Build

```bash
cmake -S . -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j
./build-release/vkml
```

### Useful Targets

```bash
cmake --build build --target shaders
cmake --build build --target vkml
cmake --build build --target clean
```

### Validation

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/vkml --frames 8
```

`renderer_resource_lifecycle` exercises real Vulkan buffer writes, image and target allocations, shader and compute pipeline creation, command allocation, and explicit cleanup without opening a window. It skips when no Vulkan device is available.

`renderer_contracts` runs without a display or GPU. It checks explicit resource lifetime violations, empty-resource cleanup, image moves, pipeline keys, FIFO preparation, worker exceptions, and Vulkan camera projection. `--frames` bounds a rendering smoke test and requires the normal scene assets, a display, the Khronos validation layer, and a Vulkan 1.3 GPU supporting the required dynamic-state extensions and descriptor indexing.
