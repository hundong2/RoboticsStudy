#!/usr/bin/env bash
set -eo pipefail

# 공식 Jazzy 설치와 오늘 빌드한 overlay를 같은 shell에 올린다.
source /opt/ros/jazzy/setup.bash
source install/2026-09-16/setup.bash
set -u

# 다른 ROS 2 실습과 discovery가 섞이지 않도록 검증 전용 domain을 사용한다.
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-99}"
DAILY_LOG_FILE="/tmp/daily_time_calibration_2026_09_16.log"

timeout --signal=INT --kill-after=3s 35s \
  ros2 launch daily_robotics_2026_09_16 daily_demo.launch.py >"${DAILY_LOG_FILE}" 2>&1 &
DAILY_LAUNCH_PID=$!

cleanup() {
  kill -INT "${DAILY_LAUNCH_PID}" 2>/dev/null || true
  sleep 1
  kill -TERM "${DAILY_LAUNCH_PID}" 2>/dev/null || true
  wait "${DAILY_LAUNCH_PID}" 2>/dev/null || true
}
trap cleanup EXIT

# 4초 window가 차고 여러 번 보정 결과가 나온 뒤 auditor가 판정할 시간을 준다.
sleep 10

echo "--- NODES ---"
ros2 node list --no-daemon --spin-time 2 | sort
echo "--- TOPICS ---"
ros2 topic list -t --no-daemon --spin-time 2 | grep -E \
  'imu/yaw_rate|lidar/yaw_rate|calibration/status|fusion/yaw_rate|calibration/audit'
echo "--- STATUS ---"
timeout 6 ros2 topic echo /calibration/status \
  daily_robotics_2026_09_16/msg/CalibrationStatus --once \
  --qos-reliability reliable --qos-durability transient_local
echo "--- AUDIT ---"
AUDIT_OUTPUT="$(timeout 6 ros2 topic echo /calibration/audit std_msgs/msg/String --once \
  --qos-reliability reliable --qos-durability transient_local)"
echo "${AUDIT_OUTPUT}"
grep -q "PASS" <<<"${AUDIT_OUTPUT}"
echo "--- LAUNCH LOG TAIL ---"
tail -n 30 "${DAILY_LOG_FILE}"
