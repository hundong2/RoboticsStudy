using System.Threading.Channels;
using Microsoft.AspNetCore.SignalR;
using RobotVision.Server.Contracts;

namespace RobotVision.Server.Api;

public sealed class DetectionFanoutWorker(
    Channel<DetectionEventDto> channel,
    IHubContext<MonitoringHub> hub,
    ILogger<DetectionFanoutWorker> logger) : BackgroundService
{
    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        await foreach (var item in channel.Reader.ReadAllAsync(stoppingToken))
        {
            try
            {
                await hub.Clients.Group(MonitoringHub.GroupName(item.DeviceId))
                    .SendAsync("detection", item, stoppingToken);
                await hub.Clients.Group("fleet").SendAsync("detection", item, stoppingToken);
            }
            catch (Exception error) when (error is not OperationCanceledException)
            {
                logger.LogError(error, "Failed to fan out event for {DeviceId}", item.DeviceId);
            }
        }
    }
}

