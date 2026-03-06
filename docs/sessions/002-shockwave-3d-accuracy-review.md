# Session 002: Shockwave 3D Accuracy Review

**Date:** March 6, 2026
**Participants:** Caleb Cohoon + Claude (Opus 4.6)
**Duration:** Documentation review session
**Repo:** [deadcast2/slopwave3d](https://github.com/deadcast2/slopwave3d)

---

## Session Goal

Review the README, CLAUDE.md, and CONTRIBUTING.md for factual accuracy of all Shockwave 3D claims. The project references Shockwave 3D's technical characteristics extensively to justify its intentional design constraints, and we wanted to ensure nothing was misleading or inaccurate.

---

## Conversation Summary

### 1. Initial Review

We read all three documentation files (README.md, CLAUDE.md, CONTRIBUTING.md) and performed an initial assessment of every claim that referenced Shockwave 3D's actual technology or history. The initial review identified five areas of concern:

1. **"Intel's IFX Toolkit"** — potentially incorrect name for the 3D engine
2. **"Shockwave 3D's only shading model"** — possibly oversimplified
3. **"50% compression quality"** — specific number that needed verification
4. **Affine texture mapping attribution** — claimed as confirmed Shockwave 3D behavior
5. **"16-bit was standard, 24-bit was a luxury"** — potentially misleading about the GPU landscape

### 2. Parallel Web Research

Five research agents were launched in parallel, each investigating one of the flagged claims. Each agent conducted multiple web searches and cross-referenced sources before reporting back. This was the core of the session — replacing assumptions with verified facts.

*(Full research findings below)*

### 3. Findings Discussion

After all five agents returned, we reviewed the findings:

- **Intel's IFX Toolkit**: The name was technically correct as an internal/developer name, but the official marketing name was "Intel Internet 3D Graphics software." Both names refer to the same technology.
- **Gouraud as "only" shading model**: Oversimplified. Shockwave 3D had four shader types (`#standard`, `#painter`, `#engrave`, `#newsprint`), plus a `flat` property. Gouraud was the standard for `#standard` shader, which was dominant, but calling it "only" was inaccurate.
- **50% compression quality**: Incorrect. Director's own Publish Settings defaulted to JPEG quality **80**. The "50" value came from Okino's third-party Shockwave 3D exporter, not Director itself.
- **Affine texture mapping**: Plausible but unverifiable. No source explicitly confirms Shockwave 3D's software renderer used affine mapping. It's a reasonable inference based on era norms, but was being stated as fact.
- **16-bit Z-buffer**: True for software rasterizers and as a compatibility baseline, but by 2001 mainstream GPUs (GeForce2 MX, Radeon 7200) already supported 24-bit. "24-bit was a luxury" overstated the case.

### 4. Edits Applied

We applied targeted edits to all three files, correcting or softening each claim to match what the research actually supports. The philosophy: keep the aesthetic justifications intact, but be honest about what's confirmed vs. inferred.

### 5. Reflection on Constraints

The session ended with a brief discussion about the value of creative constraints in game development. The project's hard limits (320x240, 128x128 textures, 10k polys, etc.) force resourcefulness and creativity — similar to how PS1 developers, Shockwave developers, and demoscene programmers made remarkable things within tight technical boundaries.

---

## Research Findings

### Intel 3D Engine Naming

Intel used two names for the technology behind Shockwave 3D's rendering engine:

- **"Intel Internet 3D Graphics"** (or "Intel Internet 3D Graphics software") — the official marketing/public-facing name used in Intel's press releases when announcing the Macromedia partnership (2000) and Director 8.5 (2001).
- **"IFX Toolkit"** (versions 1 and 2) — the internal/developer-facing name for the underlying software toolkit, created by Intel's Architecture Lab in the late 1990s. Originally targeted at game developers for compressed, low-poly mesh models with skeletal animation. The Shockwave 3D `.w3d` file format was based on this toolkit, and it later evolved into the U3D file format (circa 2004).

**Verdict:** "Intel's IFX Toolkit" was technically correct but incomplete. Updated to include the official name.

### Shockwave 3D Shading Models

Shockwave 3D supported four shader types via the `newShader` Lingo command:

1. **`#standard`** — The default. Texture mapping, specular highlights, transparency, and Gouraud-interpolated lighting. Used by the vast majority of Shockwave 3D content.
2. **`#painter`** — Non-photorealistic/toon shader with `colorSteps`, `highlightPercentage`, `shadowPercentage`, and `style` properties. This was the "toon shading" feature marketed with Director 8.5.
3. **`#engrave`** — Non-photorealistic shader simulating an engraved/etched look.
4. **`#newsprint`** — Non-photorealistic shader simulating halftone/newsprint effects.

Additionally, a `flat` property and `renderStyle` property existed in the Lingo 3D API, suggesting flat shading was available as an alternative to Gouraud interpolation. Director 11 (2007) introduced DirectX 9 native rendering, but no evidence was found that programmable GPU shaders were exposed to Lingo scripters.

**Verdict:** Gouraud was the dominant shading model in practice, but calling it the "only" model was inaccurate. Updated to "standard shading model" with acknowledgment of NPR shader types.

### Texture Compression Defaults

The claim that Shockwave 3D "defaulted to 50% compression quality" was traced to two different tools:

- **Director's own Publish Settings** defaulted to JPEG quality **80** (on a 0-100 scale), described in *Macromedia Director MX 2004: Training from the Source* as "a fairly low level of compression, resulting in a fairly high-quality image."
- **Okino PolyTrans Shockwave 3D exporter** used a texture compression slider that defaulted to **50**.
- **Autodesk 3ds Max Shockwave 3D exporter** had similar sliders (0.1 to 100 range) but documentation only explicitly states a default for geometry quality (25), not texture quality.

**Verdict:** The "50%" claim was wrong for Director itself. Updated to reference Director's actual default (80) and note that third-party exporters defaulted lower.

### Affine Texture Mapping

Extensive searching found no source that explicitly confirms Shockwave 3D's software renderer used affine (non-perspective-correct) texture mapping. What is established:

- Shockwave 3D had four renderer modes: `#openGL`, `#directX7_0`, `#directX5_2`, and `#software`.
- The software renderer was documented as lacking several features available in hardware modes (no wireframe, no point rendering, no near-filtering/anti-aliasing, `TextureRepeat` always forced true).
- Hardware paths (OpenGL/DirectX) would have used perspective-correct texturing via the GPU.
- Software rasterizers of the era commonly used affine mapping for performance, but some (including Quake's 1996 renderer) implemented perspective correction using the "every 8-16 pixels" subdivision trick.

**Verdict:** The claim is plausible and era-appropriate, but cannot be stated as confirmed Shockwave 3D behavior. Updated to frame affine mapping as characteristic of early 2000s software rasterizers generally, noting Shockwave 3D's software fallback "likely" exhibited similar behavior.

### 16-bit Z-Buffer

The research confirmed a nuanced picture:

- **Microsoft's Direct3D 9 documentation** states: "16-bit depth buffers are the most commonly supported bit-depth" — confirming 16-bit as the compatibility baseline.
- **3dfx Voodoo1/2/3** (1996-1999, widely used into 2001-2002): 16-bit z-buffer only. Z-fighting was a well-documented problem.
- **NVIDIA GeForce 256, GeForce2** (1999-2001): Supported 24-bit z-buffer (D24S8). The GeForce2 MX was one of the most popular budget GPUs of 2001-2002 — not a luxury card.
- **ATI Radeon 7200 / R100** (2000): Also supported 24-bit depth buffers.
- **Quake 2's software renderer** used 16-bit z-buffer (storing 1/Z as uint16), per Fabien Sanglard's source code review.
- **Steve Baker's "Learning to Love your Z-buffer"**: "Many of the early 3D adaptors for the PC have a 16 bit Z buffer, some others have 24 bits — and the very best have 32 bits."
- **Shockwave 3D** had a `depthBufferDepth` Lingo property to query active depth buffer precision. When using hardware renderers, it would use whatever the GPU provided. The software fallback almost certainly used 16-bit.

**Verdict:** 16-bit was the norm for software rasterizers and the lowest common denominator for compatibility, but mainstream GPUs had 24-bit by 2001. "24-bit was a luxury" was inaccurate. Updated to reflect the nuance — 16-bit was the safe target especially for browser-delivered content.

---

## Research Sources

### Intel / Shockwave 3D Engine
- [Intel press release, April 2001 — "Macromedia And Intel Bring Web 3D To The Mainstream"](https://www.intel.com/pressroom/archive/releases/2001/20010410tech.htm)
- [Intel press release, July 2000 — "Intel And Macromedia Team Up To Popularize 3D Technology On The Web"](https://www.intel.com/pressroom/archive/releases/2000/in072500.htm)
- [Okino Shockwave-3D converter documentation](https://www.okino.com/conv/exp_sw3d.htm) — IFX Toolkit v1/v2 history and evolution into U3D
- [Animation World Network — "Intel & Macromedia Partner To Bring 3D Graphics To Shockwave Player"](https://www.awn.com/news/intel-macromedia-partner-bring-3d-graphics-shockwave-player)

### Shockwave 3D Shading & Rendering
- [Director Shockwave 3D Lingo (Dean's Director Tutorials)](http://www.deansdirectortutorials.com/3D/3Dlingo.htm)
- [Director Shockwave 3D Casts Definitions](http://www.deansdirectortutorials.com/3D/3DCastMembers.htm)
- [Macromedia Director MX Lingo Dictionary (ManualsLib)](https://www.manualslib.com/manual/389805/Macromedia-Director-Mx-Lingo-Dictionary.html)
- [Adobe Director 11 Scripting Dictionary](https://usermanual.wiki/adobe/director11scripting.1321410167/amp)
- [Adobe Director 11.5 Scripting Dictionary](https://usermanual.wiki/adobe/director115scripting.1474607794/amp)
- [Macromedia Director Lingo Syntax (vim syntax file)](https://github.com/vim-scripts/Macromedia-Director-Lingo-Syntax/blob/master/syntax/lingo.vim)
- [Macromedia Director 8.5 Shockwave Studio Now Available (Game Developer)](https://www.gamedeveloper.com/game-platforms/macromedia-director-8-5-shockwave-studio-now-available)
- [Adobe Shockwave (Wikipedia)](https://en.wikipedia.org/wiki/Adobe_Shockwave)
- [Macromedia Director (Macromedia Wiki)](https://macromedia.fandom.com/wiki/Macromedia_Director)

### Texture Compression
- [Understanding Publish Settings — Macromedia Director MX 2004: Training from the Source (FlyLib)](https://flylib.com/books/en/2.874.1.112/1/)
- [Shockwave-3D 3D File Export Converter — Okino](https://www.okino.com/conv/exp_sw3d.htm)
- [Shockwave 3D Scene Export Options Dialog — Autodesk](https://help.autodesk.com/cloudhelp/2016/ENU/3DSMax/files/GUID-43AA5F8F-EDA0-4185-A017-D44F911DE447.htm)

### Shockwave 3D Renderer Paths
- [Macromedia Director 8.5 Release Notes (Adobe)](https://www.adobe.com/support/director/releasenotes/8/releasenotes_85.htm)
- [Macromedia Director 8.5 Enters the Third Dimension (CreativePro)](https://creativepro.com/macromedia-director-8-5-enters-the-third-dimension/)
- [How Shockwave 3-D Technology Works (HowStuffWorks)](https://computer.howstuffworks.com/shockwave.htm)
- [Director Online — Intel Web 3D Engine](https://director-online.dasdeck.com/buildArticle.php?id=891)
- [The Story Of Shockwave And 3D Webgames (Medium/BlueMaxima's Flashpoint)](https://medium.com/bluemaximas-flashpoint/the-story-of-shockwave-and-3d-webgames-8f3647865a7)
- [Director 8.5 Lingo Reference (InformIT)](https://www.informit.com/articles/article.aspx?p=28421)
- [Texture Mapping (Wikipedia)](https://en.wikipedia.org/wiki/Texture_mapping)

### Z-Buffer Depth Research
- [Depth Buffers (Direct3D 9) — Microsoft Learn](https://learn.microsoft.com/en-us/windows/win32/direct3d9/depth-buffers)
- [Voodoo3 (Wikipedia)](https://en.wikipedia.org/wiki/Voodoo3)
- [Voodoo2 (Wikipedia)](https://en.wikipedia.org/wiki/Voodoo2)
- [Depth Buffer 16 bits — 3dfx Voodoo 3 (HardWare.fr)](https://www.hardware.fr/articles/29-3/depth-buffer-16-bits.html)
- [z-buffer and GeForce 2 (Khronos Forums)](https://community.khronos.org/t/z-buffer-and-geforce-2/36327)
- [GeForce 2 series (Wikipedia)](https://en.wikipedia.org/wiki/GeForce_2_series)
- [Quake 2 Source Code Review — Software Renderer (Fabien Sanglard)](https://fabiensanglard.net/quake2/quake2_software_renderer.php)
- [Learning to Love your Z-buffer (sjbaker.org)](https://www.sjbaker.org/steve/omniv/love_your_z_buffer.html)

---

## Decisions Log

| Decision | Choice | Reasoning |
|----------|--------|-----------|
| Intel engine name | Changed to "Intel's Internet 3D Graphics software (internally known as the IFX Toolkit)" | "IFX Toolkit" was the internal/developer name; the official marketing name was "Intel Internet 3D Graphics." Including both is most accurate. |
| Gouraud shading description | Changed "only shading model" to "standard shading model" | Shockwave 3D had four shader types (`#standard`, `#painter`, `#engrave`, `#newsprint`). Gouraud via `#standard` was dominant but not exclusive. |
| Texture compression | Removed "50%" claim, replaced with Director's actual default (quality 80) | The 50% value came from Okino's third-party exporter, not Director itself. Director MX 2004 defaulted to JPEG quality 80. |
| Affine texture mapping | Reframed as era-typical rather than confirmed Shockwave behavior | No source explicitly confirms Shockwave 3D's software renderer used affine mapping. Plausible but unverifiable — now described as characteristic of early 2000s software rasterizers. |
| Z-buffer description | Replaced "24-bit was a luxury" with nuanced explanation | 16-bit was the software rasterizer norm and compatibility target, but mainstream GPUs (GeForce2 MX, Radeon) had 24-bit by 2001. |
| Session 001 claims | Left unchanged | Session 001 is a historical record of what was discussed at the time. The corrections live in the project documentation files and in this session log. |

---

## Files Changed

| File | Change |
|------|--------|
| `README.md` | Updated Intel engine name, affine mapping description, Gouraud shading description |
| `CLAUDE.md` | Updated constraint table: affine mapping ("era-authentic software rasterizer warping") and Gouraud ("standard shading model") |
| `CONTRIBUTING.md` | Updated affine mapping, Gouraud banding, Z-fighting, and JPG compression descriptions in the "flaws are features" table |
| `docs/sessions/002-shockwave-3d-accuracy-review.md` | Created this session log |

---

## Next Steps

Continue with engine implementation starting from the next open GitHub issue. The documentation is now on solid factual ground — all Shockwave 3D claims are either verified or properly qualified as era-appropriate inferences.

**Note:** Session 001's research section contains some of the claims that were corrected in this session (e.g., "50% compression default," "Gouraud shading only"). Session 001 is preserved as-is since it's a historical record of the original research. The corrected information lives in the project documentation files (README, CLAUDE.md, CONTRIBUTING.md) and in this session log.
