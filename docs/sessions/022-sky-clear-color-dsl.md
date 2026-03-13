# Session 022: Sky Clear Color in SlopScript

**Date:** March 12, 2026
**Participants:** Caleb Cohoon + Claude (Opus 4.6)
**Duration:** Short feature session
**Repo:** [deadcast2/slopwave3d](https://github.com/deadcast2/slopwave3d)

---

## Session Goal

Add a way to set the background clear color from SlopScript. Determine the right syntax and where the feature belongs in the DSL's design.

---

## Conversation Summary

### 1. Design Discussion — Camera Property vs. Scene Statement

Caleb asked whether the clear color should be a camera property or something else. After reviewing the existing engine code, we found that the C engine already exposes `s3d_clear_color(r, g, b, a)` and the JS wrapper has `setClearColor(r, g, b, a)` — the infrastructure was already in place, it just needed a SlopScript surface.

We recommended making it a **scene-level statement** rather than a camera property, reasoning that:

- In Shockwave 3D, the background color was a stage-level property, not per-camera
- Most engines treat clear color as a global/scene property
- It follows the existing pattern of scene-level statements like lights

The initial proposal was `background: r, g, b`.

### 2. Naming Decision — `sky:` over `background:`

Caleb chose `sky:` over `background:` for two reasons:

1. **Brevity** — shorter keyword, fits the DSL's terse style
2. **Future extensibility** — Caleb plans to eventually support passing a JPEG or array of JPEGs to create a skybox/cubemap (e.g., `sky: sunset.jpg`), making `sky` the more semantically appropriate name

### 3. Implementation

The feature was implemented as a `CallStmt` (the same AST node type used by `goto:`, `kill:`, `off:`, `on:`, `use:`), which keeps the parser changes minimal and consistent.

**Changes made:**

1. **Keyword registration** — added `'sky'` to the `KEYWORDS` set
2. **Parser** — added `'sky'` to the colon-style statement call check (alongside `goto`, `kill`, `off`, `on`, `use`)
3. **Code generation** — added a `sky` case in the `CallStmt` handler that emits `_rt.sky(r, g, b)`
4. **Runtime** — added `sky(r, g, b)` method to `SlopScriptRuntime` that converts 0–1 float values to 0–255 integers and calls `this._e.setClearColor()`
5. **Tests** — added parser test (verifies `CallStmt` AST node) and codegen test (verifies emitted JS)
6. **Documentation** — added `sky:` to CLAUDE.md under Objects, Cameras & Lights

All 67 tests pass after the changes.

---

## Decisions Log

| # | Decision | Choice | Reasoning |
|---|----------|--------|-----------|
| 1 | Where does clear color belong? | Scene-level statement, not camera property | Matches Shockwave 3D's stage-level background; most engines treat it as global/scene state |
| 2 | Keyword name | `sky:` over `background:` | Shorter; future-compatible with skybox/cubemap textures |
| 3 | Value format | 0–1 float RGB (like lights) | Consistent with how light colors are already specified in SlopScript |
| 4 | AST node type | `CallStmt` (reuse existing) | Same pattern as `goto:`, `kill:`, etc. — minimal parser changes |

---

## Files Changed

| File | Change |
|------|--------|
| `js/slop3d.js` | Added `sky` keyword, parser support, codegen case, and `sky()` runtime method |
| `tests/test_slopscript.js` | Added parser test and codegen test for `sky:` |
| `CLAUDE.md` | Documented `sky: r, g, b` syntax |

---

## Usage Example

```
scene main
    sky: 0.2, 0.1, 0.3
    cam = camera: 0, 1.5, 5
    sun = directional: 1, 0.9, 0.8, -1, -1, -1
    box = spawn: cube, crate

    update
        box.rotation.y = t * 30
```

Can also be used in the `update` block for animated backgrounds:

```
    update
        sky: sin[t * 10] * 0.5 + 0.5, 0.1, 0.3
```

---

## Next Steps

- **Skybox/cubemap support** — Caleb plans to extend `sky:` to accept JPEG paths for cubemap rendering (e.g., `sky: sunset.jpg` or an array of 6 faces). This will require C engine changes for cubemap sampling during the clear/background pass.
