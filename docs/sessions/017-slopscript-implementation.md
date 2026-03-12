# Session 017: SlopScript Implementation

**Date:** March 11, 2026
**Participants:** Caleb Cohoon + Claude (Opus 4.6)
**Duration:** Implementation session
**Repo:** [deadcast2/slopwave3d](https://github.com/deadcast2/slopwave3d)

---

## Session Goal

Implement SlopScript — the custom scripting language designed in session 016 — as a fully working transpiler targeting the Slop3D engine. The language design was already finalized; this session covered implementation of the lexer, parser, code generator, runtime helpers, browser loader, demo rewrite, and unit tests.

---

## Conversation Summary

### 1. Planning Phase

The session began by reviewing both the design document (`~/.claude/plans/splendid-snacking-spring.md`) and the session 016 log (`docs/sessions/016-slopscript-language-design.md`). An exploration agent surveyed the full JS API surface (`js/slop3d.js`, 272 lines), the current demo (`web/index.html`), and the C API header (`src/slop3d.h`).

A Plan agent then designed the detailed implementation: class signatures, AST node types, generated code examples, and the full reactivity flow from SlopScript assignment through runtime helpers to engine API calls. The plan was written to `~/.claude/plans/distributed-exploring-kite.md`.

### 2. Language Choice Discussion

Caleb asked whether the transpiler should be implemented in C or JS. Three options were evaluated:

| Option | Assessment |
|--------|-----------|
| JS transpiler (recommended) | Natural fit — string manipulation, `new Function()`, reactive getters/setters all come free |
| C interpreter/VM | Much larger effort (~5000+ lines), painful string handling, needs new WASM-side API |
| C parser + JS runtime hybrid | Awkward WASM↔JS boundary, no real benefit |

**Decision: JS**, as the transpiler's job is to produce JS targeting the existing `Slop3D` JS API.

### 3. Reactive Proxy Explanation

Caleb asked about the "reactive proxies" mentioned in the plan. The key insight: SlopScript lets you write `box.rotation.y = t * 30` as a simple assignment, but the engine API requires `engine.setObjectRotation(id, rx, ry, rz)` with all three components. The solution uses JS getter/setter properties on `SlopVec3` — setting `.y` fires an `onChange` callback that calls the engine with all three stored values. This avoids the complexity of ES6 Proxy while achieving the same reactive behavior.

### 4. GitHub Issue Creation

Caleb suggested splitting the implementation into multiple GitHub issues for tracking. Six issues were created (#13–#18), one per phase:

- #13: Runtime Helpers (SlopVec3, SlopObject, SlopCamera, SlopRuntime, math)
- #14: Lexer (indentation-aware tokenizer)
- #15: Parser (recursive descent, Pratt precedence)
- #16: Code Generator (AST → JS)
- #17: Loader (browser integration)
- #18: Demo Rewrite

A `scripting` label was also created on the repo.

### 5. Single-File Decision

Caleb asked if the SlopScript code could fit into `js/slop3d.js` rather than a separate `js/slopscript.js`. The original `Slop3D` class was 272 lines; the SlopScript transpiler targeted ~1000 lines. Combined at ~1270 lines, this preserves the "entire engine in one context window" philosophy. All SlopScript code was appended after the `Slop3D` class.

### 6. Implementation (Phases 1–5)

All five transpiler phases were implemented in a single pass, appended to `js/slop3d.js`:

**Phase 1 — Runtime Helpers:**
- `SlopVec3` — reactive 3-component vector with getter/setters firing `onChange`
- `SlopObject` — wraps object ID with reactive `.position`, `.rotation`, `.scale`, `.color`, `.alpha`, `.active`
- `SlopCamera` — singleton with reactive `.position`/`.target` flushing to `engine.setCamera()`
- `SlopRuntime` — engine ref, asset/scene registries, `spawn()`/`destroy()`/`gotoScene()`, time tracking (`t`/`dt`), implicit render
- Math wrappers — degree-based `_sin`/`_cos`/`_tan`, plus `_lerp`, `_clamp`, `_random`, `_abs`, `_min`, `_max`, `_range`

**Phase 2 — Lexer:**
- Python-style indentation tracking with `indentStack`
- Token types: INDENT, DEDENT, NEWLINE, NUMBER, IDENT, brackets, operators, EOF
- Keywords emitted as IDENT tokens, checked by parser
- Line/col tracking on every token
- Comment (`#`) and blank line skipping

**Phase 3 — Parser:**
- Recursive descent producing AST
- AST nodes: Program, AssetBlock, ModelDecl, SkinDecl, Scene, Assign, SpawnAssign, CallStmt, If, While, For, FnDef, Return, plus expression nodes (Num, Ident, Bool, Dot, BinOp, Unary, Call, Tuple, Group)
- Pratt precedence climbing for expressions: `or` < `and` < comparisons < `+-` < `*/%` < unary
- `[]` dual purpose: `sin[t]` (call) vs `[a + b]` (grouping), disambiguated by preceding IDENT
- Tuple detection in assignment RHS

**Phase 4 — Code Generator:**
- Walks AST, emits JS strings targeting runtime helpers
- Assets → `Promise.all` of `loadOBJ`/`loadTexture` calls
- Scenes → `setup` function returning `_scope` + `update` function receiving `_scope`
- Tuple RHS → `.setAll()`, scalar RHS → plain assignment
- Built-ins: `t` → `_rt.t`, `camera` → `_rt.camera`, `sin` → `_sin`
- `goto:` → `_rt.gotoScene() + return`

**Phase 5 — Loader:**
- `SlopScript.run(source, canvasId)` — full pipeline via `new Function()`
- `SlopScript.load(url, canvasId)` — fetch + run
- Auto-init on DOMContentLoaded scanning `<script type="text/slopscript">` tags

### 7. Demo Rewrite (Phase 6)

The `web/index.html` inline JS IIFE (35 lines) was replaced with a `<script type="text/slopscript">` tag containing the 3-cube demo in SlopScript (~20 lines). A standalone `web/demo.slop` file was also created.

### 8. Time and Trig Fix

Caleb noticed the demo cubes rotated slower than the original. Two issues were identified:

1. **`t` is real seconds** (1.0/sec) vs the original's fake `t += 0.02` per frame (0.6/sec at 30fps)
2. **SlopScript's `sin`/`cos` are degree-based** while the original used `Math.sin()` in radians

The demo values were recalculated to match the original visual speed:
- Camera orbit: `sin[t * 34.4]` (matching 0.6 rad/sec)
- Rotations scaled by 0.6: `t * 18`, `t * 12`, `t * 24`, `t * -9`

### 9. Unit Tests

49 unit tests were written in `tests/test_slopscript.js` using Node's built-in test runner:
- **Lexer** (9 tests) — tokenization, indentation, comments, line tracking, error cases
- **Parser** (18 tests) — assets, spawn, tuples, dot chains, expressions, control flow, precedence, booleans
- **CodeGen** (16 tests) — asset loading, spawn, setAll, builtins, math, goto, if/else, loops, functions
- **Runtime** (6 tests) — SlopVec3 reactivity, degree-based trig, lerp, clamp, range

The `Makefile` `test` target was updated to run both C and JS tests (`make test` → 97 C + 49 JS = 146 total).

### 10. Formatter Configuration

Caleb noticed `make fmt` expanded the JS significantly due to Prettier's default 80-column print width. Both formatters were reconfigured for compact output:
- **`.prettierrc`** — `printWidth: 120`, `arrowParens: "avoid"`
- **`.clang-format`** — `ColumnLimit: 120`, short functions/ifs/loops/blocks on single lines

The `fmt` Makefile target was also fixed to reference the correct files (removed deleted `web/demo.js`, added `tests/test_slopscript.js`).

### 11. Documentation Updates

Both `CLAUDE.md` and `README.md` were updated:
- **CLAUDE.md** — added full SlopScript section (syntax, built-ins, structure, transpiler pipeline, browser integration), updated file structure, build commands, and issue tracking
- **README.md** — replaced JS Quick Start with SlopScript example, added features list, updated architecture diagram and specs table

---

## Research Findings

No external web research was conducted. All implementation was based on the session 016 design document and analysis of the existing codebase.

---

## Research Sources

No external sources were consulted.

---

## Decisions Log

| # | Decision | Choice | Reasoning |
|---|----------|--------|-----------|
| 1 | Implementation language | JS (not C) | Transpiler produces JS targeting JS API — string manipulation, `new Function()`, reactive getters/setters are natural in JS |
| 2 | File organization | Single file (`js/slop3d.js`) | Keeps "entire engine in one context window" philosophy; combined ~1200 lines is manageable |
| 3 | Reactivity approach | Getter/setter properties (not Proxy) | Simpler, faster, avoids Proxy edge cases; `SlopVec3` fires `onChange` on component mutation |
| 4 | Time tracking | Runtime-managed `t`/`dt` via `performance.now()` | `onUpdate` callback receives no arguments; runtime computes its own wall-clock time |
| 5 | Implicit render | `_tick()` calls `renderScene()` after update | Eliminates boilerplate render call from every SlopScript program |
| 6 | Code execution | `new Function()` (not `eval`) | Clean scope boundary; runtime classes passed as arguments |
| 7 | Issue tracking | 6 issues (#13–#18), one per phase | Requested by Caleb for granular tracking |
| 8 | Demo speed values | Recalculated for real-time `t` + degree-based trig | Original used fake `t += 0.02` at 30fps and radian-based `Math.sin()` |
| 9 | Formatter config | `printWidth: 120`, compact C/JS formatting | Caleb requested compact style after default Prettier expanded code significantly |
| 10 | Keyword handling | Emitted as IDENT tokens, checked by parser | Simplifies lexer — no separate keyword token types needed |

---

## Files Changed

| File | Change |
|------|--------|
| `js/slop3d.js` | Added SlopScript transpiler: runtime helpers, lexer, parser, codegen, loader (~920 lines added) |
| `web/index.html` | Rewrote demo from JS IIFE to inline `<script type="text/slopscript">` |
| `web/demo.slop` | Created — standalone 3-cube demo in SlopScript |
| `tests/test_slopscript.js` | Created — 49 unit tests for lexer, parser, codegen, runtime |
| `Makefile` | Added JS tests to `test` target, fixed `fmt` target file list |
| `.prettierrc` | Updated for compact formatting (`printWidth: 120`, `arrowParens: "avoid"`) |
| `.clang-format` | Updated for compact formatting (`ColumnLimit: 120`, short blocks on single lines) |
| `CLAUDE.md` | Added SlopScript section, updated file structure, build commands, issue refs |
| `README.md` | Added SlopScript Quick Start, features, updated architecture diagram and specs |
| `~/.claude/plans/distributed-exploring-kite.md` | Created — implementation plan |

---

## Next Steps

1. **Browser testing** — verify the full demo works end-to-end in the browser via `make serve`
2. **Close GitHub issues** — close #13–#18 as each phase is verified working
3. **Error reporting** — add SlopScript line numbers in transpiler error messages for better debugging
4. **Edge cases** — test empty update blocks, no-texture spawns, nested ifs, deeply nested expressions
5. **Future: Input handling** — `if key[space]`, `mouse.clicked` (designed but not yet in scope)
6. **Future: Sound** — `sound bang = bang.mp3`, `play: bang`
7. **Future: Electron IDE** — live preview + SlopScript editor with hot-reload
