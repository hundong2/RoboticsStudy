using System.Collections.Concurrent;
using RobotVision.Server.Contracts;

namespace RobotVision.Server.Api;

public sealed class DeviceRegistry
{
    private readonly ConcurrentDictionary<string, DeviceSnapshot> _devices = new(StringComparer.Ordinal);

    public void Upsert(DetectionEventDto item)
    {
        var snapshot = new DeviceSnapshot(
            item.DeviceId,
            DateTimeOffset.UtcNow,
            item.Sequence,
            item.ModelVersion,
            item.Detections);
        _devices.AddOrUpdate(item.DeviceId, snapshot, (_, current) =>
            item.Sequence >= current.LastSequence ? snapshot : current);
    }

    public IReadOnlyCollection<DeviceSnapshot> List() =>
        _devices.Values.OrderBy(x => x.DeviceId, StringComparer.Ordinal).ToArray();

    public bool TryGet(string deviceId, out DeviceSnapshot? snapshot) =>
        _devices.TryGetValue(deviceId, out snapshot);
}

