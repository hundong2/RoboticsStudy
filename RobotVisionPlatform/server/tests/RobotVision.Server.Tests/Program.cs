using RobotVision.Server.Api;
using RobotVision.Server.Contracts;

var registry = new DeviceRegistry();
// 최신 sequence 2를 먼저 넣고 오래된 sequence 1이 상태를 덮어쓰지 못하는지 확인합니다.
registry.Upsert(new DetectionEventDto("test-device", 2, "model/1", []));
registry.Upsert(new DetectionEventDto("test-device", 1, "stale", []));
if (!registry.TryGet("test-device", out var item) || item?.LastSequence != 2)
    throw new InvalidOperationException("Registry must reject out-of-order snapshots.");
Console.WriteLine("Server core tests passed.");
