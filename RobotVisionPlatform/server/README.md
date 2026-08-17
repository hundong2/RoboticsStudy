# Server and Dashboard

서버는 ASP.NET Core 수집 API와 SignalR fan-out을 제공하고, 운영 UI는 웹 MVP와 선택형 MAUI Blazor Hybrid 앱으로 구성합니다.

```bash
dotnet build RobotVision.Server.slnx
dotnet run --project src/RobotVision.Server.Api
```

테스트 이벤트:

```bash
curl -X POST http://localhost:5080/api/events/detections \
  -H 'content-type: application/json' \
  -d '{"deviceId":"jetson-001","sequence":1,"modelVersion":"demo/1","detections":[{"label":"person","confidence":0.94,"x":0.1,"y":0.2,"width":0.3,"height":0.5}]}'
```

브라우저에서 `http://localhost:5080`을 열면 최신 장치 상태를 볼 수 있습니다. 데이터는 현재 in-memory이므로 production 단계에서 PostgreSQL과 object storage를 연결합니다.

MAUI 프로젝트는 기본 solution에서 제외되어 CI에 MAUI workload를 강제하지 않습니다. 설치 후 별도로 빌드합니다.

```bash
dotnet workload install maui
dotnet build src/RobotVision.Dashboard.Maui/RobotVision.Dashboard.Maui.csproj -f net10.0-windows10.0.19041.0
```

