#!/usr/bin/env bash
set -eo pipefail

# 이 스크립트는 먼저 install/setup.bash를 source한 colcon 작업공간에서 실행한다.
# ROS_DOMAIN_ID를 외부에서 주면 다른 ROS 실습과 DDS discovery가 섞이지 않는다.
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-127}"

tmp_dir="$(mktemp -d)"
launch_log="${tmp_dir}/launch.log"
cleanup() {
  if [[ -n "${launch_pid:-}" ]]; then
    kill -TERM -- "-${launch_pid}" 2>/dev/null || true
    # ros2 launch가 SIGTERM을 받아 자식 노드를 정리할 시간을 최대 2초 준다.
    # 그래도 process group이 남으면 CI/자동화가 영원히 기다리지 않도록 종료한다.
    for _ in $(seq 1 20); do
      kill -0 "${launch_pid}" 2>/dev/null || break
      sleep 0.1
    done
    kill -KILL -- "-${launch_pid}" 2>/dev/null || true
    wait "${launch_pid}" 2>/dev/null || true
  fi
  rm -rf "${tmp_dir}"
}
trap cleanup EXIT

# 별도 process group을 만들어 실패/종료 시 자식 노드까지 확실히 정리한다.
setsid ros2 launch daily_robotics_2026_09_27 study.launch.py >"${launch_log}" 2>&1 &
launch_pid=$!

for _ in $(seq 1 45); do
  if timeout 2 ros2 topic echo --once /study/audit_pass std_msgs/msg/Bool 2>/dev/null | grep -q 'data: true'; then
    grep 'AUDIT_PASS' "${launch_log}" | tail -n 1 || true
    echo "SMOKE_PASS: 3D deskew와 바이어스/공분산 감사 통과"
    exit 0
  fi
  if ! kill -0 "${launch_pid}" 2>/dev/null; then
    cat "${launch_log}"
    echo "SMOKE_FAIL: launch가 조기 종료됨" >&2
    exit 1
  fi
  sleep 1
done

cat "${launch_log}"
echo "SMOKE_FAIL: 45초 안에 audit_pass=true를 받지 못함" >&2
exit 1
