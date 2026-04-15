# SphinxSDK Demo — ConsoleDemo_FPN

A Windows C++ console demo for the **MRC Systems Sphinx SDK** (`SphinxLib`). It discovers a GigE Vision camera, streams images over a ring buffer, optionally applies Fixed Pattern Noise (FPN) dark-frame subtraction, runs a hand-landmark model to estimate **wrist angle** with **flexor/extensor** classification, and **broadcasts the result as JSON over TCP** on the loopback interface.

## Requirements

- Windows (x64)
- Visual Studio 2022 with the **v143** C++ build tools (MSVC 19.3+, C++17)
- The Sphinx SDK from MRC Systems (not included in this repo) — request it from the camera vendor
- [vcpkg](https://github.com/microsoft/vcpkg) for the OpenCV / ONNX Runtime dependencies
- A hand-landmark ONNX model (e.g. MediaPipe Hands `hand_landmark_full.onnx`, 224×224 RGB input, 21 landmarks output)
- A GigE Vision camera reachable on a network adapter on the host (the FPN code path activates only for `DeviceModelName == "GVRD-MRC HighSpeed"`)

## Quick start (Windows)

From an elevated PowerShell or Developer Command Prompt:

```bat
git clone git@github.com:Supat/SphinxSDK-demo.git
cd SphinxSDK-demo
scripts\setup.bat
scripts\build.bat
```

`setup.bat` clones+bootstraps vcpkg (default `C:\vcpkg`, override with `-VcpkgRoot`), installs `opencv4` + `onnxruntime` for `x64-windows`, integrates with MSBuild, and verifies the Sphinx SDK is at `..\SphinxLib\` (it cannot auto-install it — see below). `build.bat` invokes MSBuild for `Release|x64` and copies the bundled ONNX models next to the resulting `ConsoleDemo.exe`.

## Install dependencies

The project links against `SphinxLib.lib`, which ships with the Sphinx SDK. The `.vcxproj` expects it as a **sibling directory** to this repo:

```
<parent>\
├── SphinxSDK-demo\          ← this repo
│   └── ConsoleDemo_FPN\
└── SphinxLib\               ← from the Sphinx SDK installer
    ├── SphinxLib.h
    ├── Debug64\SphinxLib.lib
    └── Release64\SphinxLib.lib
```

Steps:

1. Install the Sphinx SDK from MRC Systems.
2. Copy or symlink its `SphinxLib` folder next to this repo so the layout above matches.
3. Make sure the SDK runtime DLL (`SphinxLib.dll`) is on `PATH` or sits next to the built `.exe`.

System libraries `Ws2_32.lib` and `IPHlpApi.Lib` are linked from the Windows SDK and need no extra setup.

### OpenCV + ONNX Runtime via vcpkg

```bat
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg integrate install
```

Dependencies are declared in `ConsoleDemo_FPN/vcpkg.json` (manifest mode). MSBuild auto-restores `opencv4` and `onnxruntime` for `x64-windows` on first build.

### Hand + pose models

Two pre-converted ONNX models are checked into `ConsoleDemo_FPN/`:

- **`hand_landmark_full.onnx`** (~11 MB) — MediaPipe Hands; 21 hand landmarks. Drives the wrist-vector calculation.
- **`pose_landmark_full.onnx`** (~13 MB) — MediaPipe Pose; 33 body landmarks. Used to detect the **forearm direction** (elbow → wrist) so the wrist angle is measured against the actual forearm rather than a fixed reference.

Copy both next to the built `ConsoleDemo.exe`, or change `WRIST_MODEL_PATH` / `POSE_MODEL_PATH` at the top of `ConsoleDemo.cpp` to absolute paths.

The pose estimator picks whichever side (left or right) has higher elbow+wrist visibility above `POSE_MIN_VISIBILITY` (default 0.5). If neither side is visible enough, the wrist estimator falls back to the static `WRIST_FOREARM_AXIS_DEG`. Each frame's JSON includes a `forearm` block reporting which axis was used.

To regenerate the models (e.g. with a different MediaPipe revision):

```bat
pip install tf2onnx tensorflow onnx tflite2onnx
python ConsoleDemo_FPN\scripts\prepare_models.py
```

## Build

**From Visual Studio:**

1. Open `ConsoleDemo_FPN/ConsoleDemo.sln`.
2. Select configuration `Release | x64` (or `Debug | x64`).
3. Build the solution (`Ctrl+Shift+B`).

**From a Developer Command Prompt:**

```bat
msbuild ConsoleDemo_FPN\ConsoleDemo.sln /p:Configuration=Release /p:Platform=x64
```

The output binary lands in `ConsoleDemo_FPN\Release\ConsoleDemo.exe` (or `Debug\` for Debug builds).

> Build only x64 — the Win32 configurations exist in the project file but the Sphinx library paths are wired up only for x64.

## Run

1. Connect the GigE camera and verify the host network adapter is on the same subnet.
2. From a terminal:

   ```bat
   cd ConsoleDemo_FPN\Release
   ConsoleDemo.exe
   ```

3. The app prints discovered devices. If more than one is found, type the device number and press **Enter**.
4. Press **any key** to start continuous acquisition. Frame timestamps and counters stream to the console.
5. Press **any key again** to stop. The last processed frame is written as `image.bmp` in the working directory.
6. Press **any key** once more to exit.

For `GVRD-MRC HighSpeed` cameras, the first ~10 frames are used to capture a dark reference (with `ExposureTime` temporarily set to 0); subsequent frames are dark-noise corrected automatically.

## Wrist angle output

For every frame, the app runs the hand-landmark model, computes the angle of the vector wrist (landmark 0) → middle-finger MCP (landmark 9) relative to a configurable forearm axis (default 0° = +X in the image), and classifies it:

- `angle_deg > +threshold` → `extensor`
- `angle_deg < -threshold` → `flexor`
- otherwise → `neutral`

Thresholds and the forearm axis are constants at the top of `ConsoleDemo.cpp` (`WRIST_FOREARM_AXIS_DEG`, `WRIST_NEUTRAL_THRESH_DEG`, `WRIST_FLIP_SIGN`).

The result is broadcast as **NDJSON** (one JSON object per line, `\n`-terminated) on `127.0.0.1:5555` to every connected TCP client. Listen with e.g.:

```bat
ncat 127.0.0.1 5555
```

A live OpenCV preview window opens during acquisition showing the camera frame with the hand skeleton, the wrist→middle-MCP angle vector (cyan), the configured forearm reference axis (yellow), and a HUD with `angle` / `class` / `confidence`. Set `WRIST_SHOW_PREVIEW = false` in `ConsoleDemo.cpp` to disable.

Sample line:

```json
{"frame":1234,"timestamp":987654321,"valid":true,"angle_deg":12.341,"class":"extensor","confidence":0.972,"forearm":{"axis_deg":-3.215,"source":"left_forearm","confidence":0.91}}
```

`forearm.source` is `left_forearm` or `right_forearm` when pose tracking succeeded, or `static` when it fell back to `WRIST_FOREARM_AXIS_DEG`.

## Repository layout

- `ConsoleDemo_FPN/ConsoleDemo.cpp` — discovery, init, acquisition thread, BMP writer
- `ConsoleDemo_FPN/darknoise.{h,cpp}` — SSE2 dark-frame subtraction
- `ConsoleDemo_FPN/bayer.{h,cpp}` — bilinear demosaic for Bayer pixel formats
- `ConsoleDemo_FPN/SphinxLib.h` — vendored copy of the SDK header (reference only)
