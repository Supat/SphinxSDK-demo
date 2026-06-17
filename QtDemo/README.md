# Qt GUI demo

A Qt Widgets front-end for the Sphinx SDK that reuses the FPN / Bayer logic from
`../ConsoleDemo_FPN`. It wraps the `GEV*` API in a reusable `Camera` class and
provides a window with device selection, a live image view, start/stop, and
save.

## Layout

| File | Role |
|------|------|
| `Camera.h` / `Camera.cpp` | SDK wrapper: `discover()`, `open()`, `start()`, `stop()`, `close()`. Runs the grab loop on a worker thread and emits `frameReady(QImage)`. FPN dark-frame subtraction is handled internally for `GVRD-MRC HighSpeed`. All SphinxLib/`windows.h` code is confined to `Camera.cpp`. |
| `MainWindow.h` / `MainWindow.cpp` | UI and signal wiring. |
| `main.cpp` | `QApplication` entry point. |
| `CMakeLists.txt` | Build; reuses `darknoise.cpp` / `bayer.cpp` from the console demo and links `SphinxLib.lib`. |

## Architecture

```
discover() ──► DeviceInfo list ──► open(dev)
                                     │
                              start() spawns std::thread
                                     │  grabLoop():
                                     │    GEVGetImageRingBuffer
                                     │    processFrame()  (FPN subtract)
                                     │    toQImage()       (mono8 / Bayer demosaic)
                                     ▼
   GUI thread  ◄── frameReady(QImage) (queued connection) ── worker thread
```

The grab thread emits `frameReady` with a **detached copy** of the frame (the
ring buffer is requeued immediately after). Because the worker thread differs
from the receiver's thread, Qt automatically promotes the connection to a
queued one — the GUI thread does the painting.

## Build

You need Qt installed. For the **v140 (VS2015)** toolset this repo targets, use a
**Qt 5** build for `msvc2015`/`msvc2017` (Qt 6 requires MSVC 2019+).

```bat
cd QtDemo
cmake -B build -G "Visual Studio 14 2015" -A x64 ^
      -DCMAKE_PREFIX_PATH=C:/Qt/5.15.2/msvc2017_64
cmake --build build --config Release
```

(Adjust the generator/Qt path to your setup; e.g. `Visual Studio 16 2019` with a
matching Qt build.) The post-build steps copy `SphinxLib.dll`, the SDK `Extras64`
runtime files, and the Qt DLLs (`windeployqt`) next to the executable.

## Run

The same prerequisites as the console demo apply: a reachable GigE Vision camera
on the host adapter, and `..\SphinxLib` present. Launch `SphinxQtDemo.exe`, click
**Refresh**, pick a device, **Connect**, then **Start** / **Stop**; **Save…**
writes the current frame as PNG/BMP/JPG.

## Limitations (scaffold)

- Display handles **8-bit mono** and the two 8-bit Bayer formats
  (`BAYGR8` / `BAYRG8`); 10/12/16-bit and other color formats are not yet mapped.
- No exposure/gain controls in the UI yet — the FPN exposure juggling is internal.
- Single camera (SDK id 1), matching the original demo.
