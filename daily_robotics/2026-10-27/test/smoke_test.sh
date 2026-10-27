#!/usr/bin/env bash
set -euo pipefail

# 같은 호스트의 다른 ROS 2 실습과 DDS discovery가 섞이지 않게 전용 domain을 쓴다.
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-198}"

ros2 launch daily_robotics_2026_10_27 study.launch.py > /tmp/daily_robotics_2026_10_27.log 2>&1 &
launch_pid=$!
trap 'kill "${launch_pid}" 2>/dev/null || true; wait "${launch_pid}" 2>/dev/null || true' EXIT

# 80개 exact-stamp sample을 기다리되 장애 시 무한 대기하지 않는다.
if ! timeout 35s ros2 topic echo --once /study/pass std_msgs/msg/Bool | grep -q 'data: true'; then
  echo "Smoke test failed. Launch log:" >&2
  tail -n 180 /tmp/daily_robotics_2026_10_27.log >&2
  exit 1
fi

echo "Smoke test passed: independent event-flow auditor published true."
grep -E 'AUDIT PASS' /tmp/daily_robotics_2026_10_27.log | tail -n 1 || true
