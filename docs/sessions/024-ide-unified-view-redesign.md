# Session 024: IDE Unified View Redesign — Floating Preview + Adaptive Syntax Colors

**Date:** March 13, 2026
**Participants:** Caleb Cohoon + Claude (Opus 4.6)
**Duration:** Full design and implementation session
**Repo:** [deadcast2/slopwave3d](https://github.com/deadcast2/slopwave3d)

---

## Session Goal

Redesign the slop3d IDE from a traditional split-pane layout (code editor | divider | preview) into a unified view where the code editor fills the entire window and the 3D preview floats as a chromeless, draggable panel. The editor background dynamically syncs to the scene's sky/clear color, making the code and scene feel like one continuous space. Syntax highlighting adapts to any background color using perceptually uniform color science (OKLCH).

---

## Conversation Summary

### 1. Initial Discussion — Merging Code and Preview

Caleb wanted to move away from the standard split-pane IDE layout and explore ways to make the code editor and 3D preview feel like a single unified view. Four concepts were proposed:

1. **Full-bleed canvas with frosted glass code overlay** — 3D fills the window, code editor floats on top with backdrop blur
2. **Floating code sheet** — Draggable code panel over the 3D scene (Photoshop-style tool windows)
3. **Cinema mode / code gutter** — Scene is primary, code slides in from the edge
4. **Split with shared atmosphere** — Keep side-by-side but editor background matches scene sky color

### 2. Design Decision — Editor as Environment

Caleb chose a variation: the code editor fills the entire window, and the 3D canvas preview becomes a floating panel that can be moved around. The scene's sky/clear color would be used as the editor's background color for seamless blending. This inverts the typical relationship — the editor IS the environment, the 3D scene is a window into it.

### 3. Planning Phase

Explored the codebase to understand the existing architecture:
- **Electron app** with Monaco editor, 3px draggable divider, and preview pane
- Monaco theme `slop3d-dark` defined in `slopscript.js` with hardcoded `editor.background: '#0d1117'`
- `initDivider()` in `renderer.js` handles split-pane resize
- `sky:` SlopScript statement calls `SlopRuntime.prototype.sky(r, g, b)` → `engine.setClearColor(r*255, g*255, b*255)`
- Engine default clear color is `0, 0, 0` (black) set in `s3d_init()` via `memset`
- No getter for clear color exists — interception needed on the JS side

Key insight: `defineSlopTheme()` could be extracted from `slopscript.js` as a standalone function, called both at init and dynamically when the sky color changes. Monaco supports runtime theme redefinition via `defineTheme()` + `setTheme()`.

### 4. Initial Implementation — Floating Panel + Sky Sync

**HTML restructure:** Removed `#divider` and `#preview-pane`. Editor fills `#app`. Added `#editor-bg` div for smooth CSS background transitions. Created `#preview-float` with titlebar, minimize button, canvas, error banner, status bar, and resize handle.

**CSS overhaul:** Editor uses `position: absolute; inset: 0`. Floating panel has `position: absolute; bottom: 40px; right: 24px`, rounded corners, backdrop blur, box shadow, and a drag-handle titlebar.

**JS changes:** Replaced `initDivider()` with `initFloatingPanel()` (drag, resize, minimize, window-resize clamping). Added `updateEditorBackground(r, g, b)` that converts 0-1 floats to hex, calls `defineSlopTheme()` to rebuild the Monaco theme, and updates the background div. Monkey-patched `SlopRuntime.prototype.sky` after engine loads to intercept sky color changes. Added dirty-check to avoid per-frame theme rebuilds.

**Menu:** Added "Toggle Preview" (`Cmd+P`) to the View menu in `main.js`.

### 5. Iterative Refinement — Removing Chrome

Caleb requested progressively less visual chrome on the preview panel:

**Step 1: Hover-only chrome.** Border, shadow, titlebar, status bar, minimize button, and resize handle all hidden by default. Everything fades in on hover with 250ms transitions.

**Step 2: No chrome at all.** Removed the titlebar, label, minimize button, and status bar entirely. The whole panel surface became the drag handle. Only a subtle outline and corner resize grip appeared on hover.

**Step 3: Background color sync.** Changed all default backgrounds from `#0d1117` to `#000000` to match the engine's actual default clear color (black). Updated CSS, Monaco theme, slopscript.js default, and Electron `backgroundColor`.

### 6. Resize Improvements

**All corners resizable:** Replaced the single bottom-right resize handle with 8 invisible edge/corner zones (4 edges + 4 corners), each with appropriate cursor styles.

**Simplified to corners only:** Caleb preferred just the 4 corners. Removed edge resize zones, keeping only the corner hit areas (14x14px invisible zones extending 4px outside the panel).

**Aspect ratio lock:** Corner resizing now maintains the 4:3 aspect ratio. Width drives the calculation and height follows via `height = width / (4/3)`.

### 7. Fixing the Dark Outline

After resizing, a dark outline was visible around the preview against colored backgrounds. Root cause: `border: 1px solid transparent` takes up 1px of layout space even when transparent, and nested `border-radius` on `#preview-body` and `#game-canvas` created sub-pixel gaps.

**Fix attempts:**
1. Switched to `outline` — didn't render (outline doesn't follow border-radius in Electron/Chromium)
2. Switched to `box-shadow: inset` — canvas content painted over it
3. **Final fix:** Used a `::after` pseudo-element with `pointer-events: none` and `z-index: 120`, overlaid on top of everything. Also removed the `border` entirely and dropped nested `border-radius` from body and canvas.

