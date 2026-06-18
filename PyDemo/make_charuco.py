"""Generate a printable ChArUco calibration board -> charuco_board.png.

Print it (any scale), mount it flat on a rigid surface, then run calibrate.py.
"""
import cv2

from charuco import SQUARES_X, SQUARES_Y, build_board

# ~A4 landscape at ~150 DPI; aspect roughly matches the 7x5 board.
OUT_W, OUT_H = 1754, 1240


def save_board(path: str = "charuco_board.png") -> str:
    """Render the ChArUco board to `path`; return the path written."""
    _, board = build_board()
    img = board.generateImage((OUT_W, OUT_H), marginSize=40, borderBits=1)
    cv2.imwrite(path, img)
    return path


def main() -> int:
    save_board()
    print(f"Wrote charuco_board.png  ({SQUARES_X}x{SQUARES_Y} ChArUco).")
    print("Print it, mount flat, then run:  python calibrate.py")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
