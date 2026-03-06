# CLAUDE.md — slop3d Engine Guide

## What Is This?

slop3d (repo: slopwave3d) is a compact software rasterizer 3D game engine that authentically mimics early 2000s Macromedia Shockwave 3D aesthetics. The entire engine is designed to fit in a single AI context window.

## Architecture

- **C core** (`src/slop3d.c`) — software rasterizer, no GPU APIs
- **WASM bridge** — compiled via Emscripten, exports C functions to JS
- **JS API** (`js/slop3d.js`) — `Slop3D` class wrapping WASM calls, canvas blitting, asset loading
- **Browser delivery** — `web/index.html` hosts the engine, games are scripted in JS

This mirrors Shockwave 3D's own architecture: compiled engine + scripting layer.

## Engine Constraints (Intentional)

These are deliberate design choices for authenticity, not limitations to fix:

| Constraint | Value | Why |
|-----------|-------|-----|
| Resolution | 320x240 | Quarter-VGA, Shockwave-era |
| Z-buffer | 16-bit | Authentic z-fighting artifacts |
| Max texture | 128x128, JPG only | Crunchy, web-compressed look |
| Texture filtering | Nearest-neighbor | Pixelated retro aesthetic |
| Texture mapping | Affine (NO perspective correction) | Signature Shockwave warping |
| Shading | Gouraud only (per-vertex) | Shockwave 3D's only shading model |
| Poly ceiling | ~10k per scene | Low-poly aesthetic |

**Do not "fix" or "improve" these.** The affine warping, z-fighting, and pixelation ARE the look.

## Code Conventions

- Code prefix: `s3d_` for functions, `S3D_` for types/structs
- Single global engine instance (`g_engine`), no dynamic allocation at runtime
- Fixed-size arrays: 64 textures, 128 meshes, 256 objects, 8 lights
- Column-major matrices (OpenGL convention): `m[col*4 + row]`
- CW winding = front face (backface culling always on)
- All math functions are `static inline` in `slop3d.c`

## File Structure

```
src/slop3d.c      — Entire C engine (~2000 lines target)
src/slop3d.h      — Public API header
js/slop3d.js      — JS API wrapper + canvas blitting
web/index.html    — Demo shell
web/demo.js       — Example game
Makefile          — Emscripten build (single emcc invocation)
docs/sessions/    — Development session logs
```

## Build

```bash
make              # requires Emscripten (emcc) in PATH
```

Outputs `web/slop3d_wasm.js` and `web/slop3d_wasm.wasm`.

## Key Technical Details

### Rendering Pipeline
1. Compute camera view/projection matrices
2. Sort: opaque objects first, then transparent back-to-front
3. Per object: transform vertices to clip space, compute Gouraud lighting in world space
4. Per triangle: backface cull → near-plane clip (Sutherland-Hodgman) → perspective divide → viewport transform
5. Scanline rasterize with affine interpolation of z, u, v, r, g, b
6. Per pixel: 16-bit z-test → nearest-neighbor texture sample → modulate by vertex color → fog blend → alpha blend → write

### WASM Memory
- 16MB fixed heap (`ALLOW_MEMORY_GROWTH=0`)
- Framebuffer: JS reads directly from WASM linear memory via `s3d_get_framebuffer()` pointer
- Textures/meshes uploaded by copying data into WASM heap from JS

### JS↔WASM Bridge
- Textures: JS decodes JPG via `Image()` → draws to ≤128x128 canvas → `getImageData` → copies RGBA to WASM heap
- OBJ files: JS fetches text → copies string to WASM heap → C parses
- Display: JS reads framebuffer from WASM memory → `ImageData` → `putImageData` → `drawImage` scaled with `image-rendering: pixelated`

## Session Logs

Development sessions are documented in `docs/sessions/`. Use `/project:session-log` to generate a new one after a work session.

## GitHub Issues

Implementation is tracked via sequential GitHub issues #1-#8. Complete them in order.
