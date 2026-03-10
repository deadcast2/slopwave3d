# Session 011: Meshes, Textures & OBJ Loading

**Date:** March 10, 2026
**Participants:** Caleb Cohoon + Claude (Opus 4.6)
**Duration:** Implementation session
**Repo:** [deadcast2/slopwave3d](https://github.com/deadcast2/slopwave3d)
**Issue:** [#4 — Meshes, Textures + OBJ Loading](https://github.com/deadcast2/slopwave3d/issues/4)

---

## Session Goal

Implement the full asset pipeline for slop3d: mesh and texture data structures, OBJ file loading with fan-triangulation, nearest-neighbor texture sampling in the rasterizer with affine UV interpolation, and JS/ActiveX asset loading helpers. The acceptance criteria was a textured cube rendering on screen in both the WASM and ActiveX builds.

---

## Conversation Summary

### 1. Issue Review & Codebase Exploration

We started by pulling GitHub issue #4, which defined the full scope: `S3D_Vertex`, `S3D_Triangle`, `S3D_Mesh`, and `S3D_Texture` structs, mesh/texture storage with fixed-size arrays, OBJ parsing, nearest-neighbor texture sampling, texture modulation by vertex color, and JS-side `loadTexture`/`loadOBJ` helpers.

An exploration agent surveyed the existing engine state. Key findings:
- The scanline rasterizer was fully functional for Gouraud shading (z, r, g, b interpolation)
- `S3D_ClipVert` and `S3D_ScreenVert` already had `u, v` fields but they were hardcoded to 0.0 and never used
- `ndc_to_screen()` took 6 parameters (no u, v) and hardcoded UV to zero
- No mesh, texture, or object data structures existed
- The only draw call was `s3d_draw_triangle()` with 18 float args (position + color per vertex, no UVs)

### 2. Planning Phase

A Plan agent designed the implementation across 12 steps. Key design decisions made during planning:

**Memory layout:** Pool allocator for meshes — a single shared vertex pool (32,768 vertices, ~1MB) and triangle pool (65,536 triangles, ~384KB) rather than fixed per-mesh allocations. Textures store pixel data inline (64KB each × 64 slots = ~4MB). Total addition to `g_engine`: ~5.4MB, well within the 16MB WASM heap.

**Texture upload strategy:** Direct memory write — `s3d_texture_create()` allocates a slot and `s3d_texture_get_data_ptr()` returns a pointer for JS to write RGBA data directly via `HEAPU8.set()`. This avoids exporting `malloc`/`free` for texture uploads.

**OBJ parser temp storage:** `static` arrays inside the parser function (4,096 positions, texcoords, normals each) to avoid stack overflow on Emscripten's default 64KB stack.

### 3. ActiveX Considerations

Caleb pointed out a comment on issue #4 describing the ActiveX JPEG loading strategy. The ActiveX build should use `IPicture` via `OleLoadPicture()` to decode JPEGs — this is available on stock Windows XP through the already-included `olectl.h` header, avoiding any vendored dependencies like stb_image.

The plan was expanded to include three new ActiveX IDispatch methods (`LoadTexture`, `LoadOBJ`, `DrawMesh`) with DISPIDs 14-16.

### 4. Implementation

Implementation proceeded through the planned steps:

**C data structures** (`slop3d.h`): Added `S3D_Vertex` (pos, normal, uv), `S3D_Triangle` (3 uint16 indices), `S3D_Mesh` (pool offsets/counts), `S3D_Texture` (dimensions + inline 64KB pixel array), plus constants for pool sizes.

**Engine state** (`slop3d.c`): Added texture array, mesh array, vertex pool, triangle pool, and pool counters to `S3D_Engine`.

**Rasterizer UV support**: Modified `ndc_to_screen()` to accept and pass through `u, v` parameters. Extended `s3d_rasterize_triangle()` with a `texture_id` parameter and full affine UV interpolation — `lu, lv` on the long edge, `su, sv` on the short edge, `du, dv` stepping in the inner pixel loop. When `texture_id >= 0`, the rasterizer samples the texture with nearest-neighbor (bitmask wrapping for power-of-2 dimensions) and modulates by vertex color. The existing `s3d_draw_triangle()` passes `texture_id = -1` to preserve Gouraud-only behavior.

**Texture API**: `s3d_texture_create()` finds a free slot and returns an ID. `s3d_texture_get_data_ptr()` returns a pointer to the pixel array for direct writes.

**Mesh API**: `s3d_mesh_create()` allocates contiguous slices from the vertex/triangle pools. `s3d_mesh_get_vertex_ptr()` and `s3d_mesh_get_index_ptr()` return pointers to the allocated slices.

**OBJ parser**: `s3d_mesh_load_obj()` parses OBJ text already in memory. First pass collects `v`, `vt`, `vn` into static temp arrays. Face processing handles `v/vt/vn`, `v/vt`, `v//vn`, and `v` formats, resolves 1-based and negative OBJ indices, and fan-triangulates n-gon faces. Each unique face vertex tuple becomes a new `S3D_Vertex` (accepting duplication for simplicity). Required adding `#include <stdlib.h>` for `strtod`/`strtol`.

**Mesh drawing**: `s3d_draw_mesh()` iterates all triangles in a mesh, transforms vertices to clip space via the camera VP matrix, sets vertex colors to white (1,1,1) since lighting comes in a later issue, passes through UVs, then runs the standard pipeline: near-plane clip → perspective divide → viewport transform → backface cull → rasterize with texture.

**Test fix**: Updated `tests/test_math.c` — three calls to `ndc_to_screen()` needed the two extra `u, v` arguments added.

**Makefile**: Added all new exported functions plus `_malloc`/`_free` (needed for OBJ string transfer from JS).

**JS API** (`slop3d.js`): Added cwrap bindings for all new C exports. Implemented `loadTexture(url)` (Image decode → canvas downscale to ≤128×128 → getImageData → HEAPU8.set at texture pointer), `loadOBJ(url)` (fetch → TextEncoder → malloc → copy to WASM heap → C parser → free), and `drawMesh(meshId, textureId)`.

**Demo assets**: Converted the provided `crate.png` to JPEG via `sips`. Created `web/assets/cube.obj` — a unit cube centered at the origin with CW winding (matching slop3d's front-face convention) and UV coordinates.

**Demo** (`web/demo.js`): Replaced the two-triangle demo with a textured cube — loads texture and OBJ, orbiting camera at distance 3 and height 1.5.

### 5. ActiveX Implementation

**DISPIDs** (`slop3d_guids.h`): Added `DISPID_S3D_LOADTEXTURE` (14), `DISPID_S3D_LOADOBJ` (15), `DISPID_S3D_DRAWMESH` (16).

**IDispatch methods** (`slop3d_activex.cpp`): Added dispatch table entries and three new Invoke cases:

- `LoadTexture(path)`: Reads JPEG file with `CreateFileA`/`ReadFile` into an `HGLOBAL`, creates an `IStream` via `CreateStreamOnHGlobal`, decodes with `OleLoadPicture` → `IPicture`, renders to a 128×128 DIB section via `IPicture::Render`, then copies BGRA→RGBA into the engine texture slot.
- `LoadOBJ(path)`: Reads file with `CreateFileA`/`ReadFile` into a `GlobalAlloc` buffer, calls `s3d_mesh_load_obj()` (same C parser as WASM), frees buffer, returns mesh ID.
- `DrawMesh(meshId, textureId)`: Extracts two int args, calls `s3d_draw_mesh()`.

**Test page** (`activex/test.html`): Updated to load a textured cube with the orbiting camera, matching the WASM demo.

### 6. Build Verification

Clean WASM build succeeded. All 97 existing math tests passed after the `ndc_to_screen` signature update. The WASM demo rendered the textured cube correctly in the browser.

### 7. ActiveX Debugging — File Path Resolution

The ActiveX build initially showed a black screen. Two rounds of debugging followed:

**Round 1:** The `test.html` used relative paths (`"crate.jpg"`) for `LoadTexture`/`LoadOBJ`. Since `CreateFileA` resolves relative to the process working directory (IE's CWD is typically `C:\Windows\System32`), the files weren't found. Fix: compute absolute paths from `document.URL`.

**Round 2:** The path extraction used `docUrl.indexOf('///')` to strip the `file:///` prefix. IE6/XP's `document.URL` uses `file://` (two slashes), not `file:///` (three). The `indexOf('///')` matched at the wrong position, producing `"le:\\crate.jpg"`. A second attempt with `replace(/^file:\/\/\//, '')` also failed for the same reason — the regex matched nothing, and subsequent processing produced `"file:\\crate.jpg"`.

**Final fix:** Simple string operations that handle both two and three slashes:
```javascript
var path = unescape(document.URL);
if (path.substring(0, 8) == "file:///") path = path.substring(8);
else if (path.substring(0, 7) == "file://") path = path.substring(7);
path = path.replace(/\//g, '\\');
var baseDir = path.substring(0, path.lastIndexOf('\\') + 1);
```

After this fix, both WASM and ActiveX demos rendered the textured cube successfully.

---

## Research Findings

### IPicture JPEG Loading on Windows XP

The `IPicture` COM interface (declared in `olectl.h`) provides image loading without external dependencies. The pipeline:
1. `CreateStreamOnHGlobal()` wraps raw file bytes in an `IStream`
2. `OleLoadPicture()` decodes the image (JPEG, BMP, GIF supported) into an `IPicture`
3. `IPicture::get_Width()`/`get_Height()` return dimensions in HIMETRIC units
4. `IPicture::Render()` draws to any HDC, handling scaling automatically
5. A DIB section provides direct pixel access for copying to the engine

This approach requires zero vendored code — all APIs are part of the Windows XP COM runtime.

### IE6 JScript `document.URL` Behavior

On Windows XP with IE6, `document.URL` for local files returns a `file://` URL with **two** slashes (not the three-slash `file:///` form common in modern browsers). The path component uses backslashes native to Windows. This means URL parsing code must handle both `file://C:\path` and `file:///C:/path` forms.

### OBJ File Format

The Wavefront OBJ format stores geometry data as:
- `v x y z` — vertex positions (1-based indexing)
- `vt u v` — texture coordinates
- `vn x y z` — vertex normals
- `f v/vt/vn ...` — faces referencing indices into each array independently

Key implementation details:
- Indices are 1-based (subtract 1 for C arrays)
- Negative indices are relative to the end of the current array
- Faces can have N vertices (n-gons), requiring fan-triangulation: vertices 0,1,2 → tri 1; 0,2,3 → tri 2; etc.
- Face vertex format varies: `v`, `v/vt`, `v//vn`, `v/vt/vn`

---

## Research Sources

No web research was conducted this session. All implementation was based on existing codebase knowledge, the GitHub issue specification, and the issue comment describing the IPicture strategy.

---

## Decisions Log

| # | Decision | Choice | Reasoning |
|---|----------|--------|-----------|
| 1 | Mesh memory layout | Shared pool allocator (32K verts, 64K tris) | Fixed per-mesh allocation would exceed 16MB heap; pool approach uses ~1.4MB total |
| 2 | Texture pixel storage | Inline 64KB array per texture slot | Simple, no pointer management, 64 × 64KB = 4MB fits in heap |
| 3 | Texture upload mechanism | Direct pointer write (no malloc for textures) | JS writes directly to texture pixel array via HEAPU8; simpler than allocating temp buffers |
| 4 | OBJ parser temp arrays | `static` arrays in function | Avoids stack overflow on Emscripten's 64KB default stack; 3 × 4096 × 12 bytes = 144KB |
| 5 | OBJ vertex deduplication | No deduplication (accept duplicates) | Simplicity; low-poly models (~10K poly ceiling) won't exhaust the 32K vertex pool |
| 6 | UV interpolation method | Affine (no perspective correction) | Intentional design constraint for era-authentic warping artifacts |
| 7 | Texture wrapping | Bitmask (`& (width-1)`) | Fast for power-of-2 textures; all textures are ≤128×128 powers of 2 |
| 8 | ActiveX JPEG loading | IPicture via OleLoadPicture | Zero dependencies, available on stock Windows XP through existing olectl.h include |
| 9 | ActiveX file path resolution | Compute absolute paths from document.URL | CreateFileA resolves relative to IE's CWD (System32), not the HTML directory |
| 10 | Vertex colors in mesh drawing | White (1,1,1) | Lighting is a future issue; white shows texture at full brightness |

---

## Files Changed

### Created
- `web/assets/cube.obj` — Unit cube with CW winding, UV coordinates, 6 quads → 12 triangles
- `web/assets/crate.jpg` — Test texture converted from `crate.png` via `sips`
- `docs/sessions/011-meshes-textures-obj-loading.md` — This session log

### Modified
- `src/slop3d.h` — Added S3D_Vertex, S3D_Triangle, S3D_Mesh, S3D_Texture structs; pool size constants; new function declarations
- `src/slop3d.c` — Added texture/mesh pools to engine; texture create/get_data_ptr; mesh create/get_vertex_ptr/get_index_ptr; OBJ parser; mesh drawing; rasterizer UV interpolation + texture sampling; added `#include <stdlib.h>`
- `js/slop3d.js` — Added cwrap bindings; loadTexture(), loadOBJ(), drawMesh() methods
- `Makefile` — Added 9 new exported functions including _malloc/_free
- `web/demo.js` — Replaced two-triangle demo with textured cube
- `tests/test_math.c` — Updated 3 ndc_to_screen() calls with new u,v parameters
- `activex/slop3d_guids.h` — Added DISPIDs 14-16 (LoadTexture, LoadOBJ, DrawMesh)
- `activex/slop3d_activex.cpp` — Added dispatch table entries; LoadTexture with IPicture JPEG loading; LoadOBJ with file I/O; DrawMesh dispatch
- `activex/test.html` — Textured cube demo with absolute path resolution for IE6

---

## Next Steps

- **Issue #5** — Next sequential issue (objects, transforms, scene graph)
- **Lighting** — Mesh normals are stored but unused; Gouraud per-vertex lighting will use them
- **Perspective-correct texturing** — Currently intentionally affine; could be toggled if needed
- **Mesh deletion** — No `s3d_mesh_destroy()` yet; pool space is not reclaimed
- **Non-power-of-2 textures** — Bitmask wrapping only works for power-of-2 dimensions; modulo fallback could be added
