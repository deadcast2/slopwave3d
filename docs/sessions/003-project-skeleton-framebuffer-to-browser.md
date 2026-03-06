# Session 003: Project Skeleton — Framebuffer to Browser

**Date:** March 6, 2026
**Participants:** Caleb Cohoon + Claude (Opus 4.6)
**Duration:** Implementation session
**Repo:** [deadcast2/slopwave3d](https://github.com/deadcast2/slopwave3d)

---

## Session Goal

Implement [GitHub Issue #1: Project Skeleton — Framebuffer to Browser](https://github.com/deadcast2/slopwave3d/issues/1). Get the full C→WASM→JS→Canvas pipeline working — pixels rendered in C, displayed in the browser. This is the foundational infrastructure that all subsequent issues build on.

---

## Conversation Summary

### 1. Issue Identification

We started by exploring the repository to determine what Issue #1 entailed. The repo was in a documentation-only state — two session logs from prior research and planning, plus README, CLAUDE.md, and CONTRIBUTING.md. No implementation code existed.

From `docs/sessions/001-engine-design-and-research.md`, Issue #1 was identified as **"Project Skeleton — Framebuffer to Browser"** — the first of 8 sequential implementation milestones.

### 2. Plan Mode Design

A structured plan was created covering 6 files to build:

1. **`src/slop3d.h`** — Public API header defining the function contract
2. **`src/slop3d.c`** — C engine core with framebuffer, z-buffer, and clear logic
3. **`Makefile`** — Emscripten build system
4. **`js/slop3d.js`** — JS `Slop3D` class wrapping WASM calls and canvas blitting
5. **`web/index.html`** — HTML shell with pixelated canvas styling
6. **`web/demo.js`** — Color cycling demo proving the full round-trip

Key design decisions made during planning:
- Use `MODULARIZE=1` with `EXPORT_NAME="createSlop3D"` so JS can instantiate the WASM module as a factory
- Pack RGBA into uint32_t for fast framebuffer clearing (leveraging WASM's guaranteed little-endian byte order)
- Serve from project root so `../js/slop3d.js` relative path works from `web/index.html`
- Add `make serve` target for quick local testing

### 3. Implementation

All 6 files were created. The C code and Makefile were written first, and `make` was run immediately to verify the WASM build succeeded before writing the JS and HTML. Build output:
- `web/slop3d_wasm.js` — 12KB Emscripten glue code
- `web/slop3d_wasm.wasm` — 567 bytes compiled engine

### 4. Testing

The user ran `make serve` and opened `http://localhost:8080/web/index.html` in a browser. The demo displayed a 320x240 framebuffer scaled to 640x480 with `image-rendering: pixelated`, smoothly cycling through colors via sine wave modulation on R/G/B channels. Pipeline confirmed working end-to-end.

### 5. Post-Implementation Discussion

Two topics were discussed after confirming the demo worked:

**Notable implementation details:**
- An `onUpdate(callback)` hook was added to the `Slop3D` class beyond the original plan, providing a place for game logic to run between `frame_begin` (clear) and the canvas blit. This will be the natural game loop insertion point for future issues.
- The uint32_t framebuffer fill packs RGBA as `r | (g<<8) | (b<<16) | (a<<24)`, which maps correctly to RGBA byte layout because WASM is always little-endian.
- Script loading order in `index.html` matters: `slop3d_wasm.js` → `slop3d.js` → `demo.js`.
- The `../js/slop3d.js` relative path works because the HTTP server runs from the project root.

**Code attribution:** All code uses standard Emscripten patterns and common software rasterization techniques. No specific external sources were referenced beyond the project's own design documents from Session 001. No citations needed.

---

## Decisions Log

| Decision | Choice | Reasoning |
|----------|--------|-----------|
| Engine state struct | Single `S3D_Engine` with framebuffer + zbuffer + clear color | Matches CLAUDE.md spec: single global instance, no dynamic allocation |
| Framebuffer clear method | uint32_t packed fill loop | Faster than byte-by-byte; WASM guarantees little-endian so `r\|(g<<8)\|(b<<16)\|(a<<24)` is correct |
| Z-buffer clear method | `memset(zbuffer, 0xFF, sizeof(zbuffer))` | Fills every uint16_t with 0xFFFF (max distance); works because 0xFF byte fill produces 0xFFFF for 16-bit values |
| Emscripten module mode | `MODULARIZE=1` + `EXPORT_NAME="createSlop3D"` | Wraps WASM in a factory function, avoids global namespace pollution, allows async instantiation |
| Memory | 16MB fixed heap, no growth | Per CLAUDE.md spec; ~460KB used by framebuffer + zbuffer, well within budget |
| Canvas scaling | CSS `640x480` with `image-rendering: pixelated` | 2x upscale of 320x240; pixelated filtering preserves the retro look |
| Demo type | Color cycling via sine waves | More visually interesting than static color; proves the JS↔C round-trip works continuously per frame |
| Game loop hook | `onUpdate(callback)` on Slop3D class | Not in original plan but natural extension; gives game code a place to run each frame |
| HTTP serving | `make serve` target using `python3 -m http.server 8080` | WASM requires HTTP serving (won't load via `file://`); python3 is universally available |

---

## Files Changed

| File | Change | Lines |
|------|--------|-------|
| `src/slop3d.h` | Created — public API header with constants and 7 function declarations | 16 |
| `src/slop3d.c` | Created — C engine core with `S3D_Engine` struct, init, clear, framebuffer access | 58 |
| `Makefile` | Created — Emscripten build with `all`, `clean`, `serve` targets | 25 |
| `js/slop3d.js` | Created — `Slop3D` class with WASM loading, cwrap bindings, canvas blitting, render loop | 63 |
| `web/index.html` | Created — HTML shell with dark background, centered pixelated canvas | 24 |
| `web/demo.js` | Created — color cycling demo using sine wave RGB modulation | 15 |
| `docs/sessions/003-project-skeleton-framebuffer-to-browser.md` | Created — this session log | — |

**Build outputs (generated, not committed):**

| File | Size |
|------|------|
| `web/slop3d_wasm.js` | 12KB |
| `web/slop3d_wasm.wasm` | 567 bytes |

---

## Architecture Established

This session established the full rendering pipeline that all future issues will build on:

```
Game Code (demo.js)
    ↓ setClearColor(), onUpdate()
Slop3D JS Class (slop3d.js)
    ↓ cwrap'd function calls
WASM Bridge (slop3d_wasm.js)
    ↓ exported C functions
C Engine (slop3d.c)
    ↓ writes to framebuffer[]
WASM Linear Memory
    ↑ HEAPU8.subarray() read
Slop3D JS Class (slop3d.js)
    ↓ ImageData.set() + putImageData()
Browser Canvas
    ↓ image-rendering: pixelated
Screen (640x480 display of 320x240 framebuffer)
```

---

## Next Steps

Continue with [Issue #2: Vector/Matrix Math + Camera System](https://github.com/deadcast2/slopwave3d/issues/2) — implement the math foundation (vec3, vec4, mat4 operations) and camera system (view/projection matrices) needed before triangle rasterization can begin in Issue #3.
