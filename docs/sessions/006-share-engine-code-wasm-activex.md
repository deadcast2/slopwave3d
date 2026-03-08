# Session 006: Share C Engine Code Between WASM and ActiveX Builds

**Date:** March 8, 2026
**Participants:** Caleb Cohoon + Claude (Opus 4.6)
**Duration:** Implementation session
**Repo:** [deadcast2/slopwave3d](https://github.com/deadcast2/slopwave3d)

---

## Session Goal

Eliminate the duplicated C engine by making the ActiveX build compile `src/slop3d.c` directly instead of maintaining a separate `activex/slop3d_vc6.cpp` port. This resolves GitHub issue [#10](https://github.com/deadcast2/slopwave3d/issues/10), which was created as a follow-up from Session 005's ActiveX implementation.

---

## Conversation Summary

### 1. Issue Review

Caleb asked to work on GitHub issue #10, which documented the code duplication problem introduced in Session 005. The ActiveX control required a VC6-compatible port of the engine (`activex/slop3d_vc6.cpp` + `activex/slop3d_vc6.h`), which was a line-for-line mechanical translation of `src/slop3d.c` with C99 syntax replaced by C++ equivalents. Any future engine changes would need to be made in both files.

The issue identified C99 compound literals as the primary incompatibility, with `stdint.h` and `static inline` as trivially handled secondary differences.

### 2. Exploration Phase

Two parallel exploration agents investigated the codebase:

**Agent 1: Compound Literals in slop3d.c** — Found 17 compound literal instances across the file (15 `S3D_Vec3`, 2 `S3D_Vec4`), spanning vector math helpers, matrix operations, camera initialization, and camera setter functions. Confirmed no `s3d_vec3()`/`s3d_vec4()` helpers existed in the public header yet.

**Agent 2: ActiveX Compatibility Layer** — Catalogued all six categories of differences between the WASM and VC6 builds:

| Difference | WASM | VC6 Port | Resolution |
|---|---|---|---|
| Compound literals | `(S3D_Vec3){x,y,z}` | `s3d_vec3(x,y,z)` | Add helpers to shared header |
| `stdint.h` | `#include <stdint.h>` | Manual typedefs | Guard with `_MSC_VER` check |
| `sqrtf` | `sqrtf()` | `(float)sqrt()` | `#define` in compat header |
| `inline` keyword | `static inline` | `static __forceinline` | C++ mode supports `inline` |
| For-loop declarations | `for (int i = 0; ...)` | Hoisted to block scope | C++ mode supports this |
| `EMSCRIPTEN_KEEPALIVE` | Used on public functions | Absent | Already guarded with `#ifndef` fallback |

### 3. Design and Implementation

A plan was drafted covering 8 steps. After approval, implementation proceeded:

**Step 1: Update `src/slop3d.h`** — Added a `_MSC_VER < 1600` guard around `#include <stdint.h>` with manual typedefs for VC6. Added `s3d_vec3()` and `s3d_vec4()` constructor helpers after the struct definitions, using `static inline` (valid in both C99 and C++).

**Step 2: Replace compound literals in `src/slop3d.c`** — Replaced all 17 `(S3D_Vec3){...}` and `(S3D_Vec4){...}` compound literals with `s3d_vec3(...)` and `s3d_vec4(...)` calls. Verified zero compound literals remained via grep.

**Step 3: Create `activex/slop3d_compat.h`** — A thin shim that `#define`s `sqrtf(x)` to `((float)sqrt(x))` for VC6 (`_MSC_VER < 1300`), then includes the shared `../src/slop3d.h`.

**Step 4: Eliminate the wrapper file** — Initially the plan called for an `activex/slop3d_engine.cpp` wrapper that `#include`d the compat header and `../src/slop3d.c`. Caleb observed this was unnecessary since the file was only two lines. Instead, `build.bat` was updated to use VC6 compiler flags: `/Tp ..\src\slop3d.c` (compile as C++) and `/FIslop3d_compat.h` (force-include the compat shim).

**Step 5: Update references** — Changed `activex/slop3d_activex.cpp` to include `slop3d_compat.h` instead of the old `slop3d_vc6.h`. Updated `build.bat` link line to reference `slop3d.obj` instead of `slop3d_vc6.obj`.

**Step 6: Delete old files** — Removed `activex/slop3d_vc6.cpp` (322 lines) and `activex/slop3d_vc6.h` (55 lines).

### 4. Build Testing and Bug Fixes

**WASM build:** Passed immediately after the compound literal replacement. Verified with `make clean && make`.

**ActiveX build (VC6 on Windows XP):** Required two rounds of fixes:

**Fix 1: Include path for `/FI`** — The `/FIslop3d_compat.h` flag couldn't find the file because VC6 was searching relative to the source file's directory (`../src/`), not the working directory (`activex/`). Fixed by adding `/I.` to the compile flags to add the current directory to the include search path.

**Fix 2: `near`/`far` reserved keywords** — VC6 reserves `near` and `far` as keywords (legacy 16-bit segmented memory modifiers from the DOS/Windows 3.x era). The `m4_perspective()` function used these as parameter names. Renamed to `near_val`/`far_val`. The original VC6 port had already worked around this — it was a difference we hadn't catalogued during exploration.

After both fixes, the ActiveX build compiled successfully on VC6.

### 5. Results

The final commit showed 66 lines added and 408 lines deleted — a net reduction of 342 lines. Caleb reported the overall project token count dropped from 21.8k to 18.2k tokens (16% reduction), which is significant for a project designed to fit in a single AI context window.

---

## Decisions Log

| # | Decision | Choice | Reasoning |
|---|----------|--------|-----------|
| 1 | Where to put vec helpers | `src/slop3d.h` (public header) | Available to both engine internals and external code; single definition shared by both build targets |
| 2 | How to handle `stdint.h` | `_MSC_VER` guard in `slop3d.h` | Self-contained — no external defines needed, works automatically on both platforms |
| 3 | How to handle `sqrtf` | `#define` in `activex/slop3d_compat.h` | Only affects VC6 builds; cleaner than modifying the engine source |
| 4 | ActiveX compilation strategy | `/Tp` + `/FI` compiler flags in `build.bat` | Eliminates the need for a wrapper `.cpp` file entirely; build script handles everything |
| 5 | `near`/`far` parameter names | Rename to `near_val`/`far_val` in `src/slop3d.c` | These are reserved keywords in VC6; renaming is the simplest portable fix |

---

## Files Changed

### Created
- `activex/slop3d_compat.h` — VC6 compatibility shim (sqrtf define + shared header include)

### Modified
- `src/slop3d.h` — Added `_MSC_VER` stdint guard, added `s3d_vec3()`/`s3d_vec4()` helpers
- `src/slop3d.c` — Replaced 17 compound literals with helper calls, renamed `near`/`far` parameters
- `activex/slop3d_activex.cpp` — Changed include from `slop3d_vc6.h` to `slop3d_compat.h`
- `activex/build.bat` — Updated to compile `../src/slop3d.c` directly with `/Tp`, `/FI`, `/I.` flags
- `CLAUDE.md` — Updated file structure section

### Deleted
- `activex/slop3d_vc6.cpp` — 322-line duplicated engine port
- `activex/slop3d_vc6.h` — 55-line duplicated public header

---

## Next Steps

- Continue with remaining GitHub issues (#1-#8) for engine feature implementation
- All future engine changes now only need to be made in `src/slop3d.c` — both WASM and ActiveX builds will pick them up automatically
