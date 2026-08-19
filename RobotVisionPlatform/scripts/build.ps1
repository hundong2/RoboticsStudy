$ErrorActionPreference = 'Stop'
# Resolve every path from the script location so the command works from any directory.
$root = Split-Path -Parent $PSScriptRoot

# Build the dependency-light server solution first.
dotnet build "$root/server/RobotVision.Server.slnx" --configuration Release

# A normal PowerShell may not expose MSVC; Developer PowerShell adds `cl` to PATH.
$compiler = Get-Command cl, g++, clang++ -ErrorAction SilentlyContinue | Select-Object -First 1
if ($null -eq $compiler) {
    Write-Warning 'C++ compiler was not found. Run from a Visual Studio Developer PowerShell or install LLVM/GCC.'
    exit 0
}

# Disable optional Boost HTTP so the portable C++ core is always testable.
cmake -S "$root/device" -B "$root/device/build" -DRV_ENABLE_BOOST_HTTP=OFF
cmake --build "$root/device/build" --config Release --parallel
