"""Minimal capture-to-NumPy smoke test for the ctypes Sphinx binding.

Discovers cameras; if one is present, connects, grabs a few frames into NumPy
arrays, prints their shapes/timestamps, and saves the last frame as PNG.

Run:  python capture_test.py [num_frames]
"""
import sys

import sphinx


def main(num_frames: int = 30) -> int:
    print("Discovering cameras...")
    devices = sphinx.Camera.discover()
    if not devices:
        print("No cameras found. (DLL loaded and discovery ran OK.)")
        return 0

    for d in devices:
        print(f"  [{d.index}] {d.model} / {d.manufacturer}  ip={d.ip}  adapter={d.adapter_ip}")

    dev = devices[0]
    print(f"\nConnecting to [{dev.index}] {dev.model} ...")
    cam = sphinx.Camera()
    cam.open(dev)
    print(f"  model={cam_model(cam)}")

    cam.start()
    print(f"  geometry: {cam.width}x{cam.height}  bpp={cam.bpp}  "
          f"format=0x{cam.pixel_format:08X}  color={cam.is_color}  fpn={cam.fpn}")

    last = None
    for i in range(num_frames):
        img, hdr = cam.get_frame()
        last = img
        if i < 3 or i == num_frames - 1:
            print(f"  frame {i}: shape={img.shape} dtype={img.dtype} "
                  f"ts={hdr.TimeStamp} counter={hdr.FrameCounter}")

    cam.stop()
    cam.close()

    if last is not None:
        try:
            import cv2
            # OpenCV writes BGR; our color frames are RGB.
            out = cv2.cvtColor(last, cv2.COLOR_RGB2BGR) if last.ndim == 3 else last
            cv2.imwrite("capture.png", out)
            print("\nSaved capture.png")
        except ImportError:
            print("\n(install opencv-python to save a PNG)")
    return 0


def cam_model(cam: sphinx.Camera) -> str:
    try:
        return cam.get_string("DeviceModelName")
    except sphinx.SphinxError:
        return "?"


if __name__ == "__main__":
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 30
    raise SystemExit(main(n))
