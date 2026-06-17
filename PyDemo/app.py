"""PySide6 live app: stream the Sphinx camera, estimate wrist angle with
MediaPipe, and expose camera settings.

Architecture (mirrors the C++ Qt demo):
  - Camera is opened on the GUI thread (feature ranges become available).
  - A GrabWorker QThread runs cam.start() -> grab loop -> cam.stop(), running
    MediaPipe per frame and emitting an annotated QImage + angle text.
  - Feature edits (exposure/gain/...) are issued from the GUI thread on the
    shared Camera; control-channel access is independent of the stream.
"""
from __future__ import annotations

import sys
import time

import cv2
import numpy as np
from PySide6.QtCore import Qt, QThread, Signal
from PySide6.QtGui import QImage, QPixmap
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QLabel, QComboBox, QPushButton,
    QCheckBox, QHBoxLayout, QVBoxLayout, QFormLayout, QDoubleSpinBox, QLineEdit,
    QTableWidget, QTableWidgetItem, QHeaderView, QDockWidget, QTabWidget,
    QPlainTextEdit, QMessageBox, QSlider, QAbstractItemView, QSpinBox,
)
from PySide6.QtCore import QTimer

import sphinx
from wrist import WristEstimator
from broadcaster import AngleBroadcaster

INFER_WIDTH = 800   # downscale before MediaPipe / display for smooth CPU rate


def to_qimage(rgb: np.ndarray) -> QImage:
    if rgb.ndim == 2:
        h, w = rgb.shape
        return QImage(rgb.data, w, h, w, QImage.Format_Grayscale8).copy()
    h, w, _ = rgb.shape
    return QImage(rgb.data, w, h, 3 * w, QImage.Format_RGB888).copy()


class GrabWorker(QThread):
    frameReady = Signal(QImage)
    angles = Signal(str)
    info = Signal(str)

    def __init__(self, cam: sphinx.Camera, use_mediapipe: bool,
                 broadcaster: AngleBroadcaster | None = None):
        super().__init__()
        self._cam = cam
        self._use_mp = use_mediapipe
        self._broadcaster = broadcaster
        self._running = False

    def set_mediapipe(self, on: bool):
        self._use_mp = on

    def stop(self):
        self._running = False

    def run(self):
        est = WristEstimator() if True else None  # created on this thread
        try:
            self._cam.start()
        except Exception as e:  # noqa
            self.info.emit(f"start failed: {e}")
            return
        self._running = True
        self.info.emit("Streaming.")
        frame_i = 0
        try:
            while self._running:
                try:
                    img, _hdr = self._cam.get_frame()
                except sphinx.SphinxError as e:
                    self.info.emit(str(e))
                    continue
                frame_i += 1
                rgb = img if img.ndim == 3 else cv2.cvtColor(img, cv2.COLOR_GRAY2RGB)
                # downscale for inference + display
                if rgb.shape[1] > INFER_WIDTH:
                    scale = INFER_WIDTH / rgb.shape[1]
                    rgb = cv2.resize(rgb, None, fx=scale, fy=scale, interpolation=cv2.INTER_AREA)
                if self._use_mp:
                    res = est.process(rgb)
                    rgb = WristEstimator.draw(rgb, res)
                    if res.wrists:
                        self.angles.emit("   ".join(
                            f"{w.side}: {w.angle_deg:.0f}°" for w in res.wrists))
                    else:
                        self.angles.emit("no wrist detected")
                    # broadcast structured readings to any TCP clients
                    if self._broadcaster is not None and res.wrists:
                        self._broadcaster.send({
                            "frame": frame_i,
                            "t": time.time(),
                            "wrists": [{"side": w.side, "angle_deg": round(w.angle_deg, 2)}
                                       for w in res.wrists],
                        })
                self.frameReady.emit(to_qimage(np.ascontiguousarray(rgb)))
        finally:
            self._cam.stop()
            est.close()
            self.info.emit("Stopped.")


