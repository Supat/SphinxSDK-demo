"""Shared ChArUco board definition, used by make_charuco.py and calibrate.py.

The absolute square/marker sizes only matter for *metric* pose; the camera
intrinsics and distortion coefficients we need for undistortion are
scale-independent, so you can print the board at any size — just keep it flat
and rigid.
"""
import cv2

SQUARES_X = 7          # number of chessboard squares horizontally
SQUARES_Y = 5          # vertically
SQUARE_LEN = 0.030     # metres (nominal; see note above)
MARKER_LEN = 0.022     # metres (must be < SQUARE_LEN)
DICT_ID = cv2.aruco.DICT_5X5_1000


def build_board():
    dictionary = cv2.aruco.getPredefinedDictionary(DICT_ID)
    board = cv2.aruco.CharucoBoard(
        (SQUARES_X, SQUARES_Y), SQUARE_LEN, MARKER_LEN, dictionary)
    return dictionary, board
