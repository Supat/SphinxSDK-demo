# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository purpose

A vendor demo (MRC Systems / Sphinx SDK) showing how to use `SphinxLib` to discover, connect to, and stream images from a GigE Vision camera on Windows. The single project, `ConsoleDemo_FPN`, is the FPN (Fixed Pattern Noise) variant of that demo: it adds a dark-frame subtraction path on top of the basic streaming loop.

## Build

Windows-only. The project is a Visual Studio 2022 (`v143` toolset, C++17) C++ console app.

- Open `ConsoleDemo_FPN/ConsoleDemo.sln` in Visual Studio and build (target `Debug|x64` or `Release|x64`).
- Or from a Developer Command Prompt: `msbuild ConsoleDemo_FPN\ConsoleDemo.sln /p:Configuration=Release /p:Platform=x64`.
- Required external dependency, **not in this repo**: a sibling `..\SphinxLib\` directory containing `SphinxLib.h` (also vendored here as `ConsoleDemo_FPN/SphinxLib.h` for reference) plus `Debug64\SphinxLib.lib` / `Release64\SphinxLib.lib`. Without it, linking will fail — see the `<Library Include="..\SphinxLib\...">` entries in `ConsoleDemo.vcxproj`.
- Vcpkg manifest mode is enabled (`vcpkg.json` requests `opencv4` + `onnxruntime`). Run `vcpkg integrate install` once and MSBuild auto-restores. Without these, `wrist.cpp` will not compile.
- Win32 configurations exist in the project file but the FPN library paths only wire up x64 — build x64.
- Links against `Ws2_32.lib` and `IPHlpApi.Lib`. Uses precompiled headers (`stdafx.h`); `bayer.cpp`, `darknoise.cpp`, `wrist.cpp`, and `tcp_server.cpp` opt out of PCH.

There is no test suite and no lint configuration.

## Architecture

Single-binary flow, all in `ConsoleDemo_FPN/ConsoleDemo.cpp`:

1. **`main`** — calls `GEVDiscovery` to enumerate cameras, lets the user pick one, then `GEVInit` → `GEVInitXml` → `GEVOpenStreamChannel` → `GEVTestFindMaxPacketSize` → `GEVSetPacketResend(0)`. Registers `error_callback_func` and `msg_callback_func` for SDK-driven events. On keypress, spawns the acquisition thread; on next keypress, signals it to stop.
2. **`ThreadProc`** — the grab loop. Reads `Width`/`Height`/`PixelFormat`/`PayloadSize` features, allocates a `BUFFER_COUNT` ring buffer via `GEVSetRingBuffer` (controlled by the `RING_BUFFER` define at the top of the file), starts acquisition, and loops on `GEVGetImageRingBuffer` until killed.
3. **FPN path** — gated on `DeviceModelName == "GVRD-MRC HighSpeed"`. The first 10 frames are passed through; frame 10 is captured with `ExposureTime=0` and stored as the dark reference (`darknoise_img`); from frame 11 onward, every frame goes through `darknoise_bw_subtract` (SSE2 helper in `darknoise.cpp`) before becoming the "processed" image. The exposure-time juggling around frame 10 is the trick — don't reorder it.
4. **On stop** — saves the most recent processed frame as `image.bmp` via the local `save_bmp`. Bayer pixel formats (`GVSP_PIX_BAYGR8`, `GVSP_PIX_BAYRG8`) are demosaiced through `bayer_Bilinear` in `bayer.cpp` first.

`getTimeStamp` contains a documented workaround for a SphinxLib >2.0.9 bug where `GevTimestampValue` is truncated to 32 bits — it reconstructs the high word from the previous frame's timestamp. Preserve the workaround when touching timestamp code.

5. **Wrist estimation + TCP broadcast** — added in `wrist.{h,cpp}` and `tcp_server.{h,cpp}`. Inside the grab loop, each post-FPN frame is fed to `WristEstimator::Estimate` (ONNX Runtime + OpenCV). Bayer formats are demosaiced to BGR via `bayer_Bilinear` into `rgb_live` first; mono cameras pass the raw 1-channel buffer (the estimator converts to 3-channel internally). `WristEstimator` resizes to 224×224, normalizes to `[0,1]`, runs inference, and computes the angle of the wrist→middle-MCP vector vs. a configurable forearm axis. Classification is a simple threshold on the signed angle. Result is JSON-encoded and pushed to all clients of the loopback `TcpBroadcaster` (default `127.0.0.1:5555`). Tunables live as `static const` at the top of `ConsoleDemo.cpp` (`WRIST_MODEL_PATH`, `WRIST_FOREARM_AXIS_DEG`, `WRIST_NEUTRAL_THRESH_DEG`, `WRIST_FLIP_SIGN`, `TCP_PORT`).

The hand-landmark model file (`hand_landmark_full.onnx` by default) must sit next to the built `.exe` or be referenced by absolute path. The estimator uses heuristics on output tensor shapes to identify which output is the 21-landmark tensor (63 floats) vs. the score tensor (1 float) — adjust `Impl::Impl` if a different model layout is used.

## Notes when editing

- `SphinxLib.h` is the vendor's API surface — do not modify it; it's a copy of the external SDK header.
- The `.svn` directory inside `ConsoleDemo_FPN` is from the vendor's distribution; ignore it.
- `ConsoleDemo.VC.db`, `ipch/`, and `x64/` are VS-generated and should not be hand-edited.
