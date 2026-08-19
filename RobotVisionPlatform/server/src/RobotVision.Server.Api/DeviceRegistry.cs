using System.Collections.Concurrent;
using RobotVision.Server.Contracts;

namespace RobotVision.Server.Api;

/// <summary>
/// 장치별 가장 최신 detection snapshot을 thread-safe하게 보관하는 MVP in-memory registry입니다.
/// 서버 재시작 후에도 보존하려면 이 클래스를 database repository로 교체합니다.
/// </summary>
public sealed class DeviceRegistry
{
    // 여러 HTTP request가 동시에 접근하므로 일반 Dictionary 대신 ConcurrentDictionary를 사용합니다.
    private readonly ConcurrentDictionary<string, DeviceSnapshot> _devices = new(StringComparer.Ordinal);

    /// <summary>새 event가 기존 sequence 이상일 때만 장치의 최신 상태를 갱신합니다.</summary>
    /// <param name="item">수집 API가 검증한 detection event입니다.</param>
    public void Upsert(DetectionEventDto item)
    {
        var snapshot = new DeviceSnapshot(
            item.DeviceId,
            DateTimeOffset.UtcNow,
            item.Sequence,
            item.ModelVersion,
            item.Detections);
        // 늦게 도착한 오래된 event가 최신 화면을 되돌리지 않게 sequence를 비교합니다.
        _devices.AddOrUpdate(item.DeviceId, snapshot, (_, current) =>
            item.Sequence >= current.LastSequence ? snapshot : current);
    }

    /// <summary>장치 ID로 정렬된 현재 snapshot 복사본을 반환합니다.</summary>
    public IReadOnlyCollection<DeviceSnapshot> List() =>
        _devices.Values.OrderBy(x => x.DeviceId, StringComparer.Ordinal).ToArray();

    /// <summary>장치 ID에 해당하는 최신 snapshot을 찾아 반환합니다.</summary>
    public bool TryGet(string deviceId, out DeviceSnapshot? snapshot) =>
        _devices.TryGetValue(deviceId, out snapshot);
}
