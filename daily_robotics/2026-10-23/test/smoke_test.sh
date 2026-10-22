#!/usr/bin/env bash
set -euo pipefail

# 다른 일일 실습의 DDS discovery와 섞이지 않도록 기본 도메인을 분리한다.
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-182}"

ros2 launch daily_robotics_2026_10_23 study.launch.py > /tmp/daily_robotics_2026_10_23.log 2>&1 &
launch_pid=$!
trap 'kill "${launch_pid}" 2>/dev/null || true; wait "${launch_pid}" 2>/dev/null || true' EXIT

# 200 Hz에서 정상/두 종류 OOD 구간과 에너지 bucket 제한을 모두 관찰할 시간을 준다.
if ! timeout 35s ros2 topic echo --once /study/pass std_msgs/msg/Bool | grep -q 'data: true'; then
  echo "Smoke test failed. Launch log:" >&2
  tail -n 180 /tmp/daily_robotics_2026_10_23.log >&2
  exit 1
fi

echo "Smoke test passed: independent covariance/safety auditor published true."
grep -E 'AUDIT PASS' /tmp/daily_robotics_2026_10_23.log | tail -n 1 || true
