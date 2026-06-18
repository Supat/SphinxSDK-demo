"""Composition root for the Sphinx + MediaPipe wrist-angle app (MVP).

Wires the Model (camera + services), the passive View, and the Presenter.

    View (view.py)        widgets + intent signals + render methods
    Presenter (presenter) orchestration; the only layer that knows both sides
    Model                 sphinx.Camera, FramePipeline, WristEstimator,
                          Undistorter, AngleBroadcaster
"""
import sys

from PySide6.QtWidgets import QApplication

import sphinx
from view import MainWindow
from presenter import WristPresenter
from broadcaster import AngleBroadcaster
from undistort import Undistorter


def main() -> int:
    app = QApplication(sys.argv)

    view = MainWindow()
    camera = sphinx.Camera()
    undistorter = Undistorter() if Undistorter.exists() else None
    broadcaster = AngleBroadcaster()

    # Presenter wires itself to the view's signals and keeps a reference alive.
    presenter = WristPresenter(view, camera, undistorter, broadcaster)  # noqa: F841

    view.resize(1200, 820)
    view.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
