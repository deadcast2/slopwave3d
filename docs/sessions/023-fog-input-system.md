# Session 023: Fog, Input System & SlopScript.exec Refactor

**Date:** March 13, 2026
**Participants:** Caleb Cohoon + Claude (Opus 4.6)
**Duration:** Implementation session
**Repo:** [deadcast2/slopwave3d](https://github.com/deadcast2/slopwave3d)
**Issue:** [#7 — Fog, Alpha Blending + Input](https://github.com/deadcast2/slopwave3d/issues/7)

---

## Session Goal

Implement the remaining features from GitHub issue #7: linear fog, input handling (keyboard + mouse), and alpha blending. Alpha blending was discovered to already be complete, so the session focused on fog and input, plus fixing an IDE integration bug and refactoring to prevent future occurrences.

---

## Conversation Summary

### 1. Issue Analysis & Codebase Exploration

We began by reading GitHub issue #7 which specified three features: linear fog, alpha blending, and an input system. An Explore agent was launched to audit the full codebase for existing implementations.

**Key finding:** Alpha blending was already fully implemented across all layers — C rasterizer (`src/slop3d.c:470-489`), WASM export (`s3d_object_alpha`), JS wrapper (`setObjectAlpha`), and SlopScript (`box.alpha = 0.5`). This included proper opaque-first/transparent-back-to-front sorting in the render pipeline. No work was needed here.

Fog and input had zero existing code.

### 2. Planning Phase

A detailed plan was created covering:
- **Fog:** New `S3D_Fog` struct, `eye_z` field threaded through the rasterizer for per-pixel distance, linear fog blend formula, WASM export, JS/SlopScript wrappers
- **Input:** JS-only key/mouse state tracking with document-level event listeners, coordinate mapping to 320x240, SlopScript builtins for `key_down[]`, `mouse_x`, `mouse_y`

The plan was approved and implementation began.

### 3. Fog Implementation — C Engine

The fog system required changes throughout the rendering pipeline:

1. **`S3D_Fog` struct** added to `src/slop3d.h` with `enabled`, `r`, `g`, `b`, `start`, `end` fields
2. **`eye_z` field** added to `S3D_ScreenVert` — this carries the eye-space Z distance (from `clip.w`) through to the rasterizer for per-pixel fog calculation
3. **`ndc_to_screen()`** gained a 9th parameter (`eye_z`) to pass through the view-space depth
4. **`draw_object_internal()`** updated to pass `clip.w` (which equals eye-space Z in a standard perspective projection) as the `eye_z` argument
5. **Rasterizer scanline loop** — `eye_z` was interpolated alongside all other vertex attributes (z, u, v, r, g, b) through the long edge, short edge, left-right swap, per-scanline step, and per-pixel loop
6. **Per-pixel fog blend** inserted after texture sampling but before alpha blending:
   ```c
   fog_t = clamp((eye_z - start) / (end - start), 0, 1)
   color = color * (1 - fog_t) + fog_color * fog_t
   ```
7. **`s3d_fog_set()` WASM export** added with `EMSCRIPTEN_KEEPALIVE`
8. **Makefile** updated to include `s3d_fog_set` (and previously missing light functions) in `EXPORTED_FUNCTIONS`

### 4. Fog Implementation — JS & SlopScript

- JS API: `_fogSet` cwrap added, `setFog(enabled, r, g, b, start, end)` method on `Slop3D` class
- SlopScript KEYWORDS: `'fog'` added
- SlopScript parser: `'fog'` added to colon-statement dispatch list
- SlopScript codegen: `fog` case emits `_rt.fog(args)`
- SlopRuntime: `fog(r, g, b, start, end)` method calls `_e.setFog(true, ...)`
- SlopScript syntax: `fog: r, g, b, start, end`

### 5. Input System Implementation

The input system is JS-only (no C code needed — WASM has no system access).

**Slop3D class additions:**
- `_keys` — `Uint8Array(256)` tracking key-down state by keyCode
- `_mouseX`, `_mouseY`, `_mouseButtons` — mouse state
- `document.addEventListener('keydown'/'keyup')` — document-level listeners (not canvas-level, for Electron compatibility)
- `canvas.addEventListener('mousemove'/'mousedown'/'mouseup')` — mouse listeners with coordinate mapping to `this.width`/`this.height` (320x240)
- Public API: `isKeyDown(keyCode)`, `mouseX()`, `mouseY()`, `mouseButton(n)`

**SlopScript integration:**
- `SLOP_BUILTINS` extended with `'mouse_x'`, `'mouse_y'`, `'mouse_left'`, `'mouse_right'` — these emit as `_rt.mouse_x` etc. via the existing builtins codegen path
- `SLOP_MATH` extended with `'key_down'` — but with special codegen that stringifies identifier arguments (so `key_down[w]` emits `_key_down('w')` not `_key_down(_s.w)`)
- `_KEY_MAP` object mapping key names to keyCodes: arrows, letters a-z, digits 0-9, space, enter, escape, shift, ctrl, alt
- `_key_down(name)` runtime helper function that looks up the keyCode from `_KEY_MAP` and queries `engine.isKeyDown()`
- `_key_down_engine` module-level variable set during `SlopScript.exec()` to give the helper access to the active engine
- `SlopRuntime` getters: `mouse_x`, `mouse_y` (from `_e._mouseX`/`_e._mouseY`), `mouse_left`/`mouse_right` (from `_e.mouseButton()`)

### 6. Test Fixes

The C test file `tests/test_math.c` required updates for the new `eye_z` parameter:
- `ndc_to_screen()` calls gained a 9th argument (`1.0f`)
- `S3D_ScreenVert` struct initializers gained a trailing `0` for the new field

### 7. FPS Camera Example

A sample FPS camera in SlopScript was provided using WASD movement relative to a `yaw` angle, with arrow keys for rotation and `sin[]/cos[]` for direction vectors.

### 8. IDE Input Bug — Electron Key Events

When testing in the IDE, keyboard input didn't work. Investigation revealed the IDE's `renderer.js` had its own copy of the `new Function(...)` construction that was missing the new `_key_down` parameter. This duplicate code at `ide/renderer/renderer.js:152-161` was out of sync with `js/slop3d.js`.

**First fix:** Added `_key_down` to the IDE's function parameter list.

**Still broken:** Moving keyboard listeners from `this.canvas` to `document` (removing the need for canvas focus) was the first attempted fix, but didn't solve it since the root cause was the missing function parameter.

### 9. SlopScript.exec() Refactor

Caleb identified the root cause as a maintenance hazard — the function-construction logic was duplicated between `SlopScript.run()` in `js/slop3d.js` and `runScene()` in `ide/renderer/renderer.js`. Any new runtime helper or builtin would need to be added in both places.

**Solution:** Extracted `SlopScript.exec(js, engine)` as a new static method on the `SlopScript` class. This method owns the `new Function(...)` construction, runtime wiring, and `_key_down_engine` setup. Both `SlopScript.run()` and the IDE's `runScene()` now delegate to it:

```javascript
// js/slop3d.js — single source of truth
static async exec(js, engine) {
    const fn = new Function('_e', '_rt', ..., '_key_down',
        'return (async function(){' + js + '})();');
    const rt = new SlopRuntime(engine);
    _key_down_engine = engine;
    await fn(engine, rt, ..., _key_down);
}

// ide/renderer/renderer.js — now just 3 lines
const engine = new Slop3D('game-canvas');
await engine.init();
await SlopScript.exec(js, engine);
```

### 10. Issue Closure

GitHub issue #7 was closed with a summary of all implemented features.

---

## Decisions Log

| # | Decision | Choice | Reasoning |
|---|----------|--------|-----------|
| 1 | Fog distance metric | `clip.w` (eye-space Z) | Standard perspective projection stores view-space Z in clip.w; avoids computing a separate view-space transform |
| 2 | Fog interpolation | Per-pixel via scanline rasterizer | Matches the engine's existing per-pixel approach for z, u, v, r, g, b; authentic Shockwave-era look |
| 3 | Keyboard event target | `document` (not canvas) | Canvas-level listeners require focus, which Electron's IDE shell can intercept; document-level works universally |
| 4 | No `preventDefault()` on key events | Omitted | Avoids blocking Electron menu shortcuts and normal browser behavior |
| 5 | Input system in JS only | No C code | WASM has no system access; input is inherently a browser/JS concern |
| 6 | `key_down` syntax | `key_down[w]` with string coercion | SlopScript has no string literals; special codegen converts identifier args to strings (`'w'`) for the key name lookup |
| 7 | Key name mapping | Human-readable names (`left`, `space`, `w`) | More ergonomic than raw keyCodes; matches the DSL's simplicity goals |
| 8 | `SlopScript.exec()` extraction | New static method | Eliminates duplicated function-construction code between browser loader and IDE; single point of change for new builtins |
| 9 | Skip alpha blending work | Already complete | Full implementation existed across C rasterizer, JS API, and SlopScript with proper transparency sorting |

---

## Files Changed

| File | Changes |
|------|---------|
| `src/slop3d.h` | Added `S3D_Fog` struct, `s3d_fog_set()` declaration |
| `src/slop3d.c` | Added `S3D_Fog fog` to engine struct, `eye_z` to `S3D_ScreenVert`, fog interpolation in rasterizer, `s3d_fog_set()` WASM export, `eye_z` parameter to `ndc_to_screen()` |
| `js/slop3d.js` | Added fog cwrap/wrapper, input event listeners + state + API methods, `_KEY_MAP` + `_key_down()` helper, SlopScript fog/input keywords/parser/codegen/runtime, `SlopScript.exec()` method |
| `ide/renderer/renderer.js` | Replaced duplicated function-construction with `SlopScript.exec(js, engine)` call |
| `tests/test_math.c` | Updated `ndc_to_screen()` calls and `S3D_ScreenVert` initializers for new `eye_z` field |
| `Makefile` | Added `s3d_fog_set`, `s3d_light_ambient`, `s3d_light_directional`, `s3d_light_point`, `s3d_light_spot`, `s3d_light_off` to `EXPORTED_FUNCTIONS` |

---

## Next Steps

- Remaining engine issues: #1-#6, #8 (core), #13-#18 (SlopScript)
- Test fog and input visually in the IDE with the FPS camera example
- Consider adding mouse pointer lock for proper FPS camera look-around
- Consider SlopScript syntax for disabling fog (`fog: off` or similar)
