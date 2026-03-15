# Session 029: Procedural Terrain Generation & OBJ Winding Fix

**Date:** March 15, 2026
**Participants:** Caleb Cohoon + Claude (Opus 4.6)
**Duration:** Feature implementation session
**Repo:** [deadcast2/slopwave3d](https://github.com/deadcast2/slopwave3d)

---

## Session Goal

Add a declarative terrain generation command to SlopScript for building game worlds, and fix inside-out rendering of Blender-exported OBJ models. Caleb obtained a spooky asset pack and began building a graveyard game scene (`grave_game.slop`), which exposed the need for both features.

---

## Conversation Summary

### 1. The Problem: Tiling a Ground Plane

Caleb had a unit tile OBJ (`terrain_tile.obj`) and wanted to cover a ground area with many copies of it. In current SlopScript, this would require manual `for` loops — verbose and un-slop-like. He asked whether this was an opportunity for a new DSL feature.

### 2. Initial Design: `tile:` Command

We discussed a declarative `tile:` command that would spawn a grid of objects:

```
ground = tile: terrain, terrain_tex_1, 10, 10
```

Design questions addressed:
- **Naming** — settled on `tile` initially (later renamed to `terrain`)
- **Centering** — grid centers at origin by default, with optional position args
- **Tile size** — 1x1 unit tiles matching the OBJ
- **Access** — return a wrapper object for moving the whole grid

### 3. Hills / Height Variation Feature

Caleb suggested adding seeded height variation to create hills. We explored three API styles:

| Option | Syntax | Verdict |
|--------|--------|---------|
| Property-based (separate) | `ground.hills = 42` + `ground.hills_height = 0.5` | Clean but verbose |
| Inline parameter | `ground = tile: terrain, tex, 10, 10, 0, 0, 0, 42` | Too many positional args |
| **Combined tuple** | **`ground.hills = 42, 0.5`** | **Chosen — compact, uses existing tuple syntax** |

The noise implementation uses a seeded integer hash with bilinear interpolation between grid points for smooth rolling hills.

### 4. First Implementation: Individual Tile Objects

The initial implementation spawned `cols × rows` individual `SlopObject` instances, each parented to an invisible root object (`mesh_id = -1`, which the C renderer's `S3D_CHECK_MESH` guard skips). Moving the root moved the entire grid via the scene graph hierarchy.

Changes spanned all four transpiler phases (lexer recognition → parser → AST → codegen) plus the runtime's `tile()` method and a new `SlopTileGrid` wrapper class.

### 5. Pivot: Connected Mesh Generation

Caleb asked two follow-up questions that drove a fundamental redesign:

1. **`.style` support** — Could the grid use `n64` mode for perspective-correct texturing?
2. **Connected vertices** — Instead of individual tiles with gaps at different heights, could vertices be shared so the terrain is a single smooth connected surface?

This meant the `tile:` command couldn't just spawn existing mesh instances — it needed to **generate a custom mesh** programmatically.

### 6. Programmatic Mesh Generation via WASM

We explored the C engine's mesh creation API and discovered three already-exported but unused WASM functions:

- `s3d_mesh_create(vertex_count, triangle_count)` → allocates in global pools
- `s3d_mesh_get_vertex_ptr(mesh_id)` → returns pointer for direct memory writes
- `s3d_mesh_get_index_ptr(mesh_id)` → returns pointer for triangle index writes

These were already in the Makefile's `EXPORTED_FUNCTIONS` but had no JS cwrap bindings. We added cwraps and used them to write vertex/index data directly into WASM linear memory using `Float32Array` and `Uint16Array` views on `HEAPU8.buffer`.

**Vertex layout** (32 bytes per vertex, matching `S3D_Vertex`):
```
Offset  Type      Field
0       float[3]  x, y, z (position)
12      float[3]  nx, ny, nz (normal)
24      float[2]  u, v (texcoord)
```

**Grid mesh structure** for a `cols × rows` grid:
- Vertices: `(cols+1) × (rows+1)` — shared at cell edges
- Triangles: `cols × rows × 2` — two per cell, CW winding
- UVs: tiling pattern (0→1 per cell) so the texture repeats

### 7. Rename: `tile:` → `terrain:`

Since the command now generates its own mesh rather than tiling an existing one, the mesh argument became unnecessary. We renamed `tile` to `terrain` and simplified the signature:

```
# Before: ground = tile: terrain_model, terrain_tex, 10, 10
# After:  ground = terrain: terrain_tex, 10, 10
```

The `terrain:` command takes only a texture name, dimensions, and optional position — no model asset needed.

### 8. `.hills` Implementation on Connected Mesh

The `.hills` setter was rewritten to modify vertex Y positions directly in WASM memory rather than moving individual objects. It also recomputes per-vertex normals from neighbor heights so lighting responds correctly to slopes:

```javascript
const yL = c > 0 ? this._heights[vi - 1] : y;
const yR = c < this.cols ? this._heights[vi + 1] : y;
const yD = r > 0 ? this._heights[vi - vCols] : y;
const yU = r < this.rows ? this._heights[vi + vCols] : y;
const nx = yL - yR;
const nz = yD - yU;
const ny = 2.0;
```

### 9. OBJ Winding Order Fix

Caleb noticed that models exported from Blender appeared inside-out. The root cause: OBJ files use CCW (counter-clockwise) winding by convention, but the slop3d engine uses CW (clockwise) for front faces.

The fix was a one-line swap in the OBJ parser's fan-triangulation loop (`src/slop3d.c`, line 860-864):

```c
// Before: i0, face[i], face[i+1]  (preserves OBJ CCW)
// After:  i0, face[i+1], face[i]  (flips to engine CW)
```

This is applied at parse time so all imported OBJ models render correctly regardless of the authoring tool.

### 10. External Game Directory Support

Caleb wanted to keep his game and assets outside the slop3d repo (non-redistributable assets). We explored the IDE's asset loading pipeline and found it already supports this — `ide/main.js` resolves asset paths relative to the opened `.slop` file's directory via the `get-asset-base` IPC handler. No changes needed.

---

## Technical Details

### Procedural Mesh Generation Pipeline

1. `SlopRuntime.terrain()` calls `s3d_mesh_create(vertCount, triCount)` to allocate vertex/triangle pool space
2. Gets raw pointers via `s3d_mesh_get_vertex_ptr()` and `s3d_mesh_get_index_ptr()`
3. Creates `Float32Array` and `Uint16Array` views on `HEAPU8.buffer` at those pointers
4. Writes vertex positions, normals, UVs directly into WASM linear memory
5. Writes triangle indices with CW winding
6. Creates a single `SlopObject` referencing the generated mesh
7. Returns a `SlopTileGrid` wrapper with reactive `.position`, `.rotation`, `.scale`, `.style`, `.hills`

### Resource Usage Comparison

| Approach | Objects | Vertices | Triangles |
|----------|---------|----------|-----------|
| Individual tiles (10×10) | 101 (100 tiles + 1 root) | N/A (reuses tile mesh) | N/A |
| **Connected mesh (10×10)** | **1** | **121** | **200** |

The connected mesh approach uses a single object slot (vs 101) and generates minimal geometry.

### Noise Function

Simple seeded integer hash with bilinear interpolation:

```javascript
_hash(x, z, seed) {
    let h = (seed * 374761393 + x * 668265263 + z * 1274126177) | 0;
    h = ((h ^ (h >> 13)) * 1103515245) | 0;
    return ((h ^ (h >> 16)) & 0x7fffffff) / 0x7fffffff;
}
```

Hash values at integer grid coordinates are bilinearly interpolated for smooth terrain. The `0.3` frequency multiplier in `_applyHills` controls hill size — lower values produce broader, gentler hills.

---

## Decisions Log

| # | Decision | Choice | Reasoning |
|---|----------|--------|-----------|
| 1 | Ground tiling syntax | Declarative DSL command | Manual `for` loops are verbose and un-slop-like |
| 2 | Hills API style | `ground.hills = seed, height` (tuple) | Compact, leverages existing tuple assignment syntax |
| 3 | Mesh approach | Generate single connected mesh | Shared vertices for smooth terrain, 1 object instead of 100+ |
| 4 | Mesh generation method | Direct WASM memory writes | `s3d_mesh_create` + pointer-based writes already exported |
| 5 | Command name | `terrain:` (renamed from `tile:`) | No longer tiles an existing mesh — generates its own |
| 6 | Command signature | `terrain: texture, cols, rows [, x, y, z]` | Mesh arg unnecessary since it's procedurally generated |
| 7 | OBJ winding fix | Swap i1/i2 in fan triangulation | Converts OBJ CCW convention to engine CW at parse time |
| 8 | External game support | No changes needed | IDE already resolves assets relative to opened `.slop` file |

---

## Files Changed

### Modified
- **`js/slop3d.js`** — Added `SlopTileGrid` class, `terrain()` runtime method, cwraps for `s3d_mesh_create`/`s3d_mesh_get_vertex_ptr`/`s3d_mesh_get_index_ptr`, `TerrainAssign` parser + codegen, updated `kill()` for tile grids
- **`src/slop3d.c`** — Swapped i1/i2 in OBJ fan-triangulation to fix CCW→CW winding
- **`tests/test_slopscript.js`** — Added parser, codegen, and runtime tests for `terrain:` command
- **`CLAUDE.md`** — Documented `terrain:` command and `.hills` property
- **`web/grave_game.slop`** — Updated to use `terrain:` syntax (later moved outside repo)

### Not Modified (But Relevant)
- **`src/slop3d.h`** — No changes needed; `s3d_mesh_create`, `s3d_mesh_get_vertex_ptr`, `s3d_mesh_get_index_ptr` already declared
- **`Makefile`** — Already exported the mesh creation functions; user added `ide` target
- **`ide/main.js`** — Already supported external `.slop` file asset resolution

---

## Next Steps

- Add more objects to the graveyard scene (gravestones, fences, fog)
- Consider a `scatter:` command for randomly placing objects across terrain (trees, rocks)
- Explore terrain texture blending (e.g., grass → dirt transitions)
- The terrain mesh generation opens the door for other procedural primitives (walls, paths, water planes)
