"""Generate a printable ChArUco calibration board -> charuco_board.png.

Print it (any scale), mount it flat on a rigid surface, then run calibrate.py.
"""
import cv2

from charuco import SQUARES_X, SQUARES_Y, build_board

# ~A4 landscape at ~150 DPI; aspect roughly matches the 7x5 board.
OUT_W, OUT_H = 1754, 1240


def main() -> int:
    _, board = build_board()
    img = board.generateImage((OUT_W, OUT_H), marginSize=40, borderBits=1)
    cv2.imwrite("charuco_board.png", img)
    print(f"Wrote charuco_board.png  ({SQUARES_X}x{SQUARES_Y} ChArUco).")
    print("Print it, mount flat, then run:  python calibrate.py")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
