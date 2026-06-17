# SphinxSDK Demo

Windows demos for the **MRC Systems Sphinx SDK** (`SphinxLib`). Both discover a GigE Vision camera, stream images over a ring buffer, optionally apply Fixed Pattern Noise (FPN) dark-frame subtraction, and save frames.

- **`ConsoleDemo_FPN/`** — the original console app (VS2015 / `v140`). Documented below.
- **`QtDemo/`** — a Qt Widgets GUI front-end built on a reusable `Camera` class that wraps the SDK; live view, device selection, start/stop, and save. See [`QtDemo/README.md`](QtDemo/README.md).

## ConsoleDemo_FPN

A Windows C++ console demo. It discovers a GigE Vision camera, streams images over a ring buffer, optionally applies FPN dark-frame subtraction, and saves the last frame as `image.bmp`.

## Requirements

- Windows (x64)
- Visual Studio 2015 or newer with the **v140** C++ build tools
- The Sphinx SDK from MRC Systems (not included in this repo) — request it from the camera vendor
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

## QtDemo (GUI front-end)

A Qt Widgets app that reuses `darknoise.cpp` / `bayer.cpp` and wraps the `GEV*`
API in a reusable `Camera` class (discovery, connect, threaded grab loop,
internal FPN). It provides device selection, a live image view, start/stop, and
save. Build with CMake against a Qt 5 `msvc` build (Qt 6 needs MSVC 2019+):

```bat
cd QtDemo
cmake -B build -G "Visual Studio 16 2019" -A x64 -DCMAKE_PREFIX_PATH=C:/Qt/5.15.2/msvc2019_64
cmake --build build --config Release
```

Post-build copies `SphinxLib.dll`, the SDK `Extras64` runtime, and the Qt DLLs
next to the executable. Full details in [`QtDemo/README.md`](QtDemo/README.md).

## Repository layout

- `ConsoleDemo_FPN/ConsoleDemo.cpp` — discovery, init, acquisition thread, BMP writer
- `ConsoleDemo_FPN/darknoise.{h,cpp}` — dark-frame subtraction (OpenMP `darknoise_bw_subtract` + an SSE2 variant)
- `ConsoleDemo_FPN/bayer.{h,cpp}` — bilinear demosaic for Bayer pixel formats
- `ConsoleDemo_FPN/SphinxLib.h` — vendored copy of the SDK header (reference only)
- `QtDemo/` — Qt GUI front-end (`Camera` class + `MainWindow`)
