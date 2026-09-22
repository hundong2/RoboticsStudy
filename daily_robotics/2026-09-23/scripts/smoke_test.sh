#!/usr/bin/env bash
set -eo pipefail
source /opt/ros/jazzy/setup.bash
source install/2026-09-23/setup.bash
set -u
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-123}"
LOG_FILE="/tmp/daily_impedance_2026_09_23.log"

timeout --signal=INT --kill-after=3s 14s \
  ros2 launch daily_robotics_2026_09_23 daily_demo.launch.py >"${LOG_FILE}" 2>&1 &
LAUNCH_PID=$!
cleanup() {
  kill -INT "${LAUNCH_PID}" 2>/dev/null || true
  wait "${LAUNCH_PID}" 2>/dev/null || true
}
trap cleanup EXIT
sleep 3
echo "--- NODES ---"
NODE_OUTPUT="$(ros2 node list | sort)"
echo "${NODE_OUTPUT}"
grep -q '/contact_plant' <<<"${NODE_OUTPUT}"
grep -q '/impedance_controller' <<<"${NODE_OUTPUT}"
grep -q '/energy_safety_auditor' <<<"${NODE_OUTPUT}"
echo "--- TOPICS ---"
TOPIC_OUTPUT="$(ros2 topic list | sort)"
echo "${TOPIC_OUTPUT}"
grep -q '/joint_effort_raw' <<<"${TOPIC_OUTPUT}"
grep -q '/joint_effort_allowed' <<<"${TOPIC_OUTPUT}"
echo "--- FINAL AUDIT ---"
AUDIT_OUTPUT="$(timeout 11 ros2 topic echo /contact/audit std_msgs/msg/String --once \
  --qos-reliability reliable --qos-durability transient_local)"
echo "${AUDIT_OUTPUT}"
grep -q 'PASS' <<<"${AUDIT_OUTPUT}"
grep -q 'contact=1' <<<"${AUDIT_OUTPUT}"
grep -q 'stop=1' <<<"${AUDIT_OUTPUT}"
grep -q 'recovery=1' <<<"${AUDIT_OUTPUT}"
echo "--- SAFETY STATUS ---"
timeout 3 ros2 topic echo /contact/safety_status \
  daily_robotics_2026_09_23/msg/SafetyStatus --once
echo "--- LAUNCH LOG ---"
tail -n 24 "${LOG_FILE}"
