# 00. 환경 준비

목표는 ROS 2를 설치하기 전에 학습자가 운영체제, 셸, 패키지 관리자, Python/C++ 빌드 도구의 역할을 이해하는 것입니다.

## 권장 환경

| 항목 | 권장값 | 이유 |
|---|---|---|
| OS | Ubuntu 26.04 또는 24.04 | Lyrical은 Ubuntu 26.04, Jazzy/Kilted는 Ubuntu 24.04 중심 |
| ROS | Lyrical Luth LTS | 2026년 기준 최신 LTS |
| 대안 | Jazzy Jalisco LTS | Ubuntu 24.04 기반 장기 지원 학습에 안정적 |
| 에디터 | VS Code + Remote WSL | Windows 사용자가 Linux ROS 환경을 다루기 쉬움 |
| 빌드 | colcon, ament | ROS 2 표준 빌드 흐름 |
| 언어 | Python, C++ | rclpy는 빠른 학습, rclcpp는 성능/제품화에 중요 |

## 셸 기본기

```bash
pwd
ls
cd ~/ros2_ws
mkdir -p src
```

- `pwd`는 현재 폴더를 출력합니다.
- `ls`는 현재 폴더 안의 파일과 폴더를 보여줍니다.
- `cd`는 작업 폴더를 이동합니다.
- `mkdir -p`는 중간 폴더가 없어도 한 번에 만듭니다.

## ROS 2 설치 후 반드시 확인할 것

```bash
source /opt/ros/lyrical/setup.bash
ros2 --help
ros2 doctor --report
```

- `source`는 현재 터미널 세션에 ROS 환경 변수를 적용합니다.
- `/opt/ros/<distro>/setup.bash`는 설치된 ROS 배포판의 명령어와 패키지 경로를 등록합니다.
- `ros2 doctor --report`는 설치 상태, 네트워크, 미들웨어 설정을 점검합니다.

## 워크스페이스 기본 구조

```text
ros2_ws/
  src/
    my_package/
  build/
  install/
  log/
```

- `src`에는 직접 작성하거나 가져온 ROS 패키지를 넣습니다.
- `build`는 컴파일 중간 산출물입니다.
- `install`은 실행 가능한 결과물이 모이는 곳입니다.
- `log`는 빌드 로그와 오류 추적 자료입니다.

## 학습 체크리스트

- [ ] `source`와 실행 파일 실행의 차이를 설명할 수 있다.
- [ ] `ROS_DISTRO` 환경 변수를 확인할 수 있다.
- [ ] `colcon build`가 `src` 아래 패키지를 찾아 빌드한다는 점을 설명할 수 있다.
- [ ] Python 패키지와 C++ 패키지가 모두 ament/colcon 흐름에 올라간다는 점을 이해했다.

## 실습

1. 새 터미널에서 `echo $ROS_DISTRO`를 실행합니다.
2. 값이 비어 있으면 `source /opt/ros/lyrical/setup.bash`를 실행합니다.
3. 다시 `echo $ROS_DISTRO`를 실행해 값이 바뀌었는지 확인합니다.
4. 왜 새 터미널마다 `source`가 필요한지 한 문장으로 적습니다.
