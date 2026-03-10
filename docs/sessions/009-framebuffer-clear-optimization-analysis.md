# Session 009: Framebuffer Clear Optimization Analysis

**Date:** March 9, 2026
**Participants:** Caleb Cohoon + Claude (Opus 4.6)
**Duration:** Short investigation session
**Repo:** [deadcast2/slopwave3d](https://github.com/deadcast2/slopwave3d)

---

## Session Goal

Investigate whether the framebuffer clear loop in `s3d_frame_begin()` could be optimized further — specifically, whether a single instruction could replace the per-pixel loop that fills the framebuffer with the clear color.

---

## Conversation Summary

### 1. Initial Question

Caleb asked whether the framebuffer clear loop in `s3d_frame_begin()` could be replaced with a single operation to save CPU cycles. The current implementation constructs a 32-bit RGBA color from the engine's clear color components, then iterates over all 76,800 pixels (320x240) writing the color value one at a time:

```c
uint32_t *fb = (uint32_t *)g_engine.framebuffer;
int count = S3D_WIDTH * S3D_HEIGHT;
for (int i = 0; i < count; i++) {
    fb[i] = color;
}
```

### 2. Analysis of Options

We discussed the fundamental constraint: `memset` only fills a single byte value, so it can only be used for colors where all 4 bytes are identical (e.g., `0x00000000` or `0xFFFFFFFF`). For arbitrary 32-bit RGBA colors, there is no standard C single-call equivalent.

Two alternatives were considered:

- **Doubling `memcpy` trick** — Fill the first pixel, then use `memcpy` to repeatedly double the filled region (~17 iterations via log2(76800) instead of 76,800). This leverages highly-optimized `memcpy` implementations but adds complexity.
- **Trust the compiler** — At `-O2`, Emscripten/clang should optimize the simple loop into efficient WASM output. The loop is already clean and idiomatic.

The recommendation was to keep the simple loop and verify the compiler output.

### 3. WASM Disassembly Verification

Caleb asked to verify the compiler was actually optimizing the loop. We built the project and disassembled the WASM output using `wasm-dis` (from the Binaryen toolchain installed at `/opt/homebrew/bin/wasm-dis`).

The disassembly of `func $4` (`s3d_frame_begin`) revealed:

- **8x loop unrolling** — The compiler transformed the 76,800-iteration loop into a 9,600-iteration loop, with 8 consecutive `i32.store` instructions per iteration (at offsets +3872 through +3900). The counter increments by 8 each iteration.
- **`memory.fill` for zbuffer** — The `memset` call for the zbuffer compiled to a single `memory.fill` WASM bulk memory instruction, confirming this is the optimal path for byte-pattern fills.
- **No `memory.fill` for framebuffer** — As expected, the 32-bit color fill cannot use `memory.fill` (byte-only), so the unrolled store loop is the best the compiler can produce.

The zbuffer clear (`memset` with `0xFF`) at the end of the function:
```wasm
(memory.fill
  (i32.const 311072)
  (i32.const 255)
  (i32.const 153600)
)
```

This confirmed the compiler is handling both cases optimally.

---

## Research Findings

### WASM Bulk Memory Operations

- `memory.fill` is the WASM equivalent of `memset` — it fills a memory region with a single byte value. It is used automatically by Emscripten when compiling `memset` calls.
- There is no WASM instruction for filling memory with a 32-bit pattern. The widest store instruction is `i64.store`, but there is no bulk 32-bit fill opcode in the WASM spec.
- Emscripten's `-O2` optimization level enables LLVM's loop unrolling pass, which transformed the simple for-loop into an 8x unrolled version — a standard compiler optimization.

### Toolchain Notes

- `wasm-dis` from the Binaryen toolchain (installed via Homebrew) can disassemble `.wasm` binaries into readable WAT-like text format.
- `wasm-objdump` and `wasm2wat` (from the WABT toolchain) were not installed on the system.

---

## Decisions Log

| # | Decision | Choice | Reasoning |
|---|----------|--------|-----------|
| 1 | Optimize framebuffer clear loop? | Keep as-is | Compiler already unrolls 8x at -O2; no standard C single-call for 32-bit fill; added complexity not worth marginal gain |
| 2 | Use doubling memcpy trick? | No | Adds code complexity for unclear benefit in WASM; compiler output is already well-optimized |

---

## Files Changed

No files were modified during this session. This was a pure investigation/analysis session.

---

## Next Steps

- Continue with engine feature development (next GitHub issue in sequence)
- The framebuffer clear is confirmed optimal — no action needed
