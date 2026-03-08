<p align="center">
<img width="600" alt="slopwave3d-logo" src="docs/logo.png" />
</p>

<p align="center">
<a href="https://github.com/qwibitai/nanoclaw/tree/main/repo-tokens"><img src=".github/badges/tokens.svg" alt="token count" valign="middle"></a>
</p>

A 3D game engine for people who think graphics peaked in 2002.

**slopwave3d** (slop3d) is a software rasterizer built in C that compiles to WebAssembly — no GPU, no shaders, no excuses. It also ships as an ActiveX control for Internet Explorer on Windows XP, because authenticity matters. It renders at 320x240 with 128x128 JPG textures, affine texture mapping, and Gouraud shading. Every visual "flaw" is intentional.

This is what happens when you build a 3D engine that fits in a single AI context window and refuses to look good.

## The Aesthetic

Remember Shockwave 3D? LEGO Backlot? Those weird browser games that ran in a 400px window and looked like a PlayStation had a fever dream? That's what we're going for.

- **Affine texture warping** — textures wobble and stretch on large polygons because we don't do perspective correction. This was characteristic of early 2000s software rasterizers, and Shockwave 3D's software fallback path likely exhibited the same behavior.
- **Gouraud shading** — lighting computed at vertices, interpolated across faces. Smooth gradients with Mach banding artifacts. This was Shockwave 3D's standard shading model for its `#standard` shader, used by the vast majority of Shockwave 3D content.
- **16-bit Z-buffer** — co-planar surfaces fight for their lives. Z-fighting isn't a bug, it's a feature.
- **Nearest-neighbor filtering** — pixels are chunky. Textures are crunchy. The year is 2002 and we are free.
- **320x240** — scaled up to your display with `image-rendering: pixelated`. Every pixel visible. Every pixel earned.

## Architecture

```
  Modern Browser                    Internet Explorer 6
┌─────────────────────┐          ┌──────────────────────┐
│  Your Game (JS)     │          │  Your Game (JScript) │
├─────────────────────┤          ├──────────────────────┤
│  slop3d.js (JS API) │          │  IDispatch (COM)     │
├─────────────────────┤          ├──────────────────────┤
│  WASM (Emscripten)  │          │  ActiveX (GDI)       │
├─────────────────────┤          ├──────────────────────┤
│  slop3d.c (C Core)  │          │  slop3d.c (C Core)   │
└─────────────────────┘          └──────────────────────┘
```

This mirrors how Shockwave 3D actually worked: a compiled engine (Intel's Internet 3D Graphics software, internally known as the IFX Toolkit) with a scripting layer on top (Lingo). Here, C is the engine and JavaScript is the scripting language. The ActiveX control delivers the same engine to IE on Windows XP via COM — built with Visual C++ 6.0 for maximum era authenticity.

## Specs

| | |
| --- | --- |
| Resolution | 320x240 |
| Textures | 128x128 max, JPG only, nearest-neighbor |
| Shading | Gouraud (vertex-lit) |
| Texture Mapping | Affine (no perspective correction) |
| Z-Buffer | 16-bit |
| Lights | 8 slots: ambient, directional, point, spot |
| Fog | Linear |
| Max Textures | 64 |
| Max Meshes | 128 |
| Max Objects | 256 |
| Poly Budget | ~10,000 per scene |
| Total Engine Size | ~2,700 lines |

## Build

### WASM (modern browsers)

**Prerequisites:** [Emscripten](https://emscripten.org/docs/getting_started/downloads.html) (`emcc` in PATH), a C compiler for tests, optionally [Prettier](https://prettier.io/) and Python 3.

```bash
make          # Build WASM output (web/slop3d_wasm.js + .wasm)
make serve    # Build + start local server on http://localhost:8080
make test     # Build and run C unit tests natively
make fmt      # Auto-format C (clang-format) and JS (prettier)
make clean    # Remove all build outputs
```

After building, open `http://localhost:8080/web/index.html` in a browser.

### ActiveX (Internet Explorer / Windows XP)

**Prerequisites:** Visual C++ 6.0 on Windows XP (or compatible).

```batch
cd activex
build.bat           REM Compile slop3d.dll
register.bat        REM Register with regsvr32
```

Open `activex/test.html` in Internet Explorer. Use `unregister.bat` to remove.

## Quick Start

```javascript
const engine = new Slop3D('game-canvas');
await engine.init();

const tex = await engine.loadTexture('crate.jpg');
const mesh = await engine.loadOBJ('cube.obj');
const obj = engine.createObject(mesh, tex);

engine.setCamera([0, 2, -5], [0, 0, 0], [0, 1, 0]);
engine.setDirectionalLight(0, [1, -1, 0.5], [1, 1, 1]);
engine.setFog(true, [0.2, 0.2, 0.3], 5, 20);

let angle = 0;
engine.onUpdate((dt) => {
    angle += dt;
    engine.setRotation(obj, 0, angle, 0);
});

engine.start();
```

## Why?

- The entire engine fits in an AI context window (~20K tokens). Every line is visible, every function is reachable.
- No build complexity. One C file, one JS file, one `make` command.
- Shockwave 3D died in 2019. This is its ghost, haunting your browser — and Internet Explorer.
- Sometimes you don't want 4K ray-traced reflections. Sometimes you want 128 pixels of a crate texture wobbling on a polygon.

## Development

This engine is being built incrementally and publicly with AI coding agents, following a [deliberate development philosophy](CONTRIBUTING.md) — one issue at a time, read every line, no rushing.

Each development session is documented in [`docs/sessions/`](docs/sessions/) with full conversation logs, research citations, and decision rationale. Progress is tracked through [GitHub Issues](https://github.com/deadcast2/slopwave3d/issues).

## Credits

- Token count badge by [qwibitai/nanoclaw](https://github.com/qwibitai/nanoclaw/tree/main/repo-tokens)

## License

[Unlicense](LICENSE) (public domain)
