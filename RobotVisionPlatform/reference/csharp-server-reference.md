# C# 서버 코드 사전

## `record`

값 중심의 불변 데이터 계약에 사용합니다.

```csharp
public sealed record BoundingBox(float X, float Y, float Width, float Height);
```

- 같은 필드 값을 가진 record끼리 값 비교가 가능합니다.
- request/response DTO와 snapshot에 적합합니다.
- `sealed`는 상속을 막아 계약을 단순하게 유지합니다.
- 위치: [`DeviceContracts.cs`](../server/src/RobotVision.Server.Contracts/DeviceContracts.cs)

## Dependency Injection과 생성자 주입

ASP.NET Core가 필요한 객체를 constructor parameter로 제공합니다.

```csharp
public sealed class Worker(
    Channel<DetectionEventDto> channel,
    ILogger<Worker> logger) : BackgroundService
```

`Program.cs`의 `AddSingleton`, `AddHostedService` 등록을 보고 객체를 생성합니다. 코드에서 직접 `new Worker(...)`할 필요가 없습니다.

## `AddSingleton<T>()`

application lifetime 동안 객체 하나를 공유합니다.

```csharp
builder.Services.AddSingleton<DeviceRegistry>();
```

장치 최신 상태처럼 모든 request가 같은 데이터를 봐야 할 때 적합합니다. request별 상태에는 scoped lifetime을 사용해야 합니다.

## `ConcurrentDictionary<TKey,TValue>`

동시에 여러 request가 읽고 쓸 수 있는 thread-safe dictionary입니다.

```csharp
private readonly ConcurrentDictionary<string, DeviceSnapshot> _devices = new();
```

여러 작업을 묶은 복합 불변식까지 자동으로 보호하는 것은 아닙니다. 이 프로젝트는 `AddOrUpdate` 한 번 안에서 최신 sequence를 선택합니다.

## `AddOrUpdate`

key가 없으면 추가하고, 있으면 현재 값을 받아 새 값을 계산합니다.

```csharp
devices.AddOrUpdate(id, newValue, (_, current) =>
    sequence >= current.LastSequence ? newValue : current);
```

늦게 도착한 낮은 sequence event가 최신 상태를 덮어쓰지 못하게 합니다.

## `TryGetValue` / `TryGet`

exception 없이 값 존재 여부와 값을 함께 반환하는 패턴입니다.

```csharp
if (registry.TryGet(deviceId, out var snapshot))
{
    // snapshot 사용
}
```

`out` parameter는 함수가 호출자 변수에 값을 채워 줍니다.

## `Channel<T>`

비동기 producer-consumer queue입니다.

```csharp
var channel = Channel.CreateBounded<Event>(1024);
channel.Writer.TryWrite(item);
await foreach (var item in channel.Reader.ReadAllAsync(token)) { }
```

- HTTP endpoint가 producer, `DetectionFanoutWorker`가 consumer입니다.
- bounded channel은 느린 UI 때문에 memory가 계속 늘어나는 것을 막습니다.
- `DropOldest`는 관제 화면에서 최신 상태를 우선하는 정책입니다.

## `BackgroundService`

ASP.NET Core application과 함께 시작·종료되는 장기 worker base class입니다.

```csharp
protected override async Task ExecuteAsync(CancellationToken stoppingToken)
```

`stoppingToken`을 모든 비동기 wait/전송에 전달해야 application이 빠르게 종료됩니다.

## `async`, `await`, `Task`

I/O가 끝날 때까지 thread를 막지 않고 나중에 계속 실행합니다.

```csharp
await hub.Clients.Group(group).SendAsync("detection", item, token);
```

`await`를 생략한 fire-and-forget 작업은 exception과 lifetime을 잃기 쉬우므로 background worker에서는 피합니다.

## `CancellationToken`

C++ `stop_token`과 비슷한 협력적 취소 신호입니다.

```csharp
await operation(stoppingToken);
```

취소는 실패와 구분해야 하므로 worker는 `OperationCanceledException`을 일반 오류로 기록하지 않습니다.

## Minimal API `MapGet` / `MapPost`

route와 handler를 한 곳에 선언합니다.

```csharp
app.MapGet("/api/devices", (DeviceRegistry registry) => Results.Ok(registry.List()));
app.MapPost("/api/events", (EventDto item) => Results.Accepted());
```

parameter는 route, body 또는 DI에서 자동 binding됩니다. public API가 커지면 endpoint group과 validator로 분리합니다.

## `Results.Ok`, `Accepted`, `BadRequest`, `NotFound`

| 함수 | HTTP | 의미 |
|---|---:|---|
| `Results.Ok(value)` | 200 | 조회 성공 |
| `Results.Accepted(location)` | 202 | 수집했지만 후속 처리는 비동기 |
| `Results.BadRequest(value)` | 400 | client 입력 오류 |
| `Results.NotFound()` | 404 | 장치 없음 |

## SignalR `Hub`와 group

SignalR Hub는 서버가 연결된 client의 함수를 호출할 수 있게 합니다.

```csharp
await Groups.AddToGroupAsync(Context.ConnectionId, "device:jetson-001");
await hub.Clients.Group("device:jetson-001").SendAsync("detection", item);
```

group을 사용하면 모든 client가 아닌 특정 장치 화면에만 event를 보낼 수 있습니다.

## `ILogger<T>` 구조화 logging

```csharp
logger.LogError(error, "Failed for {DeviceId}", deviceId);
```

문자열 보간보다 `{DeviceId}` template을 사용하면 log backend가 필드를 검색 가능한 값으로 보존합니다.

## MAUI `MauiApp.CreateBuilder()`와 `BlazorWebView`

`MauiApp.CreateBuilder()`는 native application, DI, logging을 구성합니다. `AddMauiBlazorWebView()`는 Razor component를 native WebView 안에서 실행할 service를 등록합니다. 현재 앱은 골격이며 SignalR/WebRTC 연결은 다음 단계입니다.