def make_editor(cam: sphinx.Camera, fi: sphinx.FeatureInfo) -> QWidget:
    """Editor widget for a feature, bound to the camera (GUI-thread writes)."""
    if fi.type in ("integer", "float"):
        spin = QDoubleSpinBox()
        spin.setDecimals(0 if fi.type == "integer" else 3)
        lo, hi = (fi.int_min, fi.int_max) if fi.type == "integer" else (fi.float_min, fi.float_max)
        spin.setRange(lo, hi) if hi > lo else spin.setRange(-1e12, 1e12)
        if fi.unit:
            spin.setSuffix(" " + fi.unit)
        try:
            spin.setValue(cam.get_int(fi.name) if fi.type == "integer" else cam.get_float(fi.name))
        except sphinx.SphinxError:
            pass
        spin.setEnabled(fi.writable)

        def on_change(v, name=fi.name, t=fi.type):
            try:
                cam.set_int(name, int(v)) if t == "integer" else cam.set_float(name, float(v))
            except sphinx.SphinxError:
                pass
        spin.valueChanged.connect(on_change)
        return spin

    if fi.type == "enum":
        combo = QComboBox()
        combo.addItems(fi.enum_entries)
        try:
            combo.setCurrentText(cam.get_enum(fi.name))
        except sphinx.SphinxError:
            pass
        combo.setEnabled(fi.writable)
        combo.currentTextChanged.connect(
            lambda t, name=fi.name: _safe(lambda: cam.set_enum(name, t)))
        return combo

    if fi.type == "bool":
        chk = QCheckBox()
        try:
            chk.setChecked(cam.get_bool(fi.name))
        except sphinx.SphinxError:
            pass
        chk.setEnabled(fi.writable)
        chk.toggled.connect(lambda on, name=fi.name: _safe(lambda: cam.set_bool(name, on)))
        return chk

    if fi.type == "command":
        btn = QPushButton("Execute")
        btn.setEnabled(fi.writable)
        btn.clicked.connect(lambda _=False, name=fi.name: _safe(lambda: cam.command(name)))
        return btn

    edit = QLineEdit()
    try:
        edit.setText(cam.get_string(fi.name))
    except sphinx.SphinxError:
        pass
    edit.setReadOnly(not fi.writable)
    edit.editingFinished.connect(lambda name=fi.name, e=edit: _safe(lambda: cam.set_string(name, e.text())))
    return edit


