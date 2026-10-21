#!/usr/bin/env bash
set -euo pipefail

# 다른 일일 패키지의 DDS 트래픽과 섞이지 않도록 독립 도메인을 쓴다.
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-177}"

ros2 launch daily_robotics_2026_10_22 study.launch.py > /tmp/daily_robotics_2026_10_22.log 2>&1 &
launch_pid=$!
trap 'kill "${launch_pid}" 2>/dev/null || true; wait "${launch_pid}" 2>/dev/null || true' EXIT

# 세 번의 서로 다른 sequence를 독립 auditor가 통과할 때까지 최대 30초 기다린다.
if ! timeout 30s ros2 topic echo --once /study/pass std_msgs/msg/Bool | grep -q 'data: true'; then
  echo "Smoke test failed. Launch log:" >&2
  tail -n 160 /tmp/daily_robotics_2026_10_22.log >&2
  exit 1
fi

echo "Smoke test passed: robust pose-graph auditor published true."
grep -E 'AUDIT PASS' /tmp/daily_robotics_2026_10_22.log | tail -n 1 || true
