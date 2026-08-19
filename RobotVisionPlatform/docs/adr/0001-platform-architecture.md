# ADR 0001: Edge C++20 + ASP.NET Core + MAUI Hybrid

- 상태: Accepted
- 날짜: 2026-08-17

## 결정

Jetson 실행부는 C++20과 NVIDIA 가속 스택을 사용합니다. 서버는 ASP.NET Core, 운영 UI는 웹과 네이티브 배포가 모두 가능한 MAUI Blazor Hybrid/shared Razor components를 사용합니다. gRPC는 control plane, WebRTC는 media plane, SignalR는 UI fan-out에 사용합니다.

## 결과

GPU zero-copy와 하드웨어 codec을 활용하면서 서버 생산성을 유지할 수 있습니다. 반면 프로토콜과 미디어 세션이 분리되므로 연결 수명주기와 correlation을 명시적으로 관리해야 합니다. MAUI는 서버 역할을 맡지 않고 관제 클라이언트로 한정합니다.

