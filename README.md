# AF Framework

Reusable C17 libraries for 2D scenes, UI, SDL3 windows, and Vulkan 1.3 rendering.
Application logic and file formats stay in the host project.

## Build

Requires CMake 3.25+, Ninja, FreeType, and HarfBuzz. Window/GPU builds also need
SDL3, Vulkan headers, volk source, `glslc`, and `spirv-val`.

```sh
git clone https://github.com/stmSi/af-framework.git
cd af-framework
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/af_framework_demo --frames 4 --output demo.png
```

Use `-DAF_FRAMEWORK_DEPENDENCY_ROOT=/path/to/third_party` for vendored dependencies.
For engine/UI only, configure with both `AF_FRAMEWORK_BUILD_PLATFORM=OFF` and
`AF_FRAMEWORK_BUILD_VULKAN=OFF` (pass each with `-D`).

## Use

```cmake
add_subdirectory(af-framework)
target_link_libraries(my_app PRIVATE af::framework)
```

| Target | Provides |
| --- | --- |
| `af::foundation` | Memory, files, logging, math |
| `af::engine` | Scenes, paths, text, animation, particles |
| `af::ui` | Drawing, inputs, actions, shortcuts |
| `af::platform` | SDL3 windows and input |
| `af::vulkan` | Vulkan rendering and capture |
| `af::canvas` | Browser Canvas rendering (Emscripten) |
| `af::framework` | All enabled layers |

Include `af/engine.h`, `af/ui.h`, or `af/vulkan.h`. The caller owns UI buffers
and callbacks; check `AfUiContext.overflowed`. Action context bit 0 is global.

To install: `cmake --install build --prefix /path/to/sdk`. Consumers use
`find_package(AFFramework CONFIG REQUIRED)` with `CMAKE_PREFIX_PATH` set to the SDK.
FreeType, HarfBuzz, and SDL3 remain external dependencies.

Deploy compiled `shaders/` beside the executable, or pass a shader directory to
`af_vk_renderer_create_with_info`. Installed SDKs expose `AFFramework_SHADER_DIRECTORY`.

Browser builds use Emscripten: `emcmake cmake -S . -B build/web`.
Provide vendored dependencies with `AF_FRAMEWORK_DEPENDENCY_ROOT`; link `af::canvas`
and include `af/canvas.h`. Vulkan defaults to off on this target. CPU billboard
particles are supported; GPU-specific VFX require the Vulkan backend.

Gradient tracks animate geometry, stop colors, and offsets. Elements 0–15 select
fill stops; 16–31 select outline stops. Geometry uses element 0 or 16.
