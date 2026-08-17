using System.Threading.Channels;
using Microsoft.AspNetCore.Http.Json;
using RobotVision.Server.Api;
using RobotVision.Server.Contracts;

var builder = WebApplication.CreateBuilder(args);
builder.Services.Configure<JsonOptions>(options =>
    options.SerializerOptions.PropertyNamingPolicy = System.Text.Json.JsonNamingPolicy.CamelCase);
builder.Services.AddSignalR();
builder.Services.AddSingleton<DeviceRegistry>();
builder.Services.AddSingleton(Channel.CreateBounded<DetectionEventDto>(new BoundedChannelOptions(1024)
{
    FullMode = BoundedChannelFullMode.DropOldest,
    SingleReader = true,
    SingleWriter = false
}));
builder.Services.AddHostedService<DetectionFanoutWorker>();
builder.Services.AddHealthChecks();

var app = builder.Build();
app.UseDefaultFiles();
app.UseStaticFiles();
app.MapHealthChecks("/health");
app.MapHub<MonitoringHub>("/hubs/monitoring");

app.MapGet("/api/devices", (DeviceRegistry registry) => Results.Ok(registry.List()));
app.MapGet("/api/devices/{deviceId}", (string deviceId, DeviceRegistry registry) =>
    registry.TryGet(deviceId, out var device) ? Results.Ok(device) : Results.NotFound());

app.MapPost("/api/events/detections", async (
    DetectionEventDto item,
    DeviceRegistry registry,
    Channel<DetectionEventDto> channel,
    CancellationToken cancellationToken) =>
{
    if (string.IsNullOrWhiteSpace(item.DeviceId) || item.Detections.Count > 1_000)
        return Results.BadRequest(new { error = "deviceId is required; max 1000 detections" });

    registry.Upsert(item);
    if (!channel.Writer.TryWrite(item))
        return Results.StatusCode(StatusCodes.Status503ServiceUnavailable);
    await Task.CompletedTask;
    return Results.Accepted($"/api/devices/{Uri.EscapeDataString(item.DeviceId)}");
});

app.Run();

public partial class Program;

