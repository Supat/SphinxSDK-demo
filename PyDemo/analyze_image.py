"""Offline wrist-angle check: run MediaPipe on a saved image or a live grab.

Usage:
  python analyze_image.py [path]      # analyze an image file (default capture.png)
  python analyze_image.py --grab [N]  # grab N frames from the camera, analyze last

Saves an annotated `<name>_annotated.png`. This is the go/no-go test: confirm
MediaPipe detects a subject on THIS camera's imagery before building the live app.
"""
import sys

import cv2
import numpy as np

from overlay import draw_overlay
from wrist import WristEstimator


def analyze(rgb: np.ndarray, out_path: str) -> None:
    est = WristEstimator()
    try:
        res = est.process(rgb)
    finally:
        est.close()

    print(f"  pose detected: {bool(res.pose_px)}")
    print(f"  hands detected: {len(res.hands_px)}")
    if not res.wrists:
        print("  no wrists found (need a hand - and ideally an arm - in frame)")
    for wr in res.wrists:
        kind = "flexion" if wr.elbow_px else "hand-orientation (no forearm)"
        print(f"  {wr.side}: {wr.angle_deg:.1f} deg  [{kind}]")

    annotated = draw_overlay(rgb, res)
    cv2.imwrite(out_path, cv2.cvtColor(annotated, cv2.COLOR_RGB2BGR))
    print(f"  saved {out_path}")


def main() -> int:
    args = sys.argv[1:]
    if args and args[0] == "--grab":
        import sphinx
        n = int(args[1]) if len(args) > 1 else 30
        devs = sphinx.Camera.discover()
        if not devs:
            print("No camera found.")
            return 1
        cam = sphinx.Camera()
        cam.open(devs[0])
        cam.start()
        rgb = None
        for _ in range(n):
            img, _ = cam.get_frame()
            rgb = img if img.ndim == 3 else cv2.cvtColor(img, cv2.COLOR_GRAY2RGB)
        cam.stop()
        cam.close()
        print(f"Grabbed {n} frames ({rgb.shape}); analyzing last:")
        analyze(rgb, "grab_annotated.png")
    else:
        path = args[0] if args else "capture.png"
        bgr = cv2.imread(path)
        if bgr is None:
            print(f"Could not read {path}")
            return 1
        rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB)
        print(f"Analyzing {path} ({rgb.shape}):")
        out = path.rsplit(".", 1)[0] + "_annotated.png"
        analyze(rgb, out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
