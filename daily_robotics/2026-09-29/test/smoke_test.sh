#!/usr/bin/env bash
set -eo pipefail

# install/setup.bash가 source된 colcon workspace에서 실행한다.
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-140}"

tmp_dir="$(mktemp -d)"
launch_log="${tmp_dir}/launch.log"
cleanup() {
  if [[ -n "${launch_pid:-}" ]]; then
    kill -TERM -- "-${launch_pid}" 2>/dev/null || true
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

# setsid로 process group을 분리해 종료 시 launch와 자식 노드를 함께 정리한다.
setsid ros2 launch daily_robotics_2026_09_29 study.launch.py >"${launch_log}" 2>&1 &
launch_pid=$!

for _ in $(seq 1 45); do
  if timeout 2 ros2 topic echo --once /study/audit_pass std_msgs/msg/Bool 2>/dev/null | grep -q 'data: true'; then
    grep 'AUDIT_PASS' "${launch_log}" | tail -n 1 || true
    echo "SMOKE_PASS: fixed-lag robust factor graph 독립 감사 통과"
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