### 8. Adaptive Syntax Colors — First Pass

With the editor background changing dynamically, fixed syntax colors became unreadable on certain backgrounds. Initial approach: defined dark-bg and light-bg RGB color pairs for each token, with linear interpolation based on background luminance.

**Problem:** RGB interpolation doesn't account for perceptual uniformity. Mid-tone greys produced muddy colors, and blue backgrounds (which contribute only 7.2% to luminance via the 0.0722 coefficient) were misjudged as darker than they appear.

### 9. Research — Perceptually Correct Adaptive Colors

Conducted web research on adaptive syntax highlighting. Key findings:

**APCA over WCAG 2:** WCAG 2's simple luminance ratio `(L1+0.05)/(L2+0.05)` treats dark-on-light and light-on-dark as equivalent (they're not). It overstates contrast for dark colors and has no polarity awareness. APCA (Advanced Perceptual Contrast Algorithm) is polarity-aware and handles dark mode correctly.

**OKLCH over HSL:** HSL lightness doesn't correspond to perceptual lightness — a yellow and blue at the same HSL L look completely different. OKLCH (OK Lightness, Chroma, Hue) is perceptually uniform: colors at the same OKLCH L actually appear equally bright. You can adjust L for contrast while preserving hue identity.

**Key techniques for mid-grey and blue backgrounds:**
- Polarity hysteresis to prevent flickering during smooth transitions
- Maximize lightness delta on mid-greys + boost chroma for extra separation
- Hue conflict detection: when token hue is within ~30° of background hue, shift away
- Gamut clamping: reduce chroma progressively if computed color falls outside sRGB

### 10. Adaptive Syntax Colors — OKLCH Implementation

Rewrote the entire color system in `slopscript.js`:

**Color pipeline:** sRGB → linear RGB → XYZ (D65) → OKLAB → OKLCH, with full round-trip support. All conversions implemented from scratch (no dependencies).

**Token definitions:** Each token is defined by hue (H) and chroma (C) only — its color identity. Lightness is computed at runtime:
- Keywords: H=295 (purple), C=0.14
- Light keywords: H=75 (gold), C=0.14
- Built-ins: H=255 (blue), C=0.12
- Global vars: H=20 (red-orange), C=0.15
- Numbers: H=45 (orange), C=0.14
- Comments: H=230 (muted grey-blue), C=0.02
- Operators/delimiters: H=200 (cyan), C=0.10
- Identifiers: H=210 (near-neutral), C=0.01

**Polarity with hysteresis:** Light text (L > 0.82) on dark backgrounds (bg L < 0.45). Dark text (L < 0.25) on light backgrounds (bg L > 0.55). Dead zone 0.45–0.55 maintains current polarity to prevent flickering.

**Hue conflict resolution:** If token hue is within 35° of background hue and background chroma > 0.04, the token hue shifts 50° away. Chroma is also boosted to compensate.

**Mid-grey chroma boost:** Backgrounds with L between 0.35–0.65 get +0.04 chroma on non-comment/non-identifier tokens for extra color separation.

**Gamut clamping:** Iteratively reduces chroma by 15% (up to 20 iterations) until the computed OKLCH color fits in sRGB.

### 11. Final Polish — Stripping Editor Chrome

Caleb requested removal of all remaining editor visual elements for a completely naked look:
- Removed indent guide vertical lines (both Monaco config `guides.indentation: false` and theme colors set to `#00000000`)
- Removed current line highlight (both background and border set to `#00000000`)
- Removed line numbers (`lineNumbers: 'off'`)
- Removed glyph margin and fold gutter (`glyphMargin: false`, `folding: false`)
- Added 24px left margin via `lineDecorationsWidth` for visual breathing room

