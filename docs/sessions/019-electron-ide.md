# Session 019: Electron IDE — Live SlopScript Editor

**Date:** March 12, 2026
**Participants:** Caleb Cohoon + Claude (Opus 4.6)
**Duration:** Full implementation session
**Repo:** [deadcast2/slopwave3d](https://github.com/deadcast2/slopwave3d)

---

## Session Goal

Build an Electron-based IDE for slop3d with a Monaco code editor on the left and a live engine preview canvas on the right. As you type SlopScript, the scene updates after a short debounce. Errors show a non-blocking banner while the previous working scene keeps running. Native file open/save dialogs for `.slop` files.

---

## Conversation Summary

### 1. Initial Idea & Discussion

Caleb proposed building an Electron-based IDE with two panels: a code editor and a live engine preview. We discussed the concept and identified that this mirrors the original Macromedia Director authoring experience — a stage view alongside a Lingo script editor.

Key discussion points:
- SlopScript already transpiles client-side with zero build step, making live preview straightforward
- Electron gives native file system access for `.slop`, `.obj`, and `.jpg` files
- Considered a pure web IDE alternative (CodePen/Shadertoy style) but chose Electron for the desktop app feel
- Monaco Editor chosen for the code editor with custom SlopScript syntax highlighting
- Debounced re-transpile on edit (~500ms) with error-resilient hot-reload

### 2. Planning Phase

Explored the codebase to understand the full SlopScript lifecycle:
- `SlopScript.run()` (js/slop3d.js:1377-1400) creates engine, transpiles, and executes
- Transpiler pipeline: `slopLex → slopParse → slopGenerate → new Function()`
- No cleanup/reset API existed — approach is to `engine.stop()` and create a fresh instance
- `SlopScript.init()` auto-detects `<script type="text/slopscript">` tags but is harmless in the IDE (finds none)

Key design decision: the IDE reimplements the `SlopScript.run()` flow inline rather than modifying the engine code, allowing it to capture the engine reference for `stop()`/restart.

### 3. Initial Implementation

Created the full Electron app structure:
- **Main process** (`ide/main.js`): BrowserWindow with `webSecurity: false` for `file://` asset loading, native File menu (New/Open/Save/Save As), IPC handlers for file dialogs, window title tracking with dirty state
- **Preload** (`ide/preload.js`): Secure context bridge exposing file operations
- **Renderer HTML** (`ide/renderer/index.html`): Split-pane layout with editor, divider, and preview pane
- **Renderer JS** (`ide/renderer/renderer.js`): Monaco initialization, debounced live preview, error handling
- **SlopScript language** (`ide/renderer/slopscript.js`): Monaco Monarch tokenizer with keyword/builtin/light highlighting and a custom dark theme
- **Styles** (`ide/renderer/styles.css`): Dark theme with `#0d1117` background, `image-rendering: pixelated` on canvas

### 4. Debugging — Editor Not Working

The app opened but Monaco didn't accept input and the demo scene didn't render. Root cause analysis identified the `<base>` tag as the culprit:

**Problem:** `loadEngine()` set `<base href="file:///path/to/web/">` before Monaco loaded. This caused Monaco's AMD `require()` to resolve module paths relative to the `web/` directory instead of `ide/renderer/`, breaking all Monaco internal module loading.

**Fix (3 changes):**
1. **Removed the `<base>` tag entirely** — it was a sledgehammer that broke everything
2. **Monkey-patched `Slop3D.prototype.loadOBJ` and `loadTexture`** after engine loading to prepend absolute `file://` paths to relative asset URLs (replacing what `<base>` was supposed to do)
3. **Reordered initialization**: Monaco loads first (with correct relative paths), then engine loads second

Also added `MonacoEnvironment.getWorkerUrl` with a data URL bootstrap for Electron worker compatibility.

### 5. Splitter Bug Fix

The resizable divider could be dragged too far right, causing the preview pane to squeeze below its CSS `min-width`, pushing the divider off-screen with no way to recover.

**Fix:**
- Added `flex-shrink: 0` and `z-index: 10` to the divider CSS so it never collapses
- Changed the clamp logic to enforce `minPreview = 360px` (was using `340` without accounting for the divider width)

### 6. Error Display — Overlay to Banner

Initially errors showed as a full overlay covering the canvas. Caleb wanted the scene to remain fully visible during errors.

**Iteration 1:** Made the overlay semi-transparent with `pointer-events: none` so the scene was partially visible behind it.

**Iteration 2 (final):** Replaced the overlay with a compact banner between the canvas and status bar. Red circle with `!` icon, error text truncated with ellipsis. Scene stays completely uncovered.

Additionally fixed the error recovery logic: on transpile error, the previous engine keeps running. On runtime error, the old engine is preserved (new engine is set up first, old one only stopped after the new one succeeds).

### 7. Canvas Scaling

The canvas was fixed at 384x288 (1.5x native 320x240). Changed to scale dynamically to fill the preview pane while maintaining 4:3 aspect ratio using `width: 100%; aspect-ratio: 4/3; max-height: 100%`. Default preview pane width increased from 420px to 520px.

### 8. App Name

Attempted to change the macOS toolbar name from "Electron" to "Slopwave3D" via the menu label and `productName` in package.json. The in-app menu updated correctly, but the macOS system menu bar still shows "Electron" — this is a known limitation when running with `electron .` in development. A proper `.app` bundle via `electron-builder` or `electron-forge` is needed to fix this. Decided not to pursue packaging at this time.

---

## Research Findings

### Electron + Monaco Integration

- Monaco's AMD loader resolves module paths relative to `document.baseURI` at the time of `require()` calls, not at the time the loader script was parsed. Dynamic `<base>` tag changes after page load affect all subsequent AMD module resolution.
- In Electron with `contextIsolation: true`, scripts loaded via `<script>` tags or `document.head.appendChild()` run in the renderer's main world — global functions are accessible. `contextIsolation` only isolates the preload script.
- Monaco Web Workers in Electron need a data URL bootstrap with `importScripts()` pointing to the worker main file using absolute paths.
- Emscripten's WASM loader captures `scriptDirectory` from `document.currentScript.src` at load time and uses it to locate the `.wasm` file. Dynamic `<script>` tags with absolute `file://` src attributes work correctly for this.
- `fetch()` with `file://` URLs works in Electron with `webSecurity: false`. `WebAssembly.instantiateStreaming` may fail (no MIME type from file://), but Emscripten falls back to `WebAssembly.instantiate` with ArrayBuffer.

### macOS Electron App Naming

- The macOS system menu bar app name comes from the Electron binary's `Info.plist`, not from `app.name` or menu labels
- Setting `productName` in `package.json` and the first menu label to "Slopwave3D" updates the About dialog and in-app menu but not the system toolbar
- Proper app naming requires packaging with `electron-builder` or `electron-forge` to create a `.app` bundle

---

## Research Sources

No external web research was conducted. All findings came from direct codebase exploration and Electron/Monaco domain knowledge.

---

## Decisions Log

| # | Decision | Choice | Reasoning |
|---|----------|--------|-----------|
| 1 | Framework | Electron | Native file system access, desktop app feel, engine already runs in Chromium |
| 2 | Code editor | Monaco Editor | Feature-rich, custom language support via Monarch tokenizer, dark theme |
| 3 | Asset path resolution | Monkey-patch engine prototype | `<base>` tag approach broke Monaco; patching `loadOBJ`/`loadTexture` is isolated and doesn't modify engine source |
| 4 | Engine code changes | None — reimplement `SlopScript.run()` inline | Keeps `js/slop3d.js` unchanged; IDE captures engine ref for stop/restart |
| 5 | Error display | Banner below canvas | Overlay covered the scene; banner keeps preview fully visible |
| 6 | Error recovery | Keep previous engine running | On transpile error, old scene stays. On runtime error, old engine preserved (new started first, old stopped only on success) |
| 7 | Canvas sizing | Dynamic with aspect-ratio | Scales to fill preview pane while maintaining 4:3; better than fixed pixel dimensions |
| 8 | WASM reload strategy | Fresh `createSlop3D()` per reload | Acceptable at 500ms debounce; simpler than caching compiled modules |
| 9 | App packaging | Deferred | macOS toolbar name issue is dev-mode only; packaging not needed yet |

---

## Files Changed

### New Files

| File | Description |
|------|-------------|
| `ide/package.json` | Electron app manifest — deps: electron, monaco-editor |
| `ide/main.js` | Main process: window, menus, IPC file handlers, title tracking |
| `ide/preload.js` | Context bridge for secure IPC |
| `ide/renderer/index.html` | Split-pane layout: editor + divider + preview |
| `ide/renderer/renderer.js` | Core logic: Monaco init, debounced live preview, error handling |
| `ide/renderer/slopscript.js` | Monaco Monarch tokenizer + dark theme for SlopScript |
| `ide/renderer/styles.css` | Dark theme, resizable split pane, error banner |

### Modified Files

| File | Change |
|------|--------|
| `Makefile` | Added `ide` target: `cd ide && npm install && npm start` |
| `.gitignore` | Added `ide/node_modules/` |

---

## Next Steps

- **File tree sidebar** — browse and manage assets (models, textures, scripts) within a project
- **Tab support** — edit multiple `.slop` files simultaneously
- **App packaging** — use `electron-builder` to create a proper `Slopwave3D.app` bundle (fixes macOS toolbar name)
- **SlopScript autocomplete** — Monaco completion provider for keywords, builtins, and spawned object names
- **Error line highlighting** — parse error line numbers from transpiler and highlight the offending line in Monaco
- **Asset preview** — click a texture or model in the file tree to preview it
