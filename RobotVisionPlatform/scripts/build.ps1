$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

dotnet build "$root/server/RobotVision.Server.slnx" --configuration Release

$compiler = Get-Command cl, g++, clang++ -ErrorAction SilentlyContinue | Select-Object -First 1
if ($null -eq $compiler) {
    Write-Warning 'C++ compiler was not found. Run from a Visual Studio Developer PowerShell or install LLVM/GCC.'
    exit 0
}

cmake -S "$root/device" -B "$root/device/build" -DRV_ENABLE_BOOST_HTTP=OFF
cmake --build "$root/device/build" --config Release --parallel

