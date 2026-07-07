#r "nuget: OpenCvSharp4, 4.9.0.20240103"
#r "nuget: OpenCvSharp4.runtime.win, 4.9.0.20240103"

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using OpenCvSharp;
using OpenCvSharp.Dnn;

string modelPath = GetStringArg("--model", "assets/models/yolox_tiny.onnx");
string imagePath = GetStringArg("--image", "assets/images/coco_000000039769.jpg");
string classPath = GetStringArg("--classes", "assets/classes/coco.names");
string outputPath = GetStringArg("--output", "outputs/yolox_result.jpg");
int inputSize = GetIntArg("--input", 640);
float confThreshold = GetFloatArg("--conf", 0.30f);
float nmsThreshold = GetFloatArg("--nms", 0.45f);
int cameraIndex = GetIntArg("--camera", -1);

if (!File.Exists(modelPath))
{
    Console.Error.WriteLine($"Model not found: {modelPath}");
    return;
}

if (!File.Exists(classPath))
{
    Console.Error.WriteLine($"Class file not found: {classPath}");
    return;
}

string[] classNames = File.ReadAllLines(classPath)
    .Where(line => !string.IsNullOrWhiteSpace(line))
    .ToArray();

using (var net = CvDnn.ReadNetFromOnnx(modelPath))
{
    Console.WriteLine($"Loaded model: {modelPath}");
    Console.WriteLine($"Classes: {classNames.Length}, input: {inputSize}, conf: {confThreshold}, nms: {nmsThreshold}");

    if (cameraIndex >= 0)
    {
        RunCamera(net, classNames, cameraIndex);
    }
    else
    {
        RunImage(net, classNames);
    }
}

void RunImage(Net net, string[] classNames)
{
    if (!File.Exists(imagePath))
    {
        Console.Error.WriteLine($"Image not found: {imagePath}");
        return;
    }

    using (var image = Cv2.ImRead(imagePath))
    {
        if (image.Empty())
        {
            Console.Error.WriteLine($"OpenCV failed to read image: {imagePath}");
            return;
        }

        List<Detection> detections = Detect(image, net, classNames);
        DrawDetections(image, detections, classNames);

        string outputDir = Path.GetDirectoryName(outputPath);
        if (!string.IsNullOrWhiteSpace(outputDir))
        {
            Directory.CreateDirectory(outputDir);
        }

        Cv2.ImWrite(outputPath, image);
        Console.WriteLine($"detections: {detections.Count}");
        Console.WriteLine($"saved: {outputPath}");

        Cv2.ImShow("YOLOX Image Detection", image);
        Cv2.WaitKey(0);
        Cv2.DestroyAllWindows();
    }
}

void RunCamera(Net net, string[] classNames, int cameraIndex)
{
    using (var capture = new VideoCapture(cameraIndex))
    {
        if (!capture.IsOpened())
        {
            Console.Error.WriteLine($"Could not open camera index {cameraIndex}");
            return;
        }

        using (var frame = new Mat())
        {
            DateTime lastFpsTime = DateTime.UtcNow;
            int frameCounter = 0;
            double shownFps = 0.0;

            Console.WriteLine("YOLOX camera detection started. Press ESC or q to exit.");
            while (true)
            {
                capture.Read(frame);
                if (frame.Empty())
                {
                    break;
                }

                List<Detection> detections = Detect(frame, net, classNames);
                DrawDetections(frame, detections, classNames);

                frameCounter++;
                double elapsed = (DateTime.UtcNow - lastFpsTime).TotalSeconds;
                if (elapsed >= 1.0)
                {
                    shownFps = frameCounter / elapsed;
                    frameCounter = 0;
                    lastFpsTime = DateTime.UtcNow;
                }

                Cv2.PutText(
                    frame,
                    $"FPS {shownFps:F1}",
                    new Point(10, 28),
                    HersheyFonts.HersheySimplex,
                    0.75,
                    Scalar.Yellow,
                    2,
                    LineTypes.AntiAlias);

                Cv2.ImShow("YOLOX Camera Detection", frame);
                int key = Cv2.WaitKey(1);
                if (key == 27 || key == 'q' || key == 'Q')
                {
                    break;
                }
            }
        }
    }

    Cv2.DestroyAllWindows();
}

