"""eink_c11_fast.py — drop-in C11 fast path for the Distiller GUI.

Monkeypatches distiller.peripheral.eink.Eink.update_screen_1bit so the GUI
drives the panel through the persistent-session C11 driver instead of the
slow Python path (per-frame reset + numba/PIL dither+pack).

C11 path per frame (verified on the CM4):
  render_gray  FS-dither + pack   ~3.4ms (Python ~125ms)
  display      persistent session ~0.46s  (Python paid reset+init per frame)
  partial      unchanged rows      ~0.00s  (menu cursor / clock ticks)
"""
import ctypes
import os
import logging

log = logging.getLogger("eink_c11_fast")

_LIB = None


def _load() -> ctypes.CDLL:
    global _LIB
    if _LIB is not None:
        return _LIB
    for c in [
        os.environ.get("EINK_C11_SO"),
        "/home/distiller/distiller-c11/build/libeink_c11.so",
    ]:
        if c and os.path.exists(c):
            _LIB = ctypes.CDLL(c)
            break
    if _LIB is None:
        raise RuntimeError("libeink_c11.so not found")
    # driver
    _LIB.einkdrv_pi_open.restype = ctypes.c_int
    _LIB.einkdrv_pi_init_fast.restype = ctypes.c_int
    _LIB.einkdrv_pi_display_partial.argtypes = [ctypes.POINTER(ctypes.c_uint8)]
    _LIB.einkdrv_pi_display_partial.restype = ctypes.c_int
    _LIB.einkdrv_pi_sleep.restype = ctypes.c_int
    # render
    _LIB.eink_pi_create.restype = ctypes.c_void_p
    _LIB.eink_pi_create.argtypes = [ctypes.c_size_t, ctypes.c_size_t]
    _LIB.eink_pi_packed_size.restype = ctypes.c_size_t
    _LIB.eink_pi_packed_size.argtypes = [ctypes.c_void_p]
    _LIB.eink_pi_render_gray.restype = ctypes.c_long
    _LIB.eink_pi_render_gray.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.POINTER(ctypes.c_uint8),
    ]
    return _LIB


_WIDTH = 240
_HEIGHT = 416
_FRAME_BYTES = (_WIDTH + 7) // 8 * _HEIGHT  # 12480


class _C11Session:
    """Single persistent driver+render session shared by the GUI."""

    def __init__(self):
        self._lib = _load()
        self._frame = None
        self._packed = (ctypes.c_uint8 * _FRAME_BYTES)()

    def open(self):
        if self._lib.einkdrv_pi_open() != 0:
            raise RuntimeError("einkdrv_pi_open failed")
        if self._lib.einkdrv_pi_init_fast() != 0:
            raise RuntimeError("einkdrv_pi_init_fast failed")
        self._frame = self._lib.eink_pi_create(_WIDTH, _HEIGHT)
        if not self._frame:
            raise RuntimeError("eink_pi_create failed")

    def display_gray(self, gray8):
        """gray8: 240x416 uint8, top row first (already flipped to match GUI).
        Returns None on success; raises on failure."""
        import numpy as np
        gray8 = np.ascontiguousarray(gray8, dtype=np.uint8)
        gptr = gray8.ctypes.data_as(ctypes.POINTER(ctypes.c_uint8))
        if self._lib.eink_pi_render_gray(self._frame, gptr, self._packed) < 0:
            raise RuntimeError("eink_pi_render_gray failed")
        if self._lib.einkdrv_pi_display_partial(self._packed) != 0:
            raise RuntimeError("einkdrv_pi_display_partial failed")


_session = None


def update_screen_1bit_c11(self, image, dithering=True):
    """Replacement for Eink.update_screen_1bit via the C11 driver."""
    import numpy as np
    from PIL import Image as _PIL
    global _session
    try:
        if _session is None:
            _session = _C11Session()
            _session.open()
            log.info("C11 e-ink session open")
        # mirror the GUI's orientation: FLIP_TOP_BOTTOM, convert L
        gray = np.array(image.transpose(_PIL.FLIP_TOP_BOTTOM).convert("L"),
                        dtype=np.uint8)
        t0 = __import__("time").time()
        _session.display_gray(gray)
        dt = __import__("time").time() - t0
        if dt > 0.005:
            log.info("C11 frame %.3fs", dt)
    except Exception as e:
        log.error("C11 display failed (%s); falling back to Python path", e)
        # fall back to the original implementation
        return _orig_update_screen_1bit(self, image, dithering=dithering)


_orig_update_screen_1bit = None


def install():
    """Patch Eink.update_screen_1bit to use the C11 fast path."""
    global _orig_update_screen_1bit
    from distiller.peripheral.eink import Eink
    if not hasattr(Eink, "_c11_installed"):
        _orig_update_screen_1bit = Eink.update_screen_1bit
        Eink.update_screen_1bit = update_screen_1bit_c11
        Eink._c11_installed = True
        log.info("C11 fast path installed on Eink.update_screen_1bit")


if __name__ == "__main__":
    import time
    logging.basicConfig(level=logging.INFO)
    from PIL import Image
    img = Image.new("L", (_WIDTH, _HEIGHT), 255)
    for y in range(_HEIGHT // 2):
        for x in range(_WIDTH):
            img.putpixel((x, y), 0)
    install()
    # create a bare object to call the patched method
    from distiller.peripheral.eink import Eink
    e = Eink.__new__(Eink)
    t0 = time.time()
    update_screen_1bit_c11(e, img)
    print("C11 home-frame render+display: %.2fs" % (time.time() - t0))
