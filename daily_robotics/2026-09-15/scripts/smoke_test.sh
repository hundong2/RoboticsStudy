#!/usr/bin/env bash
set -eo pipefail

# 공식 Jazzy 설치와 오늘 빌드한 overlay를 같은 shell에 올린다.
source /opt/ros/jazzy/setup.bash
source install/2026-09-15/setup.bash
set -u

# 다른 ROS 2 실습과 discovery가 섞이지 않도록 검증 전용 domain을 사용한다.
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-98}"
DAILY_LOG_FILE="/tmp/daily_rrt_2026_09_15.log"

timeout --signal=INT --kill-after=3s 40s \
  ros2 launch daily_robotics_2026_09_15 daily_demo.launch.py >"${DAILY_LOG_FILE}" 2>&1 &
DAILY_LAUNCH_PID=$!

cleanup() {
  kill -INT "${DAILY_LAUNCH_PID}" 2>/dev/null || true
  sleep 1
  kill -TERM "${DAILY_LAUNCH_PID}" 2>/dev/null || true
  wait "${DAILY_LAUNCH_PID}" 2>/dev/null || true
}
trap cleanup EXIT

# DDS discovery, 경로 생성, 100 Hz servo 추종, auditor의 판정을 기다린다.
sleep 12

echo "--- NODES ---"
ros2 node list --no-daemon --spin-time 2 | sort
echo "--- TOPICS ---"
ros2 topic list -t --no-daemon --spin-time 2 | grep -E \
  'planning/joint_space_world|planning/joint_trajectory|servo/joint_command|planning/rrt_diagnostics|servo/diagnostics|planning/audit'
echo "--- RRT ---"
timeout 6 ros2 topic echo /planning/rrt_diagnostics std_msgs/msg/String --once \
  --qos-reliability reliable --qos-durability transient_local
echo "--- SERVO ---"
timeout 6 ros2 topic echo /servo/diagnostics std_msgs/msg/String --once \
  --qos-reliability reliable --qos-durability transient_local
echo "--- AUDIT ---"
AUDIT_OUTPUT="$(timeout 6 ros2 topic echo /planning/audit std_msgs/msg/String --once \
  --qos-reliability reliable --qos-durability transient_local)"
echo "${AUDIT_OUTPUT}"
grep -q "PASS" <<<"${AUDIT_OUTPUT}"
echo "--- LAUNCH LOG TAIL ---"
tail -n 35 "${DAILY_LOG_FILE}"
