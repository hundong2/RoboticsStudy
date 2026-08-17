namespace RobotVision.Server.Contracts;

public sealed record BoundingBox(float X, float Y, float Width, float Height);

public sealed record DetectionDto(
    string Label,
    float Confidence,
    float X,
    float Y,
    float Width,
    float Height)
{
    public BoundingBox Box => new(X, Y, Width, Height);
}

public sealed record DetectionEventDto(
    string DeviceId,
    ulong Sequence,
    string ModelVersion,
    IReadOnlyList<DetectionDto> Detections,
    DateTimeOffset? CapturedAt = null);

public sealed record DeviceSnapshot(
    string DeviceId,
    DateTimeOffset LastSeenAt,
    ulong LastSequence,
    string ModelVersion,
    IReadOnlyList<DetectionDto> Detections);

