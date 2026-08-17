namespace RobotVision.Server.Contracts;

/// <summary>원본 영상 크기와 무관한 0~1 정규화 좌표의 객체 영역입니다.</summary>
public sealed record BoundingBox(float X, float Y, float Width, float Height);

/// <summary>장치 또는 HTTP client가 서버에 보내는 객체 하나의 탐지 결과입니다.</summary>
/// <param name="Label">모델 label 목록에 정의된 class 이름입니다.</param>
/// <param name="Confidence">일반적으로 0~1 범위인 모델 신뢰도입니다.</param>
public sealed record DetectionDto(
    string Label,
    float Confidence,
    float X,
    float Y,
    float Width,
    float Height)
{
    /// <summary>개별 좌표 필드를 하나의 BoundingBox value object로 반환합니다.</summary>
    public BoundingBox Box => new(X, Y, Width, Height);
}

/// <summary>한 프레임의 탐지 결과를 수집 API로 보내는 request 계약입니다.</summary>
/// <param name="Sequence">장치별 단조 증가 번호로 중복/역순 event를 판별합니다.</param>
public sealed record DetectionEventDto(
    string DeviceId,
    ulong Sequence,
    string ModelVersion,
    IReadOnlyList<DetectionDto> Detections,
    DateTimeOffset? CapturedAt = null);

/// <summary>관제 화면이 읽는 장치별 최신 상태의 불변 snapshot입니다.</summary>
public sealed record DeviceSnapshot(
    string DeviceId,
    DateTimeOffset LastSeenAt,
    ulong LastSequence,
    string ModelVersion,
    IReadOnlyList<DetectionDto> Detections);