List<Detection> Detect(Mat image, Net net, string[] classNames)
{
    double ratio;
    var detections = new List<Detection>();

    using (Mat input = Letterbox(image, inputSize, out ratio))
    using (var blob = CvDnn.BlobFromImage(
        input,
        1.0 / 255.0,
        new Size(inputSize, inputSize),
        new Scalar(0, 0, 0),
        true,
        false))
    {
        net.SetInput(blob);

        using (var output = net.Forward())
        {
            int rows;
            int cols;
            if (output.Dims == 3)
            {
                rows = output.Size(1);
                cols = output.Size(2);
            }
            else
            {
                rows = output.Rows;
                cols = output.Cols;
            }

            int expectedCols = classNames.Length + 5;
            if (cols != expectedCols)
            {
                throw new InvalidOperationException(
                    $"Unexpected YOLOX output shape {ShapeOf(output)}. Expected columns={expectedCols}. " +
                    "Use a YOLOX ONNX export with decoded output, or inspect the model with onnx_output_inspector.csx.");
            }

            float[] data = new float[checked((int)output.Total())];
            Marshal.Copy(output.Data, data, 0, data.Length);

            for (int row = 0; row < rows; row++)
            {
                int offset = row * cols;
                float objectness = data[offset + 4];
                if (objectness < confThreshold)
                {
                    continue;
                }

                int bestClass = -1;
                float bestScore = 0.0f;
                for (int classIndex = 0; classIndex < classNames.Length; classIndex++)
                {
                    float score = objectness * data[offset + 5 + classIndex];
                    if (score > bestScore)
                    {
                        bestScore = score;
                        bestClass = classIndex;
                    }
                }

                if (bestClass < 0 || bestScore < confThreshold)
                {
                    continue;
                }

                float cx = data[offset + 0];
                float cy = data[offset + 1];
                float width = data[offset + 2];
                float height = data[offset + 3];

                int left = (int)Math.Round((cx - width / 2.0f) / ratio);
                int top = (int)Math.Round((cy - height / 2.0f) / ratio);
                int boxWidth = (int)Math.Round(width / ratio);
                int boxHeight = (int)Math.Round(height / ratio);

                Rect box = ClipRect(new Rect(left, top, boxWidth, boxHeight), image.Cols, image.Rows);
                if (box.Width <= 1 || box.Height <= 1)
                {
                    continue;
                }

                detections.Add(new Detection(box, bestClass, bestScore));
            }
        }
    }

    return NonMaxSuppression(detections, nmsThreshold);
}

Mat Letterbox(Mat image, int targetSize, out double ratio)
{
    ratio = Math.Min((double)targetSize / image.Cols, (double)targetSize / image.Rows);
    int newWidth = Math.Max(1, (int)Math.Round(image.Cols * ratio));
    int newHeight = Math.Max(1, (int)Math.Round(image.Rows * ratio));

    var canvas = new Mat(new Size(targetSize, targetSize), MatType.CV_8UC3, new Scalar(114, 114, 114));
    using (var resized = new Mat())
    {
        Cv2.Resize(image, resized, new Size(newWidth, newHeight));
        using (var roi = new Mat(canvas, new Rect(0, 0, newWidth, newHeight)))
        {
            resized.CopyTo(roi);
        }
    }

    return canvas;
}

List<Detection> NonMaxSuppression(List<Detection> detections, float threshold)
{
    var remaining = detections.OrderByDescending(d => d.Score).ToList();
    var selected = new List<Detection>();

    while (remaining.Count > 0)
    {
        Detection current = remaining[0];
        selected.Add(current);
        remaining.RemoveAt(0);

        remaining = remaining
            .Where(candidate => candidate.ClassId != current.ClassId || IoU(candidate.Box, current.Box) < threshold)
            .ToList();
    }

    return selected;
}

