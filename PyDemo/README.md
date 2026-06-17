# Python port (ctypes)

A pure-Python interface to the Sphinx camera, wrapping `SphinxLib.dll` directly
via `ctypes`. This is the base for the planned MediaPipe wrist-angle work and a
future PySide6 UI; it replaces the C++ acquisition path with NumPy-friendly
frames.

## Files

| File | Role |
|------|------|
| `sphinx.py` | ctypes binding: SDK structs, function prototypes, and a `Camera` class (`discover`, `open`, `start`, `stop`/`close`, `get_frame` → NumPy, plus the generic GenICam feature API). FPN dark-frame subtraction and Bayer demosaic are done in NumPy/OpenCV. |
| `capture_test.py` | Smoke test: discover → connect → grab N frames → save `capture.png`. |
| `requirements.txt` | `numpy`, `opencv-python`, `mediapipe`, `PySide6`. |

## Setup

```bat
py -m pip install -r requirements.txt
python capture_test.py 30
```

`sphinx.py` finds the SDK at `..\SphinxLib` by default; override with the
`SPHINX_SDK_DIR` environment variable.

## SDK runtime quirk (important)

`SphinxLib.dll` loads its XML helpers (`libxml2.dll`, `libxslt.dll`,
`MathParser.dll`) and the GenApi schema files **from its own directory** at
`GEVInitXml` time — it ignores `PATH`, `SetDllDirectory`, and
`os.add_dll_directory`. The C++ build worked because its post-build step copies
everything from `Extras64` next to the executable. To mirror that, `sphinx.py`
**copies the `Extras64` runtime next to `SphinxLib.dll` (`Release64`) on import**
if not already present. This is why the SDK must be writable on first run.

## Status

Verified against a **GVRD-MRC MR-CAM-HR** (1280×960, BayerGR8): connect, stream,
and NumPy frame delivery all work with zero packet loss.

## Notes / next steps

- Color frames come out RGB. The demosaic uses OpenCV's `COLOR_BayerGB2RGB`
  for BayerGR8; if colors look off, that Bayer phase is the first knob (same
  issue tuned to `GRBG` in the Qt demo). A mild green cast is likely white
  balance.
- `get_frame()` is blocking; the UI/MediaPipe integration will run it on a
  worker thread and process the latest frame only.
- FPN subtraction mirrors the C++ state machine but only triggers for
  `GVRD-MRC HighSpeed` (mono); the color camera skips it.
