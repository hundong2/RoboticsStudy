# SROS2와 DDS Security enclave

## 무엇을 보호하는가

SROS2는 ROS graph 위에서 DDS Security의 인증, 접근제어, 암호화 artifact를 다루는 도구 계층이다. 보안 실행 주체는 node name과 동일하지 않은 **enclave**이며, 한 process 또는 의도적으로 묶은 process들이 identity와 permissions를 공유한다.

핵심 환경 변수는 다음 세 가지다.

- `ROS_SECURITY_KEYSTORE`: CA와 enclave artifact를 찾는 루트
- `ROS_SECURITY_ENABLE=true`: 보안 초기화 활성화
- `ROS_SECURITY_STRATEGY=Enforce`: artifact가 없거나 틀리면 실행을 거부하는 fail-closed 모드

실행 시 `--ros-args --enclave /logical/name`으로 process의 enclave를 고른다.

## Keystore 신뢰 구조

Keystore는 public CA material, private CA key, enclave별 identity/permissions를 분리한다. Runtime enclave에는 개인키, 인증서, 서명된 permissions/governance와 CA 인증서가 필요하다. CA 개인키는 새 identity/권한을 만들 수 있으므로 production robot에 상주시켜서는 안 된다.

## 최소 권한 설계 순서

1. Node가 아니라 **process/enclave 배포 단위**를 먼저 정한다.
2. 각 enclave의 publish/subscribe/service/action 계약을 표로 쓴다.
3. 완전한 graph를 insecure lab 환경에서 실행하고 `generate_policy`로 보조 interface까지 관찰한다.
4. 생성 정책을 최소 권한으로 검토하고 서명 artifact를 만든다.
5. `Enforce` 모드에서 허용 경로와 금지 경로를 모두 시험한다.
6. CA offline 보관, 인증서 만료, rotation/revocation 절차를 운영에 넣는다.

## 흔한 오해

- Topic 이름을 난독화하거나 namespace를 나누는 것은 접근제어가 아니다.
- 암호화만으로 publisher의 신원을 검증하거나 topic 권한을 제한할 수 있는 것은 아니다.
- 같은 process에 composition된 여러 node는 DDS participant/enclave 경계가 합쳐질 수 있다.
- Policy XML만 커밋한다고 보안이 켜지지 않는다. 서명 artifact, RMW 지원, 환경 변수, enclave 인자가 함께 맞아야 한다.

## 참고

- [ROS 2 Jazzy — Understanding the security keystore](https://docs.ros.org/en/jazzy/Tutorials/Advanced/Security/The-Keystore.html)
- [ROS 2 Jazzy — RMW security options](https://docs.ros.org/en/jazzy/Tutorials/Advanced/Creating-An-RMW-Implementation.html#security)
