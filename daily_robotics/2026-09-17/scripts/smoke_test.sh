#!/usr/bin/env bash
set -eo pipefail
source /opt/ros/jazzy/setup.bash
source install/2026-09-17/setup.bash
set -u
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-98}"
LOG_FILE="/tmp/daily_imu_preintegration_2026_09_17.log"

# 4초 뒤 /clock이 역행하고, 두 번째 주기의 2.5초를 지난 뒤 auditor가 판정한다.
timeout --signal=INT --kill-after=3s 18s \
  ros2 launch daily_robotics_2026_09_17 daily_demo.launch.py >"${LOG_FILE}" 2>&1 &
LAUNCH_PID=$!
cleanup() {
  kill -INT "${LAUNCH_PID}" 2>/dev/null || true
  wait "${LAUNCH_PID}" 2>/dev/null || true
}
trap cleanup EXIT
sleep 9
echo "--- NODES ---"
ros2 node list --no-daemon --spin-time 2 | sort
echo "--- AUDIT ---"
AUDIT_OUTPUT="$(timeout 6 ros2 topic echo /imu/audit std_msgs/msg/String --once \
  --qos-reliability reliable --qos-durability transient_local)"
echo "${AUDIT_OUTPUT}"
grep -q 'PASS' <<<"${AUDIT_OUTPUT}"
echo "--- LAUNCH LOG ---"
tail -n 12 "${LOG_FILE}"
