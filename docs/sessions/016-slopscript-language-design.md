# Session 016: SlopScript Language Design

**Date:** March 11, 2026
**Participants:** Caleb Cohoon + Claude (Opus 4.6)
**Duration:** Design session (no implementation)
**Repo:** [deadcast2/slopwave3d](https://github.com/deadcast2/slopwave3d)

---

## Session Goal

Design a custom scripting language for the slop3d engine — a purpose-built DSL that would make game scripting dead simple, paralleling how Shockwave 3D had Lingo as its scripting language. The language should be highly tailored to the engine, minimal in syntax, and eventually support a live-preview IDE experience.

---

## Conversation Summary

### 1. Initial Vision

Caleb proposed the idea of a custom scripting language for the slop3d engine, noting the parallel to Shockwave 3D's Lingo. He also envisioned a future Electron app with a live preview of the game alongside a code editor with real-time updates. The session was scoped as design-only — no implementation.

### 2. Codebase Exploration

An exploration agent surveyed the full engine API surface to understand what the scripting language would need to wrap:

- **JS API** (`js/slop3d.js`): `Slop3D` class with methods for camera, assets (loadTexture, loadOBJ), objects (create, destroy, position, rotation, scale, color, alpha, active), and rendering (renderScene, onUpdate callback).
- **C API** (`src/slop3d.h`): 25+ exported functions covering init, camera, textures, meshes, objects, and rendering.
- **Current game scripting pattern** (`web/index.html`): Async JS IIFEs with ~35 lines for a basic 3-cube demo — lots of boilerplate.

### 3. Language Name and Direction

Three syntax directions were evaluated:

| Option | Style | Assessment |
|--------|-------|------------|
| A | Clean/modern (GDScript/Lua) | Technically sound but generic |
| B | Retro/quirky (Lingo) | Fun but painful to write |
| C | Unique "slopwave" aesthetic | Best of both — clean syntax with era-authentic vocabulary |

**Decision: Option C** — named **SlopScript** (`.slop` extension). Era-authentic vocabulary (`model`, `skin`, `spawn`, `destroy`) with clean, indentation-based syntax.

### 4. Three-Block Structure

The language uses a fixed top-level structure inspired by how Shockwave 3D projects were organized (cast → score/setup → frame scripts):

- **`assets`** — declare models and textures, loaded async before any scene
- **`scene`** — setup code, runs once
- **`update`** — per-frame logic

Caleb confirmed this structure was "perfect as designed."

### 5. Syntax Minimization — Round 1

The first draft used `let` for declarations, explicit `render` calls, and `vec3()` constructors. Caleb pushed for less syntax. Three reductions were adopted:

1. **Drop `let`** — first assignment declares the variable (like Ruby)
2. **Implicit `render`** — engine renders automatically after every update tick
3. **Tuple syntax for vectors** — `box.position = 2.5, 0, 0` instead of `vec3(2.5, 0, 0)`

The spawn modifier chain syntax (`spawn(cube, crate) at (2.5, 0, 0) scale 0.6`) was considered but rejected.

### 6. Named Scenes and `goto:`

Caleb requested the ability to name scenes for multi-level games. Design:

- Scenes are named: `scene menu`, `scene level1`
- `goto: scene_name` switches scenes with auto-cleanup (destroys all objects from current scene)
- First scene declared is active by default
- `assets` block is global (shared across all scenes)

### 7. Nesting `update` Inside `scene`

Rather than having separate `update scenename` blocks at the top level, Caleb suggested nesting `update` inside its scene block. This reads more naturally — setup flows into per-frame logic, all scoped together by indentation:

```
scene main
    box = spawn: cube
    camera.position = 0, 1.5, 5

    update
        box.rotation.y = t * 30
```

### 8. Syntax Minimization — Round 2: Dropping Quotes and Parens

Caleb pushed further: "could we possibly even drop the `(` and `)` and even the double quotes for strings?"

- **Dropping quotes**: Adopted for asset paths — `model cube = cube.obj` instead of `"cube.obj"`. Parser knows asset declarations always have filename values.
- **Dropping parens on tuples**: Adopted for assignments — `box.position = 2.5, 0, 0`. Parser knows right side of `=` with comma-separated values is a tuple.
- **Function calls**: Needed a new syntax since parens were removed.

Caleb proposed **colon syntax**: `spawn: cube, crate` instead of `spawn(cube, crate)`. This removes ambiguity — the colon clearly delimits function name from arguments.

### 9. Square Brackets for Expression Calls

For functions embedded in math expressions (like `sin(t) * 5`), parens couldn't be used. Options evaluated:

| Delimiter | Example | Assessment |
|-----------|---------|------------|
| Pipes `\|\|` | `sin\|t\| * 5` | Retro feel, conflicts with boolean or |
| Backticks `` ` `` | `` sin`t` * 5 `` | Unusual, JS template literal confusion |
| Square brackets `[]` | `sin[t] * 5` | Clean, no conflicts, unique feel |
| Angle brackets `<>` | `sin<t> * 5` | Conflicts with comparison operators |

**Decision: Square brackets.** This gives SlopScript a distinctive visual identity — no parentheses appear anywhere in the language.

### 10. Order of Operations / Grouping

Without parentheses for grouping expressions like `(a + b) * c`, a solution was needed. Three options:

| Approach | Example | Assessment |
|----------|---------|------------|
| `[]` dual purpose | `[a + b] * c` | Simple rule, one delimiter |
| `{}` for grouping | `{a + b} * c` | Two distinct delimiters |
| No grouping | Use temp variables | Too limiting |

**Decision: `[]` serves double duty.** Function call when preceded by an identifier (`sin[t]`), grouping when standalone (`[a + b] * c`). The parser checks if a token before `[` is a known identifier to disambiguate.

---

## Research Findings

### Engine API Analysis

The exploration agent cataloged the full slop3d API surface that SlopScript must wrap:

**JS API methods** (from `js/slop3d.js`, 273 lines):
- Lifecycle: `constructor`, `init()`, `start()`, `stop()`
- Camera: `setCamera()`, `setCameraFov()`, `setCameraClip()`
- Assets: `loadTexture()`, `loadOBJ()`
- Objects: `createObject()`, `destroyObject()`, position/rotation/scale/color/alpha/active setters
- Rendering: `renderScene()`, `onUpdate()`

**C API** (from `src/slop3d.h`, 118 lines):
- Fixed limits: 64 textures, 128 meshes, 256 objects, 8 lights
- S3D_Vertex struct: position (xyz), normal (xyz), UV (uv)
- Column-major 4x4 matrices
- ID-based object management (all IDs are integers)

**Current game scripting pattern**: Async JS IIFE, ~35 lines for 3-cube demo. SlopScript reduces this to 17 lines (full demo) or 7 lines (spinning cube).

### Transpiler Architecture Design

A Plan agent designed the full implementation approach:

**Pipeline**: Source → Lexer → Parser → AST → CodeGen → JS string → eval()

**Runtime helpers** (~170 lines):
- `SlopVec3` — reactive vector using JS Proxy, calls engine setter on mutation
- `SlopObject` — wraps object ID, proxies property assignments to API calls
- `SlopCamera` — wraps camera state, flushes on render
- Math wrappers with degree→radian conversion

**Lexer** (~180 lines): Indentation-aware tokenizer (Python-style indent/dedent tracking)

**Parser** (~350 lines): Recursive descent, small grammar covering assets/scene/update blocks, assignments, control flow, expressions with precedence

**Code generator** (~250 lines): AST → JS targeting Slop3D API via runtime helpers

**Total estimated size**: ~1000 lines in a single `js/slopscript.js` file

---

## Research Sources

No external web research was conducted in this session. All design work was based on analysis of the existing codebase and iterative design discussion.

---

## Decisions Log

| # | Decision | Choice | Reasoning |
|---|----------|--------|-----------|
| 1 | Session scope | Design only, defer implementation | Caleb wanted to brainstorm and refine before building |
| 2 | Language name | SlopScript (`.slop`) | Fits the "slopwave" aesthetic, clear parallel to Lingo |
| 3 | Vocabulary | `model`, `skin`, `spawn`, `destroy` | Era-authentic terms that match Shockwave 3D concepts |
| 4 | Top-level structure | `assets` / `scene name` / `update` (nested) | Mirrors Shockwave project organization (cast → score → frame scripts) |
| 5 | Variable declarations | No `let` — first assignment declares | Reduces syntax noise, like Ruby/Lingo |
| 6 | Render call | Implicit (auto after update) | Eliminates a line that would appear in every program |
| 7 | Vector syntax | Bare tuples: `2.5, 0, 0` | No `vec3()` wrapper needed, cleaner code |
| 8 | Statement function calls | Colon syntax: `spawn: cube, crate` | Removes parens, colon clearly delimits name from args |
| 9 | Expression function calls | Square brackets: `sin[t] * 5` | Zero parens in the language, unique visual identity |
| 10 | String literals in assets | No quotes: `model cube = cube.obj` | Parser knows asset values are always filenames |
| 11 | Named scenes | `scene menu`, `scene level1` | Enables multi-level games |
| 12 | Scene switching | `goto: scene_name` with auto-cleanup | Destroys current scene objects, runs new scene setup |
| 13 | Update block placement | Nested inside scene (indented) | Natural scoping, no need to repeat scene name |
| 14 | Grouping in expressions | `[]` dual purpose (call + grouping) | `sin[t]` = call, `[a + b] * c` = grouping. Disambiguated by preceding identifier. |
| 15 | Implementation file | Single `js/slopscript.js` (~1000 lines) | Matches engine's compactness philosophy |

---

## Files Changed

| File | Change |
|------|--------|
| `~/.claude/plans/splendid-snacking-spring.md` | Created — full SlopScript language design document |
| `~/.claude/projects/.../memory/project_slopscript.md` | Created — memory file with key design decisions |
| `~/.claude/projects/.../memory/MEMORY.md` | Created — memory index |
| `docs/sessions/016-slopscript-language-design.md` | Created — this session log |

No engine code was modified in this session.

---

## Next Steps

1. **Implement Phase 1**: Runtime helpers (`SlopVec3`, `SlopObject`, `SlopCamera`, math wrappers) — test by hand-writing the JS that SlopScript would generate
2. **Implement Phase 2**: Indentation-aware lexer/tokenizer
3. **Implement Phase 3**: Recursive descent parser producing AST
4. **Implement Phase 4**: Code generator (AST → JS)
5. **Implement Phase 5**: Browser loader (`loadSlop()`, `<script type="text/slopscript">`)
6. **Implement Phase 6**: Rewrite demo in SlopScript, add error reporting
7. **Future**: Input handling (`key[space]`), sound, tweens, collision
8. **Future**: Electron IDE with live preview + SlopScript editor
