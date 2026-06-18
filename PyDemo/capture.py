"""Capture thread: drives the camera + FramePipeline off the GUI thread.

Emits the domain ProcessedFrame (image + readings) — no QImage, no overlay —
so the presenter/view own all rendering. Reads a shared PipelineOptions so the
presenter can flip toggles live without re-tangling state into the thread.
"""
from __future__ import annotations

from PySide6.QtCore import QThread, Signal

import sphinx
from pipeline import FramePipeline, PipelineOptions


class CaptureThread(QThread):
    frameReady = Signal(object)   # ProcessedFrame
    info = Signal(str)

    def __init__(self, camera: sphinx.Camera, pipeline: FramePipeline,
                 options: PipelineOptions):
        super().__init__()
        self._camera = camera
        self._pipeline = pipeline
        self._options = options
        self._running = False

    def stop(self) -> None:
        self._running = False

    def run(self) -> None:
        try:
            self._camera.start()
        except Exception as e:  # noqa: BLE001 - surface any SDK/start failure
            self.info.emit(f"start failed: {e}")
            return
        self._running = True
        self.info.emit("Streaming.")
        try:
            while self._running:
                try:
                    raw, _hdr = self._camera.get_frame()
                except sphinx.SphinxError as e:
                    self.info.emit(str(e))
                    continue
                self.frameReady.emit(self._pipeline.process(raw, self._options))
        finally:
            self._camera.stop()
            self._pipeline.close()
            self.info.emit("Stopped.")
