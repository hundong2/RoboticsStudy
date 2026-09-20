#!/usr/bin/env bash
set -eo pipefail
source /opt/ros/jazzy/setup.bash
source install/2026-09-21/setup.bash
set -u
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-121}"
LOG_FILE="/tmp/daily_deskew_2026_09_21.log"

timeout --signal=INT --kill-after=3s 20s \
  ros2 launch daily_robotics_2026_09_21 daily_demo.launch.py >"${LOG_FILE}" 2>&1 &
LAUNCH_PID=$!
cleanup() {
  kill -INT "${LAUNCH_PID}" 2>/dev/null || true
  wait "${LAUNCH_PID}" 2>/dev/null || true
}
trap cleanup EXIT
sleep 5
echo "--- NODES ---"
ros2 node list --no-daemon --spin-time 2 | sort
echo "--- TF ---"
set +e
TF_OUTPUT="$(timeout --signal=INT 4s ros2 run tf2_ros tf2_echo map laser -r 5 2>&1)"
TF_STATUS=$?
set -e
echo "${TF_OUTPUT}"
# Jazzy tf2_echo에는 --once가 없어 timeout(124)이 정상 종료 수단이다.
if [[ "${TF_STATUS}" -ne 0 && "${TF_STATUS}" -ne 124 ]]; then
  exit "${TF_STATUS}"
fi
grep -q 'Translation:' <<<"${TF_OUTPUT}"
echo "--- AUDIT ---"
AUDIT_OUTPUT="$(timeout 10 ros2 topic echo /mapping/audit std_msgs/msg/String --once \
  --qos-reliability reliable --qos-durability transient_local)"
echo "${AUDIT_OUTPUT}"
grep -q 'PASS' <<<"${AUDIT_OUTPUT}"
echo "--- LAUNCH LOG ---"
tail -n 16 "${LOG_FILE}"
