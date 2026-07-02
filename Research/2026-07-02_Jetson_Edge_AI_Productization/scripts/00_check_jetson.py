import importlib
import platform
import shutil
import subprocess
import sys


def run(cmd):
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, check=False)
        return result.stdout.strip() or result.stderr.strip()
    except FileNotFoundError:
        return "not found"


def check_module(name):
    try:
        module = importlib.import_module(name)
        return getattr(module, "__version__", "installed")
    except Exception as exc:
        return f"missing ({exc.__class__.__name__})"


def main():
    print("== System ==")
    print(f"python: {sys.version.split()[0]}")
    print(f"platform: {platform.platform()}")
    print(f"machine: {platform.machine()}")

    print("\n== NVIDIA / Jetson ==")
    print(f"nvcc: {run(['nvcc', '--version'])}")
    print(f"trtexec: {run(['trtexec', '--version'])}")
    print(f"tegrastats: {'found' if shutil.which('tegrastats') else 'not found'}")
    print(f"nvpmodel: {run(['nvpmodel', '-q'])}")

    print("\n== Python packages ==")
    for name in ["cv2", "numpy", "torch", "ultralytics", "onnx", "onnxruntime"]:
        print(f"{name}: {check_module(name)}")

    print("\n== Torch CUDA ==")
    try:
        import torch

        print(f"cuda available: {torch.cuda.is_available()}")
        if torch.cuda.is_available():
            print(f"cuda device: {torch.cuda.get_device_name(0)}")
    except Exception as exc:
        print(f"torch check failed: {exc}")

    print("\n== Cameras ==")
    print(run(["v4l2-ctl", "--list-devices"]))


if __name__ == "__main__":
    main()