The final result: pure syntax-highlighted code floating on the scene's sky color, with a chromeless 3D preview that can be dragged and resized from corners.

---

## Research Findings

### OKLCH Color Space

OKLCH (OK Lightness, Chroma, Hue) is a perceptually uniform color space created by Björn Ottosson in 2020. Unlike HSL where "lightness 50%" produces vastly different perceived brightness across hues (yellow appears much brighter than blue), OKLCH L=0.5 looks equally bright regardless of hue. This makes it ideal for computing contrast-meeting colors programmatically.

- **L** (lightness): 0 = black, 1 = white, perceptually linear
- **C** (chroma): 0 = grey, higher = more saturated. Maximum depends on hue and lightness.
- **H** (hue): 0-360 degrees, perceptually uniform spacing

The conversion pipeline goes: sRGB → linearize gamma → XYZ (D65 whitepoint) → LMS (cone response) → cube root → OKLAB → polar form (OKLCH).

### APCA Contrast Algorithm

APCA (Advanced Perceptual Contrast Algorithm) by Andrew Somers is the successor to WCAG 2's contrast ratio formula. Key improvements:

- **Polarity-aware**: distinguishes text-on-background vs background-on-text. Human vision processes these differently.
- **Dark mode correct**: WCAG 2 was designed for light backgrounds and overstates contrast for dark-on-dark pairs.
- **Blue handling**: Accounts for blue's low luminance contribution (7.2%) more accurately.

Target values: |Lc| >= 75 for body text, >= 60 for secondary UI text, >= 45 for decorative elements.

### The Blue Background Problem

Blue is perceptually special because the luminance coefficients (ITU-R BT.709) weight blue at only 0.0722 vs green at 0.7152. A vivid blue `rgb(50, 50, 200)` has luminance ~0.065 despite feeling medium-bright. This causes luminance-based algorithms to select dark-mode colors when light-mode would be more readable.

Solution: Use OKLCH lightness instead of luminance for polarity decisions, and detect hue conflicts when token hue is near background hue.

### Mid-Grey Challenge

Backgrounds with OKLCH L ~0.45-0.65 are the hardest because neither light nor dark text achieves great contrast. Techniques:
1. Push token lightness as far from background as possible (toward L=0.95 or L=0.10)
2. Boost chroma — vivid colors provide perceptual separation even with modest luminance contrast
3. Use hysteresis on polarity switching to prevent flickering in the transition zone

---

## Research Sources

