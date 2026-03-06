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
The affine texture warping, z-fighting, Gouraud banding, and nearest-neighbor pixelation are intentional design choices based on research into actual Shockwave 3D rendering. Don't "fix" or "improve" these. If you're unsure whether something is a bug or a feature, check the engine specs in CLAUDE.md.

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
