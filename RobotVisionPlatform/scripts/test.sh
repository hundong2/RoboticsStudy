#!/usr/bin/env bash
# Run server, model-tool, and already-configured C++ tests; stop at the first failure.
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
dotnet run --project "$repo_root/server/tests/RobotVision.Server.Tests" --configuration Release
python3 -m unittest discover -s "$repo_root/device/tools/model/tests" -v
ctest --test-dir "$repo_root/device/build" --output-on-failure
