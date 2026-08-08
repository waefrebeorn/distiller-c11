# distiller-c11 — the Distiller One SDK, properly ported

The v1 beta's e-ink path is Python + numba JIT (2.3s cold start).
The v2's "fix" is a pure-Python loop (slow warm path). This repo
is the C11 port: the same software, written properly — modular,
opaque structs, C11 only, no third-party dependencies.

## Modules (each self-contained, no god headers)

| Module | File | Responsibility |
|---|---|---|
| dither | `src/dither/dither.h` `.c` | Bayer8 (vectorized) + Floyd-Steinberg (row-vectorized) |
| pack | `src/pack/pack.h` `.c` | 1-bit LSB-first packing (e-ink byte layout) |
| image | `src/image/image.h` `.c` | grayscale, resize, crop, rotate (image_ops) |
| compose | `src/compose/compose.h` `.c` | text + template rendering onto a canvas |
| eink | `src/eink/eink.h` `.c` | frame pipeline: image -> dither -> pack -> bytes |

## Rules (wubu-c11-discipline)

- `typedef struct foo foo;` opaque handles, `foo_create/free` API
- minimal includes, include-what-you-use
- C11 only: `-std=c11`, `_POSIX_C_SOURCE 200809L` where POSIX is needed
- every module compiles + tests standalone
- no monoliths: a .c over ~500 lines gets split

## Build

```sh
cmake -B build && cmake --build build
ctest --test-dir build
```

## Status

- [x] dither (Bayer8 + FS vectorized, NEON-accelerated optional)
- [x] pack (1-bit LSB-first)
- [x] image (grayscale / resize / crop / rotate)
- [x] compose (text render)
- [x] eink (full pipeline)
- [x] tests green (ctest)
