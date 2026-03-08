# Session 007: Triangle Rasterizer & Z-Buffer

**Date:** March 8, 2026
**Participants:** Caleb Cohoon + Claude (Opus 4.6)
**Duration:** Full implementation session
**Repo:** [deadcast2/slopwave3d](https://github.com/deadcast2/slopwave3d)
**Issue:** [#3 — Triangle Rasterizer + Z-Buffer](https://github.com/deadcast2/slopwave3d/issues/3)

---

## Session Goal

Implement the scanline triangle rasterizer — the heart of the slop3d engine. This includes the full vertex transformation pipeline (clip space → near-plane clipping → perspective divide → viewport transform → backface culling → scanline rasterization) with 16-bit z-buffer depth testing and affine attribute interpolation.

---

## Conversation Summary

### 1. Planning Phase

The session began in plan mode. We explored the existing codebase to understand what Issues #1 and #2 had established:

- **Math library** (Issue #2): Vec3, Vec4, Mat4 with all operations — multiply, perspective, lookat, inverse_affine
- **Camera system** (Issue #2): S3D_Camera with position/target/up, computes view/projection/VP matrices
- **Frame management** (Issue #1): 320×240 RGBA8 framebuffer + 16-bit z-buffer already allocated and cleared each frame
- **JS wrapper** (Issue #1): Slop3D class with canvas blitting via `putImageData`, animation loop via `requestAnimationFrame`

With this foundation, we designed an 8-step implementation plan covering structs, helpers, clipping, rasterization, the public draw function, build updates, a test scene, and unit tests.

### 2. C Engine Implementation

All new rendering code was added to `src/slop3d.c` (~280 lines), placed between the engine state definition and the exported functions section.

**New structs:**
- `S3D_ClipVert` — clip-space vertex carrying `S3D_Vec4 clip` plus `r, g, b, u, v` attributes
- `S3D_ScreenVert` — screen-space vertex with `x, y, z, u, v, r, g, b`

Both include `u, v` texture coordinates even though texturing isn't implemented yet, to avoid restructuring later.

**Helper functions (static inline):**
- `lerp_clip_vert(a, b, t)` — linearly interpolates all ClipVert fields for clipping
- `ndc_to_screen(ndc_x, ndc_y, ndc_z, r, g, b)` — NDC→screen coordinate transform with Y-flip (NDC +Y up → screen +Y down) and z mapped from [-1,1] to [0,1]
- `is_front_facing(a, b, c)` — 2D cross product test; positive = CW in screen space (Y-down) = front face
- `clampf(v, lo, hi)` — float clamping utility

**Near-plane clipping:**
- `clip_near_plane(in[], in_count, out[])` — Sutherland-Hodgman algorithm against the near plane (`clip.w + clip.z >= 0`). Operates in clip space before perspective divide since the divide is undefined for w ≤ 0. A triangle (3 verts) can produce 0–4 output vertices; the caller fans the result into triangles.

**Scanline rasterizer:**
- `s3d_rasterize_triangle(v0, v1, v2)` — the core rasterizer taking 3 screen-space vertices:
  1. Sort vertices by Y ascending (3-element bubble sort)
  2. Walk scanlines from top to bottom, interpolating the long edge (v0→v2) and short edges (v0→v1, v1→v2) independently
  3. Per scanline: ensure left-to-right ordering, walk pixels
  4. Per pixel: lerp z and colors (affine — no perspective correction), convert z to uint16, test against z-buffer, write color + depth on pass
  5. Color packed as `R | (G<<8) | (B<<16) | (0xFF<<24)` matching the existing framebuffer format

**Public draw function (exported):**
- `s3d_draw_triangle(x0,y0,z0,r0,g0,b0, x1,..., x2,...)` — 18 float parameters (verbose but avoids struct passing through WASM). Pipeline: VP matrix transform → near-plane clip → fan to triangles → perspective divide → viewport transform → backface cull → rasterize.

### 3. Build & JS Integration

- **`src/slop3d.h`** — added `s3d_draw_triangle` declaration
- **`Makefile`** — added `_s3d_draw_triangle` to EXPORTED_FUNCTIONS
- **`js/slop3d.js`** — added cwrap binding (18 number params) and a convenience method `drawTriangle(v0, v1, v2)` where each vertex is `{x, y, z, r, g, b}`

### 4. Test Scene

Replaced the color-cycling demo in `web/demo.js` with:
- Two triangles: front (RGB vertex colors at z=0) and back (CMY vertex colors at z=-2)
- Orbiting camera using `sin(t)` / `cos(t)` for position
- Validates all acceptance criteria: rendering, Gouraud interpolation, z-occlusion, camera movement, backface culling

### 5. Unit Tests

Added 10 new tests to `tests/test_math.c` (20 new assertions, 97 total):

- **Viewport transform** (2 tests): NDC origin→screen center, NDC corners→screen corners with Y-flip
- **Backface culling** (2 tests): CW = front-facing, CCW = back-facing
- **Near-plane clipping** (4 tests): all-inside (3 verts out), all-behind (0 verts), one-behind (4 verts = quad), two-behind (3 verts = triangle)
- **Rasterizer integration** (2 tests): triangle fills pixels (center pixel ≠ clear color), z-buffer occlusion (front red triangle wins over back blue triangle)

### 6. Scanline Overflow Bug Fix

After the initial implementation, Caleb noticed that at certain camera rotations, the bottom scanlines of triangles would extend too far left and right. The root cause was edge interpolation `t` values exceeding the [0,1] range due to floating point imprecision near degenerate edge configurations.

**Fix (2 changes):**
1. Clamped all edge interpolation `t` values to [0,1] using `clampf()` — prevents extrapolation past edge endpoints
2. Changed the lower-half degenerate fallback from snapping to `v1` to snapping to `v2` — correct endpoint for flat-bottom triangles where `v1.y ≈ v2.y`

### 7. ActiveX Control Update

Extended the ActiveX control to expose `DrawTriangle` via COM:

- **`slop3d_guids.h`** — added `DISPID_S3D_DRAWTRIANGLE = 13`
- **`slop3d_activex.cpp`** — added `DrawTriangle` to `g_dispIdTable` and a new `Invoke` case extracting 18 float args via `GetFloatArg()`
- **`test.html`** — updated to match the WASM demo scene (two triangles, orbiting camera)

### 8. ActiveX Auto-Start Fix

Caleb found that the ActiveX control wouldn't auto-start rendering. The issue: when `engine.Start()` is called from inline `<script>` at page load, the control's window (`m_hwnd`) hasn't been created yet — IE hasn't triggered in-place activation. The `SetTimer` call was guarded by `if (m_hwnd && !m_running)`, silently failing.

**Fix:** Added a `m_pendingStart` flag to `CSlop3DControl`. When `Start()` is called before the window exists, it sets the flag. When `InPlaceActivate()` later creates the window, it checks the flag and starts the timer. This lets `engine.Start()` work whether called from inline script or user interaction.

### 9. ActiveX Test Page Styling

Updated `activex/test.html` to match the WASM page's minimal appearance — just a centered 640×480 control on a dark `#1a1a2e` background. Replaced CSS flexbox (unsupported in IE6) with table-based centering (`<table width="100%" height="100%">` with `vertical-align: middle`).

### 10. Documentation Review

Reviewed CLAUDE.md and README.md — both already accurately describe the rendering pipeline, architecture, and constraints. No updates needed; the docs were written to describe the full planned engine, and Issue #3 implements what was already documented.

---

## Decisions Log

| # | Decision | Choice | Reasoning |
|---|----------|--------|-----------|
| 1 | API signature for draw_triangle | 18 flat floats | Avoids struct passing through WASM boundary; verbose but simple |
| 2 | Include u,v in ScreenVert now | Yes | Avoids restructuring when texturing is added in a later issue |
| 3 | Clip in clip space, not NDC | Clip space | Perspective divide is undefined for w ≤ 0 (vertices behind camera) |
| 4 | Z-test comparison | `z16 <= zbuffer` | Less-or-equal allows coplanar overwrites, matches z-fighting aesthetic |
| 5 | Backface cull sign convention | Positive cross = front | CW winding in 3D → positive 2D cross product in Y-down screen space |
| 6 | Scanline overflow fix | Clamp t to [0,1] | Prevents floating point extrapolation past edge endpoints |
| 7 | ActiveX auto-start | m_pendingStart flag | Defers timer setup until window exists; supports inline script |
| 8 | IE6 centering CSS | Table-based layout | Flexbox not supported in IE6; table centering works everywhere |

---

## Files Changed

| File | Action | Description |
|------|--------|-------------|
| `src/slop3d.c` | Modified | Added ~280 lines: structs, helpers, clipper, rasterizer, draw function |
| `src/slop3d.h` | Modified | Added `s3d_draw_triangle` declaration |
| `js/slop3d.js` | Modified | Added cwrap binding + `drawTriangle()` convenience method |
| `Makefile` | Modified | Added `_s3d_draw_triangle` to EXPORTED_FUNCTIONS |
| `web/demo.js` | Modified | Replaced color-cycling with two-triangle orbiting camera demo |
| `tests/test_math.c` | Modified | Added 10 tests (viewport, backface, clipping, rasterizer integration) |
| `activex/slop3d_guids.h` | Modified | Added `DISPID_S3D_DRAWTRIANGLE = 13` |
| `activex/slop3d_activex.h` | Modified | Added `m_pendingStart` member |
| `activex/slop3d_activex.cpp` | Modified | Added DrawTriangle dispatch, pending start logic |
| `activex/test.html` | Modified | Matched WASM demo scene, IE6-compatible centering, auto-start |

---

## Commits

| Hash | Message |
|------|---------|
| `28b2456` | Implement triangle rasterizer and z-buffer (Issue #3) |
| `f35b2af` | Fix scanline overflow at triangle edges |
| `ba6d2df` | Expose DrawTriangle via ActiveX and update test page |

---

## Test Results

```
97 passed, 0 failed
```

All 77 existing tests continue to pass. 20 new assertions added across 10 new test functions covering viewport transform, backface culling, near-plane clipping, and rasterizer integration.

---

## Next Steps

- **Issue #4** — next in the sequential implementation plan (check GitHub issues for scope)
- **Delta time** — both WASM and ActiveX demos use fixed `t += 0.02` per frame; a proper `dt` parameter would make animation frame-rate independent (planned as part of the full `onUpdate(dt)` API shown in README)
- **Session log** — this document (007)
