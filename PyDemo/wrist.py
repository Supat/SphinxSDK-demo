"""Wrist-angle estimation from an RGB frame using the MediaPipe **Tasks** API
(PoseLandmarker + HandLandmarker).

"Wrist angle" = flexion/extension: the angle between the forearm (elbow -> wrist,
from Pose) and the hand axis (wrist -> middle-finger MCP, from Hands). 180 deg =
straight wrist. We run both models and match each hand to the nearest Pose wrist
to recover the forearm. Angles are 2D image-plane estimates from a single
uncalibrated camera (see PyDemo/README.md caveats).

The installed mediapipe exposes only `mediapipe.tasks` (no legacy `solutions`),
so we use the Tasks landmarkers and download the .task models on first use.
"""
from __future__ import annotations

import math
import os
import urllib.request
from dataclasses import dataclass, field

import mediapipe as mp
import numpy as np
from mediapipe.tasks import python as mp_python
from mediapipe.tasks.python import vision

_MODELS = {
    "pose_landmarker.task":
        "https://storage.googleapis.com/mediapipe-models/pose_landmarker/"
        "pose_landmarker_lite/float16/latest/pose_landmarker_lite.task",
    "hand_landmarker.task":
        "https://storage.googleapis.com/mediapipe-models/hand_landmarker/"
        "hand_landmarker/float16/latest/hand_landmarker.task",
}
_MODEL_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "models")

# landmark indices
L_ELBOW, R_ELBOW = 13, 14
L_WRIST, R_WRIST = 15, 16
L_PINKY, R_PINKY = 17, 18      # pose pinky knuckle
L_INDEX, R_INDEX = 19, 20      # pose index knuckle
H_WRIST, H_MIDDLE_MCP = 0, 9

# pose landmark sets per side: (wrist, elbow, index_knuckle, pinky_knuckle)
_SIDE_POSE = {
    "left": (L_WRIST, L_ELBOW, L_INDEX, L_PINKY),
    "right": (R_WRIST, R_ELBOW, R_INDEX, R_PINKY),
}


def ensure_models(model_dir: str = _MODEL_DIR) -> dict:
    os.makedirs(model_dir, exist_ok=True)
    paths = {}
    for name, url in _MODELS.items():
        p = os.path.join(model_dir, name)
        if not os.path.exists(p):
            print(f"[wrist] downloading {name} ...")
            urllib.request.urlretrieve(url, p)
        paths[name] = p
    return paths


@dataclass
class WristResult:
    side: str
    angle_deg: float          # 2D image-plane flexion proxy (legacy)
    wrist_px: tuple
    elbow_px: tuple | None
    hand_tip_px: tuple
    flex_deg: float | None = None   # anatomical flexion/extension (3D, signed)


@dataclass
class FrameResult:
    wrists: list = field(default_factory=list)
    pose_px: list = field(default_factory=list)   # list of (x,y) for pose landmarks
    hands_px: list = field(default_factory=list)   # list of list of (x,y)


def _angle(a, b, c) -> float:
    ba, bc = a - b, c - b
    nba, nbc = np.linalg.norm(ba), np.linalg.norm(bc)
    if nba < 1e-6 or nbc < 1e-6:
        return float("nan")
    cos = float(np.dot(ba, bc) / (nba * nbc))
    return math.degrees(math.acos(max(-1.0, min(1.0, cos))))


def _flexion3d(elbow, wrist, index, pinky):
    """Signed anatomical wrist flexion/extension in degrees from 3D points
    (all in one camera-aligned frame, e.g. pose world landmarks).

    Builds the medio-lateral axis (index->pinky knuckle) as the flexion axis,
    projects the forearm (elbow->wrist) and hand (wrist->knuckle midpoint)
    vectors into the sagittal plane, and returns the signed angle between them
    about that axis. 0 = straight wrist; sign = flexion vs extension.
    """
    forearm = wrist - elbow
    hand = (index + pinky) / 2.0 - wrist
    axis = pinky - index
    na = np.linalg.norm(axis)
    if na < 1e-6:
        return None
    axis = axis / na
    fp = forearm - np.dot(forearm, axis) * axis
    hp = hand - np.dot(hand, axis) * axis
    nf, nh = np.linalg.norm(fp), np.linalg.norm(hp)
    if nf < 1e-6 or nh < 1e-6:
        return None
    fp, hp = fp / nf, hp / nh
    ang = math.degrees(math.atan2(float(np.dot(np.cross(fp, hp), axis)),
                                  float(np.dot(fp, hp))))
    return ang


