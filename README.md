# SphinxSDK Demo — ConsoleDemo_FPN

A Windows C++ console demo for the **MRC Systems Sphinx SDK** (`SphinxLib`). It discovers a GigE Vision camera, streams images over a ring buffer, optionally applies Fixed Pattern Noise (FPN) dark-frame subtraction, runs a hand-landmark model to estimate **wrist angle** with **flexor/extensor** classification, and **broadcasts the result as JSON over TCP** on the loopback interface.

## Requirements

- Windows (x64)
- Visual Studio 2022 with the **v143** C++ build tools (MSVC 19.3+, C++17)
- The Sphinx SDK from MRC Systems (not included in this repo) — request it from the camera vendor
- [vcpkg](https://github.com/microsoft/vcpkg) for the OpenCV / ONNX Runtime dependencies
- A hand-landmark ONNX model (e.g. MediaPipe Hands `hand_landmark_full.onnx`, 224×224 RGB input, 21 landmarks output)
- A GigE Vision camera reachable on a network adapter on the host (the FPN code path activates only for `DeviceModelName == "GVRD-MRC HighSpeed"`)

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

### Hand landmark model

Download or convert a MediaPipe Hands landmark model to ONNX and place it next to the built `ConsoleDemo.exe` as `hand_landmark_full.onnx` (or change `WRIST_MODEL_PATH` at the top of `ConsoleDemo.cpp`). Expected I/O: 1×224×224×3 float input in `[0,1]`, output containing 21 landmarks (63 floats) and an optional 1-element score tensor.

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

Sample line:

```json
{"frame":1234,"timestamp":987654321,"valid":true,"angle_deg":12.341,"class":"extensor","confidence":0.972}
```

## Repository layout

- `ConsoleDemo_FPN/ConsoleDemo.cpp` — discovery, init, acquisition thread, BMP writer
- `ConsoleDemo_FPN/darknoise.{h,cpp}` — SSE2 dark-frame subtraction
- `ConsoleDemo_FPN/bayer.{h,cpp}` — bilinear demosaic for Bayer pixel formats
- `ConsoleDemo_FPN/SphinxLib.h` — vendored copy of the SDK header (reference only)
