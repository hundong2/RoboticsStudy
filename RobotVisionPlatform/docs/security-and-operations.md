# 보안 및 운영

- 장치별 인증서와 mTLS를 사용하고 bootstrap token은 1회용으로 제한합니다.
- private key와 Wi-Fi 비밀번호는 이미지·Git·평문 TOML에 넣지 않습니다.
- 모델/설정 bundle은 서명과 SHA-256을 확인한 뒤 staging 경로에 내려받고 atomic switch합니다.
- 명령은 allow-list, TTL, idempotency key, issuer, audit event를 요구합니다.
- 카메라 영상은 최소 수집·최소 보존·접근 감사 원칙을 적용하고 얼굴/번호판 정책을 별도로 정합니다.
- root가 아닌 systemd 사용자, read-only root filesystem, 필요한 device/capability만 허용합니다.

관측 항목은 capture/inference/publish FPS, end-to-end latency p50/p95/p99, queue drops, reconnect count, temperature, power mode, disk, active model hash입니다. 로그·metric·trace에는 같은 `device_id`와 `correlation_id`를 넣습니다.

장애 시 장치는 지수 backoff+jitter로 재접속하고 최근 이벤트를 용량 제한 spool에 보관합니다. 디스크가 차면 원본 프레임보다 오래된 비중요 이벤트를 먼저 제거합니다. 서버 장애가 장치 로컬 동작을 중단시키면 안 됩니다.

