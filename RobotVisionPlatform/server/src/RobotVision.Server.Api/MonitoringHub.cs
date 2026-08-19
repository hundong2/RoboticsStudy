using Microsoft.AspNetCore.SignalR;

namespace RobotVision.Server.Api;

/// <summary>웹/MAUI client가 특정 장치의 실시간 event를 구독하는 SignalR endpoint입니다.</summary>
public sealed class MonitoringHub : Hub
{
    /// <summary>현재 연결을 지정 장치의 SignalR group에 추가합니다.</summary>
    public Task WatchDevice(string deviceId) =>
        Groups.AddToGroupAsync(Context.ConnectionId, GroupName(deviceId));

    /// <summary>현재 연결을 지정 장치 group에서 제거합니다.</summary>
    public Task StopWatchingDevice(string deviceId) =>
        Groups.RemoveFromGroupAsync(Context.ConnectionId, GroupName(deviceId));

    /// <summary>장치 ID를 서버 전체에서 일관된 group 이름으로 변환합니다.</summary>
    /// <exception cref="ArgumentException">deviceId가 비어 있거나 64자를 초과하면 발생합니다.</exception>
    public static string GroupName(string deviceId)
    {
        if (string.IsNullOrWhiteSpace(deviceId) || deviceId.Length > 64)
            throw new ArgumentException("deviceId must be 1–64 non-whitespace characters.", nameof(deviceId));
        return $"device:{deviceId}";
    }
}
