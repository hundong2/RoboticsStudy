#!/usr/bin/env bash
set -eo pipefail

source /opt/ros/jazzy/setup.bash
source install/2026-09-25/setup.bash
set -u

# 다른 ROS 실습이나 호스트 프로세스와 discovery graph가 섞이지 않게 별도 Domain을 쓴다.
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-125}"
LOG_FILE="/tmp/daily_trajectory_2026_09_25.log"

timeout --signal=INT --kill-after=3s 14s \
  ros2 launch daily_robotics_2026_09_25 daily_demo.launch.py >"${LOG_FILE}" 2>&1 &
LAUNCH_PID=$!

cleanup() {
  kill -INT "${LAUNCH_PID}" 2>/dev/null || true
  wait "${LAUNCH_PID}" 2>/dev/null || true
}
trap cleanup EXIT

# DDS discovery 시간은 호스트 부하와 daemon 상태에 따라 달라진다. 고정 sleep 대신 최대 5초만 polling한다.
GRAPH_READY=0
ACTION_OUTPUT=""
NODE_OUTPUT=""
for _ in $(seq 1 20); do
  ACTION_OUTPUT="$(ros2 action list | sort)"
  NODE_OUTPUT="$(ros2 node list | sort)"
  if grep -q '/execute_joint_trajectory' <<<"${ACTION_OUTPUT}" && \
     grep -q '/bounded_trajectory_server' <<<"${NODE_OUTPUT}" && \
     grep -q '/tracking_guard' <<<"${NODE_OUTPUT}"; then
    GRAPH_READY=1
    break
  fi
  sleep 0.25
done

echo "--- ACTIONS ---"
echo "${ACTION_OUTPUT}"
test "${GRAPH_READY}" -eq 1
grep -q '/execute_joint_trajectory' <<<"${ACTION_OUTPUT}"

echo "--- NODES ---"
echo "${NODE_OUTPUT}"
grep -q '/bounded_trajectory_server' <<<"${NODE_OUTPUT}"
grep -q '/tracking_guard' <<<"${NODE_OUTPUT}"

echo "--- INDEPENDENT AUDIT ---"
AUDIT_OUTPUT="$(timeout 9 ros2 topic echo /trajectory/audit std_msgs/msg/String --once \
  --qos-reliability reliable --qos-durability transient_local)"
echo "${AUDIT_OUTPUT}"
grep -q 'AUDIT_PASS' <<<"${AUDIT_OUTPUT}"
grep -q 'nonzero_start=1' <<<"${AUDIT_OUTPUT}"
grep -q 'nonzero_end=1' <<<"${AUDIT_OUTPUT}"

sleep 1
echo "--- CLIENT RESULT ---"
grep 'CLIENT_PASS' "${LOG_FILE}"

echo "--- LAUNCH LOG ---"
tail -n 28 "${LOG_FILE}"