double IoU(Rect a, Rect b)
{
    int x1 = Math.Max(a.X, b.X);
    int y1 = Math.Max(a.Y, b.Y);
    int x2 = Math.Min(a.X + a.Width, b.X + b.Width);
    int y2 = Math.Min(a.Y + a.Height, b.Y + b.Height);

    int intersectionWidth = Math.Max(0, x2 - x1);
    int intersectionHeight = Math.Max(0, y2 - y1);
    double intersection = intersectionWidth * intersectionHeight;
    double union = (a.Width * a.Height) + (b.Width * b.Height) - intersection;
    return union <= 0 ? 0 : intersection / union;
}

Rect ClipRect(Rect box, int imageWidth, int imageHeight)
{
    int x = Math.Max(0, box.X);
    int y = Math.Max(0, box.Y);
    int right = Math.Min(imageWidth - 1, box.X + box.Width);
    int bottom = Math.Min(imageHeight - 1, box.Y + box.Height);
    return new Rect(x, y, Math.Max(0, right - x), Math.Max(0, bottom - y));
}

void DrawDetections(Mat image, List<Detection> detections, string[] classNames)
{
    foreach (Detection detection in detections)
    {
        Scalar color = ColorForClass(detection.ClassId);
        Cv2.Rectangle(image, detection.Box, color, 2);

        string label = $"{classNames[detection.ClassId]} {detection.Score:P0}";
        int baseline;
        Size textSize = Cv2.GetTextSize(label, HersheyFonts.HersheySimplex, 0.55, 2, out baseline);
        int labelTop = Math.Max(0, detection.Box.Y - textSize.Height - baseline - 4);
        var labelBox = new Rect(
            detection.Box.X,
            labelTop,
            Math.Min(textSize.Width + 8, image.Cols - detection.Box.X),
            textSize.Height + baseline + 6);

        Cv2.Rectangle(image, labelBox, color, -1);
        Cv2.PutText(
            image,
            label,
            new Point(detection.Box.X + 4, labelTop + textSize.Height + 1),
            HersheyFonts.HersheySimplex,
            0.55,
            Scalar.White,
            2,
            LineTypes.AntiAlias);
    }
}

Scalar ColorForClass(int classId)
{
    int r = (37 * classId + 80) % 255;
    int g = (17 * classId + 160) % 255;
    int b = (29 * classId + 220) % 255;
    return new Scalar(b, g, r);
}

string ShapeOf(Mat mat)
{
    if (mat.Dims <= 2)
    {
        return $"{mat.Rows}x{mat.Cols}";
    }

    return string.Join("x", Enumerable.Range(0, mat.Dims).Select(i => mat.Size(i)));
}

string GetStringArg(string name, string fallback)
{
    for (int i = 0; i < Args.Count; i++)
    {
        if (Args[i] == name && i + 1 < Args.Count)
        {
            return Args[i + 1];
        }
    }

    return fallback;
}

int GetIntArg(string name, int fallback)
{
    for (int i = 0; i < Args.Count; i++)
    {
        if (Args[i] == name && i + 1 < Args.Count && int.TryParse(Args[i + 1], out int value))
        {
            return value;
        }
    }

    return fallback;
}

float GetFloatArg(string name, float fallback)
{
    for (int i = 0; i < Args.Count; i++)
    {
        if (Args[i] == name && i + 1 < Args.Count && float.TryParse(Args[i + 1], out float value))
        {
            return value;
        }
    }

    return fallback;
}

sealed class Detection
{
    public Detection(Rect box, int classId, float score)
    {
        Box = box;
        ClassId = classId;
        Score = score;
    }

    public Rect Box { get; private set; }
    public int ClassId { get; private set; }
    public float Score { get; private set; }
}
