#!/usr/bin/env bash
set -euo pipefail

# 다른 일일 실습과 DDS discovery 공간이 섞이지 않도록 전용 domain을 사용한다.
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-184}"

ros2 launch daily_robotics_2026_10_24 study.launch.py > /tmp/daily_robotics_2026_10_24.log 2>&1 &
launch_pid=$!
trap 'kill "${launch_pid}" 2>/dev/null || true; wait "${launch_pid}" 2>/dev/null || true' EXIT

# 20 Hz keyframe 120개와 100회 이상의 marginalization을 기다리되 무한정 대기하지 않는다.
if ! timeout 35s ros2 topic echo --once /study/pass std_msgs/msg/Bool | grep -q 'data: true'; then
  echo "Smoke test failed. Launch log:" >&2
  tail -n 160 /tmp/daily_robotics_2026_10_24.log >&2
  exit 1
fi

echo "Smoke test passed: independent VIO auditor published true."
grep -E 'AUDIT PASS' /tmp/daily_robotics_2026_10_24.log | tail -n 1 || true
