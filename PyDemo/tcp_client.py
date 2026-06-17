"""Example consumer for the wrist-angle TCP broadcast.

Connects to the app's broadcaster and prints each JSON reading as it arrives.

Usage:  python tcp_client.py [host] [port]   (defaults 127.0.0.1 5555)
"""
import json
import socket
import sys


def main() -> int:
    host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 5555

    print(f"Connecting to {host}:{port} ...")
    sock = socket.create_connection((host, port))
    print("Connected. Waiting for readings (Ctrl+C to quit).")
    buf = b""
    try:
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                print("Server closed the connection.")
                break
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                if not line.strip():
                    continue
                msg = json.loads(line)
                angles = ", ".join(f"{w['side']}={w['angle_deg']:.1f}deg"
                                   for w in msg.get("wrists", []))
                print(f"frame {msg.get('frame')}: {angles}")
    except KeyboardInterrupt:
        pass
    finally:
        sock.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
