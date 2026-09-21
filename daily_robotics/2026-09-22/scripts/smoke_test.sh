#!/usr/bin/env bash
set -eo pipefail
source /opt/ros/jazzy/setup.bash
source install/2026-09-22/setup.bash
set -u
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-122}"
LOG_FILE="/tmp/daily_imm_fault_2026_09_22.log"

timeout --signal=INT --kill-after=3s 22s \
  ros2 launch daily_robotics_2026_09_22 daily_demo.launch.py >"${LOG_FILE}" 2>&1 &
LAUNCH_PID=$!
cleanup() {
  kill -INT "${LAUNCH_PID}" 2>/dev/null || true
  wait "${LAUNCH_PID}" 2>/dev/null || true
}
trap cleanup EXIT
sleep 2
echo "--- NODES ---"
ros2 node list --no-daemon --spin-time 2 | sort
echo "--- FINAL AUDIT ---"
AUDIT_OUTPUT="$(timeout 18 ros2 topic echo /actuator/audit std_msgs/msg/String --once \
  --qos-reliability reliable --qos-durability transient_local)"
echo "${AUDIT_OUTPUT}"
grep -q 'PASS' <<<"${AUDIT_OUTPUT}"
grep -q 'actuator_stop=1' <<<"${AUDIT_OUTPUT}"
grep -q 'transport_stop=1' <<<"${AUDIT_OUTPUT}"
grep -q 'final_recovery=1' <<<"${AUDIT_OUTPUT}"
echo "--- LAUNCH LOG ---"
tail -n 24 "${LOG_FILE}"
