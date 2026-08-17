$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

# Run fast server behavior and dependency-free model-manifest tests.
dotnet run --project "$root/server/tests/RobotVision.Server.Tests" --configuration Release
python -m unittest discover -s "$root/device/tools/model/tests" -v
# C++ tests are optional on Windows machines without a configured compiler/build tree.
if (Test-Path "$root/device/build/CTestTestfile.cmake") {
    ctest --test-dir "$root/device/build" -C Release --output-on-failure
} else {
    Write-Warning 'C++ build directory is unavailable; device tests were skipped.'
}