### Color Science & Contrast
- [OKLCH in CSS: why quit RGB-HSL](https://evilmartians.com/chronicles/oklch-in-css-why-quit-rgb-hsl) — Evil Martians
- [OKLCH Perceived Lightness](https://www.stefanjudis.com/today-i-learned/oklch-perceived-lightness/) — Stefan Judis
- [Accessible Palette: Stop Using HSL](https://www.wildbit.com/blog/accessible-palette-stop-using-hsl-for-color-systems) — Wildbit
- [Exploring the OKLCH Ecosystem](https://evilmartians.com/chronicles/exploring-the-oklch-ecosystem-and-its-tools) — Evil Martians
- [apcach GitHub](https://github.com/antiflasher/apcach) — OKLCH + APCA color generation library

### APCA & WCAG
- [APCA in a Nutshell](https://git.apcacontrast.com/documentation/APCA_in_a_Nutshell.html)
- [Why APCA as a New Contrast Method](https://git.apcacontrast.com/documentation/WhyAPCA.html)
- [APCA Easy Intro](https://git.apcacontrast.com/documentation/APCAeasyIntro.html)
- [APCA Contrast Calculator](https://apcacontrast.com/)
- [SAPC-APCA GitHub](https://github.com/Myndex/SAPC-APCA) — reference JS implementation
- [W3C WCAG 2.0 Contrast Technique G17](https://www.w3.org/TR/WCAG20-TECHS/G17.html)
- [WCAG Contrast Formula Explained](https://mallonbacka.com/blog/2023/03/wcag-contrast-formula/) — Matthew Hallonbacka

### Accessibility & Luminance
- [W3C Relative Luminance](https://www.w3.org/WAI/GL/wiki/Relative_luminance)
- [NASA Luminance Contrast Color Guidelines](https://colorusage.arc.nasa.gov/guidelines_lum_cont.php)
- [MDN: Colors and Luminance Accessibility](https://developer.mozilla.org/en-US/docs/Web/Accessibility/Guides/Colors_and_Luminance)
- [WebAIM: Contrast and Color Accessibility](https://webaim.org/articles/contrast/)
- [Impact of Color and Polarity on Visual Resolution](https://pmc.ncbi.nlm.nih.gov/articles/PMC9185210/) — PMC

### Adaptive Syntax Highlighting
- [Qt Adaptive Coloring for Syntax Highlighting](https://doc.qt.io/archives/qq/qq26-adaptivecoloring.html) — Qt Quarterly

---

## Decisions Log

| # | Decision | Choice | Reasoning |
|---|----------|--------|-----------|
| 1 | IDE layout model | Editor fills window, preview floats | Creates a unified view where code and scene share the same visual space |
| 2 | Background sync mechanism | Monkey-patch `SlopRuntime.prototype.sky` | Non-invasive — doesn't modify engine code, intercepts at the JS bridge layer |
| 3 | Default background color | `#000000` (black) | Matches engine's actual default clear color (`memset` zeros in `s3d_init`) |
| 4 | Preview panel chrome | Chromeless by default, subtle outline on hover | Maximizes the seamless illusion of code floating in the scene |
| 5 | Hover outline technique | `::after` pseudo-element with `pointer-events: none` | `border` caused layout gaps, `outline` didn't follow border-radius, `box-shadow: inset` was painted over by canvas |
| 6 | Resize behavior | 4 corners only, locked to 4:3 aspect ratio | Matches the canvas aspect ratio, simpler than 8-edge resize |
| 7 | Drag behavior | Entire panel surface is draggable | No titlebar needed — keeps the chromeless look |
| 8 | Color space for adaptive syntax | OKLCH (not HSL, not RGB) | Perceptually uniform — same L value looks equally bright across all hues |
| 9 | Palette strategy | Per-token lightness computation from background | Better than blending between two fixed palettes — handles arbitrary backgrounds |
| 10 | Polarity switching | Hysteresis band at OKLCH L 0.45–0.55 | Prevents color flickering during smooth sky color transitions |
| 11 | Hue conflict handling | Shift token hue 50° away when within 35° of background | Prevents blue keywords from disappearing on blue backgrounds |
| 12 | Mid-grey strategy | Boost chroma +0.04 on non-comment tokens | Vivid color provides separation when luminance contrast is limited |
| 13 | Gamut clamping | Iterative chroma reduction (×0.85 per step) | Simple and reliable — always produces a valid sRGB color |
| 14 | Editor chrome removal | No line numbers, indent guides, line highlight, glyph margin, or fold gutter | Maximizes the "code floating in space" aesthetic |
| 15 | Left margin | 24px via `lineDecorationsWidth` | Visual breathing room without any visible chrome |

---

## Files Changed

| File | Action | Description |
|------|--------|-------------|
| `ide/renderer/index.html` | Modified | Removed split layout (divider + preview-pane). Added `#editor-bg` div, `#preview-float` with canvas, error banner, and 4 corner resize zones |
| `ide/renderer/styles.css` | Rewritten | Full layout overhaul — editor fills window, floating panel with chromeless default + hover-only outline via `::after`, corner resize hit areas, adaptive status/error styling |
| `ide/renderer/renderer.js` | Rewritten | Removed `initDivider()`. Added `initFloatingPanel()` (drag, corner resize with 4:3 lock, window-resize clamping). Added `updateEditorBackground()` with dirty-check. Monkey-patches `SlopRuntime.prototype.sky`. Stores `monacoRef` for runtime theme updates. Stripped editor chrome (no line numbers, guides, glyph margin, folding) |
| `ide/renderer/slopscript.js` | Rewritten | Full OKLCH color science pipeline (sRGB↔linear↔XYZ↔OKLAB↔OKLCH). Token colors defined by hue+chroma identity. `defineSlopTheme()` computes per-token lightness from background, handles hue conflicts, mid-grey chroma boost, polarity hysteresis, and gamut clamping. Adaptive UI colors for all editor chrome |
| `ide/main.js` | Modified | Added "Toggle Preview" menu item (`Cmd+P`) under View. Changed `backgroundColor` to `#000000` |

---

## Next Steps

- **Persist panel position/size** — Save the floating preview's position and dimensions to user preferences so it restores on relaunch
- **Keyboard shortcut for preview position** — Snap-to-corner shortcuts (e.g., `Cmd+1` through `Cmd+4` for each corner)
- **Smooth theme transitions** — Monaco's `defineTheme`+`setTheme` causes a brief repaint flash; investigate whether CSS-based text coloring could provide smoother transitions
- **APCA contrast verification** — Add actual APCA Lc computation to verify tokens meet |Lc| >= 75 threshold, with fallback to near-neutral colors
- **Test with diverse sky colors** — Systematically test the adaptive colors against red, green, white, mid-grey, saturated blue, and dark teal backgrounds to identify any remaining readability issues
