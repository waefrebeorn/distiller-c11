# distiller-c11 — the Distiller device path, reimplemented from scratch

A ground-up C11 reimplementation (SLERM) of the Distiller One
device software — the e-ink render pipeline, the SAM/audio
hardware layer, the app registry, and the speech-AI slots. Not a
fork, not vendored: every byte is original, based on the device
protocol format truth (the v1 beta archive + the CM5 SDK).

**The measured win over both original implementations:**

| Path | Cold start | Warm 240x416 render |
|---|---|---|
| v1 numba (2024, on device) | 2,310 ms (JIT) | 4.0 ms |
| v2 pure-Python (2026 "fix") | 0 ms | ~1,500 ms (regression) |
| **C11 (ours)** | **0 ms** | **3.1 ms on the CM4** |

The dev team's 2026 composer REGRESSED the e-ink path (pure
Python loops ~500x slower warm). This repo fixes it properly —
faster than numba warm AND no JIT cold start, with zero
dependencies.

## Modules (all self-contained, opaque structs, C11 only)

```
src/
  dither/    bayer8 + floyd-steinberg + threshold dithering
  pack/      1-bit LSB-first e-ink packing (v2 layout)
  image/     grayscale (BT.601) + resize + crop + rotate
  compose/   5x7 bitmap-font text rendering on a canvas
  eink/      full pipeline: image -> dither -> pack
  hw/
    sam/     SAM RP2040 UART protocol (9600 baud, CRC-8/MAXIM,
             button bitmask 1/2/4/8, info lines)
    einkdrv/ SPI display driver (240x416, DC=6/RST=13/BUSY=9,
             0x10/0x13/0x12 display, 216-byte LUT_ALL from v1
             truth, init/fast-init/sleep sequences)
    audio/   WM8960 sysfs gain/volume control (v2 path)
  ui/
    registry/ apps.json parser (the cohesive registry uniting
              the e-ink menu and the web UI)
    gui/     app shell: menu render via compose->dither->pack,
             edge-triggered button handling
  ai/
    ai.c     ASR + TTS pluggable pipeline slots (honest empty
             with no recognizer installed — never fabricates)
```

11 test suites, all passing on the dev box AND the actual CM4
(compiled in place with `gcc -std=c11 -O2 -Wall -Wextra`, zero
warnings, ASan/UBSan clean).

## The 2026 e-ink research

See `docs/eink-2026-research.md`: the Modos/Caster breakthrough
(60Hz e-ink monitor — the CONTROLLER is the bottleneck, not the
panel), the waveform format truth (lut[src][dst][frame], 0=GND
1=VNEG 2=VPOS), per-pixel regions + early cancellation + hybrid
greyscale mapped to this C11 driver, and the Japanese e-ink
scene (M5Stack PaperS3, qiita, tsubasa.tech).

## Build

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

No third-party dependencies. Pure C11 + POSIX (pthread for the
SAM monitor thread). The GUI is NOT changed — same visual output
as the v1 menu, rendered through the faster pipeline.
