using System.Threading.Channels;
using Microsoft.AspNetCore.SignalR;
using RobotVision.Server.Contracts;

namespace RobotVision.Server.Api;

/// <summary>
/// HTTP ingest와 SignalR 전송을 분리하는 background consumer입니다.
/// 수집 request가 느린 client에게 직접 묶여 대기하지 않도록 bounded channel을 읽습니다.
/// </summary>
public sealed class DetectionFanoutWorker(
    Channel<DetectionEventDto> channel,
    IHubContext<MonitoringHub> hub,
    ILogger<DetectionFanoutWorker> logger) : BackgroundService
{
    /// <summary>application 종료 token이 취소될 때까지 channel event를 client group에 전송합니다.</summary>
    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        // ReadAllAsync는 새 item을 비동기로 기다리고 channel이 완료되면 loop를 종료합니다.
        await foreach (var item in channel.Reader.ReadAllAsync(stoppingToken))
        {
            try
            {
                // 상세 화면에는 해당 장치만, fleet 화면에는 전체 event를 보냅니다.
                await hub.Clients.Group(MonitoringHub.GroupName(item.DeviceId))
                    .SendAsync("detection", item, stoppingToken);
                await hub.Clients.Group("fleet").SendAsync("detection", item, stoppingToken);
            }
            catch (Exception error) when (error is not OperationCanceledException)
            {
                // 하나의 전송 실패가 worker 전체를 종료하지 않도록 기록 후 다음 item을 처리합니다.
                logger.LogError(error, "Failed to fan out event for {DeviceId}", item.DeviceId);
            }
        }
    }
}
