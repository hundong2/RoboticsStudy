#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dotnet build "$repo_root/server/RobotVision.Server.slnx" --configuration Release
cmake -S "$repo_root/device" -B "$repo_root/device/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DRV_ENABLE_BOOST_HTTP=OFF
cmake --build "$repo_root/device/build" --parallel

