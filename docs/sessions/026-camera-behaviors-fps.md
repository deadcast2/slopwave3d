# Session 026: Camera Behaviors — FPS Camera

**Date:** March 14, 2026
**Participants:** Caleb Cohoon + Claude (Opus 4.6)
**Duration:** Single session
**Repo:** [deadcast2/slopwave3d](https://github.com/deadcast2/slopwave3d)

---

## Session Goal

Design and implement a camera behavior system for SlopScript that lets users create pre-configured camera types (starting with FPS) without writing boilerplate input-handling code. The system should be extensible for future behaviors like third-person and orbit cameras.

---

## Conversation Summary

### 1. Problem Statement

Caleb identified that creating an interactive camera in SlopScript requires significant boilerplate — manually reading keyboard/mouse input, computing yaw/pitch, deriving forward/right vectors, and updating position/target every frame. He wanted a way to declare a camera's "behavior" at creation time so the engine handles the input logic automatically.

### 2. Codebase Exploration (Plan Mode)

An exploration agent traced the full camera lifecycle end-to-end:

- **SlopScript syntax** — `cam = camera: px, py, pz` parsed into `CameraAssign` AST nodes
- **Transpiler** — codegen emits `_rt.camera(args)` calls
- **SlopRuntime** — `camera()` method creates `SlopCamera` with reactive `.position`/`.target` properties that flush to C via `SlopVec3` onChange callbacks
- **C engine** — `s3d_camera_create()` allocates a slot, `update_camera()` recomputes lookat/perspective/VP matrices
- **Input system** — keyboard tracking via `_keys` array (keydown/keyup on document), mouse position tracking on canvas, `key_down[]` built-in available in SlopScript
- **Render loop** — 30 FPS capped, calls user `update` block then `s3d_render_scene()`

Key finding: the existing architecture already has JS updating camera position/target reactively, with changes flowing through to C. A behavior system just needs to automate what users would write manually in the `update` block.

### 3. Syntax Design Decision

Three syntax options were proposed:

| Option | Example | Pros/Cons |
|--------|---------|-----------|
| Behavior first | `cam = camera: fps, 0, 1.5, 5` | Mirrors light type pattern, unambiguous (ident vs number) |
| Behavior last | `cam = camera: 0, 1.5, 5, fps` | Buried at end, ambiguous with 6-arg target variant |
| Separator | `cam = camera: 0, 1.5, 5 \| fps` | Introduces new operator not used elsewhere |

**Decision: Behavior as first argument.** The parser distinguishes behavior (identifier token) from position args (numeric expressions). This is clean and reads naturally. Caleb confirmed this choice.

### 4. Implementation Layer Decision

Three options for where the FPS logic should live:

- **JS runtime only** — behavior updates camera position/target from JS, flushes to C
- **Partially in C** — add euler angles to camera struct
- **SlopScript macro expansion** — transpile behavior into user code

**Decision: JS runtime only.** The C engine is a dumb rasterizer that knows position + target. FPS behavior is high-level game logic that derives position/target from input — it belongs in JS. This keeps the C/JS boundary clean and means new behaviors don't require WASM recompilation. Caleb confirmed this choice.

### 5. Update Timing Decision

Should the behavior update run before or after the user's `update` block?

**Decision: Before update.** This lets users read the behavior-computed position and override it (e.g., clamping Y height). If behavior ran after, users couldn't easily constrain movement.

### 6. Implementation

All changes were made in `js/slop3d.js` (plus tests, demo, and docs):

**SlopCamera class** — Added `_behavior`, `_yaw`, `_pitch`, `_speed` (default 5), `_sensitivity` (default 0.15) fields with `speed`/`sensitivity` getters/setters for SlopScript access.

**Parser** — After eating `camera:`, the parser checks if the next token is an IDENT in the `CAMERA_BEHAVIORS` set (`fps`). If so, it consumes it as the behavior and eats the following comma before parsing numeric position args. The `CameraAssign` AST node gained an optional `behavior` field.

**Codegen** — If `node.behavior` is set, emits `_rt.camera(args, 'behavior')` with the behavior string as a trailing argument. Otherwise emits the existing call without it.

**Runtime** — `camera()` detects a trailing string argument as the behavior name. After creating the camera, calls `_attachBehavior(cam, behavior)` which:
- Computes initial yaw/pitch from the position-to-target vector
- Calls `_setupPointerLock()` to register click-to-capture on the canvas
- Adds the camera to `_behaviorCameras` array

**FPS update logic** (`_updateFPS`) — Per-frame:
1. Mouse look: accumulate `movementX`/`movementY` deltas when pointer is locked, apply to yaw/pitch (pitch clamped to ±89°)
2. WASD movement: compute forward/right vectors from yaw, move position relative to facing direction
3. Space/Shift: vertical movement
4. Normalize horizontal movement vector to prevent diagonal speed boost
5. Derive target from position + yaw/pitch
6. Flush position and target to C engine
7. Reset mouse deltas

**Pointer lock** — Click on canvas triggers `requestPointerLock()`. Mouse deltas are accumulated from `movementX`/`movementY` in the mousemove handler (only when locked). This is era-authentic — Shockwave games required clicking to activate input.

**Render loop** — `_updateBehaviors()` call inserted before `scene.update()` so behaviors run first and users can override.

**Scene cleanup** — `gotoScene()` clears `_behaviorCameras` array alongside existing camera destruction.

### 7. IDE Focus Fix

After initial implementation, Caleb reported that clicking the preview canvas in the Electron IDE to activate the FPS camera caused WASD keypresses to be inserted into the Monaco editor source code.

**Root cause:** Pointer lock captures the mouse but doesn't change keyboard focus. Monaco's hidden textarea retained focus and processed keydown events as text input.

**Fix (two-pronged):**
1. **`preventDefault()` on keydown/keyup** when pointer lock is active — stops key events from reaching Monaco's input handler
2. **Focus the canvas on pointer lock** — set `tabindex="0"` on canvas, call `canvas.focus()` in the `pointerlockchange` handler to pull focus away from Monaco

### 8. Control Inversion

After testing, Caleb manually inverted the yaw direction (changed `+=` to `-=` on `_mouseDeltaX`) and swapped the A/D strafe directions to match expected FPS conventions. These changes were applied directly by the user via the formatter/linter.

---

## Decisions Log

| # | Decision | Choice | Reasoning |
|---|----------|--------|-----------|
| 1 | Syntax for camera behaviors | Behavior as first arg: `camera: fps, 0, 1.5, 5` | Mirrors light type pattern, unambiguous ident-vs-number detection, reads naturally |
| 2 | Implementation layer | JS runtime only, no C changes | Camera behavior is game logic, not rendering. Keeps C/JS boundary clean. No WASM recompile for new behaviors |
| 3 | Update timing | Behavior runs before user's `update` block | Lets users override/constrain behavior-computed positions |
| 4 | Mouse input method | Pointer Lock API with click-to-capture | Era-authentic (Shockwave required click to activate), standard browser API for FPS controls |
| 5 | IDE focus handling | preventDefault + canvas.focus() on pointer lock | Two-pronged approach prevents keystrokes from reaching Monaco editor |

---

## Files Changed

| File | Change |
|------|--------|
| `js/slop3d.js` | Added camera behavior system: SlopCamera fields (`_behavior`, `_yaw`, `_pitch`, `_speed`, `_sensitivity`), pointer lock setup, mouse delta tracking, `_attachBehavior()`, `_updateBehaviors()`, `_updateFPS()`, `preventDefault` on keys during pointer lock, canvas focus management. Modified parser to detect behavior identifiers, codegen to pass behavior string, `camera()` to accept trailing behavior arg, render loop to call `_updateBehaviors()`, `gotoScene()` to clear behavior cameras |
| `tests/test_slopscript.js` | Added 3 tests: parser with fps behavior, parser without behavior (null check), codegen with fps behavior string |
| `web/index.html` | Updated demo scene to use `camera: fps, 0, 1.5, 5` instead of manual camera orbit |
| `CLAUDE.md` | Added `cam = camera: fps, px, py, pz` syntax documentation with `.speed` and `.sensitivity` properties |

---

## Next Steps

- Add `orbit` camera behavior (orbits around a target point with mouse drag)
- Add `third_person` camera behavior (follows a spawned object with offset)
- Consider exposing `cam.yaw` and `cam.pitch` as readable properties in SlopScript for advanced use cases
- Test FPS camera with actual 3D scenes containing floors/walls for more realistic feel