def _safe(fn):
    try:
        fn()
    except sphinx.SphinxError:
        pass


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Sphinx + MediaPipe — Wrist Angle")
        self.cam = sphinx.Camera()
        self.worker: GrabWorker | None = None
        self.devices: list[sphinx.DeviceInfo] = []
        self.broadcaster = AngleBroadcaster()

        # controls row
        self.device_combo = QComboBox()
        self.refresh_btn = QPushButton("Refresh")
        self.connect_btn = QPushButton("Connect")
        self.start_btn = QPushButton("Start")
        self.stop_btn = QPushButton("Stop")
        self.mp_chk = QCheckBox("Wrist angle (MediaPipe)")
        self.mp_chk.setChecked(True)
        self.bcast_chk = QCheckBox("Broadcast TCP")
        self.port_spin = QSpinBox()
        self.port_spin.setRange(1024, 65535)
        self.port_spin.setValue(5555)
        self.bcast_status = QLabel("clients: 0")

        controls = QHBoxLayout()
        controls.addWidget(QLabel("Device:"))
        controls.addWidget(self.device_combo, 1)
        controls.addWidget(self.refresh_btn)
        controls.addWidget(self.connect_btn)
        controls.addSpacing(12)
        controls.addWidget(self.start_btn)
        controls.addWidget(self.stop_btn)
        controls.addSpacing(12)
        controls.addWidget(self.mp_chk)
        controls.addSpacing(12)
        controls.addWidget(self.bcast_chk)
        controls.addWidget(QLabel("port:"))
        controls.addWidget(self.port_spin)
        controls.addWidget(self.bcast_status)

        self.view = QLabel("No image")
        self.view.setAlignment(Qt.AlignCenter)
        self.view.setMinimumSize(800, 600)
        self.view.setStyleSheet("background:#202020; color:#888;")

        self.angle_label = QLabel("—")
        self.angle_label.setStyleSheet("font-size:18px; font-weight:bold; padding:4px;")

        self.log = QPlainTextEdit()
        self.log.setReadOnly(True)
        self.log.setMaximumBlockCount(500)
        self.log.setFixedHeight(110)

        central = QWidget()
        v = QVBoxLayout(central)
        v.addLayout(controls)
        v.addWidget(self.angle_label)
        v.addWidget(self.view, 1)
        v.addWidget(self.log)
        self.setCentralWidget(central)

        # settings dock
        self.control_host = QWidget()
        self.control_form = QFormLayout(self.control_host)
        self.feature_table = QTableWidget(0, 3)
        self.feature_table.setHorizontalHeaderLabels(["Feature", "Value", "Unit"])
        self.feature_table.horizontalHeader().setSectionResizeMode(1, QHeaderView.Stretch)
        self.feature_table.verticalHeader().setVisible(False)
        self.feature_table.setEditTriggers(QAbstractItemView.NoEditTriggers)
        tabs = QTabWidget()
        tabs.addTab(self.control_host, "Controls")
        tabs.addTab(self.feature_table, "All Features")
        dock = QDockWidget("Camera Settings", self)
        dock.setWidget(tabs)
        dock.setMinimumWidth(360)
        self.addDockWidget(Qt.RightDockWidgetArea, dock)

        # wiring
        self.refresh_btn.clicked.connect(self.on_refresh)
        self.connect_btn.clicked.connect(self.on_connect)
        self.start_btn.clicked.connect(self.on_start)
        self.stop_btn.clicked.connect(self.on_stop)
        self.mp_chk.toggled.connect(self.on_mp_toggle)
        self.bcast_chk.toggled.connect(self.on_broadcast_toggle)

        # refresh the connected-client count periodically
        self._bcast_timer = QTimer(self)
        self._bcast_timer.setInterval(1000)
        self._bcast_timer.timeout.connect(self._update_bcast_status)
        self._bcast_timer.start()

        self.update_buttons()
        self.on_refresh()

    # ---- slots ----
    def on_refresh(self):
        try:
            self.devices = sphinx.Camera.discover()
        except sphinx.SphinxError as e:
            self.devices = []
            self.log_msg(str(e))
        self.device_combo.clear()
        for d in self.devices:
            self.device_combo.addItem(f"{d.model} — {d.manufacturer} ({d.ip})")
        self.log_msg(f"Found {len(self.devices)} device(s).")
        self.update_buttons()

    def on_connect(self):
        idx = self.device_combo.currentIndex()
        if not (0 <= idx < len(self.devices)):
            return
        try:
            self.cam.open(self.devices[idx])
        except sphinx.SphinxError as e:
            QMessageBox.warning(self, "Connect failed", str(e))
            return
        self.log_msg(f"Connected to {self.devices[idx].model}.")
        self.populate_settings()
        self.update_buttons()

    def on_start(self):
        if self.worker:
            return
        self.worker = GrabWorker(self.cam, self.mp_chk.isChecked(), self.broadcaster)
        self.worker.frameReady.connect(self.on_frame)
        self.worker.angles.connect(self.angle_label.setText)
        self.worker.info.connect(self.log_msg)
        self.worker.finished.connect(self.on_worker_finished)
        self.worker.start()
        self.update_buttons()

    def on_stop(self):
        if self.worker:
            self.worker.stop()
            self.worker.wait(3000)

    def on_worker_finished(self):
        self.worker = None
        self.update_buttons()

    def on_mp_toggle(self, on: bool):
        if self.worker:
            self.worker.set_mediapipe(on)

    def on_broadcast_toggle(self, on: bool):
        if on:
            try:
                self.broadcaster.start(port=self.port_spin.value())
                self.log_msg(f"Broadcasting wrist angles on TCP :{self.port_spin.value()}")
            except OSError as e:
                self.bcast_chk.setChecked(False)
                QMessageBox.warning(self, "Broadcast failed", str(e))
                return
        else:
            self.broadcaster.stop()
            self.log_msg("Broadcast stopped.")
        self.port_spin.setEnabled(not on)
        self._update_bcast_status()

    def _update_bcast_status(self):
        if self.broadcaster.is_running():
            self.bcast_status.setText(f"clients: {self.broadcaster.client_count()}")
        else:
            self.bcast_status.setText("off")

    def on_frame(self, image: QImage):
        self.view.setPixmap(QPixmap.fromImage(image).scaled(
            self.view.size(), Qt.KeepAspectRatio, Qt.SmoothTransformation))

    def log_msg(self, msg: str):
        self.log.appendPlainText(msg)

    # ---- settings ----
    def populate_settings(self):
        # Controls tab: a curated subset.
        while self.control_form.rowCount():
            self.control_form.removeRow(0)
        targets = [
            ("ExposureAuto", "Exposure Auto"), ("ExposureTime", "Exposure"),
            ("GainAuto", "Gain Auto"), ("Gain", "Gain"),
            ("AcquisitionFrameRate", "Frame Rate"),
            ("BalanceWhiteAuto", "White Balance"),
        ]
        for name, label in targets:
            fi = self.cam.describe(name)
            if fi.available and fi.type != "unknown":
                self.control_form.addRow(label + ":", make_editor(self.cam, fi))

        # All-features tab.
        feats = self.cam.feature_list()
        self.feature_table.setRowCount(len(feats))
        for r, fi in enumerate(feats):
            item = QTableWidgetItem(fi.display_name or fi.name)
            item.setToolTip(fi.tooltip or fi.name)
            self.feature_table.setItem(r, 0, item)
            self.feature_table.setCellWidget(r, 1, make_editor(self.cam, fi))
            self.feature_table.setItem(r, 2, QTableWidgetItem(fi.unit))

    def update_buttons(self):
        streaming = self.worker is not None
        self.device_combo.setEnabled(not streaming)
        self.refresh_btn.setEnabled(not streaming)
        self.connect_btn.setEnabled(not streaming and bool(self.devices))
        self.start_btn.setEnabled(self.cam.is_open and not streaming)
        self.stop_btn.setEnabled(streaming)

    def closeEvent(self, event):
        self.on_stop()
        self.broadcaster.stop()
        self.cam.close()
        super().closeEvent(event)


def main() -> int:
    app = QApplication(sys.argv)
    w = MainWindow()
    w.resize(1200, 820)
    w.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
