# SROS2 enclave 최소 권한 계약

이 문서는 소스코드가 기대하는 **논리적 권한 경계**다. 실제 `permissions.p7s`, 인증서, 개인키는 `sros2`로 배포 환경에서 생성하며 저장소에 커밋하지 않는다.

| Enclave | 허용 publish | 허용 subscribe | 책임 |
|---|---|---|---|
| `/daily_slam/simulator` | `/slam/raw_path` | 없음 | 센서/odometry 입력을 모사 |
| `/daily_slam/optimizer` | `/slam/optimized_path`, `/slam/optimizer_stats` | `/slam/raw_path` | graph 최적화만 수행 |
| `/daily_slam/auditor` | `/slam/audit` | `/slam/raw_path`, `/slam/optimized_path` | 독립 회귀 판정 |

ROS 2 parameter, logging, graph discovery용 숨은 topic/service도 실제 process가 만든다. 따라서 표만 손으로 DDS permissions XML로 옮기지 말고, 데모 graph를 완전히 실행한 뒤 `ros2 security generate_policy`로 관찰한 정책에서 시작하여 최소 권한으로 검토한다.

## 제품화 체크

- CA 개인키는 로봇에서 제거하고 오프라인 서명 시스템에 보관한다.
- `ROS_SECURITY_STRATEGY=Enforce`로 artifact 누락/서명 실패를 fail-closed 처리한다.
- simulator identity가 optimizer identity와 다른지 인증서 subject와 enclave directory로 확인한다.
- auditor에는 입력 topic의 publish 권한을 주지 않는다.
- key rotation, 인증서 만료, 탈취 identity 폐기 절차를 배포 파이프라인에 포함한다.
- DDS vendor와 domain별로 discovery/parameter/service 보조 권한까지 통합 시험한다.
