# Session 020: Multi-Camera System

**Date:** March 12, 2026
**Participants:** Caleb Cohoon + Claude (Opus 4.6)
**Duration:** Implementation session
**Repo:** [deadcast2/slopwave3d](https://github.com/deadcast2/slopwave3d)

---

## Session Goal

Replace the engine's single implicit camera with a multi-camera system where cameras are spawnable first-class objects via SlopScript. No default camera — users must explicitly create one before anything renders.

---

## Conversation Summary

### 1. Initial Request & Exploration

Caleb requested multiple cameras in a scene, spawnable like other objects via the DSL, with no default/implicit camera. Two exploration agents were launched in parallel:

- **Agent 1** explored the C engine's camera implementation — found a single `S3D_Camera camera` embedded in `S3D_Engine` (not an array like objects/lights), three public API functions (`s3d_camera_set`, `s3d_camera_fov`, `s3d_camera_clip`), and the camera referenced directly in `s3d_render_scene()`.

- **Agent 2** explored the JS API and SlopScript transpiler — found `SlopCamera` as a reactive wrapper class, `camera` as a singleton on `SlopRuntime`, `camera` hardcoded as a `SLOP_BUILTINS` entry mapping to `_rt.camera` in codegen, and documented the full spawn/light creation patterns to follow.

### 2. Design Phase

A Plan agent designed the implementation following the existing lights pattern (fixed-size array with find-free-slot allocation). Key design questions surfaced:

1. **Auto-activation:** Should the first camera auto-activate, or require explicit `use:` every time?
2. **Creation syntax:** How many args? Full 6-arg (pos + target), 3-arg (pos only), or 0-arg (all defaults)?
3. **Switching keyword:** `use:`, `activate:`, or `view:`?

### 3. User Design Decisions

Caleb chose:
- **First camera auto-activates** — keeps simple single-camera scripts simple
- **`cam = camera: px, py, pz`** — 3 args for position, target defaults to origin. Optional 6-arg form for explicit target.
- **`use: cam`** — clean imperative keyword consistent with `kill:`/`off:`/`on:` pattern

### 4. Implementation

Implementation proceeded layer by layer:

**C Engine (`slop3d.h` + `slop3d.c`):**
- Added `S3D_MAX_CAMERAS 8` define and `int active` field to `S3D_Camera` struct
- Replaced `S3D_Camera camera` with `S3D_Camera cameras[S3D_MAX_CAMERAS]` + `int active_camera` (-1 = none)
- Removed default camera init from `s3d_init()` (was setting pos 0,0,5 and fov 60)
- Changed `update_camera(void)` to `update_camera(int id)`
- Replaced 3 old functions with 9 new ones: `s3d_camera_create`, `s3d_camera_destroy`, `s3d_camera_pos`, `s3d_camera_target`, `s3d_camera_set_fov`, `s3d_camera_set_clip`, `s3d_camera_activate`, `s3d_camera_off`, `s3d_camera_get_active`
- Updated `s3d_render_scene()` to early-return when `active_camera < 0`
- Split position and target into separate C functions (matching how `SlopObject` has separate position/rotation calls)

**JS API (`slop3d.js` — Slop3D class):**
- Replaced old cwrap bindings (`_cameraSet`, `_cameraFov`, `_cameraClip`) with 9 new ones
- Replaced `setCamera`/`setCameraFov`/`setCameraClip` with `createCamera`, `destroyCamera`, `setCameraPos`, `setCameraTarget`, `setCameraFov`, `setCameraClip`, `setCameraActive`, `setCameraOff`

**SlopCamera class rewrite:**
- Now takes `(engine, id)` like `SlopLight`
- Separate `position` and `target` `SlopVec3` instances, each calling their own C function via callback
- Added `id` getter

**SlopRuntime changes:**
- Removed `this.camera` singleton from constructor
- Added `_sceneCameras` tracking array
- Added `camera(...args)` factory method — creates via C API, wraps in `SlopCamera`, auto-activates the first camera created
- Added `use(target)` method — calls `setCameraActive` for `SlopCamera` instances
- Updated `off()` — handles `SlopCamera` (deactivates without destroying)
- Updated `on()` — handles `SlopCamera` (re-activates)
- Updated `kill()` — handles `SlopCamera` (destroys slot and clears tracking)
- Updated `gotoScene()` — cleans up cameras alongside lights and objects

**SlopScript transpiler changes:**
- Added `camera` and `use` to `KEYWORDS` set
- Removed `camera` from `SLOP_BUILTINS`
- Added `CameraAssign` parser node (before `LightAssign` check in `parseAssignOrExpr`)
- Added `CameraAssign` codegen: emits `_rt.camera(args)`
- Added `use` to `CallStmt` parser recognition (alongside `goto`/`kill`/`off`/`on`)
- Added `use` to `CallStmt` codegen: emits `_rt.use(arg)`
- Removed `camera` → `_rt.camera` mapping from `emitExpr` Ident case

**Makefile:**
- Replaced 3 old exported functions with 9 new ones in `EXPORTED_FUNCTIONS`

### 5. Post-Implementation Review

After all tests passed (111 C + 65 JS), two issues were identified:

1. **Typo in cwrap binding:** `_cameraCameraOff` instead of `_cameraOff`. Fixed with a replace-all.

2. **Zero-arg camera creation:** `cam = camera:` with no args was silently allowed, producing a degenerate view matrix (eye == target at origin). Caleb agreed this should be an explicit error.

### 6. Argument Validation

Added explicit validation at both layers:
- **Parser:** throws `"camera: requires at least 3 args (px, py, pz) at line N"` if fewer than 3 args
- **Runtime:** throws `"camera() requires at least 3 args (px, py, pz)"` as a safety net
- Replaced `|| 0` fallback pattern with proper conditional defaults for the optional target args

---

## Decisions Log

| # | Decision | Choice | Reasoning |
|---|----------|--------|-----------|
| 1 | Camera storage | Fixed array of 8 (`S3D_MAX_CAMERAS`) | Follows lights/objects pattern, no dynamic allocation |
| 2 | No default camera | `active_camera = -1` on init | User requirement — explicit camera creation required |
| 3 | First camera auto-activates | Yes, in JS `camera()` factory | Convenience for the 90% single-camera case |
| 4 | Creation syntax | `cam = camera: px, py, pz` | 3 required args, optional 3 more for target |
| 5 | Camera switching | `use: cam` keyword | Consistent with `kill:`/`off:`/`on:` pattern |
| 6 | Separate pos/target C functions | `s3d_camera_pos` + `s3d_camera_target` | Matches how objects have separate position/rotation calls |
| 7 | Scene cleanup | Cameras destroyed on `gotoScene` | Consistent with lights/objects lifecycle |
| 8 | Minimum args enforced | Parse error if < 3 args | Prevents degenerate eye==target at origin |
| 9 | `on:` re-activates camera | Sets as active camera | Symmetric with `off:` which deactivates |

---

## Files Changed

| File | Change |
|------|--------|
| `src/slop3d.h` | Added `S3D_MAX_CAMERAS`, replaced 3 camera function declarations with 9 new ones |
| `src/slop3d.c` | Added `active` to camera struct, camera array + active index in engine, 9 new API functions, updated `s3d_init` and `s3d_render_scene` |
| `js/slop3d.js` | Rewrote camera cwraps/methods, rewrote `SlopCamera` class, rewrote `SlopRuntime` (camera factory, use, off/on/kill/gotoScene), added `CameraAssign` parser + codegen, added `use` CallStmt, updated KEYWORDS/BUILTINS |
| `web/index.html` | Updated demo SlopScript to use `cam = camera:` syntax |
| `ide/renderer/renderer.js` | Updated default SlopScript template |
| `ide/renderer/slopscript.js` | Removed `camera` from globals, added `camera`/`use` to keywords |
| `tests/test_math.c` | Rewrote camera tests for multi-camera API (8 tests), added camera setup to rasterizer tests |
| `tests/test_slopscript.js` | Updated camera codegen test, added `use:` test, updated full spinning cube test |
| `Makefile` | Replaced 3 old camera exports with 9 new ones |
| `CLAUDE.md` | Updated syntax overview, built-ins, Objects & Lights section, rendering pipeline description, code conventions |

---

## Next Steps

- Build WASM and test in browser to verify the full pipeline works end-to-end
- Consider adding a parser test for the `< 3 args` error case
- Multi-camera demo scene showing camera switching (e.g., security camera views, cutscene transitions)
