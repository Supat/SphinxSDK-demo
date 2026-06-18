"""Presenter: the only place that knows about both the View and the Model.

Subscribes to the View's intent signals, drives the camera/services/capture
thread, and pushes results back through the View's render methods. Holds no
widgets and does no rendering (no cv2 overlay, no QImage).
"""
from __future__ import annotations

import os
import subprocess
import sys
import time

from PySide6.QtCore import QTimer

import sphinx
from broadcaster import AngleBroadcaster, local_ip
from capture import CaptureThread
from pipeline import FramePipeline, PipelineOptions, format_angles, to_payload

# Curated controls to surface (presentation policy lives here, not in the View).
CONTROL_TARGETS = [
    ("AcquisitionMode", "Acquisition Mode"),
    ("AutoControl", "Auto Control"),
    ("ExposureAuto", "Exposure Auto"), ("ExposureTime", "Exposure"),
    ("GainAuto", "Gain Auto"),
    ("Gain", "Gain"), ("GlobalGain", "Gain"),
    ("GainRed", "Gain Red"), ("GainGreen", "Gain Green"), ("GainBlue", "Gain Blue"),
    ("AcquisitionFrameRate", "Frame Rate"),
    ("BalanceWhiteAuto", "White Balance"),
]


class WristPresenter:
    def __init__(self, view, camera: sphinx.Camera, undistorter, broadcaster: AngleBroadcaster):
        self.view = view
        self.camera = camera
        self.undistorter = undistorter
        self.broadcaster = broadcaster
        self.pipeline = FramePipeline(undistorter)
        self.options = PipelineOptions(use_undistort=undistorter is not None)
        self.thread: CaptureThread | None = None
        self.devices: list[sphinx.DeviceInfo] = []

        # subscribe to View intents
        view.refreshRequested.connect(self.refresh)
        view.connectRequested.connect(self.connect_device)
        view.startRequested.connect(self.start)
        view.stopRequested.connect(self.stop)
        view.mediapipeToggled.connect(self.set_mediapipe)
        view.undistortToggled.connect(self.set_undistort)
        view.orientationChanged.connect(self.set_rotation)
        view.mirrorToggled.connect(self.set_mirror)
        view.broadcastToggled.connect(self.set_broadcast)
        view.generateBoardRequested.connect(self.generate_board)
        view.runCalibrationRequested.connect(self.run_calibration)
        view.testBroadcastRequested.connect(self.test_broadcast)
        view.testPatternToggled.connect(self.set_test_pattern)
        view.closeRequested.connect(self.shutdown)

        # poll the broadcast client count -> View
        self._timer = QTimer()
        self._timer.setInterval(1000)
        self._timer.timeout.connect(self._tick)
        self._timer.start()

        # synthetic test-pattern emitter (off until toggled)
        self._demo_i = 0
        self._demo_timer = QTimer()
        self._demo_timer.setInterval(50)   # ~20 Hz
        self._demo_timer.timeout.connect(self._emit_test_pattern)

        model = undistorter.model if undistorter is not None else ""
        view.set_undistort_available(undistorter is not None, model)
        self.refresh()

    # ---- intent handlers ----
    def refresh(self):
        try:
            self.devices = sphinx.Camera.discover()
        except sphinx.SphinxError as e:
            self.devices = []
            self.view.log_msg(str(e))
        self.view.show_devices(
            [f"{d.model} — {d.manufacturer} ({d.ip})" for d in self.devices])
        self.view.log_msg(f"Found {len(self.devices)} device(s).")

    def connect_device(self, idx: int):
        if not (0 <= idx < len(self.devices)):
            return
        try:
            self.camera.open(self.devices[idx])
        except sphinx.SphinxError as e:
            self.view.show_error("Connect failed", str(e))
            return
        self.view.log_msg(f"Connected to {self.devices[idx].model}.")
        self._populate_settings()
        self.view.set_connected(True)

    def start(self):
        if self.thread is not None:
            return
        self.thread = CaptureThread(self.camera, self.pipeline, self.options)
        self.thread.frameReady.connect(self.on_frame)
        self.thread.info.connect(self.view.log_msg)
        self.thread.finished.connect(self._on_thread_finished)
        self.thread.start()
        self.view.set_streaming(True)

    def stop(self):
        if self.thread is not None:
            self.thread.stop()
            self.thread.wait(3000)

    def _on_thread_finished(self):
        self.thread = None
        self.view.set_streaming(False)

    def set_mediapipe(self, on: bool):
        self.options.use_mediapipe = on

    def set_undistort(self, on: bool):
        self.options.use_undistort = on

    def set_rotation(self, degrees: int):
        self.options.rotation = degrees

    def set_mirror(self, on: bool):
        self.options.mirror = on

    def set_broadcast(self, on: bool, port: int):
        if on:
            try:
                self.broadcaster.start(port=port)
                self.view.log_msg(f"Broadcasting wrist angles on {local_ip()}:{port}")
            except OSError as e:
                self.view.uncheck_broadcast()
                self.view.show_error("Broadcast failed", str(e))
                return
        else:
            self.broadcaster.stop()
            self.view.log_msg("Broadcast stopped.")
        self.view.set_broadcast_enabled_state(not on)
        self._tick()

    # ---- frame results ----
    def on_frame(self, pf):
        self.view.show_frame(pf)
        if pf.result is not None:
            self.view.show_angle(format_angles(pf.result))
            if self.broadcaster.is_running() and pf.result.wrists:
                self.broadcaster.send(to_payload(pf.result, pf.frame_index))

    def _tick(self):
        if self.broadcaster.is_running():
            self.view.set_broadcast_status(f"clients: {self.broadcaster.client_count()}")
        else:
            self.view.set_broadcast_status("off")

    def _populate_settings(self):
        curated = []
        for name, label in CONTROL_TARGETS:
            fi = self.camera.describe(name)
            if fi.available and fi.type != "unknown":
                curated.append((label, fi))
        all_features = self.camera.feature_list()
        self.view.populate_features(curated, all_features, access=self.camera)

    # ---- tools ----
    def generate_board(self):
        from make_charuco import save_board
        here = os.path.dirname(os.path.abspath(__file__))
        path = os.path.join(here, "charuco_board.png")
        save_board(path)
        self.view.log_msg(f"Saved {path}")
        self.view.show_info("ChArUco board", f"Saved board to:\n{path}\n\n"
                            "Print it, mount it flat, then run Lens Calibration.")

    def run_calibration(self):
        if self.thread is not None:
            self.view.show_error("Calibration", "Stop streaming before calibrating.")
            return
        # Release the camera so the external calibrator can open it exclusively.
        self.camera.close()
        self.view.set_connected(False)
        here = os.path.dirname(os.path.abspath(__file__))
        subprocess.Popen([sys.executable, "calibrate.py"], cwd=here)
        self.view.log_msg("Launched calibrate.py — reconnect the camera after it finishes.")
        self.view.show_info(
            "Lens Calibration",
            "Calibration launched in a separate window.\n\n"
            "Capture board views there (SPACE), press 'c' to save calib.json, "
            "then reconnect the camera here and enable Lens Correction.")

    def test_broadcast(self):
        if not self.broadcaster.is_running():
            self.view.show_error("Test Broadcast", "Enable Broadcast TCP first.")
            return
        here = os.path.dirname(os.path.abspath(__file__))
        port = self.broadcaster.port
        ip = local_ip()
        flags = getattr(subprocess, "CREATE_NEW_CONSOLE", 0)
        subprocess.Popen([sys.executable, "tcp_client.py", ip, str(port)],
                         cwd=here, creationflags=flags)
        self.view.log_msg(f"Opened broadcast test client on {ip}:{port}")

    def set_test_pattern(self, on: bool):
        if on:
            if not self.broadcaster.is_running():
                self.view.show_info(
                    "Test Pattern",
                    "Emitting a synthetic sweeping wrist angle. Enable "
                    "Broadcast TCP so connected clients receive it.")
            self._demo_i = 0
            self._demo_timer.start()
            self.view.log_msg("Broadcast test pattern: ON")
        else:
            self._demo_timer.stop()
            self.view.log_msg("Broadcast test pattern: OFF")

    DEMO_PERIOD_S = 30.0   # one full 0 -> 90 -> 0 sweep lasts this long

    def _emit_test_pattern(self):
        self._demo_i += 1
        # 0 -> 90 -> 0 triangle sweep over DEMO_PERIOD_S seconds
        t = self._demo_i * self._demo_timer.interval() / 1000.0
        phase = (t % self.DEMO_PERIOD_S) / self.DEMO_PERIOD_S
        angle = 90.0 * (1.0 - abs(2.0 * phase - 1.0))
        self.broadcaster.send({
            "frame": self._demo_i,
            "t": time.time(),
            "wrists": [{"side": "demo", "angle_deg": round(angle, 2)}],
        })

    def shutdown(self):
        self._demo_timer.stop()
        self.stop()
        self.broadcaster.stop()
        self.camera.close()
