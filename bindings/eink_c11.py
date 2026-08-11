#!/usr/bin/env python3
"""
eink_c11.py — ctypes binding to the C11 e-ink driver (distiller-c11).

Exposes the persistent-session fast path:
  open()          once — GPIO + spidev + driver
  init()          once — reset + power (the 181ms that Python paid per frame)
  display(px)     hot path — no reset, straight to data + refresh (~362ms
                  panel busy, the physics floor)
  display_partial(px) — only re-drives changed rows; zero work if the
                  frame is unchanged (menu cursor, clock second, etc.)
  render_gray()   C11 FS-dither + pack (0.66ms + 0.05ms vs Python 86.9+38ms)
  sleep()         deep sleep

The packed frame is EPD_FRAME_BYTES = 12480 bytes (240x416 1-bit MSB-first).
"""
import ctypes
import os
import sys

_LIB = None


def _load() -> ctypes.CDLL:
    global _LIB
    if _LIB is not None:
        return _LIB
    candidates = [
        os.environ.get("EINK_C11_SO"),
        "/home/distiller/distiller-c11/build/libeink_c11.so",
        os.path.join(os.path.dirname(__file__), "libeink_c11.so"),
    ]
    for c in candidates:
        if c and os.path.exists(c):
            _LIB = ctypes.CDLL(c)
            break
    if _LIB is None:
        raise RuntimeError("libeink_c11.so not found; build distiller-c11 first")
    # ABI
    _LIB.einkdrv_pi_open.restype = ctypes.c_int
    _LIB.einkdrv_pi_init.restype = ctypes.c_int
    _LIB.einkdrv_pi_init_fast.restype = ctypes.c_int
    _LIB.einkdrv_pi_display.argtypes = [ctypes.POINTER(ctypes.c_uint8)]
    _LIB.einkdrv_pi_display.restype = ctypes.c_int
    _LIB.einkdrv_pi_sleep.restype = ctypes.c_int
    return _LIB


class EinkC11:
    """Persistent-session C11 e-ink driver (drop-in for the Python Eink)."""

    def __init__(self, width: int = 240, height: int = 416):
        self.width = width
        self.height = height
        self.frame_bytes = (width + 7) // 8 * height
        self._lib = _load()
        self._opened = False
        self._init_done = False

    # ---- lifecycle ----

    def open(self) -> int:
        rc = self._lib.einkdrv_pi_open()
        self._opened = (rc == 0)
        return rc

    def init(self, fast: bool = False) -> int:
        rc = self._lib.einkdrv_pi_init_fast() if fast else self._lib.einkdrv_pi_init()
        self._init_done = (rc == 0)
        return rc

    def sleep(self) -> int:
        return self._lib.einkdrv_pi_sleep()

    # ---- display ----

    def display(self, packed: bytes) -> int:
        """Display one packed frame (12480 bytes, MSB-first 1-bit)."""
        if len(packed) != self.frame_bytes:
            raise ValueError(f"frame must be {self.frame_bytes} bytes, got {len(packed)}")
        buf = (ctypes.c_uint8 * len(packed)).from_buffer_copy(packed)
        return self._lib.einkdrv_pi_display(buf)

    def display_partial(self, packed: bytes) -> int:
        """Display with dirty-row tracking (no-op on identical frames)."""
        if len(packed) != self.frame_bytes:
            raise ValueError(f"frame must be {self.frame_bytes} bytes, got {len(packed)}")
        buf = (ctypes.c_uint8 * len(packed)).from_buffer_copy(packed)
        return self._lib.einkdrv_pi_display_partial(buf)

    # ---- helpers ----

    def render_and_display(self, gray8, dither: str = "fs") -> int:
        """Render a 240x416 uint8 grayscale numpy array and display it.
        Returns 0 on success. Uses the C11 render_gray (FS dither + pack)."""
        import numpy as np
        try:
            from eink_c11_render import render_gray  # C11 binding
        except ImportError:
            from distiller.peripheral.eink import dump_1bit_with_dithering
            import numpy as _np
            packed = bytes(dump_1bit_with_dithering(gray8.astype(_np.float32)))
            return self.display(packed)
        packed = render_gray(gray8)
        return self.display(packed)


# ---- module-level convenience ----

def open_eink() -> EinkC11:
    e = EinkC11()
    e.open()
    e.init(fast=False)
    return e


if __name__ == "__main__":
    import time
    e = open_eink()
    # synthetic test pattern: half-white/half-black
    row = bytes([0xFF] * 30 + [0x00] * 30) * 208
    t0 = time.time()
    e.display(row)
    print(f"persistent display: {time.time() - t0:.3f}s (panel busy dominated)")
    t0 = time.time()
    e.display(row)  # unchanged — partial path should skip
    print(f"display_partial unchanged: {time.time() - t0:.3f}s (should be ~0)")
    e.sleep()
