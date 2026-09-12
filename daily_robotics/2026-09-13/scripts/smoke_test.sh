#!/usr/bin/env bash
set -euo pipefail

# 다른 ROS 실습/daemon graph와 섞이지 않도록 이 smoke test만의 Domain ID를 사용한다.
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-93}"

# 이 스크립트는 install setup을 source한 뒤 실행한다. timeout 종료 코드는 의도된 것이므로 직접 판정한다.
log_file="$(mktemp)"
cleanup() {
  if [[ -n "${launch_pid:-}" ]]; then
    # setsid로 만든 launch process group 전체에 신호를 보내 child node가 orphan으로 남지 않게 한다.
    kill -TERM -- "-${launch_pid}" 2>/dev/null || true
    for _ in $(seq 1 10); do
      kill -0 "${launch_pid}" 2>/dev/null || break
      sleep 0.2
    done
    kill -KILL -- "-${launch_pid}" 2>/dev/null || true
    wait "${launch_pid}" 2>/dev/null || true
  fi
  rm -f "${log_file}"
}
trap cleanup EXIT

setsid ros2 launch daily_robotics_2026_09_13 daily_demo.launch.py >"${log_file}" 2>&1 &
launch_pid=$!

for _ in $(seq 1 30); do
  if timeout 2 ros2 control list_hardware_components 2>/dev/null | grep -q "StudyArm"; then
    break
  fi
  sleep 1
done

for _ in $(seq 1 20); do
  if timeout 2 ros2 control list_controllers 2>/dev/null | grep -q "ik_controller.*active"; then
    break
  fi
  sleep 1
done

timeout 5 ros2 control list_hardware_components
timeout 5 ros2 control list_controllers
audit="$(timeout 18 ros2 topic echo /arm/audit std_msgs/msg/String --full-length 2>/dev/null || true)"
printf '%s\n' "${audit}"

if ! grep -q "label=active" < <(timeout 5 ros2 control list_hardware_components); then
  echo "FAIL: StudyArm이 active가 아닙니다" >&2
  exit 1
fi
if ! grep -q "ik_controller.*active" < <(timeout 5 ros2 control list_controllers); then
  echo "FAIL: ik_controller가 active가 아닙니다" >&2
  exit 1
fi
if ! grep -q "PASS" <<<"${audit}"; then
  echo "FAIL: auditor PASS를 관찰하지 못했습니다" >&2
  sed -n '1,240p' "${log_file}" >&2
  exit 1
fi
if ! grep -Eq 'lambda=0\.(00[6-9]|0[1-9]|1[0-9])' <<<"${audit}"; then
  echo "FAIL: 특이점 근방 adaptive damping 증가를 관찰하지 못했습니다" >&2
  exit 1
fi

echo "SMOKE_TEST_PASS"
