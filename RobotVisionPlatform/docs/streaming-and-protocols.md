# 통신과 영상 전송

## 권장 조합

| 데이터 | 기본 | 대안 | 이유 |
|---|---|---|---|
| live video | WebRTC + H.264 | SRT/RTSP | 브라우저/MAUI 저지연 재생, congestion control |
| detection/health | gRPC bidirectional stream | MQTT | typed contract, 장치 명령을 같은 세션에서 전달 |
| dashboard update | SignalR | SSE | .NET 클라이언트와 웹 fan-out |
| clip upload | HTTPS object upload | gRPC chunk | 재시도와 대용량 분리 |

protobuf 자체가 작은 수치 이벤트에 효율적이므로 이미 압축된 JPEG/H.264를 gzip으로 다시 압축하지 않습니다. gRPC message compression은 큰 반복 텍스트에만 측정 후 적용합니다. 영상은 NVENC에서 H.264 low-latency preset을 사용하고, 해상도/FPS/bitrate를 네트워크 상태에 맞춰 조절합니다.

`shared/proto/vision/v1/device.proto`의 `Connect`는 장치가 시작하는 장기 bidi stream입니다. heartbeat, detection batch, model state를 보내고 서버는 config/model/action 명령을 돌려줍니다. 메시지에 `device_id`, `sequence`, UTC timestamp, schema version을 두어 재연결과 중복을 처리합니다.

WebRTC signaling은 인증된 HTTPS/SignalR endpoint로 추가하고, NAT 환경에서는 STUN/TURN을 둡니다. LAN 전용 MVP라도 TLS와 장치 신원은 생략하지 않습니다.

## 공식 참고

- [NVIDIA Jetson WebRTC hardware acceleration](https://docs.nvidia.com/jetson/archives/r36.3/DeveloperGuide/SD/HardwareAccelerationInTheWebrtcFramework.html)
- [gRPC C++ basics](https://grpc.io/docs/languages/cpp/basics/)
- [ASP.NET Core gRPC services](https://learn.microsoft.com/aspnet/core/grpc/services?view=aspnetcore-10.0)
- [ASP.NET Core SignalR](https://learn.microsoft.com/aspnet/core/signalr/introduction)

