import argparse
import time

import cv2


def csi_pipeline(width, height, fps, sensor_id=0):
    return (
        f"nvarguscamerasrc sensor-id={sensor_id} ! "
        f"video/x-raw(memory:NVMM), width=(int){width}, height=(int){height}, "
        f"format=(string)NV12, framerate=(fraction){fps}/1 ! "
        "nvvidconv ! video/x-raw, format=(string)BGRx ! "
        "videoconvert ! video/x-raw, format=(string)BGR ! appsink drop=true sync=false"
    )


def open_capture(args):
    if args.source == "csi":
        return cv2.VideoCapture(
            csi_pipeline(args.width, args.height, args.fps, args.sensor_id),
            cv2.CAP_GSTREAMER,
        )
    if args.source == "usb":
        cap = cv2.VideoCapture(args.device)
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, args.width)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, args.height)
        cap.set(cv2.CAP_PROP_FPS, args.fps)
        return cap
    if args.source == "rtsp":
        return cv2.VideoCapture(args.uri)
    return cv2.VideoCapture(args.file)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", choices=["csi", "usb", "rtsp", "file"], default="csi")
    parser.add_argument("--device", type=int, default=0)
    parser.add_argument("--sensor-id", type=int, default=0)
    parser.add_argument("--uri", default="")
    parser.add_argument("--file", default="")
    parser.add_argument("--width", type=int, default=1280)
    parser.add_argument("--height", type=int, default=720)
    parser.add_argument("--fps", type=int, default=30)
    args = parser.parse_args()

    cap = open_capture(args)
    if not cap.isOpened():
        raise RuntimeError("camera open failed")

    last = time.perf_counter()
    fps_value = 0.0

    while True:
        ok, frame = cap.read()
        if not ok:
            print("frame read failed")
            break

        now = time.perf_counter()
        dt = now - last
        last = now
        if dt > 0:
            fps_value = 0.9 * fps_value + 0.1 * (1.0 / dt)

        cv2.putText(
            frame,
            f"FPS: {fps_value:.1f}",
            (20, 40),
            cv2.FONT_HERSHEY_SIMPLEX,
            1.0,
            (0, 255, 0),
            2,
        )
        cv2.imshow("camera preview", frame)
        if cv2.waitKey(1) & 0xFF in (27, ord("q")):
            break

    cap.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
