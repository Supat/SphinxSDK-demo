"""Frame-processing pipeline (Model layer) — Qt-free.

Takes a raw camera frame plus options and returns a ProcessedFrame (image +
wrist result). Shared by the GUI capture thread and the offline tools, so there
is one processing path rather than several copies.
"""
from __future__ import annotations

import time
from dataclasses import dataclass

import cv2
import numpy as np

from wrist import WristEstimator, FrameResult


@dataclass
class PipelineOptions:
    use_mediapipe: bool = True
    use_undistort: bool = True
    infer_width: int = 800


@dataclass
class ProcessedFrame:
    image: np.ndarray                 # RGB, undistorted + downscaled (no overlay)
    result: FrameResult | None        # wrist detection, or None if mediapipe off
    frame_index: int


class FramePipeline:
    """Stateful (frame counter + lazily-created estimator). The estimator is
    created on first use so it lives on whichever thread calls process()."""

    def __init__(self, undistorter=None):
        self._undistorter = undistorter
        self._estimator: WristEstimator | None = None
        self._i = 0

    def close(self) -> None:
        if self._estimator is not None:
            self._estimator.close()
            self._estimator = None

    def process(self, raw: np.ndarray, opts: PipelineOptions) -> ProcessedFrame:
        self._i += 1
        rgb = raw if raw.ndim == 3 else cv2.cvtColor(raw, cv2.COLOR_GRAY2RGB)
        if opts.use_undistort and self._undistorter is not None:
            rgb = self._undistorter.apply(rgb)
        if rgb.shape[1] > opts.infer_width:
            s = opts.infer_width / rgb.shape[1]
            rgb = cv2.resize(rgb, None, fx=s, fy=s, interpolation=cv2.INTER_AREA)

        result = None
        if opts.use_mediapipe:
            if self._estimator is None:
                self._estimator = WristEstimator()
            result = self._estimator.process(rgb)
        return ProcessedFrame(np.ascontiguousarray(rgb), result, self._i)


def format_angles(result: FrameResult) -> str:
    if not result or not result.wrists:
        return "no wrist detected"
    return "   ".join(f"{w.side}: {w.angle_deg:.0f}°" for w in result.wrists)


def to_payload(result: FrameResult, frame_index: int) -> dict:
    return {
        "frame": frame_index,
        "t": time.time(),
        "wrists": [{"side": w.side, "angle_deg": round(w.angle_deg, 2)}
                   for w in result.wrists],
    }
