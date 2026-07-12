# RedHill

RedHill is a realtime deferred PBR renderer written from scratch using Direct3D 12.

![RedHill](https://github.com/user-attachments/assets/748106ed-0daf-4ded-b846-44ffaa70d4f4)

## Demo

https://private-user-images.githubusercontent.com/64743019/620187314-e63bfc7c-cfc6-4699-a513-af76525ea19f.mp4





## Features

- Deferred pipeline with a 3-part G-buffer (albedo / normal / metallic-roughness-AO)
- Cook-Torrance BRDF + IBL via the split-sum approximation (UE4-based pipeline)
- Modern D3D12 from scratch: explicit resource barriers, a persistent/transient descriptor-heap allocator, PSOs, HDR cubemap pipeline
- Reverse-Z depth for precision
- Shadow mapping
- Tangent-space normal mapping with MikkTSpace
- Mipmap generation using compute shaders
- HDR pipeline with ACES filmic tone mapping and gamma correction

## How it works

RedHill is a deferred renderer with 4 rendering passes in the render loop:

- Shadow pass: generates the shadow map (as currently there is only one directional light).
This pass outputs an R32_FLOAT texture that will be used by the light pass.
- Geometry pass: rasterizes the scene geometry into the G-buffer. Currently it outputs three
render targets that will be used by the light pass: albedo (in a R8G8B8A8_UNORM texture), the normal (in a
R16G16B16A16_FLOAT texture for precision and to handle negative values) and a material target (in a R8G8B8A8_UNORM
texture where the metallic value is in the red channel, the roughness in the green one and the ambient occlusion
in the blue one). Here I considered two main improvements that were dropped to maintain the scope: a fourth render
target with emissive information to support emissive materials and implementing octahedral encoding on the normal render target in order to reduce memory bandwidth.
- Light pass: shades the objects in the scene implementing a PBR pipeline. It uses Cook-Torrance BRDF and implements IBL with the split-sum approximation. It also applies the shadow map. In order to maintain the original time scope of the project, RedHill currently only has one light so the benefit of building a deferred pipeline is not fully exploited. Extending the amount and types of lights is one of the future improvements planned.
- Skybox pass: draws the skybox.

Making all this possible requires an important amount of work during initialization (this could be cached from previous executions to avoid paying the cost every time). Besides building the needed Direct3D structures (device, command list, descriptor heaps, resources), the renderer bakes the environment textures needed to draw the skybox and to compute a correct IBL. There is a struct in Renderer.h that for each environment stores the HDR equirect, the environment cubemap, an irradiance cubemap, a prefilter cubemap and a shared BRDF lookup table. These are produced by a chain of bake passes:

- Cubemap projection: projects the HDR equirect into the 6 faces of the environment cubemap (also used to draw the skybox).
- Mip generation: fills the cubemap's 11 mips using a box-filter compute downsample.
- Irradiance convolution: convolves the cubemap into a small irradiance cubemap for the diffuse IBL term.
- Prefilter: convolves the environment with the GGX lobe into a 5-mip cubemap, one roughness per mip. It samples the cubemap mips instead of the base level to fight fireflies, choosing a blurrier source mip when a sample covers a larger solid angle.
- BRDF LUT: integrates the split-sum scale and bias term into a shared lookup table.

## Controls

The renderer has 2 modes that can be cycled by pressing space: a test sphere grid with various values of metallic and roughness to test the correctness of the PBR implementation and a model renderer that loads and draws the damaged helmet model with its textures. Also the background environment can be swapped by pressing ctrl.

- **Space** — cycle render mode (sphere grid / model)
- **Ctrl** — swap the background environment
- **Mouse** — camera movement and zoom

## Building

RedHill targets Direct3D 12 on Windows and builds with Visual Studio + MSBuild.

**Requirements**
- Windows 10 or 11, x64
- Visual Studio 2022 **17.14 or newer** with the *Desktop development with C++* workload (the project uses the `v145` platform toolset and C++20)
- A recent Windows 10/11 SDK

## Assets & credits

Third-party assets live under `RedHill/resources/` and keep their own licenses — see
[`RedHill/resources/CREDITS.md`](RedHill/resources/CREDITS.md). In short: the DamagedHelmet
model is © ctxwing under CC BY 4.0, and the HDR environments are from Poly Haven under CC0.
The RedHill source code is covered by [LICENSE](LICENSE).

## Third-party libraries (`../thirdparty/`)

| Library | Author | License |
| --- | --- | --- |
| stb_image | Sean Barrett | Public domain / MIT |
| tiny_obj_loader | Syoyo Fujita | MIT |
| MikkTSpace | Morten S. Mikkelsen | zlib-style (see file header) |

## Future plans

- The need for an antialiasing technique is self-evident by the images produced so TAA would be a good addition. Besides, it will serve as an introduction to temporal techniques.
- Adding a post-process pass and decoupling the color encode and gamma correct from the light and skybox passes.
- Increment the amount and complexity of lights in the scene.
- Decouple some logic from the Renderer class in order to make it easier to understand and to extend. Add a RenderPass class or something similar in order to abstract some of the logic and also
implement a better handling of the resource state transitions (ideally automatic).
- I want to start building up from there adding different scenarios to end up implementing ReSTIR.

## Pending improvements

- CreateDefaultResource raises a warning when executed over a buffer, as buffers ignore the provided state and are always created in common.
- Some initialization-only resources like upload heaps or init-only descriptor heaps survive during the entire lifetime of the renderer. Add a free list to manage them.
- In the environment creation, handling of the heaps is not really optimal. Each bake step sets up its own heap because of the compute mip map generation between them. Not critical as it is part of initialization but could be improved.
- I am using only a constant buffer in the shader. That works because we are only drawing one object; if we wanted to support a full scene we would need to separate the scene constants and the object constants in different constant buffers.
