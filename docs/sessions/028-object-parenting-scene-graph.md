# Session 028: Object Parenting (Scene Graph Hierarchy)

**Date:** March 14, 2026
**Participants:** Caleb Cohoon + Claude (Opus 4.6)
**Duration:** Implementation session
**Repo:** [deadcast2/slopwave3d](https://github.com/deadcast2/slopwave3d)

---

## Session Goal

Implement GitHub Issue #12: add parent-child relationships between objects so transforms propagate down the hierarchy. This enables compound objects like turrets on vehicles, items attached to characters, or articulated limbs -- common patterns in Shockwave 3D games.

---

## Conversation Summary

### 1. Issue Review and Exploration

We started by reading GitHub Issue #12, which specified adding a `parent_id` field to `S3D_Object`, a `s3d_object_parent()` API function, recursive world matrix computation with per-frame caching, and updates to the render pipeline to use world matrices instead of local model matrices.

Two exploration agents were launched in parallel:

**Agent 1 (C engine)** traced the object system and transform pipeline:
- `S3D_Object` struct (`slop3d.h:101-112`) had `active, mesh_id, texture_id, position, rotation, scale, color, alpha, texmap, model` -- no parenting fields
- `rebuild_model_matrix()` (`slop3d.c:1004-1011`) computed `model = T * Ry * Rx * Rz * S` and was called by position/rotation/scale setters
- `s3d_render_scene()` (`slop3d.c:1147-1201`) used `obj->model` directly for MVP, lighting, and transparent sort distance
- `s3d_object_destroy()` (`slop3d.c:1035-1038`) simply set `active = 0`
- `m4_multiply()` existed for column-major matrix multiplication

**Agent 2 (JS/WASM bridge)** traced the SlopScript spawn/kill pipeline:
- `SlopObject` class used reactive `SlopVec3` properties with callback-driven WASM updates
- `spawn()` called `_objectCreate()` WASM function and returned a `SlopObject` wrapper
- `kill()` called `_objectDestroy()` and removed from `_sceneObjects` array
- No existing parenting or hierarchy code existed anywhere

### 2. Design Planning

A Plan agent designed the implementation approach, addressing four key design questions:

1. **DSL syntax**: Property style (`obj.parent = other`) was chosen over call style (`attach: obj, other`) because it follows the existing reactive property pattern used by position, rotation, scale, color, alpha, and style.

2. **Dirty flag strategy**: Rebuild all world matrices once per frame at the start of `s3d_render_scene()` using recursive computation with frame-stamp memoization. With max 256 objects this is trivially cheap and avoids the complexity of propagating dirty flags through chains.

3. **World matrix storage**: Add a `world` field (`S3D_Mat4`) alongside `model`. The `model` matrix remains the local transform; `world` is computed per frame.

4. **Destroy behavior**: When a parent is destroyed, children are unparented with local transforms unchanged (no world-position baking). Decomposing a matrix back into TRS is fragile with non-uniform scale.

### 3. DSL Syntax: `turret.dad = tank` / `turret.mom = tank`

Caleb requested a fun twist on the typical "parent" naming. Instead of `.parent`, the DSL uses `.dad` and `.mom` as interchangeable aliases. Users pick whichever they prefer -- both call the same underlying `_objectParent` WASM function.

To unparent, a new constant `none` was introduced (transpiles to `null`), following the `SLOP_CONSTANTS` pattern established in session 027 for `ps1`, `n64`, and `fps`:

```
turret.dad = tank           // parent turret to tank
turret.mom = tank           // same thing, just an alias
turret.dad = none           // unparent
```

The key insight was that **no parser or codegen changes were needed** for the property syntax. The existing assignment codegen already handles dotted property paths -- `turret.dad = tank` emits `_s.turret.dad = _s.tank;`, and the JS setter fires the WASM call. Only `none` required a one-line addition to `SLOP_CONSTANTS`.

### 4. C Engine Implementation

**Header (`slop3d.h`):** Added three fields to `S3D_Object`:
- `int parent_id` (-1 = no parent)
- `uint32_t world_frame` (memoization frame stamp)
- `S3D_Mat4 world` (computed world matrix)

Added `s3d_object_parent()` declaration.

**Object create (`slop3d.c`):** Initialized `parent_id = -1`, `world_frame = 0`, and `world = model` in `s3d_object_create()`.

**Object destroy (`slop3d.c`):** Before setting `active = 0`, `s3d_object_destroy()` now iterates all objects and sets `parent_id = -1` on any children of the destroyed object.

**Parent function (`slop3d.c`):** New `s3d_object_parent(object_id, parent_id)` validates both IDs, rejects self-parenting, and walks up the chain from `parent_id` to detect cycles before setting the parent.

**World matrix computation (`slop3d.c`):** Two new static functions:
- `get_world_matrix(id)`: Recursively computes `world = parent.world * local.model`. Uses a global `g_frame_counter` for memoization -- if `obj->world_frame == g_frame_counter`, returns cached `world` immediately. If a parent has been deactivated, auto-unparents the child.
- `rebuild_world_matrices()`: Increments `g_frame_counter` and calls `get_world_matrix()` for each active object.

**Render scene (`slop3d.c`):** Three changes:
1. Calls `rebuild_world_matrices()` after camera check, before sorting
2. Transparent sort distance uses `obj->world.m[12/13/14]` instead of `obj->model`
3. Both render calls (opaque and transparent) use `obj->world` for MVP and lighting matrices

### 5. JS Runtime Changes

**WASM binding:** Added `_objectParent` cwrap for `s3d_object_parent`.

**SlopObject class:** Added `_parent = null` backing field and two getter/setter pairs:
- `.dad` getter returns `_parent`; setter sets `_parent` and calls `_objectParent(id, v ? v._id : -1)`
- `.mom` getter returns `_parent`; setter delegates to `.dad` setter

**kill() method:** Before destroying an object, iterates `_sceneObjects` and clears `_parent` on any JS-side children whose `_parent === target`.

**SLOP_CONSTANTS:** Added `none: 'null'`.

### 6. Makefile

Added `_s3d_object_parent` to `EXPORTED_FUNCTIONS`.

### 7. Testing

All tests passed throughout development:
- **122 C tests** (111 existing + 11 new parenting tests)
- **76 JS tests** (73 existing + 3 new codegen tests)

New C tests:
- `parent_world_matrix`: Parent at x=10, child at x=5, verify child world x=15
- `parent_chain_3_deep`: Three objects in chain, verify cumulative translation
- `parent_self_rejected`: Self-parenting silently rejected
- `parent_cycle_rejected`: A->B, then B->A rejected (would create cycle)
- `parent_destroy_unparents_children`: Destroying parent sets child's parent_id to -1
- `parent_unparent`: Setting parent_id to -1 removes parenting

New JS tests:
- `turret.dad = tank` transpiles to `_s.turret.dad = _s.tank;`
- `turret.mom = tank` transpiles to `_s.turret.mom = _s.tank;`
- `turret.dad = none` transpiles to `_s.turret.dad = null;`

### 8. Documentation and Issue Closure

Updated `CLAUDE.md` to document:
- `.dad`/`.mom` reactive properties on objects
- `none` constant in the built-ins section
- World matrix rebuild step in the rendering pipeline

GitHub Issue #12 was closed.

---

## Research Findings

### Scene Graph Hierarchies in Software Rasterizers

The implementation follows standard scene graph conventions:
- **World matrix = parent.world * child.local**: Transforms accumulate multiplicatively up the chain
- **Column-major matrices (OpenGL convention)**: Translation is stored in `m[12], m[13], m[14]`
- **Memoized recursive computation**: A frame stamp avoids redundant matrix multiplications when multiple children share a parent or when chains are deep

### Cycle Detection

Parent chains form a DAG (directed acyclic graph). Cycles would cause infinite recursion in `get_world_matrix()`. The implementation prevents this by walking up from `parent_id` before accepting it -- if the walk reaches `object_id`, the assignment is silently rejected.

---

## Research Sources

- No external research was required for this session. Scene graph hierarchy is a well-established pattern in 3D engines.
- [Session 001](001-engine-design-and-research.md) -- original Shockwave 3D research confirming compound objects were a common pattern
- [Session 027](027-perspective-correct-texturing-dsl-constants.md) -- established the `SLOP_CONSTANTS` pattern reused here for `none`

---

## Decisions Log

| # | Decision | Choice | Reasoning |
|---|----------|--------|-----------|
| 1 | DSL syntax for parenting | `.dad`/`.mom` reactive properties | Follows existing property pattern; fun alternative to typical "parent" naming |
| 2 | Dirty flag vs per-frame rebuild | Per-frame rebuild with memoization | Simpler than dirty flag propagation; 256 objects makes cost negligible |
| 3 | World matrix storage | Separate `world` field alongside `model` | `model` stays as local transform; `world` computed per frame |
| 4 | Destroy behavior | Unparent children, keep local transforms | Baking world position requires matrix decomposition, fragile with non-uniform scale |
| 5 | Cycle detection | Walk up parent chain before accepting | Simple, O(depth) check; prevents infinite recursion in world matrix computation |
| 6 | Unparent syntax | `turret.dad = none` | `none` as DSL constant maps to `null`; extends existing `SLOP_CONSTANTS` pattern |
| 7 | `.dad`/`.mom` implementation | `.dad` has full logic, `.mom` delegates | Avoids duplicating the WASM call; both return same `_parent` backing field |

---

## Files Changed

| File | Changes |
|------|---------|
| `src/slop3d.h` | Added `parent_id`, `world_frame`, `world` fields to `S3D_Object`; added `s3d_object_parent()` declaration |
| `src/slop3d.c` | Added `g_frame_counter`, `get_world_matrix()`, `rebuild_world_matrices()`, `s3d_object_parent()`; updated `s3d_object_create()` init, `s3d_object_destroy()` to unparent children, `s3d_render_scene()` to use world matrices |
| `js/slop3d.js` | Added `_objectParent` cwrap, `_parent` field and `.dad`/`.mom` getter/setters on `SlopObject`, child cleanup in `kill()`, `none: 'null'` in `SLOP_CONSTANTS` |
| `Makefile` | Added `_s3d_object_parent` to `EXPORTED_FUNCTIONS` |
| `tests/test_math.c` | Added 6 parenting tests: world matrix, 3-deep chain, self-parent rejection, cycle rejection, destroy unparents, unparent |
| `tests/test_slopscript.js` | Added 3 codegen tests: dad assignment, mom assignment, none-as-null unparenting |
| `CLAUDE.md` | Documented `.dad`/`.mom` properties, `none` constant, world matrix step in render pipeline |

---

## Next Steps

- Visual testing in browser: create parent-child object hierarchies and verify transforms propagate correctly
- Test with rotation inheritance (parent rotation should orbit children)
- Consider adding a `children` getter on `SlopObject` for introspection
- Continue with remaining GitHub issues for engine features
