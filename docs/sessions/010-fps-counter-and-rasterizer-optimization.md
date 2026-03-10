# Session 010: FPS Counter, Frame Cap & Rasterizer Optimization

**Date:** March 9, 2026
**Participants:** Caleb Cohoon + Claude (Opus 4.6)
**Repo:** [deadcast2/slopwave3d](https://github.com/deadcast2/slopwave3d)

---

## Session Goal

Add an FPS counter and frame time display to both the WASM and ActiveX renderers, align both renderers to ~30 FPS, and identify/implement high-impact rasterizer optimizations.

---

## Conversation Summary

### 1. FPS Counter — Initial Implementation

Caleb requested an FPS counter and per-frame render time display, with the requirement that the stats be rendered to HTML divs rather than drawn onto the framebuffer. Both the WASM demo (`web/index.html`) and ActiveX test (`activex/test.html`) needed updating.

**WASM side:** Added a `#stats` div with `#fps` and `#frametime` spans overlaid on the canvas via absolute positioning. Modified `Slop3D.start()` in `js/slop3d.js` to track frame timing using `performance.now()`, averaging over 500ms windows to avoid jittery readouts.

**ActiveX side:** Added equivalent HTML elements and JScript timing code using `new Date().getTime()` (JScript-compatible, since `performance.now()` doesn't exist in IE 8).

### 2. ActiveX Overlay Problem — Windowed Controls

The initial ActiveX implementation used `position: absolute` to overlay the stats on top of the ActiveX `<object>` element. This failed because ActiveX controls are windowed elements in IE — they always render on top of regular HTML regardless of z-index. This is a well-known IE limitation with windowed controls (ActiveX, `<select>`, `<iframe>`).

**Fix:** Moved the stats div below the `<object>` element instead of overlaying it. Changed from `<div>` to inline `<span>` elements for a compact single-line layout.

### 3. IE 8 Compatibility Fixes

The original CSS used several properties not supported in IE 8:
- `display: inline-block` — unreliable on divs in IE 8 table cells
- `pointer-events: none` — not supported
- `text-shadow` — not supported
- `position: relative` inside `<td>` — inconsistent positioning context

**Fix:** Replaced the table-based centering layout with a simple div using `margin: 0 auto` and `top: 50%; margin-top: -250px` for vertical centering. Removed all unsupported CSS properties.

### 4. Aligning Stats Layout

Caleb requested the WASM version match the ActiveX layout — stats displayed inline below the canvas, bottom-left. Updated `web/index.html` to use the same non-overlay approach with inline spans.

### 5. 30 FPS Alignment

Caleb asked whether both renderers were capped at 30 FPS. Investigation revealed:
- **ActiveX:** Capped at ~30 FPS via `SetTimer(m_hwnd, 1, 33, NULL)`
- **WASM:** Uncapped, running at display refresh rate (typically 60 FPS) via `requestAnimationFrame`

The WASM render loop was modified to throttle to 30 FPS using a rolling target time approach. The initial implementation used remainder correction (`now - (dt % frameInterval)`) which drifted to ~28.6 FPS on 60Hz displays because `requestAnimationFrame` fires at 16.67ms intervals — waiting for the 3rd frame (50ms) consistently undershoots 33.33ms.

**Fix:** Switched to a `_nextFrame` target that advances by exactly `1000/30` ms each frame. If the target falls behind, it resets to prevent frame bursts. This achieved a stable 30.0 FPS.

### 6. ActiveX Timer Resolution

The ActiveX renderer was reporting ~21.4 FPS instead of ~30 FPS. This was diagnosed as a Windows timer resolution issue:

Windows XP has a default system timer granularity of ~15.625ms (64 Hz). `SetTimer` rounds the requested interval up to the next multiple of this granularity:
- `SetTimer(33ms)` → `ceil(33 / 15.625)` = 3 ticks = 46.875ms ≈ **21.3 FPS**
- `SetTimer(30ms)` → `ceil(30 / 15.625)` = 2 ticks = 31.25ms ≈ **32 FPS**

**Fix:** Changed `SetTimer` interval from 33ms to 30ms in both call sites (`Start()` and `InPlaceActivate()`). This lands on 2 system ticks, yielding ~32 FPS — close enough to 30 without adding high-resolution timer complexity.

### 7. Rasterizer Optimization Analysis

Caleb asked about high-impact optimizations. Four candidates were identified:

| # | Optimization | Impact | Compiler handles? |
|---|-------------|--------|-------------------|
| 1 | Additive stepping (replace per-pixel lerp) | High — eliminates 5 muls/pixel | No — algorithmic change |
| 2 | Integer z-buffer stepping | Medium — removes float→int per pixel | No — data type change |
| 3 | Remove unnecessary inner-loop clamps | Low-medium — removes branches | No — can't prove value range |
| 4 | `memset` framebuffer clear | Low — replace clear loop | Yes — LLVM auto-vectorizes at -O2 |

### 8. Compiler Optimization Discussion

Caleb asked which optimizations the compiler (`emcc -O2`) might already handle. Analysis:
- **#4 is handled** — the simple constant-fill loop is a textbook auto-vectorization target
- **`clampf` inlining is handled** — `static inline` trivial function, call overhead eliminated
- **#1, #2, #3 are NOT handled** — these require algorithmic or semantic reasoning the compiler can't perform

### 9. Implementing Additive Stepping (#1)

The original inner loop computed per-pixel interpolation as:
```c
float s = ((float)x + 0.5f - xl) * inv_span;
float z = zl + (zr - zl) * s;
// ... same for r, g, b
```

This required computing `s` (1 multiply + 1 add) then 4 lerps (4 multiplies + 4 adds) = **5 multiplies + 5 adds per pixel**.

The optimized version precomputes step values once per scanline:
```c
float dz = (zr - zl) * inv_span;
float dr = (rr - rl) * inv_span;
// ... seeds initial values at ix_start
// inner loop just does: z += dz; cr_f += dr; ...
```

This reduces the inner loop to **4 adds per pixel** (z + r + g + b). At 320×240 resolution with high triangle coverage, this saves up to ~384,000 multiplies per frame.

Build and all 97 unit tests passed after the change.

---

## Decisions Log

| # | Decision | Choice | Reasoning |
|---|----------|--------|-----------|
| 1 | Stats rendering target | HTML divs, not framebuffer | Keeps engine framebuffer clean; stats are a dev overlay concern |
| 2 | ActiveX stats placement | Below the control, not overlaid | ActiveX windowed controls always render on top of HTML in IE |
| 3 | IE 8 layout approach | `margin: 0 auto` + `top: 50%` centering | Replaced table-based layout to avoid positioning context issues |
| 4 | FPS update frequency | Every 500ms | Balances readability with responsiveness |
| 5 | WASM frame cap method | Rolling target time with `requestAnimationFrame` | Remainder correction drifted; target approach gives exact 30 FPS |
| 6 | ActiveX `SetTimer` interval | 30ms (was 33ms) | Lands on 2 system ticks at 15.6ms resolution ≈ 32 FPS |
| 7 | Skip high-res timer for ActiveX | Accept ~32 FPS | Extra code for `timeBeginPeriod`/multimedia timers not worth the complexity |
| 8 | Rasterizer optimization choice | Additive stepping (#1 only) | Highest impact, compiler can't do it, minimal code increase |

---

## Files Changed

| File | Change |
|------|--------|
| `web/index.html` | Added `#game-container`, `#stats` div with FPS/frame time spans |
| `js/slop3d.js` | Added frame timing to `start()`, 30 FPS cap with rolling target |
| `activex/test.html` | Replaced table layout with div centering, added stats below control, added JScript timing |
| `activex/slop3d_activex.cpp` | Changed `SetTimer` from 33ms to 30ms (both call sites) |
| `src/slop3d.c` | Rasterizer inner loop: additive stepping replaces per-pixel lerp |

---

## Commits

| Hash | Message |
|------|---------|
| `8b22c1b` | Add FPS counter and frame time display to WASM and ActiveX demos |
| `c856c59` | Cap WASM render loop to ~30 FPS, fix ActiveX timer resolution |
| `5df8e10` | Optimize rasterizer inner loop: additive stepping over per-pixel lerp |

---

## Next Steps

- Remaining rasterizer optimizations (#2 integer z-stepping, #3 remove unnecessary clamps) could be revisited when poly counts increase
- Continue with GitHub issues #1-#8 implementation
- Texture mapping will be the next major rendering feature, adding u/v interpolation to the same scanline loop
