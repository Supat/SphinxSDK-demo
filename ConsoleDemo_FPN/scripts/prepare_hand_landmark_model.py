"""Download MediaPipe Hands hand_landmark_full.tflite and convert to ONNX.

Usage:
    pip install tflite2onnx onnx requests
    python prepare_hand_landmark_model.py [output_path]

Default output_path: ../hand_landmark_full.onnx (next to ConsoleDemo.exe build).

Notes:
- The official MediaPipe distribution ships TFLite only.
- tflite2onnx prints "Data type float16 not supported" warnings; the resulting
  graph runs fine with onnxruntime CPU EP. If accuracy matters, re-quantize.
- Model I/O: input "input_1" [1,3,224,224] float, outputs include "Identity"
  [1,63] (image-space landmarks in pixels) and a 1-element hand-presence score.
"""

from pathlib import Path
import sys
import urllib.request

URL = "https://storage.googleapis.com/mediapipe-assets/hand_landmark_full.tflite"


def main():
    here = Path(__file__).resolve().parent
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else here.parent / "hand_landmark_full.onnx"
    tflite_path = here / "hand_landmark_full.tflite"

    if not tflite_path.exists():
        print(f"Downloading {URL} ...")
        urllib.request.urlretrieve(URL, tflite_path)
    print(f"TFLite at {tflite_path} ({tflite_path.stat().st_size} bytes)")

    import tflite2onnx
    print(f"Converting -> {out}")
    tflite2onnx.convert(str(tflite_path), str(out))
    print(f"Done. ONNX size: {out.stat().st_size} bytes")


if __name__ == "__main__":
    main()
