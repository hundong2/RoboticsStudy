#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dotnet run --project "$repo_root/server/tests/RobotVision.Server.Tests" --configuration Release
ctest --test-dir "$repo_root/device/build" --output-on-failure