class WristEstimator:
    def __init__(self, max_hands: int = 2, min_conf: float = 0.5):
        models = ensure_models()
        self._pose = vision.PoseLandmarker.create_from_options(
            vision.PoseLandmarkerOptions(
                base_options=mp_python.BaseOptions(model_asset_path=models["pose_landmarker.task"]),
                running_mode=vision.RunningMode.IMAGE,
                num_poses=1,
                min_pose_detection_confidence=min_conf,
                min_tracking_confidence=min_conf))
        self._hands = vision.HandLandmarker.create_from_options(
            vision.HandLandmarkerOptions(
                base_options=mp_python.BaseOptions(model_asset_path=models["hand_landmarker.task"]),
                running_mode=vision.RunningMode.IMAGE,
                num_hands=max_hands,
                min_hand_detection_confidence=min_conf,
                min_tracking_confidence=min_conf))

    def close(self) -> None:
        self._pose.close()
        self._hands.close()

    def process(self, rgb: np.ndarray) -> FrameResult:
        h, w = rgb.shape[:2]
        rgb = np.ascontiguousarray(rgb)
        mp_img = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
        pose = self._pose.detect(mp_img)
        hands = self._hands.detect(mp_img)

        res = FrameResult()

        pose_wrists = {}  # side -> (wrist_px, elbow_px)
        side_flex = {}    # side -> anatomical flexion/extension (deg)
        if pose.pose_landmarks:
            lms = pose.pose_landmarks[0]
            res.pose_px = [(p.x * w, p.y * h) for p in lms]
            for side, (wi, ei, _ii, _pi) in _SIDE_POSE.items():
                wpx = np.array([lms[wi].x * w, lms[wi].y * h])
                epx = np.array([lms[ei].x * w, lms[ei].y * h])
                pose_wrists[side] = (wpx, epx)
            # 3D anatomical flexion from world landmarks (same camera-aligned frame)
            world = pose.pose_world_landmarks[0] if pose.pose_world_landmarks else None
            if world is not None:
                wpts = np.array([[lm.x, lm.y, lm.z] for lm in world])
                for side, (wi, ei, ii, pi) in _SIDE_POSE.items():
                    side_flex[side] = _flexion3d(wpts[ei], wpts[wi], wpts[ii], wpts[pi])

        for hand in (hands.hand_landmarks or []):
            res.hands_px.append([(p.x * w, p.y * h) for p in hand])
            hw = np.array([hand[H_WRIST].x * w, hand[H_WRIST].y * h])
            hm = np.array([hand[H_MIDDLE_MCP].x * w, hand[H_MIDDLE_MCP].y * h])

            side, elbow = "hand", None
            if pose_wrists:
                side = min(pose_wrists, key=lambda s: np.linalg.norm(pose_wrists[s][0] - hw))
                elbow = pose_wrists[side][1]

            if elbow is not None:
                angle = _angle(elbow, hw, hm)
            else:
                v = hm - hw
                angle = math.degrees(math.atan2(-v[1], v[0])) % 360.0

            res.wrists.append(WristResult(
                side=side, angle_deg=angle,
                wrist_px=(float(hw[0]), float(hw[1])),
                elbow_px=None if elbow is None else (float(elbow[0]), float(elbow[1])),
                hand_tip_px=(float(hm[0]), float(hm[1])),
                flex_deg=side_flex.get(side)))
        return res
