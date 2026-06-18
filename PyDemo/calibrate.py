"""Live lens calibration against the Sphinx camera using a ChArUco board.

Captures several views of the board, then calibrates with BOTH the standard
(pinhole + radial/tangential) and fisheye models and keeps whichever has the
lower reprojection error. Saves the result to calib.json, which undistort.py /
the app load.

Controls (in the preview window):
  SPACE  capture the current view (board must be well detected)
  c      calibrate with all captured views and save calib.json
  q      quit without saving

Tips: cover the whole field of view, tilt the board, include the corners, and
capture 12-20 varied views for a good fit.
"""
import json

import cv2
import numpy as np

import sphinx
from charuco import build_board

MIN_CORNERS = 6
MIN_VIEWS = 6


def _calibrate_standard(obj, img, size):
    rms, K, dist, _, _ = cv2.calibrateCamera(obj, img, size, None, None)
    return rms, K, dist.ravel()


def _calibrate_fisheye(obj, img, size):
    objf = [o.reshape(-1, 1, 3).astype(np.float64) for o in obj]
    imgf = [i.reshape(-1, 1, 2).astype(np.float64) for i in img]
    K = np.zeros((3, 3))
    D = np.zeros((4, 1))
    flags = (cv2.fisheye.CALIB_RECOMPUTE_EXTRINSIC
             | cv2.fisheye.CALIB_FIX_SKEW)
    crit = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 50, 1e-6)
    rms, K, D, _, _ = cv2.fisheye.calibrate(objf, imgf, size, K, D, flags=flags, criteria=crit)
    return rms, K, D.ravel()


def calibrate_and_save(obj_points, img_points, size) -> None:
    print(f"\nCalibrating from {len(obj_points)} views (image size {size})...")

    results = {}
    try:
        rms, K, dist = _calibrate_standard(obj_points, img_points, size)
        results["standard"] = (rms, K, dist)
        print(f"  standard: reprojection RMS = {rms:.4f}")
    except cv2.error as e:
        print(f"  standard failed: {e}")

    try:
        rms, K, dist = _calibrate_fisheye(obj_points, img_points, size)
        results["fisheye"] = (rms, K, dist)
        print(f"  fisheye:  reprojection RMS = {rms:.4f}")
    except cv2.error as e:
        print(f"  fisheye failed (often too few/degenerate views): {e}")

    if not results:
        print("Both models failed; capture more, varied views.")
        return

    model = min(results, key=lambda m: results[m][0])
    rms, K, dist = results[model]
    out = {
        "model": model,
        "K": K.tolist(),
        "dist": dist.tolist(),
        "image_size": [int(size[0]), int(size[1])],
        "rms": float(rms),
    }
    with open("calib.json", "w") as f:
        json.dump(out, f, indent=2)
    print(f"\nKept '{model}' (RMS {rms:.4f}). Saved calib.json.")


def main() -> int:
    _, board = build_board()
    detector = cv2.aruco.CharucoDetector(board)

    devs = sphinx.Camera.discover()
    if not devs:
        print("No camera found.")
        return 1
    cam = sphinx.Camera()
    cam.open(devs[0])
    cam.start()
    print("SPACE = capture view,  c = calibrate + save,  q = quit")

    obj_points, img_points, size = [], [], None
    win = "calibrate - SPACE capture / c calibrate / q quit"
    try:
        while True:
            frame, _ = cam.get_frame()
            rgb = frame if frame.ndim == 3 else cv2.cvtColor(frame, cv2.COLOR_GRAY2RGB)
            gray = cv2.cvtColor(rgb, cv2.COLOR_RGB2GRAY)
            size = (gray.shape[1], gray.shape[0])
            disp = cv2.cvtColor(rgb, cv2.COLOR_RGB2BGR)

            ch_corners, ch_ids, _m_corners, _m_ids = detector.detectBoard(gray)
            n = 0 if ch_ids is None else len(ch_ids)
            if n > 0:
                cv2.aruco.drawDetectedCornersCharuco(disp, ch_corners, ch_ids)
            cv2.putText(disp, f"views: {len(obj_points)}   corners: {n}",
                        (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 0), 2)
            cv2.imshow(win, disp)

            k = cv2.waitKey(1) & 0xFF
            if k == ord(' '):
                if n >= MIN_CORNERS:
                    obj, imgp = board.matchImagePoints(ch_corners, ch_ids)
                    if obj is not None and len(obj) >= MIN_CORNERS:
                        obj_points.append(obj)
                        img_points.append(imgp)
                        print(f"  captured view {len(obj_points)} ({len(obj)} pts)")
                else:
                    print(f"  only {n} corners - reposition the board")
            elif k == ord('c'):
                if len(obj_points) < MIN_VIEWS:
                    print(f"  need >= {MIN_VIEWS} views (have {len(obj_points)})")
                    continue
                calibrate_and_save(obj_points, img_points, size)
                break
            elif k == ord('q'):
                print("Quit without saving.")
                break
    finally:
        cam.stop()
        cam.close()
        cv2.destroyAllWindows()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
