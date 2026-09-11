#!/usr/bin/env bash
set -eo pipefail

# 공식 Jazzy 설치와 오늘 빌드한 overlay를 같은 shell에 올린다.
source /opt/ros/jazzy/setup.bash
source install/2026-09-12/setup.bash
set -u

# 다른 ROS 2 실습과 discovery가 섞이지 않도록 이 검증만의 domain을 사용한다.
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-95}"
DAILY_LOG_FILE="/tmp/daily_vo_2026_09_12.log"

timeout --signal=INT --kill-after=3s 30s \
  ros2 launch daily_robotics_2026_09_12 daily_demo.launch.py >"${DAILY_LOG_FILE}" 2>&1 &
DAILY_LAUNCH_PID=$!

cleanup() {
  kill -INT "${DAILY_LAUNCH_PID}" 2>/dev/null || true
  sleep 1
  kill -TERM "${DAILY_LAUNCH_PID}" 2>/dev/null || true
  sleep 1
  kill -KILL "${DAILY_LAUNCH_PID}" 2>/dev/null || true
  wait "${DAILY_LAUNCH_PID}" 2>/dev/null || true
}
trap cleanup EXIT

# DDS discovery, 20개 이상 auditor 비교, 의도적 outlier 주입을 기다린다.
sleep 7

echo "--- LAUNCH LOG HEAD ---"
head -n 20 "${DAILY_LOG_FILE}"
echo "--- NODES ---"
ros2 node list --no-daemon --spin-time 2 | sort
echo "--- TOPICS ---"
ros2 topic list -t --no-daemon --spin-time 2 | grep -E \
  'camera/features|lidar/features|sync/metric_features|vo/odometry|vo/audit' || true
echo "--- SYNC DIAGNOSTICS ---"
timeout 6 ros2 topic echo /sync/diagnostics std_msgs/msg/String --once
echo "--- VO DIAGNOSTICS ---"
timeout 6 ros2 topic echo /vo/diagnostics std_msgs/msg/String --once
echo "--- AUDIT ---"
timeout 6 ros2 topic echo /vo/audit std_msgs/msg/String --once \
  --qos-reliability reliable --qos-durability transient_local
echo "--- LAUNCH LOG TAIL ---"
tail -n 30 "${DAILY_LOG_FILE}"
