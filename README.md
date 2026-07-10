# RedHill
Renderer to practise graphics programming and learn new techniques while serving as portfolio piece.
Red Hill is a pbr deferred renderer. It has a three parts g-buffer with albedo, normal and material (metallic, roughness and ao) and a separate light pass that implements a Cook-Torrance brdf and IBL following the model showed by Unreal Engine in Real Shading in Unreal Engine 4 by Brian Karis. It also implements normal mapping using the third party library Mikktspace.

The renderer has 2 modes that can be cycled by pressing space: a sphere grid whith various values of metallic and roughness and a model renderer that loads and draws the damaged helmet model with its textures. Also the background enviroment can be swapped by pressing ctrl.

## Controls

- **Space** — cycle render mode (sphere grid / model)
- **Ctrl** — swap the background environment
- **Mouse** — camera movement and zoom

## Building

RedHill targets Direct3D 12 on Windows and builds with Visual Studio + MSBuild.

**Requirements**
- Windows 10 or 11, x64
- Visual Studio 2022 **17.14 or newer** with the *Desktop development with C++* workload (the project uses the `v145` platform toolset and C++20)
- A recent Windows 10/11 SDK
- NuGet package restore enabled (the default; needed for the D3D12 Agility SDK and DXC — see below)

**Dependencies**

The [D3D12 Agility SDK](https://www.nuget.org/packages/Microsoft.Direct3D.D3D12) and [DirectX Shader Compiler](https://www.nuget.org/packages/Microsoft.Direct3D.DXC) are referenced through `RedHill/packages.config` and restored by NuGet on first build, so the `packages/` folder is **not** committed. If a build fails complaining about missing package files, restore them manually:

```
nuget restore RedHill.sln
```
or, from a Developer command prompt: `msbuild RedHill.sln -t:restore`.

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

- One of the most important features in a renderer are shadows and this renderer lacks shadows completely. An implementation of shadow maps should be done.

- The need of an antialiasing technique is self evident by the images produced so TAA would be a good addition. Besides it will serve as an introduction to temporal techniques.

- I want to start building up from there adding different scenarios to end up implementing ReSTIR.

## Pending improvements

- CreateDefaultResource raises a warning when executed over a buffer as they ignore the provided state and are created always in common.
- Some initialization only resources like upload heaps or init only descriptor heaps survive during the entire lifetime of the renderer. Add a to free list to manage them.
- In the environment creation handling of the heaps is not really optimal. Each bake step set ups its own heap because of the compute mip map generation between them. Not critical as it is part of initialization but could be improved.
- I am using only a constant buffer in the shader. That works because we are only drawing one object if we wanted to support a full scene we would need to separate the scene constants and the object constants in different constant buffers.
