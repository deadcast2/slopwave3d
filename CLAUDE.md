# CLAUDE.md — slop3d Engine Guide

## What Is This?

slop3d (repo: slopwave3d) is a compact software rasterizer 3D game engine that authentically mimics early 2000s Macromedia Shockwave 3D aesthetics. The entire engine is designed to fit in a single AI context window.

## Architecture

- **C core** (`src/slop3d.c`) — software rasterizer, no GPU APIs
- **WASM bridge** — compiled via Emscripten, exports C functions to JS
- **JS API** (`js/slop3d.js`) — `Slop3D` class wrapping WASM calls, canvas blitting, asset loading
- **SlopScript** (`js/slop3d.js`) — custom DSL transpiler, runtime helpers, and browser loader (appended to same file)
- **Browser delivery** — `web/index.html` hosts the engine, games are scripted in SlopScript or JS

This mirrors Shockwave 3D's own architecture: compiled engine + scripting layer (Lingo → SlopScript).

## Engine Constraints (Intentional)

These are deliberate design choices for authenticity, not limitations to fix:

| Constraint | Value | Why |
|-----------|-------|-----|
| Resolution | 320x240 | Quarter-VGA, Shockwave-era |
| Z-buffer | 16-bit | Authentic z-fighting artifacts |
| Max texture | 128x128, JPG only | Crunchy, web-compressed look |
| Texture filtering | Nearest-neighbor | Pixelated retro aesthetic |
| Texture mapping | Affine (NO perspective correction) | Era-authentic software rasterizer warping |
| Shading | Gouraud only (per-vertex) | Shockwave 3D's standard shading model |
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
src/slop3d.c          — Entire C engine (~2000 lines target)
src/slop3d.h          — Public API header
js/slop3d.js          — JS API wrapper + SlopScript transpiler + runtime
web/index.html        — Demo shell (SlopScript)
web/demo.slop         — Standalone demo in SlopScript
tests/test_math.c     — C engine unit tests
tests/test_slopscript.js — SlopScript lexer/parser/codegen tests
Makefile              — Emscripten build (single emcc invocation)
docs/sessions/        — Development session logs
```

## Build

### WASM (modern browsers)

```bash
make              # requires Emscripten (emcc) in PATH
make serve        # build + start local server on port 8080
make test         # run C + JS unit tests
make fmt          # auto-format C (clang-format) and JS (prettier)
make clean        # remove build outputs
```

Outputs `web/slop3d_wasm.js` and `web/slop3d_wasm.wasm`. Open `http://localhost:8080/web/index.html` to run.

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

## SlopScript

SlopScript is a custom DSL (`.slop` extension) that transpiles to JS targeting the Slop3D API. It parallels how Shockwave 3D had Lingo. The transpiler runs in the browser with no build step.

### Syntax Overview

```
assets
    model cube = cube.obj
    skin crate = crate.jpg

scene main
    box = spawn: cube, crate
    box.position = 2.5, 0, 0
    camera.position = 0, 1.5, 5

    update
        box.rotation.y = t * 30
```

### Key Syntax Rules
- **Indentation-based** (4 spaces), no semicolons or braces
- **No parentheses anywhere** — `:` for statement calls (`spawn: cube`), `[]` for expression calls (`sin[t]`)
- **No `let`** — first assignment declares a variable
- **No quotes** on asset paths
- **Bare tuples** for vectors: `box.position = 2.5, 0, 0`
- **`[]` dual purpose** — `sin[t]` (call when preceded by ident), `[a + b] * c` (grouping when standalone)
- **Implicit render** — engine renders automatically after every update tick

### Built-ins
- `t` (elapsed seconds), `dt` (delta time)
- `camera` singleton with `.position`, `.target`, `.fov`, `.near`, `.far`
- Math (degree-based): `sin[]`, `cos[]`, `tan[]`, `lerp[]`, `clamp[]`, `random[]`, `abs[]`, `min[]`, `max[]`
- Control flow: `if`/`elif`/`else`, `while`, `for x in range[n]`, `fn name: args`, `return`
- Boolean: `and`, `or`, `not`, `true`, `false`

### Structure
- `assets` — global block, loaded once before any scene
- `scene name` — named scene with setup code + nested `update` block
- `goto: scenename` — switches scenes, auto-destroys current scene's objects
- First scene declared is active by default

### Transpiler Pipeline
Source → Lexer (indent-aware) → Parser (recursive descent) → AST → CodeGen → JS string → `new Function()`

### Browser Integration
- Inline: `<script type="text/slopscript">` with `data-canvas` attribute
- External: `<script type="text/slopscript" src="demo.slop">`
- Programmatic: `SlopScript.run(source, canvasId)` or `SlopScript.load(url, canvasId)`

## Session Logs

Development sessions are documented in `docs/sessions/`. Use `/project:session-log` to generate a new one after a work session.

## GitHub Issues

Implementation is tracked via GitHub issues. Engine core: #1-#8. SlopScript: #13-#18.
