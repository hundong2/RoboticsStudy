#!/usr/bin/env bash
set -euo pipefail

# 다른 일일 실습의 DDS discovery와 섞이지 않도록 기본 도메인을 분리한다.
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-175}"

ros2 launch daily_robotics_2026_10_21 study.launch.py > /tmp/daily_robotics_2026_10_21.log 2>&1 &
launch_pid=$!
trap 'kill "${launch_pid}" 2>/dev/null || true; wait "${launch_pid}" 2>/dev/null || true' EXIT

# 200 Hz에서 600개 exact-stamp 표본과 여러 접촉 전이를 모을 시간을 포함한다.
if ! timeout 30s ros2 topic echo --once /study/pass std_msgs/msg/Bool | grep -q 'data: true'; then
  echo "Smoke test failed. Launch log:" >&2
  tail -n 140 /tmp/daily_robotics_2026_10_21.log >&2
  exit 1
fi

echo "Smoke test passed: independent contact-estimation auditor published true."
grep -E 'AUDIT PASS' /tmp/daily_robotics_2026_10_21.log | tail -n 1 || true
