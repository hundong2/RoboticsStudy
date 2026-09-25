#!/usr/bin/env bash
set -eo pipefail

source /opt/ros/jazzy/setup.bash
source install/2026-09-26/setup.bash
set -u

# 호스트의 다른 ROS 실습과 DDS discovery graph가 섞이지 않도록 별도 Domain을 쓴다.
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-126}"
LOG_FILE="/tmp/daily_contact_qp_2026_09_26.log"

timeout --signal=INT --kill-after=3s 14s \
  ros2 launch daily_robotics_2026_09_26 daily_demo.launch.py >"${LOG_FILE}" 2>&1 &
LAUNCH_PID=$!

cleanup() {
  kill -INT "${LAUNCH_PID}" 2>/dev/null || true
  wait "${LAUNCH_PID}" 2>/dev/null || true
}
trap cleanup EXIT

# DDS 발견 시간은 환경 부하에 따라 달라지므로 고정 sleep 대신 최대 5초 polling한다.
GRAPH_READY=0
NODE_OUTPUT=""
TOPIC_OUTPUT=""
for _ in $(seq 1 20); do
  NODE_OUTPUT="$(ros2 node list | sort)"
  TOPIC_OUTPUT="$(ros2 topic list | sort)"
  if grep -q '/bounded_contact_allocator' <<<"${NODE_OUTPUT}" && \
     grep -q '/whole_body_auditor' <<<"${NODE_OUTPUT}" && \
     grep -q '/wbc/contact_solution' <<<"${TOPIC_OUTPUT}"; then
    GRAPH_READY=1
    break
  fi
  sleep 0.25
done

echo "--- NODES ---"
echo "${NODE_OUTPUT}"
test "${GRAPH_READY}" -eq 1
grep -q '/wrench_command_publisher' <<<"${NODE_OUTPUT}"

echo "--- TOPICS ---"
echo "${TOPIC_OUTPUT}"
grep -q '/wbc/desired_wrench' <<<"${TOPIC_OUTPUT}"
grep -q '/wbc/joint_effort' <<<"${TOPIC_OUTPUT}"

echo "--- CONTACT SOLUTION ---"
SOLUTION_OUTPUT="$(timeout 5 ros2 topic echo /wbc/contact_solution \
  daily_robotics_2026_09_26/msg/ContactSolution --once)"
echo "${SOLUTION_OUTPUT}"
grep -q 'iterations: 64' <<<"${SOLUTION_OUTPUT}"
grep -q 'feasible: true' <<<"${SOLUTION_OUTPUT}"

echo "--- INDEPENDENT AUDIT ---"
AUDIT_OUTPUT="$(timeout 9 ros2 topic echo /wbc/audit std_msgs/msg/String --once \
  --qos-reliability reliable --qos-durability transient_local)"
echo "${AUDIT_OUTPUT}"
grep -q 'AUDIT_PASS' <<<"${AUDIT_OUTPUT}"
grep -q 'bounded_iters=1' <<<"${AUDIT_OUTPUT}"
grep -q 'jacobian_tau_finite=1' <<<"${AUDIT_OUTPUT}"

echo "--- LAUNCH LOG ---"
tail -n 24 "${LOG_FILE}"
