# Contributing to slopwave3d

## Development Philosophy: Deliberate AI-Assisted Engineering

This project is built with AI coding agents (Claude Code), but we follow a deliberate, measured approach to development. Speed is not the goal — understanding is.

### Core Principles

**1. One issue at a time.**
Work through GitHub issues sequentially. Don't jump ahead. Each issue builds on the last, and skipping steps means skipping understanding. Resist the urge to "just knock out a few more" in one session.

**2. Read every line.**
Treat all AI-generated code as if it came from a junior developer. Read through it, understand it, run it, test it. If you can't explain what a function does, don't merge it. As Addy Osmani puts it: "the LLM is an assistant, not an autonomously reliable coder."

**3. Small commits, frequent checkpoints.**
One task, one commit, one review. Don't let the AI blast through an entire feature without stopping to verify. Frequent commits create save points — if something goes wrong, you can revert without losing hours of work.

**4. Take breaks between sessions.**
Let changes sit. Come back with fresh eyes. A session log exists for every work period (see `docs/sessions/`) so you can pick up exactly where you left off without losing context. There's no rush.

**5. The "flaws" are features.**
The visual artifacts in slop3d are not bugs — they are researched, intentional design choices that reproduce how Shockwave 3D actually rendered. Each one traces back to a real technical limitation of the era:

| Artifact | Why It's Here |
|----------|---------------|
| **Affine texture warping** | Early 2000s software rasterizers commonly skipped perspective-correct texture mapping for performance. Shockwave 3D's software fallback path — which lacked several features available in its hardware-accelerated modes — likely exhibited the same behavior. Textures wobble and stretch on large polygons because UV coordinates are interpolated linearly in screen space, not divided by depth. This is the single most recognizable visual trait of era-appropriate software rasterizers. |
| **Gouraud banding (Mach bands)** | Shockwave 3D's `#standard` shader — used by the vast majority of Shockwave 3D content — used Gouraud shading: lighting computed at vertices and interpolated across faces. (Shockwave 3D also offered non-photorealistic shader types like `#painter` and `#engrave`, but these were specialized and rarely used in games.) On low-poly meshes, Gouraud shading creates visible color steps at triangle edges where interpolation gradients meet. Per-pixel lighting didn't reach consumer hardware until DirectX 8 pixel shaders (2000) and wasn't widespread until Doom 3 (2004). |
| **Z-fighting** | Our 16-bit Z-buffer gives only 65,536 depth values across the entire view frustum. Co-planar or near-co-planar surfaces compete for the same depth slots, causing flickering. 16-bit was the most commonly supported depth buffer format and the norm for software rasterizers. By 2001, mainstream GPUs supported 24-bit, but 16-bit remained the safe target for maximum compatibility — especially for browser-delivered content like Shockwave 3D. |
| **Nearest-neighbor pixelation** | Textures are sampled at the nearest texel with no interpolation, producing hard blocky pixels when surfaces are close to the camera. Bilinear filtering existed but was computationally expensive in software and not always enabled. |
| **Low resolution (320x240)** | Shockwave 3D content ran in small embedded browser windows, not fullscreen. The low resolution, scaled up with `image-rendering: pixelated`, recreates the feeling of a 2002 browser game sitting in a 400px iframe. |
| **JPG compression artifacts** | Shockwave 3D aggressively compressed textures to keep file sizes small for 56k modem delivery — Director's publish settings defaulted to JPEG quality 80, and third-party exporters like Okino's often defaulted even lower. The blurriness and blocking artifacts in JPG textures are part of the look. |

If you're working on the engine and find yourself thinking "I should fix this," check this table first. If it's listed here, leave it alone. These constraints are what make slop3d look like slop3d.

If you're unsure whether something is an intentional artifact or an actual bug, check the engine specs in CLAUDE.md or open a discussion.

**6. Document everything.**
Every development session gets a session log with full conversation details, research citations, and decision rationale. Use `/project:session-log` at the end of each session. This project is built in public — the process matters as much as the result.

**7. Understand before you generate.**
Before asking an AI to write code, make sure you understand the problem. Read the relevant issue, review existing code, and have a mental model of what the solution should look like. The AI accelerates implementation — it doesn't replace comprehension.

### The One-Session Rule

A good session tackles **one issue or less**. If an issue is too large for a single session, break it into smaller pieces. If you finish an issue early, spend the remaining time reading and understanding the code that was written — don't immediately start the next issue.

### Code Review Checklist

Before merging any AI-generated code, verify:

- [ ] You can explain what every function does without reading comments
- [ ] The code follows existing conventions (see CLAUDE.md for the `s3d_`/`S3D_` prefix rules)
- [ ] No "improvements" were added beyond what the issue requested
- [ ] The acceptance criteria from the GitHub issue are met
- [ ] The engine constraints (320x240, 128x128 textures, affine mapping, etc.) are respected
- [ ] You've actually run the code and seen it work

### Session Workflow

1. Open the next GitHub issue
2. Read the issue description and acceptance criteria
3. Read any existing code that the new work builds on
4. Work through the tasks with the AI agent
5. Test and verify the acceptance criteria
6. Commit with a clear message referencing the issue
7. Run `/project:session-log` to generate the session document
8. Push and close the issue

### Pull Requests

If contributing externally:

- Reference the GitHub issue number
- Keep changes scoped to one issue
- Include before/after screenshots for any visual changes
- Don't refactor or "clean up" code outside the scope of your PR
- Respect the intentional constraints — this engine is supposed to look like 2002

## Sources & Inspiration

This development philosophy draws from:

- [Deliberate Agentic Development](https://github.com/Matt-Hulme/deliberate-agentic-development) by Matt Hulme — structured workflows for AI agents with checkpoints, reviews, and human oversight
- [My LLM Coding Workflow Going Into 2026](https://addyosmani.com/blog/ai-coding-workflow/) by Addy Osmani — practical advice on treating AI output with the same scrutiny as human code
- [AI Coding Best Practices in 2025](https://dev.to/ranndy360/ai-coding-best-practices-in-2025-4eel) — community-sourced guidelines for working with AI coding tools
- [The Complete Guide to Agentic Coding in 2026](https://www.teamday.ai/blog/complete-guide-agentic-coding-2026) — defining boundaries for where AI fits in your workflow
