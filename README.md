# SphinxSDK Demo — ConsoleDemo_FPN

A Windows C++ console demo for the **MRC Systems Sphinx SDK** (`SphinxLib`). It discovers a GigE Vision camera, streams images over a ring buffer, optionally applies Fixed Pattern Noise (FPN) dark-frame subtraction, runs a hand-landmark model to estimate **wrist angle** with **flexor/extensor** classification, and **broadcasts the result as JSON over TCP** on the loopback interface.

## Requirements

- Windows (x64)
- Visual Studio 2022 with the *Desktop development with C++* workload (v143 toolset, MSVC 19.3+, C++17, Windows 10/11 SDK)
- The Sphinx SDK from MRC Systems — vendor-only, request it from the camera vendor
- A GigE Vision camera reachable on a network adapter on the host (the FPN code path activates only for `DeviceModelName == "GVRD-MRC HighSpeed"`)

The OpenCV / ONNX Runtime / hand+pose ONNX dependencies are handled by the install scripts described below.

## Quick start (Windows)

From PowerShell or a Developer Command Prompt, after the Sphinx SDK is in place (see *Manual dependency install* → *1. Sphinx SDK*):

```bat
git clone git@github.com:Supat/SphinxSDK-demo.git
cd SphinxSDK-demo
scripts\setup.bat
scripts\build.bat
```

The result is `ConsoleDemo_FPN\x64\Release\ConsoleDemo.exe` with both ONNX models copied next to it, ready to run.

---

## Dependency installation

There are two paths: **automated** (recommended) or **manual** (if you want to control each piece).

### Automated: `scripts\setup.bat`

A thin wrapper around `scripts\setup.ps1`. Idempotent — safe to re-run.

```bat
scripts\setup.bat
```

Optional flags (forwarded to the `.ps1`):

| Flag                           | Effect                                                                 |
|--------------------------------|------------------------------------------------------------------------|
| `-VcpkgRoot D:\vcpkg`          | Install vcpkg to a custom location (default `C:\vcpkg`, or `$env:VCPKG_ROOT`). |
| `-SkipVcpkg`                   | Don't touch vcpkg (use if you already have OpenCV / ONNX Runtime wired up). |

What it does, in order:

1. **Verifies prerequisites** — `git` on `PATH`, and Visual Studio 2022 with the C++ workload (located via `vswhere.exe`). Fails with a clear message if either is missing; it does not install Visual Studio for you.
2. **Checks for the Sphinx SDK** at `..\SphinxLib\` and warns if missing. The Sphinx SDK is vendor-only (MRC Systems) and cannot be auto-installed — see *Manual* step 1 below for the layout.
3. **Sets up vcpkg**: clones `microsoft/vcpkg` into `$VcpkgRoot` (or `git pull`s it if already present), runs `bootstrap-vcpkg.bat`, runs `vcpkg integrate install` so MSBuild picks up the manifest in `ConsoleDemo_FPN/vcpkg.json`, and pre-installs `opencv4:x64-windows` + `onnxruntime:x64-windows`.
4. **Persists `VCPKG_ROOT`** as a User environment variable.

> First-run vcpkg install builds OpenCV from source and can take 20–40 min and a few GB of disk. Subsequent runs are seconds.

### Manual

If you'd rather wire each dependency yourself, do these four things:

**1. Sphinx SDK** (vendor-only, no automation possible). The `.vcxproj` expects it as a **sibling directory** to this repo:

```
<parent>\
├── SphinxSDK-demo\          ← this repo
│   └── ConsoleDemo_FPN\
└── SphinxLib\               ← from the Sphinx SDK installer
    ├── SphinxLib.h
    ├── Debug64\SphinxLib.lib
    └── Release64\SphinxLib.lib
```

Make sure `SphinxLib.dll` is on `PATH` or sits next to the built `.exe`. `Ws2_32.lib` and `IPHlpApi.Lib` come from the Windows SDK — no setup required.

**2. Visual Studio 2022** with the *Desktop development with C++* workload (provides the v143 toolset, MSVC 19.3+, and the Windows 10/11 SDK). C++17 is required.

**3. vcpkg + C++ libraries** (OpenCV for image conversion / overlay drawing, ONNX Runtime for inference):

```bat
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
C:\vcpkg\vcpkg integrate install
C:\vcpkg\vcpkg install opencv4:x64-windows onnxruntime:x64-windows
setx VCPKG_ROOT C:\vcpkg
```

Dependencies are declared in `ConsoleDemo_FPN/vcpkg.json` (manifest mode). After `vcpkg integrate install`, MSBuild auto-restores them on first build, so the explicit `vcpkg install` line above is just a pre-warm.

**4. ONNX models** — already checked in:

- `ConsoleDemo_FPN/hand_landmark_full.onnx` (~11 MB) — MediaPipe Hands, 21 hand landmarks.
- `ConsoleDemo_FPN/pose_landmark_full.onnx` (~13 MB) — MediaPipe Pose, 33 body landmarks (used for forearm tracking).

`scripts\build.bat` copies both next to the produced `.exe`. If you build manually, copy them yourself or change `WRIST_MODEL_PATH` / `POSE_MODEL_PATH` at the top of `ConsoleDemo.cpp` to absolute paths.

To regenerate the models from the official MediaPipe TFLite weights:

```bat
pip install tf2onnx tensorflow onnx tflite2onnx
python ConsoleDemo_FPN\scripts\prepare_models.py
```

The pose estimator picks whichever side (left or right) has higher elbow+wrist visibility above `POSE_MIN_VISIBILITY` (default 0.5). If neither side is visible enough, the wrist estimator falls back to the static `WRIST_FOREARM_AXIS_DEG`. Each frame's JSON includes a `forearm` block reporting which axis was used.

---

## Compilation

There are three options.

### A. `scripts\build.bat` (recommended)

```bat
scripts\build.bat            :: Release|x64 (default)
scripts\build.bat Debug      :: Debug|x64
```

It locates MSBuild via `vswhere`, builds the solution with `/m` (parallel), and copies both ONNX models into the output directory so the resulting `ConsoleDemo.exe` runs immediately.

Output: `ConsoleDemo_FPN\x64\<Config>\ConsoleDemo.exe`

### B. Visual Studio GUI

1. Open `ConsoleDemo_FPN\ConsoleDemo.sln`.
2. Select `Release | x64` (or `Debug | x64`).
3. Build (`Ctrl+Shift+B`).
4. Manually copy `ConsoleDemo_FPN\hand_landmark_full.onnx` and `pose_landmark_full.onnx` next to the built exe.

### C. MSBuild from a Developer Command Prompt

```bat
msbuild ConsoleDemo_FPN\ConsoleDemo.sln /p:Configuration=Release /p:Platform=x64 /m
copy ConsoleDemo_FPN\hand_landmark_full.onnx ConsoleDemo_FPN\x64\Release\
copy ConsoleDemo_FPN\pose_landmark_full.onnx ConsoleDemo_FPN\x64\Release\
```

> Build x64 only — the Win32 configurations exist in the project file but the Sphinx library paths are wired up only for x64.

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
