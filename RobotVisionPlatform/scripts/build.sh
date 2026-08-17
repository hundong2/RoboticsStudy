#!/usr/bin/env bash
# Build both the .NET server and portable C++ device core on Linux/Jetson.
set -euo pipefail
# BASH_SOURCE makes paths independent of the caller's current directory.
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dotnet build "$repo_root/server/RobotVision.Server.slnx" --configuration Release
cmake -S "$repo_root/device" -B "$repo_root/device/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DRV_ENABLE_BOOST_HTTP=OFF
cmake --build "$repo_root/device/build" --parallel
