#r "nuget: OpenCvSharp4, 4.9.0.20240103"
#r "nuget: OpenCvSharp4.runtime.win, 4.9.0.20240103"

using System;
using OpenCvSharp;

int cameraIndex = GetIntArg("--camera", 0);

using (var capture = new VideoCapture(cameraIndex))
{
    if (!capture.IsOpened())
    {
        Console.Error.WriteLine($"Could not open camera index {cameraIndex}");
        return;
    }

    using (var frame = new Mat())
    {
        Console.WriteLine("Camera preview started. Press ESC or q to exit.");

        while (true)
        {
            capture.Read(frame);
            if (frame.Empty())
            {
                Console.Error.WriteLine("Camera returned an empty frame.");
                break;
            }

            Cv2.ImShow("Camera Preview", frame);
            int key = Cv2.WaitKey(1);
            if (key == 27 || key == 'q' || key == 'Q')
            {
                break;
            }
        }
    }
}

Cv2.DestroyAllWindows();

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
