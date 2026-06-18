"""TCP server that streams wrist-angle readings to connected clients.

Each reading is sent as one line of JSON (newline-delimited / JSONL), e.g.:

    {"frame": 42, "t": 1718638655.12, "wrists": [{"side": "left", "angle_deg": 168.4}]}

Any number of clients may connect (one-to-many). Sends happen from the grab
worker thread; the accept loop runs on its own daemon thread. Dead/slow clients
are dropped rather than blocking acquisition.
"""
from __future__ import annotations

import json
import socket
import threading


def local_ip() -> str:
    """Best-effort primary LAN IPv4 of this host (not loopback). Uses a UDP
    socket's chosen route; sends nothing. Falls back to 127.0.0.1."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("8.8.8.8", 80))
        return s.getsockname()[0]
    except OSError:
        return "127.0.0.1"
    finally:
        s.close()


class AngleBroadcaster:
    SEND_TIMEOUT = 2.0   # seconds; a client slower than this is dropped

    def __init__(self):
        self._srv: socket.socket | None = None
        self._clients: list[socket.socket] = []
        self._lock = threading.Lock()
        self._accept_thread: threading.Thread | None = None
        self._running = False
        self.host = ""
        self.port = 0

    def is_running(self) -> bool:
        return self._running

    def start(self, host: str = "0.0.0.0", port: int = 5555) -> None:
        self.stop()
        srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        srv.bind((host, port))
        srv.listen(5)
        srv.settimeout(0.5)
        self._srv = srv
        self.host, self.port = host, port
        self._running = True
        self._accept_thread = threading.Thread(target=self._accept_loop, daemon=True)
        self._accept_thread.start()

    def _accept_loop(self) -> None:
        while self._running:
            try:
                conn, _addr = self._srv.accept()
            except TimeoutError:
                continue
            except OSError:
                break
            conn.settimeout(self.SEND_TIMEOUT)
            with self._lock:
                self._clients.append(conn)

    def client_count(self) -> int:
        with self._lock:
            return len(self._clients)

    def send(self, payload: dict) -> None:
        """Serialize payload as a JSON line and push to all clients."""
        if not self._running:
            return
        line = (json.dumps(payload) + "\n").encode("utf-8")
        dead = []
        with self._lock:
            for c in self._clients:
                try:
                    c.sendall(line)
                except OSError:
                    dead.append(c)
            for c in dead:
                self._clients.remove(c)
                try:
                    c.close()
                except OSError:
                    pass

    def stop(self) -> None:
        self._running = False
        t = self._accept_thread
        if t and t.is_alive():
            t.join(timeout=1.0)
        self._accept_thread = None
        with self._lock:
            for c in self._clients:
                try:
                    c.close()
                except OSError:
                    pass
            self._clients.clear()
        if self._srv:
            try:
                self._srv.close()
            except OSError:
                pass
            self._srv = None
