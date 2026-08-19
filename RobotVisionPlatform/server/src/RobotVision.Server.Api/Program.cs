using System.Threading.Channels;
using Microsoft.AspNetCore.Http.Json;
using RobotVision.Server.Api;
using RobotVision.Server.Contracts;

var builder = WebApplication.CreateBuilder(args);
// JSON request/response를 C# PascalCase 대신 JavaScript 친화적인 camelCase로 통일합니다.
builder.Services.Configure<JsonOptions>(options =>
    options.SerializerOptions.PropertyNamingPolicy = System.Text.Json.JsonNamingPolicy.CamelCase);
builder.Services.AddSignalR();
// Registry와 channel은 application 전체에서 하나만 존재해야 하므로 singleton입니다.
builder.Services.AddSingleton<DeviceRegistry>();
builder.Services.AddSingleton(Channel.CreateBounded<DetectionEventDto>(new BoundedChannelOptions(1024)
{
    // 순간 burst가 1024개를 넘으면 오래된 UI 알림을 버리고 최신 상태를 우선합니다.
    FullMode = BoundedChannelFullMode.DropOldest,
    SingleReader = true,
    SingleWriter = false
}));
builder.Services.AddHostedService<DetectionFanoutWorker>();
builder.Services.AddHealthChecks();

var app = builder.Build();
// wwwroot/index.html을 기본 관제 페이지로 제공합니다.
app.UseDefaultFiles();
app.UseStaticFiles();
app.MapHealthChecks("/health");
app.MapHub<MonitoringHub>("/hubs/monitoring");

// 현재 장치 목록 또는 장치 하나의 최신 snapshot을 조회하는 read API입니다.
app.MapGet("/api/devices", (DeviceRegistry registry) => Results.Ok(registry.List()));
app.MapGet("/api/devices/{deviceId}", (string deviceId, DeviceRegistry registry) =>
    registry.TryGet(deviceId, out var device) ? Results.Ok(device) : Results.NotFound());

app.MapPost("/api/events/detections", async (
    DetectionEventDto item,
    DeviceRegistry registry,
    Channel<DetectionEventDto> channel,
    CancellationToken cancellationToken) =>
{
    // 잘못된 ID와 비정상적으로 큰 payload를 background queue에 넣기 전에 거부합니다.
    if (string.IsNullOrWhiteSpace(item.DeviceId) || item.Detections is null || item.Detections.Count > 1_000)
        return Results.BadRequest(new { error = "deviceId is required; max 1000 detections" });

    // 조회용 최신 상태를 먼저 갱신하고, 실시간 UI fan-out은 bounded channel에 위임합니다.
    registry.Upsert(item);
    if (!channel.Writer.TryWrite(item))
        return Results.StatusCode(StatusCodes.Status503ServiceUnavailable);
    await Task.CompletedTask;
    return Results.Accepted($"/api/devices/{Uri.EscapeDataString(item.DeviceId)}");
});

app.Run();

// Exposing the generated top-level Program type lets integration-test projects reference the host.
public partial class Program;
