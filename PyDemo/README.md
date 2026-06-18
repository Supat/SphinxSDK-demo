# Python port (ctypes)

A pure-Python interface to the Sphinx camera, wrapping `SphinxLib.dll` directly
via `ctypes`. This is the base for the planned MediaPipe wrist-angle work and a
future PySide6 UI; it replaces the C++ acquisition path with NumPy-friendly
frames.

## Architecture (MVP)

The live app follows Model–View–Presenter so the UI, orchestration, and domain
logic stay separable and testable:

```
app.py            composition root: builds Model + View + Presenter and wires them
view.py           View (passive): widgets, intent signals, render methods. All Qt
                  and rendering live here; it holds no camera/service references.
presenter.py      Presenter: the only layer that knows both sides. Handles View
                  intents, drives the camera/services/thread, pushes results back.
capture.py        CaptureThread (QThread): runs the pipeline off the GUI thread,
                  emits domain ProcessedFrame (image + readings) — no QImage.
pipeline.py       FramePipeline (Qt-free): raw frame + options -> ProcessedFrame;
                  also format_angles() / to_payload(). Reused by the offline tools.
overlay.py        draw_overlay(): renders pose/wrist results onto an image.
sphinx.py         Model: Camera (acquisition + GenICam features), FPN, demosaic.
wrist.py          WristEstimator (MediaPipe Tasks Pose+Hands); estimation only.
undistort.py      Undistorter service.  broadcaster.py  AngleBroadcaster service.
```

User action → View emits an intent signal → Presenter acts on the Model →
Presenter calls View render methods. The View never touches the camera/services
directly (feature editors go through a small `FeatureAccess` interface).

## Files

| File | Role |
|------|------|
| `sphinx.py` | Model: ctypes SDK binding + `Camera` (`discover`/`open`/`start`/`stop`/`get_frame`→NumPy + GenICam feature API). FPN + Bayer demosaic in NumPy/OpenCV. |
| `pipeline.py` | Qt-free `FramePipeline` (undistort → downscale → estimate) + `format_angles`/`to_payload`. |
| `wrist.py` | `WristEstimator` — MediaPipe Tasks Pose+Hands; wrist flexion. Auto-downloads `.task` models to `models/`. |
| `overlay.py` | `draw_overlay()` — pose/wrist visualisation onto a frame. |
| `capture.py` | `CaptureThread` — runs the pipeline off the GUI thread. |
| `view.py` | Passive PySide6 View + feature-editor factory. |
| `presenter.py` | `WristPresenter` — orchestration. |
| `app.py` | Composition root (`python app.py` / `run_app.bat`). |
| `broadcaster.py` | `AngleBroadcaster` — TCP server streaming readings as JSONL. |
| `tcp_client.py` | Example broadcast consumer. |
| `capture_test.py` | Smoke test: discover → grab N → save `capture.png`. |
| `analyze_image.py` | Offline estimator on an image or live grab; saves annotated PNG. |
| `charuco.py` / `make_charuco.py` | Shared ChArUco board + printable generator. |
| `calibrate.py` | Live lens calibration → `calib.json`. |
| `undistort.py` | `Undistorter` — applies `calib.json` (cached remap). |

## Code style

Linted with [ruff](https://docs.astral.sh/ruff/); config in `pyproject.toml`
(line length 100; `E,W,F,B,I,UP,N` selected). Run `ruff check .` from `PyDemo/`.
Intentional naming exceptions are scoped per-file with rationale: the ctypes
binding mirrors the C SDK names (`sphinx.py`), and Qt signals/overrides are
camelCase by Qt convention (`view.py`, `capture.py`).

A pre-commit hook (repo-root `.pre-commit-config.yaml`) runs `ruff check` on
staged `PyDemo/*.py` so violations are caught before commit. Enable it once per
clone:

```bash
python -m pip install pre-commit
python -m pre_commit install
```

## Lens-distortion correction

The camera streams **raw, uncorrected** pixels (no distortion-correction exists
in the SDK or the camera's GenICam map — verified), so correct it with a
one-time calibration:

1. `python make_charuco.py` → `charuco_board.png`. Print it, mount it flat/rigid.
2. `python calibrate.py` → live window. Hold the board across the field of view
   at varied angles; **SPACE** captures a view (aim for 12–20), **c** calibrates
   and saves `calib.json`, **q** quits. It calibrates with **both** the standard
   (pinhole) and **fisheye** models and keeps whichever has the lower
   reprojection error (printed as RMS).
3. In `app.py`, the **Undistort** checkbox is enabled once `calib.json` exists;
   correction is applied at full resolution before inference/display, so wrist
   angles are measured on the corrected geometry.

The square/marker sizes in `charuco.py` don't need to be metric-accurate — the
intrinsics and distortion coefficients are scale-independent.
| `requirements.txt` | `numpy`, `opencv-python`, `mediapipe`, `PySide6`. |

## Wrist-angle estimation

Run `python app.py`, or double-click one of the launchers from File Explorer
(double-clicking `app.py` itself is unreliable — it may open in an editor, and a
console-mode error window closes before you can read it):

- **`run_app.bat`** — keeps a console open (shows logs, pauses on error). Use
  this first / when troubleshooting.
- **`run_app_quiet.bat`** — runs under `pythonw.exe` with no console window.
  Use once you've confirmed it works; if the window doesn't appear, fall back to
  `run_app.bat` to see the error.

Both pin the working directory and pick the right Python. Connect → Start
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

## TCP broadcast

Tick **Broadcast TCP** (and a port, default 5555) to start an embedded TCP
server. While streaming with MediaPipe on, every frame with a detected wrist is
pushed to all connected clients as one line of JSON:

```json
{"frame": 42, "t": 1718638655.12, "wrists": [{"side": "left", "angle_deg": 168.4}]}
```

The toolbar shows the live client count. Consume it from anything that speaks
TCP — e.g. the included example:

```bat
python tcp_client.py 127.0.0.1 5555
```

Notes: it's a one-to-many TCP **server** (clients connect to the app), not UDP
broadcast. Slow/dead clients are dropped (2 s send timeout) so they never stall
acquisition. Bind is `0.0.0.0`, so remote machines on the network can connect;
restrict via firewall if needed.

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
