"""Download MediaPipe Hands + Pose TFLite models and convert to ONNX.

Usage:
    pip install tf2onnx tensorflow onnx tflite2onnx
    python prepare_models.py [output_dir]

Default output_dir: parent directory (next to ConsoleDemo.exe build).

- hand_landmark_full.tflite -> hand_landmark_full.onnx (via tflite2onnx)
- pose_landmark_full.tflite -> pose_landmark_full.onnx (via tf2onnx; tflite2onnx
  cannot handle pose's padding op)
"""

from pathlib import Path
import subprocess
import sys
import urllib.request

MODELS = {
    "hand_landmark_full": {
        "url": "https://storage.googleapis.com/mediapipe-assets/hand_landmark_full.tflite",
        "converter": "tflite2onnx",
    },
    "pose_landmark_full": {
        "url": "https://storage.googleapis.com/mediapipe-assets/pose_landmark_full.tflite",
        "converter": "tf2onnx",
    },
}


def convert_tflite2onnx(tflite_path: Path, onnx_path: Path):
    import tflite2onnx
    tflite2onnx.convert(str(tflite_path), str(onnx_path))


def convert_tf2onnx(tflite_path: Path, onnx_path: Path):
    subprocess.check_call([
        sys.executable, "-m", "tf2onnx.convert",
        "--tflite", str(tflite_path),
        "--output", str(onnx_path),
        "--opset", "13",
    ])


def main():
    here = Path(__file__).resolve().parent
    out_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else here.parent
    out_dir.mkdir(parents=True, exist_ok=True)

    for name, info in MODELS.items():
        tflite_path = here / f"{name}.tflite"
        onnx_path = out_dir / f"{name}.onnx"
        if not tflite_path.exists():
            print(f"Downloading {info['url']} ...")
            urllib.request.urlretrieve(info["url"], tflite_path)
        print(f"Converting {tflite_path.name} -> {onnx_path}")
        if info["converter"] == "tflite2onnx":
            convert_tflite2onnx(tflite_path, onnx_path)
        else:
            convert_tf2onnx(tflite_path, onnx_path)
        print(f"  -> {onnx_path.stat().st_size} bytes")


if __name__ == "__main__":
    main()
