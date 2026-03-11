# Session 012: Scanline Gap Bugfix

**Date:** March 10, 2026
**Participants:** Caleb Cohoon + Claude (Opus 4.6)
**Duration:** Debugging session
**Repo:** [deadcast2/slopwave3d](https://github.com/deadcast2/slopwave3d)

---

## Session Goal

Diagnose and fix an intermittent blank scanline appearing on the textured cube as it rotates. The artifact was introduced in the session 011 commit that added mesh, texture, and OBJ loading support.

---

## Conversation Summary

### 1. Bug Report

Caleb reported that as the 3D textured crate spins, a blank horizontal scanline occasionally flickers across one of the cube faces. The artifact was intermittent — appearing only at certain rotation angles.

### 2. Initial Investigation — Span Threshold

The first hypothesis examined the span threshold guard in the scanline rasterizer at `slop3d.c:442`:

```c
float span = xr - xl;
if (span < 0.001f)
    continue;
```

This threshold was intended to prevent division-by-zero when computing `inv_span = 1.0f / span`. However, `0.001f` is large enough to skip scanlines where two triangle edges are very close but still straddle a pixel center. When two triangles share an edge on a cube face, both could skip the same scanline near that shared edge.

The threshold was changed from `0.001f` to `0.0f` (later refined to `1e-6f`). This alone did not fix the bug.

### 3. Deep Analysis — The Real Bug

A thorough analysis of the rasterizer's y-range computation revealed a mismatch between the scanline range and the pixel sampling point.

The y-range was computed as:

```c
int y_start = (int)ceilf(v0.y);           // line 349
int y_end   = (int)ceilf(v2.y) - 1;       // line 350
```

This defines the range as `ceil(v0.y) <= y < ceil(v2.y)`, meaning a pixel row y is drawn when `v0.y <= y < v2.y`.

But the actual interpolation sample point used a half-pixel offset:

```c
float fy = (float)y + 0.5f;               // line 359
```

This created a systematic off-by-half mismatch. The range selected rows based on integer y, but the interpolation sampled at y+0.5.

### 4. The Failure Scenario

A cube face is rendered as two triangles sharing a diagonal edge. When the cube rotates to an angle where this diagonal projects as nearly horizontal at a half-pixel center (e.g., screen y=130.5), the bug manifests:

- **Triangle A** (above the diagonal): `v2.y = 130.5`, so `y_end = ceil(130.5) - 1 = 130`. At row y=130, `fy = 130.5 = v2.y` — both the long edge and short edge converge to the same endpoint, giving span=0. Nothing is drawn.
- **Triangle B** (below the diagonal): `v0.y = 130.5`, so `y_start = ceil(130.5) = 131`. Row y=130 is excluded entirely.

Result: row 130 is blank — neither triangle draws it. As the cube rotates, the projected diagonal sweeps through different y values, and each time it crosses a half-pixel center, the blank scanline appears for one or more frames.

### 5. The Fix

The fix was to remove the `+0.5f` offset from the sample point:

```c
float fy = (float)y;    // was: (float)y + 0.5f
```

This makes the interpolation sample point consistent with the y-range convention. With `fy = y`:

- **Triangle A**: `y_end = ceil(130.5) - 1 = 130`. At y=130, `fy = 130.0 < v2.y = 130.5`. The triangle has nonzero width — the edges haven't converged yet.
- **Triangle B**: `y_start = ceil(130.5) = 131`. Starts at the next row. No gap.

For two adjacent triangles sharing an edge at screen y=E, the range `[ceil(v0.y), ceil(E)-1]` for the upper triangle and `[ceil(E), ceil(v2.y)-1]` for the lower triangle are always adjacent with no overlap or gap.

Sampling at integer y (top of pixel) rather than y+0.5 (pixel center) is a common convention in classic software rasterizers and is appropriate for this era-authentic engine.

### 6. Span Threshold Refinement

After the main fix, Caleb asked about the purpose of the original `span < 0.001f` guard. The guard existed to prevent numerical instability from `1.0f / span` when span is extremely small. While `span <= 0.0f` was sufficient (when span is tiny but positive, `ix_start > ix_end` prevents the inner loop from executing), Caleb preferred the extra safety margin of `1e-6f`. This is ~1000x smaller than a pixel, so no legitimate scanlines would be skipped.

Final threshold: `span < 1e-6f`.

---

## Research Findings

No external research was conducted. The debugging was performed through static analysis of the rasterizer code, tracing through concrete geometric scenarios with specific vertex coordinates to identify the failure case.

Key technical insight: in a scanline rasterizer with top/bottom triangle half-splitting, the y-range computation and the interpolation sample point must use the same convention. A half-pixel mismatch creates a one-row gap at shared edges when those edges are nearly horizontal.

---

## Decisions Log

| # | Decision | Choice | Reasoning |
|---|----------|--------|-----------|
| 1 | Span threshold value | `1e-6f` | Small enough to never skip visible pixels, large enough to guard against div-by-zero edge cases |
| 2 | Pixel sample point | `fy = (float)y` (no +0.5 offset) | Makes sample point consistent with `ceil()`-based y-range. More era-authentic (many classic rasterizers sampled at integer coordinates) |

---

## Files Changed

| File | Change |
|------|--------|
| `src/slop3d.c:359` | Changed `float fy = (float)y + 0.5f` to `float fy = (float)y` |
| `src/slop3d.c:442` | Changed `if (span < 0.001f)` to `if (span < 1e-6f)` |

---

## Next Steps

- Continue with GitHub issue tracker (issues #5-#8)
- The x-axis pixel sampling (`ix_start + 0.5f` on line 455) has the same half-pixel offset pattern — could cause individual missing pixels along shared diagonal edges, though this is far less visible than a full blank scanline. Worth monitoring.
