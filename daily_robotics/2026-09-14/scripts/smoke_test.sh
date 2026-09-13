#!/usr/bin/env bash
set -eo pipefail

# 공식 Jazzy 설치와 오늘 빌드한 overlay를 같은 shell에 올린다.
source /opt/ros/jazzy/setup.bash
source install/2026-09-14/setup.bash
set -u

# 다른 ROS 2 실습과 discovery가 섞이지 않도록 이 검증 전용 domain을 사용한다.
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-97}"
DAILY_LOG_FILE="/tmp/daily_navigation_2026_09_14.log"

timeout --signal=INT --kill-after=3s 45s \
  ros2 launch daily_robotics_2026_09_14 daily_demo.launch.py >"${DAILY_LOG_FILE}" 2>&1 &
DAILY_LAUNCH_PID=$!

cleanup() {
  kill -INT "${DAILY_LAUNCH_PID}" 2>/dev/null || true
  sleep 1
  kill -TERM "${DAILY_LAUNCH_PID}" 2>/dev/null || true
  wait "${DAILY_LAUNCH_PID}" 2>/dev/null || true
}
trap cleanup EXIT

# DDS discovery와 4초 시점 heartbeat 단절, 0.5초 뒤 복구, auditor의 8초 판정을 기다린다.
sleep 10

echo "--- NODES ---"
ros2 node list --no-daemon --spin-time 2 | sort
echo "--- TOPICS ---"
ros2 topic list -t --no-daemon --spin-time 2 | grep -E \
  'planning/global_path|perception/obstacles|planning/local_trajectory|nav/bt_status|safety/deadline_status|nav/audit'
echo "--- OPTIMIZER ---"
timeout 6 ros2 topic echo /planning/optimizer_diagnostics std_msgs/msg/String --once \
  --qos-reliability reliable --qos-durability transient_local
echo "--- SUPERVISOR ---"
timeout 6 ros2 topic echo /safety/deadline_status std_msgs/msg/String --once \
  --qos-reliability reliable --qos-durability transient_local
echo "--- BT STATUS ---"
timeout 6 ros2 topic echo /nav/bt_status std_msgs/msg/String --once \
  --qos-reliability reliable --qos-durability transient_local
echo "--- AUDIT ---"
AUDIT_OUTPUT="$(timeout 6 ros2 topic echo /nav/audit std_msgs/msg/String --once \
  --qos-reliability reliable --qos-durability transient_local)"
echo "${AUDIT_OUTPUT}"
grep -q "PASS" <<<"${AUDIT_OUTPUT}"
echo "--- LAUNCH LOG TAIL ---"
tail -n 35 "${DAILY_LOG_FILE}"
