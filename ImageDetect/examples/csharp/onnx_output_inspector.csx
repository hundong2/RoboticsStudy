#r "nuget: OpenCvSharp4, 4.9.0.20240103"
#r "nuget: OpenCvSharp4.runtime.win, 4.9.0.20240103"

using System;
using System.IO;
using System.Linq;
using OpenCvSharp;
using OpenCvSharp.Dnn;

string modelPath = GetStringArg("--model", "assets/models/yolox_tiny.onnx");
string imagePath = GetStringArg("--image", "assets/images/coco_000000039769.jpg");
int inputSize = GetIntArg("--input", 640);

if (!File.Exists(modelPath))
{
    Console.Error.WriteLine($"Model not found: {modelPath}");
    return;
}

if (!File.Exists(imagePath))
{
    Console.Error.WriteLine($"Image not found: {imagePath}");
    return;
}

using (var image = Cv2.ImRead(imagePath))
using (var blob = CvDnn.BlobFromImage(
    image,
    1.0 / 255.0,
    new Size(inputSize, inputSize),
    new Scalar(0, 0, 0),
    true,
    false))
using (var net = CvDnn.ReadNetFromOnnx(modelPath))
{
    net.SetInput(blob);

    string[] outputNames = net.GetUnconnectedOutLayersNames();
    Console.WriteLine($"model: {modelPath}");
    Console.WriteLine($"input blob: {ShapeOf(blob)}");
    Console.WriteLine($"output count: {outputNames.Length}");

    foreach (string outputName in outputNames)
    {
        using (var output = net.Forward(outputName))
        {
            Console.WriteLine($"{outputName}: dims={output.Dims}, shape={ShapeOf(output)}, type={output.Type()}");
        }
    }
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
