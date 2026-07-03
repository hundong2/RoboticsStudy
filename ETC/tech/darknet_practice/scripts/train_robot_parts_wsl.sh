#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CFG="$ROOT/project/robot_parts.cfg"
DATA="$ROOT/project/robot_parts.data"

if ! command -v darknet >/dev/null 2>&1; then
  echo "darknet 명령을 찾지 못했습니다. scripts/setup_darknet_wsl.sh로 먼저 설치하세요."
  exit 1
fi

if [ ! -f "$CFG" ]; then
  echo "cfg 파일이 없습니다: $CFG"
  echo "Darknet 저장소의 cfg/yolov4-tiny.cfg를 project/robot_parts.cfg로 복사한 뒤 클래스 수를 수정하세요."
  exit 1
fi

powershell.exe -ExecutionPolicy Bypass -File "$ROOT/scripts/create_dataset_lists.ps1" -AsWslPaths >/dev/null

darknet detector -map -dont_show train "$DATA" "$CFG"
