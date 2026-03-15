# Session 027: Perspective-Correct Texturing + DSL Constants

**Date:** March 14, 2026
**Participants:** Caleb Cohoon + Claude (Opus 4.6)
**Duration:** Implementation session
**Repo:** [deadcast2/slopwave3d](https://github.com/deadcast2/slopwave3d)

---

## Session Goal

Implement GitHub Issue #9: optional perspective-correct texture mapping using the Abrash/Hecker subdivision technique, alongside the existing affine mapping. Additionally, introduce DSL constants and refactor the FPS camera syntax to use a reactive property pattern instead of a constructor parameter.

---

## Conversation Summary

### 1. Issue Review and Exploration

We started by reading GitHub Issue #9, which specified adding an optional perspective-correct texture mapping mode alongside the existing affine mapping. The issue called for the Abrash/Hecker subdivision technique -- computing true perspective-correct UVs every 16 pixels and linearly interpolating between checkpoints.

An exploration agent was launched to deeply analyze the scanline rasterizer in `src/slop3d.c`, tracing the full pipeline from `S3D_ScreenVert` through `interpolate_edge` to the per-pixel inner loop. Key findings:

- UV interpolation was purely affine: `cu += du; cv += dv` per pixel
- `S3D_ScreenVert` had no `w` field, but `eye_z` (used for fog) was already `clip.w`
- `S3D_EdgeVals` carried `u, v` but no perspective-correct interpolants
- No per-object texture mapping mode flags existed
- The rasterizer was called via `draw_object_internal` -> `s3d_rasterize_triangle`

### 2. Implementation Planning

A planning phase designed the approach:

- **Per-object flag**: Add `int texmap` to `S3D_Object` with constants `S3D_TEXMAP_AFFINE` (0) and `S3D_TEXMAP_PERSPECTIVE` (1)
- **Vertex data**: Add `uw, vw, invw` (u/w, v/w, 1/w) to `S3D_ScreenVert` and `S3D_EdgeVals` -- these are linear in screen space and safe to interpolate
- **Reuse `eye_z`**: Since `eye_z == clip.w`, no separate `w` field needed
- **Rasterizer structure**: Extract the pixel body into a macro (`S3D_RASTER_PIXEL`) shared by both paths, with two loop structures -- affine (unchanged) and perspective (16-pixel subdivision)
- **Pipeline threading**: Pass `texmap` through `s3d_render_scene` -> `draw_object_internal` -> `s3d_rasterize_triangle`

### 3. C Engine Implementation

The implementation proceeded in order:

**Header (`slop3d.h`):** Added `S3D_TEXMAP_AFFINE`/`S3D_TEXMAP_PERSPECTIVE` constants, `int texmap` field on `S3D_Object`, and `s3d_object_texmap()` declaration.

**Rasterizer types (`slop3d.c`):** Added `float uw, vw, invw` to `S3D_ScreenVert` and `S3D_EdgeVals`. Updated `ndc_to_screen` to compute `invw = 1/eye_z`, `uw = u * invw`, `vw = v * invw`. Updated `interpolate_edge` to interpolate the new fields.

**Pixel body macro:** Extracted the ~40-line pixel body (z-test, texture sampling, Gouraud modulation, fog blending, alpha blending, framebuffer write) into `S3D_RASTER_PIXEL(...)`. This avoided duplicating the pixel logic across the two loop paths.

**Perspective subdivision path:** When `texmap == S3D_TEXMAP_PERSPECTIVE && tex != NULL`, the rasterizer:
1. Computes `uw, vw, invw` start values and per-pixel deltas from edge values
2. Iterates in 16-pixel chunks (`S3D_PERSP_SUBDIV`)
3. At each chunk boundary, divides to recover true `u = uw/invw`, `v = vw/invw`
4. Derives affine sub-steps within the chunk
5. Runs the same pixel body macro as the affine path

Short spans (<16 pixels) and non-divisible spans are handled naturally -- the last chunk uses whatever pixels remain.

**Pipeline threading:** Added `int texmap` parameter to `s3d_rasterize_triangle` and `draw_object_internal`. Updated both call sites in `s3d_render_scene` (opaque and transparent passes) to pass `obj->texmap`.

**API function:** Added `s3d_object_texmap()` and initialized `texmap = S3D_TEXMAP_AFFINE` in `s3d_object_create`.

### 4. JS Bridge and Makefile

Added `_s3d_object_texmap` to the Makefile's `EXPORTED_FUNCTIONS`. Added cwrap binding and `texmap` getter/setter on `SlopObject` in `js/slop3d.js`.

### 5. DSL Syntax: `box.style = ps1` / `box.style = n64`

Caleb requested a more expressive DSL syntax instead of raw integers. Rather than `box.texmap = 0`, the syntax became `box.style = ps1` (affine) and `box.style = n64` (perspective-correct) -- named after the consoles iconic for each rendering style.

This required introducing **DSL constants** -- bare identifiers that compile to literal values. A `SLOP_CONSTANTS` map was added to the codegen:

```javascript
const SLOP_CONSTANTS = { ps1: '0', n64: '1' };
```

The `Ident` case in codegen checks `SLOP_CONSTANTS` before falling through to builtins or scene variables. The JS-side property was renamed from `texmap` to `style` (the C API remains `s3d_object_texmap` internally).

### 6. FPS Camera Refactor: `cam.act_as = fps`

Caleb then requested removing the `fps` parameter from camera creation. The old syntax `camera: fps, 0, 1.5, 5` became:

```
cam = camera: 0, 1.5, 5
cam.act_as = fps
```

This involved:

- **Parser**: Removed the `CAMERA_BEHAVIORS` set and behavior detection from `camera:` parsing. Camera creation now always takes just position (and optional target) args.
- **Codegen**: Removed the behavior branch from `CameraAssign` -- cameras always emit `_rt.camera(args)` without a behavior string.
- **Runtime**: Removed behavior parameter handling from `camera()`. The behavior string extraction (`typeof args[args.length - 1] === 'string'`) was deleted.
- **SlopCamera**: Added `act_as` getter/setter that calls `_rt._attachBehavior(this, v)`.
- **DSL constant**: Added `fps: "'fps'"` to `SLOP_CONSTANTS`, so the bare identifier `fps` compiles to the JS string literal `'fps'`.
- **Demo**: Updated `web/index.html` to use the new two-line syntax.

### 7. Testing

All tests passed throughout:
- 111 C unit tests (math, camera, viewport, backface, clipping, rasterizer)
- 73 JS tests (lexer, parser, codegen, runtime helpers) after adding new tests for `style` assignment with `ps1`/`n64` constants, `act_as` with `fps` constant, and camera creation without behavior

The `S3D_ScreenVert` initializers in `test_math.c` were updated to include the three new fields (`uw, vw, invw`) to eliminate compiler warnings.

---

## Research Findings

### Abrash/Hecker Subdivision Technique

The perspective-correct texture mapping approach is based on two key insights from 1990s game development:

1. **Screen-space linearity of reciprocals**: While `u` and `v` are NOT linear in screen space (causing affine warping), `u/w`, `v/w`, and `1/w` ARE linear in screen space. This is because the perspective projection divides by `w`, so pre-dividing by `w` "undoes" the nonlinearity.

2. **Subdivision amortizes division cost**: Full perspective correction requires a division per pixel (`u = (u/w) / (1/w)`). The Abrash/Hecker technique computes true perspective UVs only every N pixels (typically 16) and linearly interpolates between these checkpoints. Within a 16-pixel span, the error from linear interpolation is visually imperceptible.

The technique was widely used in software rasterizers of the era including Quake's software renderer (Michael Abrash, *Graphics Programming Black Book*) and is described in Chris Hecker's "Texture Mapping" series in *Game Developer Magazine* (1995-1996).

### Historical Context

As noted in Issue #9, Shockwave 3D supported hardware-accelerated rendering (OpenGL/DirectX) where perspective-correct mapping was standard. Most players experienced Shockwave 3D with perspective-correct textures. Affine warping only appeared on the software fallback path. Both modes are historically authentic.

---

## Research Sources

- Chris Hecker, "Texture Mapping" series, *Game Developer Magazine* (1995-1996) -- [PDF archive](https://www.gamers.org/dEngine/quake/papers/checker_texmap.html)
- Michael Abrash, *Graphics Programming Black Book* -- subdivision technique for efficient perspective correction
- [Session 002](002-shockwave-3d-accuracy-review.md) -- research confirming Shockwave 3D's hardware renderers used perspective-correct mapping

---

## Decisions Log

| # | Decision | Choice | Reasoning |
|---|----------|--------|-----------|
| 1 | Per-object vs global texmap mode | Per-object (`S3D_Object.texmap`) | Matches issue spec; allows mixing styles in one scene |
| 2 | Separate `w` field vs reuse `eye_z` | Reuse `eye_z` | `eye_z` is already `clip.w`; avoids redundant field |
| 3 | Pixel body duplication strategy | Extract into `S3D_RASTER_PIXEL` macro | Zero overhead, avoids ~40 lines of duplication, both paths share identical pixel logic |
| 4 | Per-pixel branch vs per-scanline branch | Per-scanline branch (two loop structures) | Inner loop is hot code; per-pixel branch wastes cycles |
| 5 | Subdivision size | 16 pixels (`S3D_PERSP_SUBDIV`) | Industry standard from Abrash/Hecker; good balance of quality vs cost |
| 6 | DSL syntax for texture mode | `box.style = ps1` / `box.style = n64` | Evocative console names instead of raw integers; more memorable |
| 7 | DSL constant mechanism | `SLOP_CONSTANTS` map in codegen | Simple, extensible; bare identifiers compile to literals |
| 8 | FPS camera syntax | `cam.act_as = fps` (reactive property) | Consistent with other property-based patterns; removes special parser case |
| 9 | Where to handle `act_as` | Setter on `SlopCamera` calls `_attachBehavior` | Keeps behavior attachment logic in one place; works with DSL constant pattern |

---

## Files Changed

| File | Changes |
|------|---------|
| `src/slop3d.h` | Added `S3D_TEXMAP_AFFINE`/`S3D_TEXMAP_PERSPECTIVE` constants, `int texmap` field on `S3D_Object`, `s3d_object_texmap()` declaration |
| `src/slop3d.c` | Added `S3D_PERSP_SUBDIV` constant, `uw/vw/invw` to `S3D_ScreenVert` and `S3D_EdgeVals`, `S3D_RASTER_PIXEL` macro, perspective subdivision branch in rasterizer, threaded `texmap` through draw pipeline, added `s3d_object_texmap()` API |
| `js/slop3d.js` | Added `style` getter/setter on `SlopObject`, `act_as` getter/setter on `SlopCamera`, `_objectTexmap` cwrap, `SLOP_CONSTANTS` map (`ps1`, `n64`, `fps`), removed camera behavior parsing/codegen/runtime handling |
| `Makefile` | Added `_s3d_object_texmap` to `EXPORTED_FUNCTIONS` |
| `tests/test_math.c` | Updated `S3D_ScreenVert` initializers with new `uw, vw, invw` fields |
| `tests/test_slopscript.js` | Added tests for `style = ps1`, `style = n64`, `act_as = fps`; updated camera parser/codegen tests |
| `web/index.html` | Updated demo to use `cam.act_as = fps` syntax |

---

## Next Steps

- Update `CLAUDE.md` documentation to reflect new SlopScript syntax (`box.style = ps1`/`n64`, `cam.act_as = fps`)
- Visual testing with large textured quads to verify perspective-correct mode eliminates warping
- Consider adding more DSL constants as the language grows (e.g., for future camera behaviors like `orbit`, `third_person`)
- Continue with remaining GitHub issues for engine features
