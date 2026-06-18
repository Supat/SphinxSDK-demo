"""Rendering of wrist/pose results onto an image (View-layer concern).

Kept separate from estimation (wrist.py) and free of Qt, so it can be reused by
the GUI view and the offline analyzer alike.
"""
from __future__ import annotations

import cv2

from wrist import FrameResult

# minimal pose skeleton (shoulder/elbow/wrist) index pairs
_POSE_EDGES = [(11, 13), (13, 15), (12, 14), (14, 16), (11, 12)]


def draw_overlay(rgb, result: FrameResult):
    """Return an annotated copy: pose skeleton, hand points, forearm/hand
    vectors, and the wrist-angle label."""
    out = rgb.copy()
    if result.pose_px:
        for a, b in _POSE_EDGES:
            if a < len(result.pose_px) and b < len(result.pose_px):
                cv2.line(out, tuple(map(int, result.pose_px[a])),
                         tuple(map(int, result.pose_px[b])), (0, 200, 0), 2)
    for hand in result.hands_px:
        for x, y in hand:
            cv2.circle(out, (int(x), int(y)), 2, (200, 200, 0), -1)
    for wr in result.wrists:
        wpx = tuple(map(int, wr.wrist_px))
        tip = tuple(map(int, wr.hand_tip_px))
        cv2.line(out, wpx, tip, (0, 255, 255), 2)
        if wr.elbow_px is not None:
            cv2.line(out, tuple(map(int, wr.elbow_px)), wpx, (255, 128, 0), 2)
        cv2.circle(out, wpx, 6, (0, 0, 255), -1)
        cv2.putText(out, f"{wr.side}: {wr.angle_deg:.0f} deg",
                    (wpx[0] + 8, wpx[1] - 8), cv2.FONT_HERSHEY_SIMPLEX,
                    0.7, (0, 0, 255), 2, cv2.LINE_AA)
    return out
