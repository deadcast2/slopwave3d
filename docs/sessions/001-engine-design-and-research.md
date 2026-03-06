# Session 001: Engine Design & Shockwave 3D Research

**Date:** March 5, 2026
**Participants:** Caleb Cohoon + Claude (Opus 4.6)
**Duration:** Initial planning session
**Repo:** [deadcast2/slopwave3d](https://github.com/deadcast2/slopwave3d)

---

## Session Goal

Design a compact 3D game engine inspired by early 2000s Macromedia Shockwave 3D games. The engine must be small enough to fit entirely in an AI context window, enabling AI-assisted development across the entire codebase simultaneously.

---

## Conversation Summary

### 1. Language & Platform Selection

We started by evaluating which language would produce the most compact engine. Options discussed:

- **JavaScript + raw WebGL** — most compact, browser-native, thematically authentic
- **C + sokol or raylib** — native performance, minimal boilerplate
- **Software rasterizer in C** — no GPU API, pure CPU rendering like actual Shockwave
- **Zig** — modern C alternative, compact syntax

**Decision: Software rasterizer in C.** No GPU APIs at all — pure CPU rendering. This is the most authentic approach to Shockwave 3D's software renderer fallback path, and keeps the engine self-contained without GPU API dependencies.

### 2. Resolution & Texture Constraints

We deliberately handicapped the engine for authenticity:

| Constraint | Value | Rationale |
|-----------|-------|-----------|
| Resolution | **320x240** | Classic quarter-VGA, very Shockwave-era |
| Max texture size | **128x128** | Low but recognizable, good Shockwave feel |
| Texture format | **JPG only** | Matches Shockwave's compressed web delivery |

### 3. Architecture Decision

Three architectures were considered:

- **C→WASM + JS API** — Engine in C compiled to WASM, games scripted in JS
- **Pure C + SDL2** — Everything native, no browser
- **All C → WASM** — Engine and games in C, compiled to WASM

**Decision: C→WASM + JS API.** This directly mirrors the Shockwave architecture — a compiled engine (Shockwave used Intel's IFX Toolkit) with a scripting layer on top (Shockwave used Lingo). In slop3d, C is the compiled engine and JavaScript is the scripting language.

### 4. Shockwave 3D Research

Three parallel research agents investigated Shockwave 3D's technical specifications, visual characteristics of actual games, and appropriate software rasterization techniques.

*(Full research findings below in the Research section)*

### 5. Engine Design

Based on research, we mapped authentic Shockwave 3D characteristics to slop3d design decisions:

| Shockwave 3D Feature | slop3d Implementation |
|---|---|
| Gouraud shading only | Per-vertex lighting, colors interpolated across triangles |
| Affine texture mapping (SW renderer) | No perspective correction — intentional warping |
| 256x256 typical textures | 128x128 max (even crunchier) |
| ~10k polygon ceiling | Same low-poly target |
| 50% JPEG compression default | JPG-only textures |
| Linear fog | Simple linear fog (start/end distances) |
| Small browser viewport | 320x240 framebuffer |
| Bilinear or nearest-neighbor | Nearest-neighbor (more retro) |
| 16-bit Z-buffer common | 16-bit Z-buffer (authentic z-fighting!) |

### 6. GitHub Issues

The implementation was broken into 8 sequential GitHub issues, each a self-contained milestone with task checklists and acceptance criteria:

1. **Project Skeleton — Framebuffer to Browser** (core, infrastructure)
2. **Vector/Matrix Math + Camera System** (core, math)
3. **Triangle Rasterizer + Z-Buffer** (core, rendering)
4. **Meshes, Textures + OBJ Loading** (core, assets)
5. **Scene Objects + Transforms** (core, scene)
6. **Gouraud Lighting System** (core, rendering)
7. **Fog, Alpha Blending + Input** (core, effects)
8. **JS API Polish + Demo Scene** (api, demo)

---

## Research Findings

### Shockwave 3D Technical Specifications

#### Rendering Engine
- Based on **Intel's IFX Toolkit** (Web 3D Engine), developed in partnership between Intel and Macromedia
- Supported both **hardware-accelerated rendering** (OpenGL and Direct3D) and a **software rasterizer fallback**
- Software renderer allowed content to run on systems without hardware 3D acceleration
- Released July 2000 (announcement), deployed in Director 8.5 (April 2001)
- Over 200 million installed players by 2001
- Discontinued April 9, 2019

#### Shading & Lighting
- **Gouraud shading only** — no per-pixel Phong shading
- Lighting calculated at vertices and interpolated across triangle surfaces
- Four light types: **ambient** (general illumination), **directional** (distance-independent), **point** (omnidirectional from a location), **spotlight** (focused beams)
- Per-vertex diffuse and specular color values
- Prelit vertices with lightmap support
- **Toon/cel shading** support for non-photorealistic rendering
- Characteristic artifacts: Mach banding from color interpolation, missing specular highlights on small polygon faces

#### Texture System
- Recommended resolutions: **256x256 or 512x512** pixels (powers of 2)
- Supported dimensions: 8, 16, 32, 64, 128, 256, 512 pixels
- Texture channels: diffuse, specular, ambient, luminous/self-illumination
- **Bilinear filtering** for texture expansion
- **Mipmapping** with trilinear filtering support
- Texture tiling via extended UV coordinates (0.0 to 1.0 standard mapping)
- Formats: RGBA8880 (24-bit) and others
- **Default 50% compression quality** — heavily lossy for narrow-bandwidth web streaming
- Intel's compression library used (could be very slow on moderate-sized models)

#### Polygon Counts
- Highly compressed, low polygon count meshes for web delivery
- Typical range: **~860 to ~3,400 polygons** per model
- **~10,000 polygons** as a practical performance ceiling
- Adaptive LOD (Level of Detail) via multiresolution mesh (MRM) technology
- LOD modifier could dynamically adjust polygon detail based on distance/hardware

#### W3D File Format
- **Chunk-based binary format** with chunk headers containing type and size
- MSB flag in chunk size indicating sub-chunks; lower 31 bits store actual size
- Contents: polygon mesh data, vertex normals, UV coordinates, mesh bounding boxes, animation keyframes (standard and compressed), bone hierarchies, material definitions, surface types (32 types: metal, concrete, grass, etc.), collision geometry, particle emitters, vertex colors
- Export from 3DS Max, Maya, and other major 3D tools
- Geometry compression via Intel's compression library (lossy, adjustable quality)

#### Additional Features
- **Alpha blending** for semi-transparent surfaces
- **Cubic environment mapping** (skyboxes) using 6 images at 256x256 or 512x512
- **Particle physics system** for fog, smoke, fire, water, dust, sparks, explosions
- **Ray-casting collision detection** via `modelsUnderRay`
- **Havok physics engine integration** (2001)
- Camera FOV adjustable (typical: 60, 75, 90 degrees)
- Shockwave Multiuser Server 3 supporting up to 2,000 simultaneous users
- Optimized for **56k modem delivery** — achieved 30fps over dial-up

#### Notable Shockwave 3D Games
- **LEGO Studios Backlot** (2002) — 3D adventure/platformer in a LEGO movie studio, third-person with Havok Physics
- **LEGO Supersonic RC** (2003-2004) — RC car racing through toy store environments
- **Spybot: The Nightfall Incident** — turn-based strategy
- **Oysterhead Maze** (2001) — maze navigation game
- **Wonderville 3D** (2004) — educational hoverboard exploration game
- **3D Groove Games** — advergames including Real Pool and Tank Wars
- **Hot Rods** (2006), **Arctic 3D Racer**, **Baja**, **Snowfight 3D**

#### Visual Characteristics of Shockwave 3D Games
- Angular, blocky low-poly geometry
- Simple/minimal texturing (bandwidth constraints)
- Toon shading gave a cartoony, hand-drawn appearance
- Small embedded browser windows (not full-screen)
- Sat visually between 2D Flash animation and full 3D console graphics
- Hardware acceleration was rare for browser content, making it stand out

### Software Rasterization Techniques (Era-Appropriate)

#### Rasterization Algorithms
- **Scanline rasterization** was the standard for software renderers of the era — uses active edge tables, sorted by X coordinate with gradients
- **Half-space rasterization** is more modern and benefits from parallelism (used by GPUs) but was less common in early 2000s software implementations
- For authenticity, scanline is the right choice

#### Texture Mapping
- **Affine texture mapping** was standard in early 2000s software renderers — creates characteristic warping/distortion
- The **PlayStation 1** extensively used affine mapping, breaking triangles into smaller segments
- **Perspective-correct mapping** requires per-vertex 1/w calculations and per-pixel division — more expensive
- Optimization: interpolate u/z, v/z, and 1/z, recalculate every 8-16 pixels (Michael Abrash's technique)
- **For Shockwave authenticity: affine mapping is correct** — the warping is the look

#### Z-Buffer
- **16-bit Z-buffer** was standard but produces z-fighting on co-planar surfaces
- Precision is non-uniform — closer values are much more precise than distant values
- As zNear approaches 0, precision degrades dramatically
- Recommended clipping: zNear ~0.1-1.0, zFar ~1000-10000

#### Texture Filtering
- **Nearest-neighbor**: simplest, uses closest texel directly, creates blockiness and aliasing
- **Bilinear filtering**: samples 4 nearest texels and interpolates, smoother but more expensive
- Shockwave 3D supported both; nearest-neighbor gives stronger retro aesthetic

#### Vertex Lighting vs Per-Pixel
- **Gouraud shading** (vertex lighting): compute at vertices, interpolate colors — THE standard before pixel shaders (2000-2004)
- DirectX 8 (November 2000) introduced Pixel Shader 1.0
- ATI Radeon 9700 (August 2002) was first DirectX 9 card
- Doom 3 (2004) demonstrated real-time per-pixel lighting on consumer hardware
- **Shockwave 3D documentation confirms Gouraud shading only**

#### Transparency
- Standard alpha blending: `Result = Source * SourceAlpha + Destination * (1 - SourceAlpha)`
- Premultiplied alpha could encode both regular and additive blending

#### Fog
- **Linear fog**: interpolates between fog start and fog end distances
- Formula: `fogFactor = (zFar - Z) / (zFar - zStart)`
- Exponential fog (GL_EXP, GL_EXP2) also existed but linear is simpler and sufficient

---

## Final Engine Specifications

```
Resolution:      320x240 RGBA8888 framebuffer
Z-buffer:        16-bit (uint16_t)
Textures:        128x128 max, JPG only, nearest-neighbor, 64 slots
Shading:         Gouraud (per-vertex, interpolated)
Texture mapping: Affine (no perspective correction)
Lights:          8 slots — ambient, directional, point, spot
Fog:             Linear (start/end distance)
Transparency:    Alpha blending, back-to-front object sort
Meshes:          128 slots, OBJ loading
Objects:         256 slots with independent transforms
Architecture:    C core → WASM (Emscripten) → JS API
Memory:          16MB fixed WASM heap
Target size:     ~2,740 lines / ~20K tokens
```

---

## File Structure

```
slopwave3d/
  src/
    slop3d.c          # Entire C engine core (~2,000 lines)
    slop3d.h          # Public API header (~150 lines)
  js/
    slop3d.js         # JS API wrapper + canvas blitting (~400 lines)
  web/
    index.html        # Demo harness (~50 lines)
    demo.js           # Example game script (~150 lines)
  docs/
    sessions/         # Development session logs
  Makefile            # Emscripten build (~30 lines)
```

---

## Research Sources

### Shockwave 3D Technical Documentation
- [Intel And Macromedia Team Up To Popularize 3D Technology On The Web](https://www.intel.com/pressroom/archive/releases/2000/in072500.htm)
- [Macromedia And Intel Bring Web 3D To The Mainstream (2001)](https://www.intel.com/pressroom/archive/releases/2001/20010410tech.htm)
- [Director Shockwave 3D Casts Definitions](http://www.deansdirectortutorials.com/3D/3DCastMembers.htm)
- [Director Cubic VR - Skybox in Shockwave 3D](http://www.deansdirectortutorials.com/3D/CubicVR/)
- [Shockwave-3D 3D File Export Converter - Okino](https://www.okino.com/conv/exp_sw3d.htm)
- [W3D File Format Documentation — OpenSAGE](https://opensage.readthedocs.io/file-formats/w3d/)
- [Shockwave 3D Tools 4 U - Director Online](https://director-online.dasdeck.com/buildArticle.php?id=1018)
- [Macromedia Director 8.5 Enters the Third Dimension | CreativePro](https://creativepro.com/macromedia-director-8-5-enters-the-third-dimension/)
- [Building the Model | Architecture Fly-Through in Shockwave 3D | Peachpit](https://www.peachpit.com/articles/article.aspx?p=30607&seqNum=3)
- [How Shockwave 3-D Technology Works | HowStuffWorks](https://computer.howstuffworks.com/shockwave.htm)

### Shockwave 3D History & Games
- [The Story Of Shockwave And 3D Webgames | BlueMaxima's Flashpoint | Medium](https://medium.com/bluemaximas-flashpoint/the-story-of-shockwave-and-3d-webgames-8f3647865a7)
- [Adobe Shockwave - Wikipedia](https://en.wikipedia.org/wiki/Adobe_Shockwave)
- [LEGO Studios Backlot - Brickipedia](https://brickipedia.fandom.com/wiki/Backlot)
- [LEGO Studios Backlot - The Cutting Room Floor](https://tcrf.net/LEGO_Studios_Backlot)
- [Oysterhead Maze - Lost Media Wiki](https://lostmediawiki.com/Oysterhead_Maze_(found_3D_Shockwave_game;_2001))
- [Wonderville 3D - Lost Media Wiki](https://lostmediawiki.com/Wonderville_3D_(lost_Shockwave_3D_educational_game;_2004))
- [3D Groove Games - Lost Media Wiki](https://lostmediawiki.com/3D_Groove_Games_(partially_lost_online_games;_1998-2009))
- [Adobe Shockwave vs Flash Comparison](https://alex-dev.org/adobe-shockwave-vs-flash-a-comparative-analysis-of-multimedia-platforms/)

### Software Rasterization & Rendering Techniques
- [Texture mapping - Wikipedia](https://en.wikipedia.org/wiki/Texture_mapping)
- [Chris Hecker's Texture Mapping Articles](https://www.gamers.org/dEngine/quake/papers/checker_texmap.html)
- [Software 3D Perspective Correct Textured Triangles - GitHub](https://github.com/Haggarman/Software-3D-Perspective-Correct-Textured-Triangles)
- [Scanline rendering - Wikipedia](https://en.wikipedia.org/wiki/Scanline_rendering)
- [Optimizing the basic rasterizer | The ryg blog](https://fgiesen.wordpress.com/2013/02/10/optimizing-the-basic-rasterizer/)
- [Depth Buffer Precision - OpenGL Wiki](https://www.khronos.org/opengl/wiki/Depth_Buffer_Precision)
- [Maximizing Depth Buffer Range and Precision - Outerra](https://outerra.blogspot.com/2012/11/maximizing-depth-buffer-range-and.html)
- [Z-buffering - Wikipedia](https://en.wikipedia.org/wiki/Z-buffering)
- [Depth Buffers (Direct3D 9) - Microsoft](https://learn.microsoft.com/en-us/windows/win32/direct3d9/depth-buffers)
- [Alpha compositing - Wikipedia](https://en.wikipedia.org/wiki/Alpha_compositing)
- [Alpha Blending - Khronos OpenGL Wiki](https://www.khronos.org/opengl/wiki/Alpha_Blending)
- [Fog Formulas (Direct3D 9) - Microsoft](https://learn.microsoft.com/en-us/windows/win32/direct3d9/fog-formulas)
- [Implementing Fog in Direct3D - NVIDIA](https://developer.download.nvidia.com/assets/gamedev/docs/Fog2.pdf)
- [Texture filtering - Wikipedia](https://en.wikipedia.org/wiki/Texture_filtering)
- [Gouraud shading - Wikipedia](https://en.wikipedia.org/wiki/Gouraud_shading)
- [Phong shading - Wikipedia](https://en.wikipedia.org/wiki/Phong_shading)
- [Z-fighting - Wikipedia](https://en.wikipedia.org/wiki/Z-fighting)
- [Cel shading - Wikipedia](https://en.wikipedia.org/wiki/Cel_shading)

### Reference Software Rasterizer Implementations
- [swrast - Minimal C89 software rasterizer (public domain)](https://github.com/AgentD/swrast)
- [softrast - Small C rasterizer](https://github.com/petermcevoy/softrast)
- [mtrebi/Rasterizer - C++ CPU rasterizer](https://github.com/mtrebi/Rasterizer)
- [Developing a Software Renderer - Trenki's Dev Blog](https://trenki2.github.io/blog/2017/06/06/developing-a-software-renderer-part1/)

---

## Decisions Log

| Decision | Choice | Reasoning |
|----------|--------|-----------|
| Language | C | Most compact for a software rasterizer, compiles to WASM |
| Rendering | Software rasterizer (no GPU) | Authentic to Shockwave's SW renderer path |
| Distribution | C→WASM→Browser via JS API | Mirrors Shockwave's compiled engine + scripting architecture |
| Resolution | 320x240 | Quarter-VGA, authentic to the era |
| Max texture | 128x128 | Low but recognizable, crunchier than Shockwave's typical 256x256 |
| Texture format | JPG only | Matches Shockwave's compressed web delivery |
| Shading | Gouraud only | Confirmed as Shockwave 3D's only shading model |
| Texture mapping | Affine (no perspective correction) | Authentic to SW3D's software renderer, creates signature warping |
| Z-buffer | 16-bit | Period-authentic, produces characteristic z-fighting |
| Texture filtering | Nearest-neighbor | Stronger retro aesthetic than bilinear |
| Fog | Linear | Simplest, appropriate for the era |
| Code prefix | `s3d_` / `S3D_` | Short for "slop3d" — repo is "slopwave3d" but code stays compact |
| Architecture | Single global engine instance | No handles, no ref counting — appropriate for tiny retro engine |
| Memory | Fixed arrays, no runtime malloc | Predictable, cache-friendly, eliminates memory bugs |

---

## Next Steps

Begin implementation starting with [Issue #1: Project Skeleton](https://github.com/deadcast2/slopwave3d/issues/1) — get the C→WASM→Canvas pipeline working with a solid colored rectangle on screen.
