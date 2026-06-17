"""Apply lens-distortion correction from calib.json (produced by calibrate.py).

Supports both the standard (pinhole) and fisheye models. Remap maps are built
once per image size and cached, so per-frame cost is just a cv2.remap. If the
live image size differs from the calibration size, the camera matrix is scaled
accordingly.
"""
from __future__ import annotations

import json
import os

import cv2
import numpy as np

DEFAULT_CALIB = os.path.join(os.path.dirname(os.path.abspath(__file__)), "calib.json")


class Undistorter:
    def __init__(self, calib_path: str = DEFAULT_CALIB):
        with open(calib_path) as f:
            d = json.load(f)
        self.model = d["model"]
        self.K = np.array(d["K"], dtype=np.float64)
        self.dist = np.array(d["dist"], dtype=np.float64)
        self.calib_size = tuple(d["image_size"])   # (w, h)
        self._maps: dict = {}

    @staticmethod
    def exists(calib_path: str = DEFAULT_CALIB) -> bool:
        return os.path.exists(calib_path)

    def _maps_for(self, size: tuple):
        if size in self._maps:
            return self._maps[size]
        w, h = size
        # scale intrinsics if the live resolution differs from calibration
        sx, sy = w / self.calib_size[0], h / self.calib_size[1]
        K = self.K.copy()
        K[0, 0] *= sx; K[0, 2] *= sx
        K[1, 1] *= sy; K[1, 2] *= sy

        if self.model == "fisheye":
            new_k = cv2.fisheye.estimateNewCameraMatrixForUndistortRectify(
                K, self.dist, (w, h), np.eye(3), balance=0.0)
            m1, m2 = cv2.fisheye.initUndistortRectifyMap(
                K, self.dist, np.eye(3), new_k, (w, h), cv2.CV_16SC2)
        else:
            new_k, _ = cv2.getOptimalNewCameraMatrix(K, self.dist, (w, h), 0)
            m1, m2 = cv2.initUndistortRectifyMap(
                K, self.dist, None, new_k, (w, h), cv2.CV_16SC2)
        self._maps[size] = (m1, m2)
        return m1, m2

    def apply(self, img: np.ndarray) -> np.ndarray:
        h, w = img.shape[:2]
        m1, m2 = self._maps_for((w, h))
        return cv2.remap(img, m1, m2, cv2.INTER_LINEAR)


if __name__ == "__main__":
    # Self-test with a synthetic calibration on capture.png (if present).
    import sys
    test_calib = {
        "model": "standard",
        "K": [[800.0, 0.0, 640.0], [0.0, 800.0, 480.0], [0.0, 0.0, 1.0]],
        "dist": [-0.3, 0.1, 0.0, 0.0, 0.0],
        "image_size": [1280, 960],
        "rms": 0.0,
    }
    with open("_calib_test.json", "w") as f:
        json.dump(test_calib, f)
    u = Undistorter("_calib_test.json")
    img = cv2.imread("capture.png")
    if img is None:
        img = np.zeros((960, 1280, 3), np.uint8)
    out = u.apply(img)
    cv2.imwrite("undistort_test.png", out)
    os.remove("_calib_test.json")
    print(f"undistort self-test OK: in {img.shape} -> out {out.shape}, wrote undistort_test.png")
