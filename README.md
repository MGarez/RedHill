# RedHill

Red Hill is a realtime deferred PBR renderer written from scratch using Direct3D 12.

![Demo Image](https://private-user-images.githubusercontent.com/64743019/620184486-d05be097-f301-4330-8f33-1e2154490118.jpg?jwt=eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJpc3MiOiJnaXRodWIuY29tIiwiYXVkIjoicmF3LmdpdGh1YnVzZXJjb250ZW50LmNvbSIsImtleSI6ImtleTUiLCJleHAiOjE3ODM3MDk0NTgsIm5iZiI6MTc4MzcwOTE1OCwicGF0aCI6Ii82NDc0MzAxOS82MjAxODQ0ODYtZDA1YmUwOTctZjMwMS00MzMwLThmMzMtMWUyMTU0NDkwMTE4LmpwZz9YLUFtei1BbGdvcml0aG09QVdTNC1ITUFDLVNIQTI1NiZYLUFtei1DcmVkZW50aWFsPUFLSUFWQ09EWUxTQTUzUFFLNFpBJTJGMjAyNjA3MTAlMkZ1cy1lYXN0LTElMkZzMyUyRmF3czRfcmVxdWVzdCZYLUFtei1EYXRlPTIwMjYwNzEwVDE4NDU1OFomWC1BbXotRXhwaXJlcz0zMDAmWC1BbXotU2lnbmF0dXJlPWQyZjUzMWUzN2Q3ZDVjOTUyMjNmYWRhMjU4OWNiYWU2Y2VjYmQyMzczZWY4MzcyZDlkODBjOGFiMDQ5MTY3NWEmWC1BbXotU2lnbmVkSGVhZGVycz1ob3N0JnJlc3BvbnNlLWNvbnRlbnQtdHlwZT1pbWFnZSUyRmpwZWcifQ.mwjQ_QToK2u0bpiXBmCKpRfa2ZxYAQgCMcl6-fjbt4o)

## Demo

https://private-user-images.githubusercontent.com/64743019/620187314-e63bfc7c-cfc6-4699-a513-af76525ea19f.mp4

## Features

- Deferred pipeline with a 3-parts G-buffer (albedo / normal / metallic-roughness-AO)
- Cook-Torrance BRDF + IBL via the split-sum approximation (UE4 based pipeline)
- Modern D3D12 from scratch: explicit resource barriers, descriptor heaps, PSOs, HDR cubemap pipeline
- Reverse-Z depth for precision
- Shadow mapping
- Tangent-space normal mapping with MikkTSpace
- Mipmap generation using compute shaders.

## How it works

Red Hill is a deferred renderer with 4 rendering passes in the render loop:

- Shadow pass: pass generating the shadow map (as currently there is only one directional light).
This pass outputs an R32_FLOAT texture that will be used by the light pass.
- Geometry pass: generates the geometry and its information needed to be drawn. Currently it outputs three
render targets that will be used by the light pass: albedo (in a R8G8B8A8_UNORM texture), the normal (in a
R16G16B16A16_FLOAT texture for precision and to handle negative values) and a material pass (in a R8G8B8A8_UNORM
texture where the metallic value is in the red channel, the roughness in the green one and the ambient occlusion
in the blue one). Here I considered two main improvements that were dropped to mantain the scope: a fourth render
target with emissive information to support emissive materials and implementinc octahedral encoding on the normal render target in order to reduce memory bandwidth.
- Light pass: shades the objects in the scene implementing a PBR pipeline. It uses Cook-Torrance BRDF and implements IBL with the split-sum approximation. It also applies the shadow map. In order to mantain the original time scope of the project Red Hill currently only has one light so the benefit of building a deferred pipeline is not fully exploited. Extending the amount and types of lights is one of the future improvements planned.
- Skybox pass: draws the skybox.

In order to make all this possible we need to do an important amount of work during the initialization. This could be cached from previous executions to avoid having the costly initialization every time. During this initialization the renderer builds the needed Direct3D structures (device, command list, descriptor heaps, resources) and initializes the enviroment textures needed to draw the skybox and to compute a correct IBL. To do this there is an struct in Renderer.h that for each enviroment stores: the hdr equirect with the skybox texture, an enviroment cubemap with 11 different mips (generated during initialization using a compute shader), an irradiance cubemap and a prefilter cubemap with 5 different mip levels that are used depending on the roughness of the material. Also a shared BRDF lookup table with the scale and bias values needed for the split sum.

## Controls

The renderer has 2 modes that can be cycled by pressing space: a test sphere grid whith various values of metallic and roughness to test the correctness of the PBR implementation and a model renderer that loads and draws the damaged helmet model with its textures. Also the background enviroment can be swapped by pressing ctrl.

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

- The need of an antialiasing technique is self evident by the images produced so TAA would be a good addition. Besides it will serve as an introduction to temporal techniques.
- Adding a post-process pass and decoupling the color encode and gamma correct from the light and skybox passes.
- Increment the amount and complexity of lights in the scene.
- Decouple some logic from the Renderer class in order to make it easier to understand and to extend. Add a RenderPass class or something similar in order to abstract some of the logic and also
implement a better handling of the resource state transitions (ideally automatic).
- I want to start building up from there adding different scenarios to end up implementing ReSTIR.

## Pending improvements

- CreateDefaultResource raises a warning when executed over a buffer as they ignore the provided state and are created always in common.
- Some initialization only resources like upload heaps or init only descriptor heaps survive during the entire lifetime of the renderer. Add a to free list to manage them.
- In the environment creation, handling of the heaps is not really optimal. Each bake step set ups its own heap because of the compute mip map generation between them. Not critical as it is part of initialization but could be improved.
- I am using only a constant buffer in the shader. That works because we are only drawing one object if we wanted to support a full scene we would need to separate the scene constants and the object constants in different constant buffers.
