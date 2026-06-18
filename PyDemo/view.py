"""View layer (passive). Builds widgets, emits intent signals for user actions,
and exposes render methods the presenter calls. It holds NO camera/service refs
and contains no orchestration — all Qt and rendering live here, nothing else.
"""
from __future__ import annotations

from typing import Protocol

import numpy as np
from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QAction, QImage, QKeySequence, QPixmap
from PySide6.QtWidgets import (
    QAbstractItemView,
    QCheckBox,
    QComboBox,
    QDockWidget,
    QDoubleSpinBox,
    QFormLayout,
    QHBoxLayout,
    QHeaderView,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QPlainTextEdit,
    QPushButton,
    QSpinBox,
    QStyle,
    QTableWidget,
    QTableWidgetItem,
    QTabWidget,
    QVBoxLayout,
    QWidget,
)

from overlay import draw_overlay
from pipeline import ProcessedFrame


# ---- feature accessor interface (the View edits features through this) ------
class FeatureAccess(Protocol):
    def get_int(self, name: str) -> int: ...
    def set_int(self, name: str, value: int) -> None: ...
    def get_float(self, name: str) -> float: ...
    def set_float(self, name: str, value: float) -> None: ...
    def get_string(self, name: str) -> str: ...
    def set_string(self, name: str, value: str) -> None: ...
    def get_bool(self, name: str) -> bool: ...
    def set_bool(self, name: str, value: bool) -> None: ...
    def get_enum(self, name: str) -> str: ...
    def set_enum(self, name: str, entry: str) -> None: ...
    def command(self, name: str) -> None: ...


def _safe(fn):
    try:
        fn()
    except Exception:  # noqa: BLE001 - feature R/W is best-effort in the grid
        pass


def to_qimage(rgb: np.ndarray) -> QImage:
    if rgb.ndim == 2:
        h, w = rgb.shape
        return QImage(rgb.data, w, h, w, QImage.Format_Grayscale8).copy()
    h, w, _ = rgb.shape
    return QImage(rgb.data, w, h, 3 * w, QImage.Format_RGB888).copy()


def make_editor(access: FeatureAccess, fi) -> QWidget:
    """Build an editor widget for one feature, bound to the accessor."""
    if fi.type in ("integer", "float"):
        spin = QDoubleSpinBox()
        spin.setDecimals(0 if fi.type == "integer" else 3)
        lo, hi = (fi.int_min, fi.int_max) if fi.type == "integer" else (fi.float_min, fi.float_max)
        spin.setRange(lo, hi) if hi > lo else spin.setRange(-1e12, 1e12)
        if fi.unit:
            spin.setSuffix(" " + fi.unit)
        _safe(lambda: spin.setValue(access.get_int(fi.name) if fi.type == "integer"
                                    else access.get_float(fi.name)))
        spin.setEnabled(fi.writable)

        def on_change(v, name=fi.name, t=fi.type):
            _safe(lambda: access.set_int(name, int(v)) if t == "integer"
                  else access.set_float(name, float(v)))
        spin.valueChanged.connect(on_change)
        return spin

    if fi.type == "enum":
        combo = QComboBox()
        combo.addItems(fi.enum_entries)
        _safe(lambda: combo.setCurrentText(access.get_enum(fi.name)))
        combo.setEnabled(fi.writable)
        combo.currentTextChanged.connect(
            lambda t, name=fi.name: _safe(lambda: access.set_enum(name, t)))
        return combo

    if fi.type == "bool":
        chk = QCheckBox()
        _safe(lambda: chk.setChecked(access.get_bool(fi.name)))
        chk.setEnabled(fi.writable)
        chk.toggled.connect(lambda on, name=fi.name: _safe(lambda: access.set_bool(name, on)))
        return chk

    if fi.type == "command":
        btn = QPushButton("Execute")
        btn.setEnabled(fi.writable)
        btn.clicked.connect(lambda _=False, name=fi.name: _safe(lambda: access.command(name)))
        return btn

    edit = QLineEdit()
    _safe(lambda: edit.setText(access.get_string(fi.name)))
    edit.setReadOnly(not fi.writable)
    edit.editingFinished.connect(
        lambda name=fi.name, e=edit: _safe(lambda: access.set_string(name, e.text())))
    return edit


