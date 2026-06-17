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
| `wrist.py` | `WristEstimator` — MediaPipe **Tasks** Pose + Hands; computes wrist flexion (forearm vs hand axis) and draws an overlay. Auto-downloads the `.task` models to `models/` on first use. |
| `analyze_image.py` | Offline check: run the estimator on an image (`python analyze_image.py [path]`) or a live grab (`python analyze_image.py --grab [N]`); saves an annotated PNG. |
| `app.py` | PySide6 live app: stream + MediaPipe overlay + wrist-angle readout, device select/connect/start-stop, exposure/gain controls, and a generic feature grid. |
| `requirements.txt` | `numpy`, `opencv-python`, `mediapipe`, `PySide6`. |

## Wrist-angle estimation

Run `python app.py`, or **double-click `run_app.bat`** from File Explorer
(double-clicking `app.py` itself is unreliable — it may open in an editor, and
a console-mode error window closes before you can read it; the `.bat` pins the
working directory, picks the right Python, and pauses on error). Connect → Start
streams frames; with the **Wrist angle (MediaPipe)** box checked, each frame runs
Pose + Hands and the detected wrist flexion is drawn on the view and shown in the
readout.

"Wrist angle" is defined as the flexion/extension angle between the **forearm**
(elbow→wrist, from Pose) and the **hand axis** (wrist→middle-finger MCP, from
Hands); 180° = straight wrist. Each detected hand is matched to the nearest Pose
wrist to recover the forearm. If no body is found, the hand orientation vs
horizontal is reported instead.

Caveats: these are **2D image-plane** estimates from a single uncalibrated
camera — out-of-plane wrist rotation biases the angle, and MediaPipe's `z` is
not metric. Good for trends/gestures; not degree-accurate without calibration
or multiple views. Inference runs on a worker thread at a downscaled width
(`INFER_WIDTH`) for a smooth CPU frame rate.

MediaPipe here is the **Tasks** API (this build has no legacy `solutions`); the
pose/hand `.task` bundles download once into `PyDemo/models/`.

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

## Notes

- Color frames come out RGB. The demosaic uses OpenCV's `COLOR_BayerGB2RGB`
  for BayerGR8; if colors look off, that Bayer phase is the first knob (same
  issue tuned to `GRBG` in the Qt demo). A mild green cast is likely white
  balance.
- `get_frame()` is blocking; `app.py` runs it on a worker thread and processes
  the latest frame only (downscaled before inference).
- FPN subtraction mirrors the C++ state machine but only triggers for
  `GVRD-MRC HighSpeed` (mono); the color camera skips it.
- Feature edits in the UI are issued from the GUI thread on the shared camera
  (control channel), independent of the worker's stream.
