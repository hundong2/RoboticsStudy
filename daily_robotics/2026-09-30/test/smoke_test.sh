#!/usr/bin/env bash
set -eo pipefail

# 같은 PC에서 다른 ROS 실습과 DDS discovery가 섞이지 않도록 domain을 분리한다.
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-144}"

tmp_dir="$(mktemp -d)"
launch_log="${tmp_dir}/launch.log"
cleanup() {
  if [[ -n "${launch_pid:-}" ]]; then
    # setsid로 만든 process group 전체에 TERM을 보내고, 2초 뒤 남은 프로세스만 KILL한다.
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

setsid ros2 launch daily_robotics_2026_09_30 study.launch.py >"${launch_log}" 2>&1 &
launch_pid=$!

for _ in $(seq 1 35); do
  if timeout 2 ros2 topic echo --once /study/audit_pass std_msgs/msg/Bool 2>/dev/null | grep -q 'data: true'; then
    grep 'AUDIT_PASS' "${launch_log}" | tail -n 1 || true
    echo "SMOKE_PASS: 연속 접촉 추정과 마찰 폴백 독립 감사 통과"
    exit 0
  fi
  if ! kill -0 "${launch_pid}" 2>/dev/null; then
    cat "${launch_log}"
    echo "SMOKE_FAIL: launch가 조기 종료됨" >&2
    exit 1
  fi
  sleep 1
done

cat "${launch_log}"
echo "SMOKE_FAIL: 35초 안에 audit_pass=true를 받지 못함" >&2
exit 1
