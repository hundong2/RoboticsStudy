#!/usr/bin/env bash
set -euo pipefail

# 서로 다른 일일 실습이 같은 DDS discovery 공간에서 섞이지 않도록 도메인을 고정한다.
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-173}"

ros2 launch daily_robotics_2026_10_20 study.launch.py > /tmp/daily_robotics_2026_10_20.log 2>&1 &
launch_pid=$!
trap 'kill "${launch_pid}" 2>/dev/null || true; wait "${launch_pid}" 2>/dev/null || true' EXIT

# auditor가 충분한 exact-stamp 표본을 모아 true를 발행할 때까지 최대 30초만 기다린다.
if ! timeout 30s ros2 topic echo --once /study/pass std_msgs/msg/Bool | grep -q 'data: true'; then
  echo "Smoke test failed. Launch log:" >&2
  tail -n 120 /tmp/daily_robotics_2026_10_20.log >&2
  exit 1
fi

echo "Smoke test passed: independent pose-fusion auditor published true."
# 성공 판정에 사용한 실제 RMSE·지연 factor·solver 관측치를 CI 로그에도 남긴다.
grep -E 'AUDIT PASS' /tmp/daily_robotics_2026_10_20.log | tail -n 1 || true
