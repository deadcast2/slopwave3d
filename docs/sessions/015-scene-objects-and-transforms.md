# Session 015: Scene Objects & Transforms

**Date:** March 11, 2026
**Participants:** Caleb Cohoon + Claude (Opus 4.6)
**Duration:** Implementation session
**Repo:** [deadcast2/slopwave3d](https://github.com/deadcast2/slopwave3d)
**GitHub Issue:** [#5 — Scene Objects + Transforms](https://github.com/deadcast2/slopwave3d/issues/5)

---

## Session Goal

Implement the object/scene system (GitHub Issue #5) so multiple independently transformed meshes can be rendered in a scene. Prior to this, the engine could only draw meshes directly at world origin via `s3d_draw_mesh()` with no per-object transforms, color modulation, or alpha blending.

---

## Conversation Summary

### 1. Issue Review & Codebase Exploration

Fetched GitHub Issue #5 and launched an exploration agent to understand the current engine state. Key findings:

- The engine had a working rasterizer, camera system, mesh/texture loading, and OBJ parsing (~963 lines of C)
- `s3d_draw_mesh()` transformed vertices directly using the camera VP matrix with hardcoded white vertex color `(1, 1, 1)` — no model matrix support
- The rasterizer had no alpha blending — pixels were simply overwritten on z-test pass
- All matrix math utilities already existed: `m4_translate`, `m4_rotate_x/y/z`, `m4_scale`, `m4_multiply`, etc.
- No object concept existed anywhere in the codebase

### 2. Plan Design

A Plan agent designed the full implementation strategy covering:

- `S3D_Object` struct with position, euler rotation, scale, color, alpha, and cached model matrix
- Object API (create, destroy, setters for each property)
- Model matrix rebuild helper using T * Ry * Rx * Rz * S order
- Alpha blending in the rasterizer with separate opaque/transparent paths
- `s3d_render_scene()` with opaque-first then back-to-front transparent sorting
- Refactoring `s3d_draw_mesh` into an internal `draw_object_internal` helper

Key design decisions during planning:
- **Per-object alpha rather than per-vertex alpha** — the issue specifies object-level alpha, so we pass `obj_alpha` as a flat parameter to the rasterizer rather than interpolating alpha per-vertex
- **Insertion sort for transparent objects** — at most 256 objects, trivial cost, no need for qsort
- **Centroid distance approximation** — use the model matrix translation column `(m[12], m[13], m[14])` as the object origin for distance calculation

### 3. Backwards Compatibility Decision

The initial plan preserved `s3d_draw_mesh` for backwards compatibility. Caleb directed that backwards compatibility was not needed — old code should be removed to keep the codebase compact. The plan was updated to delete `s3d_draw_mesh` and `s3d_draw_triangle` entirely.

### 4. Implementation

All changes were implemented in a single pass across 5 files:

**Header (`src/slop3d.h`):**
- Added `S3D_MAX_OBJECTS 256`
- Added `S3D_Object` struct definition
- Replaced `s3d_draw_triangle`/`s3d_draw_mesh` declarations with 9 new object API function declarations

**Engine core (`src/slop3d.c`):**
- Added `S3D_Object objects[S3D_MAX_OBJECTS]` to `S3D_Engine` struct (~32KB)
- Added `rebuild_model_matrix()` — computes T * Ry * Rx * Rz * S
- Implemented all 8 object setter functions with validation
- Modified `s3d_rasterize_triangle` to accept `float obj_alpha`:
  - Opaque path (`alpha >= 1.0`): unchanged behavior, writes z-buffer
  - Transparent path (`alpha < 1.0`): z-test read-only (no z-write), reads existing framebuffer pixel, blends `final = src * alpha + dst * (1 - alpha)`
- Added `draw_object_internal()` — static helper that takes MVP, color, and alpha
- Deleted `s3d_draw_triangle` and `s3d_draw_mesh` entirely
- Implemented `s3d_render_scene()`:
  1. Collects active objects into opaque and transparent arrays
  2. Renders all opaque objects (z-buffer handles ordering)
  3. Sorts transparent objects back-to-front by squared distance from camera
  4. Renders transparent objects far-to-near

**JS wrapper (`js/slop3d.js`):**
- Removed `drawTriangle` and `drawMesh` cwrap bindings and methods
- Added cwrap bindings and public methods for all 9 new functions

**Makefile:**
- Removed `_s3d_draw_triangle` and `_s3d_draw_mesh` from exports
- Added 9 new exported function symbols

**Tests (`tests/test_math.c`):**
- Added `make_test_tri_mesh()` helper to create 1-triangle meshes for testing
- Rewrote `rasterize_fills_pixels` and `rasterize_zbuffer_occlusion` tests to use the object system

**Demo (`web/index.html`):**
- Creates 3 cubes demonstrating the system:
  - obj1: rotating cube at origin with default color
  - obj2: small blue-tinted cube offset to the right, spinning on two axes
  - obj3: semi-transparent red-tinted cube, demonstrating alpha blending

### 5. Verification

- Native tests: 97/97 passed, 0 failures
- WASM build: compiled cleanly with no warnings
- Issue #5 closed on GitHub with implementation summary

---

## Research Findings

No external research was needed for this session. The implementation was based entirely on the existing codebase patterns and the requirements in GitHub Issue #5.

---

## Decisions Log

| # | Decision | Choice | Reasoning |
|---|----------|--------|-----------|
| 1 | Object struct fields | Store position/rotation/scale separately + cached model mat4 | Setters update individual properties and rebuild the matrix, avoiding manual matrix construction by users |
| 2 | Rotation order | T * Ry * Rx * Rz * S | Standard game engine convention — yaw (Y) applied first, then pitch (X), then roll (Z) |
| 3 | Alpha blending approach | Per-object `obj_alpha` parameter to rasterizer | Issue specifies per-object alpha, not per-vertex; avoids interpolation overhead in the common opaque case |
| 4 | Transparent z-buffer behavior | Read-only z-test, no z-write | Transparent objects must not occlude objects behind them; z-test still prevents rendering behind opaque geometry |
| 5 | Transparent sort method | Insertion sort on squared distance | At most 256 objects; insertion sort is simple and efficient for small N; squared distance avoids sqrt |
| 6 | Centroid distance calculation | Use model matrix translation column | `model.m[12/13/14]` gives world-space object origin directly — simple and sufficient |
| 7 | Backwards compatibility | Remove `s3d_draw_mesh` and `s3d_draw_triangle` entirely | Per Caleb's direction — keep codebase compact, object system is the sole rendering path |
| 8 | Color modulation | Object color multiplied into vertex color in ClipVert | Vertex color modulates texture sample; object color modulates vertex color — matches Shockwave 3D's color model |

---

## Files Changed

| File | Action | Description |
|------|--------|-------------|
| `src/slop3d.h` | Modified | Added `S3D_MAX_OBJECTS`, `S3D_Object` struct, 9 new function declarations; removed `s3d_draw_triangle`/`s3d_draw_mesh` declarations |
| `src/slop3d.c` | Modified | Added object storage, `rebuild_model_matrix`, all object API functions, `draw_object_internal`, `s3d_render_scene` with sorting, alpha blending in rasterizer; removed `s3d_draw_triangle`/`s3d_draw_mesh` |
| `js/slop3d.js` | Modified | Added cwrap bindings and public methods for 9 new functions; removed `drawTriangle`/`drawMesh` |
| `Makefile` | Modified | Updated `EXPORTED_FUNCTIONS` — removed 2 old, added 9 new |
| `web/index.html` | Modified | Updated demo to create 3 objects with different transforms, colors, and alpha |
| `tests/test_math.c` | Modified | Added `make_test_tri_mesh` helper; rewrote 2 rasterizer tests to use object system |

---

## Next Steps

- **Issue #6** — Next sequential issue in the implementation backlog
- The object system enables future features like:
  - Lighting (Issue #6 likely) — Gouraud shading can now use per-object normals transformed by the model matrix
  - Fog — distance-based fog can use object world positions
  - Scene management — object show/hide, object picking by ID
