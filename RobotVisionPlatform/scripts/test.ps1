$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

dotnet run --project "$root/server/tests/RobotVision.Server.Tests" --configuration Release
if (Test-Path "$root/device/build/CTestTestfile.cmake") {
    ctest --test-dir "$root/device/build" -C Release --output-on-failure
} else {
    Write-Warning 'C++ build directory is unavailable; device tests were skipped.'
}

