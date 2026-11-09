#!/usr/bin/env bash
set -euo pipefail

# 같은 호스트의 다른 학습 노드와 DDS discovery가 섞이지 않도록 독립 domain을 쓴다.
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-210}"

ros2 launch daily_robotics_2026_11_10 study.launch.py > /tmp/daily_robotics_2026_11_10.log 2>&1 &
launch_pid=$!
trap 'kill "${launch_pid}" 2>/dev/null || true; wait "${launch_pid}" 2>/dev/null || true' EXIT

# auditor가 60개 exact-stamp sample을 모을 때까지 기다리되 CI가 무한 대기하지 않게 한다.
if ! timeout 45s ros2 topic echo --once /study/pass std_msgs/msg/Bool | grep -q 'data: true'; then
  echo "Smoke test failed. Launch log:" >&2
  tail -n 260 /tmp/daily_robotics_2026_11_10.log >&2
  exit 1
fi

echo "Smoke test passed: independent NDT auditor published true."
grep -E 'AUDIT PASS' /tmp/daily_robotics_2026_11_10.log | tail -n 1 || true
