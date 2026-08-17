using Microsoft.AspNetCore.SignalR;

namespace RobotVision.Server.Api;

public sealed class MonitoringHub : Hub
{
    public Task WatchDevice(string deviceId) =>
        Groups.AddToGroupAsync(Context.ConnectionId, GroupName(deviceId));

    public Task StopWatchingDevice(string deviceId) =>
        Groups.RemoveFromGroupAsync(Context.ConnectionId, GroupName(deviceId));

    public static string GroupName(string deviceId) => $"device:{deviceId}";
}