class MainWindow(QMainWindow):
    # ---- intent signals (the presenter subscribes) ----
    refreshRequested = Signal()
    connectRequested = Signal(int)
    startRequested = Signal()
    stopRequested = Signal()
    mediapipeToggled = Signal(bool)
    undistortToggled = Signal(bool)
    orientationChanged = Signal(int)       # rotation degrees: 0/90/180/270
    mirrorToggled = Signal(bool)
    broadcastToggled = Signal(bool, int)   # (on, port)
    generateBoardRequested = Signal()
    runCalibrationRequested = Signal()
    closeRequested = Signal()

    def __init__(self):
        super().__init__()
        self.setWindowTitle("Sphinx + MediaPipe — Wrist Angle")
        self._streaming = False
        self._open = False
        self._has_devices = False

        self.device_combo = QComboBox()
        self.refresh_btn = QPushButton("Refresh")
        self.connect_btn = QPushButton("Connect")
        self.start_btn = QPushButton("Start")
        self.stop_btn = QPushButton("Stop")
        # built-in (QStyle) icons — cross-platform, no asset files needed
        st = self.style()
        self.refresh_btn.setIcon(st.standardIcon(QStyle.SP_BrowserReload))
        self.connect_btn.setIcon(st.standardIcon(QStyle.SP_DriveNetIcon))
        self.start_btn.setIcon(st.standardIcon(QStyle.SP_MediaPlay))
        self.stop_btn.setIcon(st.standardIcon(QStyle.SP_MediaStop))
        self.mp_chk = QCheckBox("Wrist angle (MediaPipe)")
        self.mp_chk.setChecked(True)
        self.undistort_chk = QCheckBox("Lens Correction")
        self.undistort_chk.setEnabled(False)
        self.orient_combo = QComboBox()
        for label, deg in (("0°", 0), ("90° CW", 90), ("180°", 180), ("270° CW", 270)):
            self.orient_combo.addItem(label, deg)
        self.mirror_chk = QCheckBox("Mirror")
        self.bcast_chk = QCheckBox("Broadcast TCP")
        self.port_spin = QSpinBox()
        self.port_spin.setRange(1024, 65535)
        self.port_spin.setValue(5555)
        self.bcast_status = QLabel("off")
        self.bcast_status.setFixedWidth(80)
        self.bcast_status.setAlignment(Qt.AlignLeft | Qt.AlignVCenter)

        controls = QHBoxLayout()
        controls.addWidget(QLabel("Device:"))
        controls.addWidget(self.device_combo, 1)
        controls.addWidget(self.refresh_btn)
        controls.addWidget(self.connect_btn)
        controls.addSpacing(12)
        controls.addWidget(self.start_btn)
        controls.addWidget(self.stop_btn)

        controls2 = QHBoxLayout()
        controls2.addWidget(self.mp_chk)
        controls2.addWidget(self.undistort_chk)
        controls2.addSpacing(12)
        controls2.addWidget(QLabel("Orientation:"))
        controls2.addWidget(self.orient_combo)
        controls2.addWidget(self.mirror_chk)
        controls2.addSpacing(12)
        controls2.addWidget(self.bcast_chk)
        controls2.addWidget(QLabel("port:"))
        controls2.addWidget(self.port_spin)
        controls2.addWidget(self.bcast_status)
        controls2.addStretch(1)

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
        v.addLayout(controls2)
        v.addWidget(self.angle_label)
        v.addWidget(self.view, 1)
        v.addWidget(self.log)
        self.setCentralWidget(central)

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

        # translate widget events -> intent signals
        self.refresh_btn.clicked.connect(self.refreshRequested)
        self.connect_btn.clicked.connect(
            lambda: self.connectRequested.emit(self.device_combo.currentIndex()))
        self.start_btn.clicked.connect(self.startRequested)
        self.stop_btn.clicked.connect(self.stopRequested)
        self.mp_chk.toggled.connect(self.mediapipeToggled)
        self.undistort_chk.toggled.connect(self.undistortToggled)
        self.orient_combo.currentIndexChanged.connect(
            lambda _i: self.orientationChanged.emit(self.orient_combo.currentData()))
        self.mirror_chk.toggled.connect(self.mirrorToggled)
        self.bcast_chk.toggled.connect(
            lambda on: self.broadcastToggled.emit(on, self.port_spin.value()))

        self._build_menu()
        self._update_buttons()

    def _build_menu(self) -> None:
        """Menu bar reusing the same intent signals as the toolbar. Start/Stop/
        Connect/Refresh actions are enable-synced in _update_buttons()."""
        mb = self.menuBar()

        file_m = mb.addMenu("&File")
        self.act_refresh = QAction("&Refresh", self)
        self.act_refresh.setShortcut(QKeySequence("F5"))
        self.act_refresh.triggered.connect(self.refreshRequested)
        file_m.addAction(self.act_refresh)
        self.act_connect = QAction("&Connect", self)
        self.act_connect.triggered.connect(
            lambda: self.connectRequested.emit(self.device_combo.currentIndex()))
        file_m.addAction(self.act_connect)
        file_m.addSeparator()
        quit_act = QAction("&Quit", self)
        quit_act.setShortcut(QKeySequence.Quit)
        quit_act.triggered.connect(self.close)
        file_m.addAction(quit_act)

        cam_m = mb.addMenu("&Camera")
        self.act_start = QAction("&Start", self)
        self.act_start.triggered.connect(self.startRequested)
        cam_m.addAction(self.act_start)
        self.act_stop = QAction("Sto&p", self)
        self.act_stop.triggered.connect(self.stopRequested)
        cam_m.addAction(self.act_stop)

        tools_m = mb.addMenu("&Tools")
        board_act = QAction("&Generate ChArUco Board…", self)
        board_act.triggered.connect(self.generateBoardRequested)
        tools_m.addAction(board_act)
        self.act_calibrate = QAction("Run Lens &Calibration…", self)
        self.act_calibrate.triggered.connect(self.runCalibrationRequested)
        tools_m.addAction(self.act_calibrate)

        help_m = mb.addMenu("&Help")
        about_act = QAction("&About", self)
        about_act.triggered.connect(
            lambda: QMessageBox.about(
                self, "About",
                "Sphinx + MediaPipe wrist-angle demo.\n"
                "GigE Vision capture, lens correction, wrist-angle estimation,\n"
                "and TCP broadcast."))
        help_m.addAction(about_act)

    # ---- render methods (called by the presenter) ----
    def show_devices(self, names: list[str]) -> None:
        self.device_combo.clear()
        self.device_combo.addItems(names)
        self._has_devices = bool(names)
        self._update_buttons()

    def set_connected(self, on: bool) -> None:
        self._open = on
        self._update_buttons()

    def set_streaming(self, on: bool) -> None:
        self._streaming = on
        self._update_buttons()

    def show_frame(self, pf: ProcessedFrame) -> None:
        img = draw_overlay(pf.image, pf.result) if pf.result is not None else pf.image
        self.view.setPixmap(QPixmap.fromImage(to_qimage(img)).scaled(
            self.view.size(), Qt.KeepAspectRatio, Qt.SmoothTransformation))

    def show_angle(self, text: str) -> None:
        self.angle_label.setText(text)

    def log_msg(self, msg: str) -> None:
        self.log.appendPlainText(msg)

    def show_error(self, title: str, msg: str) -> None:
        QMessageBox.warning(self, title, msg)

    def show_info(self, title: str, msg: str) -> None:
        QMessageBox.information(self, title, msg)

    def set_broadcast_status(self, text: str) -> None:
        self.bcast_status.setText(text)

    def set_broadcast_enabled_state(self, port_editable: bool) -> None:
        self.port_spin.setEnabled(port_editable)

    def uncheck_broadcast(self) -> None:
        self.bcast_chk.setChecked(False)

    def set_undistort_available(self, available: bool, model: str = "") -> None:
        self.undistort_chk.setEnabled(available)
        self.undistort_chk.setChecked(available)
        self.undistort_chk.setToolTip(
            f"Lens model: {model}" if available else "Run calibrate.py to create calib.json first.")

    def populate_features(self, curated: list, all_features: list, access: FeatureAccess) -> None:
        while self.control_form.rowCount():
            self.control_form.removeRow(0)
        for label, fi in curated:
            self.control_form.addRow(label + ":", make_editor(access, fi))
        self.feature_table.setRowCount(len(all_features))
        for r, fi in enumerate(all_features):
            item = QTableWidgetItem(fi.display_name or fi.name)
            item.setToolTip(fi.tooltip or fi.name)
            self.feature_table.setItem(r, 0, item)
            self.feature_table.setCellWidget(r, 1, make_editor(access, fi))
            self.feature_table.setItem(r, 2, QTableWidgetItem(fi.unit))

    def _update_buttons(self) -> None:
        s = self._streaming
        can_connect = not s and self._has_devices
        can_start = self._open and not s
        self.device_combo.setEnabled(not s)
        self.refresh_btn.setEnabled(not s)
        self.connect_btn.setEnabled(can_connect)
        self.start_btn.setEnabled(can_start)
        self.stop_btn.setEnabled(s)
        # keep menu actions in sync with the toolbar
        self.act_refresh.setEnabled(not s)
        self.act_connect.setEnabled(can_connect)
        self.act_start.setEnabled(can_start)
        self.act_stop.setEnabled(s)

    def closeEvent(self, event):
        self.closeRequested.emit()
        super().closeEvent(event)
