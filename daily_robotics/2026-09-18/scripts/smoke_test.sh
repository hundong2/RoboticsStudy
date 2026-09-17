#!/usr/bin/env bash
set -eo pipefail
source /opt/ros/jazzy/setup.bash
source install/2026-09-18/setup.bash
set -u
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-99}"
LOG_FILE="/tmp/daily_grid_mapping_2026_09_18.log"

timeout --signal=INT --kill-after=3s 16s \
  ros2 launch daily_robotics_2026_09_18 daily_demo.launch.py >"${LOG_FILE}" 2>&1 &
LAUNCH_PID=$!
cleanup() {
  kill -INT "${LAUNCH_PID}" 2>/dev/null || true
  wait "${LAUNCH_PID}" 2>/dev/null || true
}
trap cleanup EXIT
sleep 4
echo "--- NODES ---"
ros2 node list --no-daemon --spin-time 2 | sort
echo "--- AUDIT ---"
AUDIT_OUTPUT="$(timeout 7 ros2 topic echo /map/audit std_msgs/msg/String --once \
  --qos-reliability reliable --qos-durability transient_local)"
echo "${AUDIT_OUTPUT}"
grep -q 'PASS' <<<"${AUDIT_OUTPUT}"
echo "--- LAUNCH LOG ---"
tail -n 12 "${LOG_FILE}"
