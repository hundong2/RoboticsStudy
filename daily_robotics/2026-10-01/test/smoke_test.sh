#!/usr/bin/env bash
set -eo pipefail

# 같은 PC의 다른 DDS 실습과 discovery가 섞이지 않도록 별도 domain을 사용한다.
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-152}"

tmp_dir="$(mktemp -d)"
launch_log="${tmp_dir}/launch.log"
cleanup() {
  if [[ -n "${launch_pid:-}" ]]; then
    kill -TERM -- "-${launch_pid}" 2>/dev/null || true
    for _ in $(seq 1 20); do
      kill -0 "${launch_pid}" 2>/dev/null || break
      sleep 0.1
    done
    kill -KILL -- "-${launch_pid}" 2>/dev/null || true
    wait "${launch_pid}" 2>/dev/null || true
  fi
  rm -rf "${tmp_dir}"
}
trap cleanup EXIT

setsid ros2 launch daily_robotics_2026_10_01 study.launch.py >"${launch_log}" 2>&1 &
launch_pid=$!

for _ in $(seq 1 20); do
  if timeout 1 ros2 topic echo --once /study/audit_pass std_msgs/msg/Bool 2>/dev/null | grep -q 'data: true'; then
    grep 'AUDIT_PASS' "${launch_log}" | tail -n 1 || true
    echo "SMOKE_PASS: SNS 포화·task scaling·조건수 감시 독립 감사 통과"
    exit 0
  fi
  if ! kill -0 "${launch_pid}" 2>/dev/null; then
    cat "${launch_log}"
    echo "SMOKE_FAIL: launch가 조기 종료됨" >&2
    exit 1
  fi
  sleep 0.5
done

cat "${launch_log}"
echo "SMOKE_FAIL: 약 30초 안에 audit_pass=true를 받지 못함" >&2
exit 1
