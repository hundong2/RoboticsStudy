param(
    [Parameter(Mandatory = $true)]
    [string]$ImagePath,

    [string]$Root = (Resolve-Path "$PSScriptRoot\..").Path,
    [string]$DarknetCommand = "darknet"
)

$ErrorActionPreference = "Stop"

$weightsDir = Join-Path $Root "weights"
$weightsPath = Join-Path $weightsDir "yolov4-tiny.weights"
$weightsUrl = "https://github.com/hank-ai/darknet/releases/download/v2.0/yolov4-tiny.weights"

New-Item -ItemType Directory -Force -Path $weightsDir | Out-Null

if (-not (Test-Path -LiteralPath $weightsPath)) {
    Write-Host "Downloading yolov4-tiny.weights..."
    Invoke-WebRequest -Uri $weightsUrl -OutFile $weightsPath
}

if (-not (Test-Path -LiteralPath $ImagePath)) {
    throw "이미지를 찾을 수 없습니다: $ImagePath"
}

$darknetExists = Get-Command $DarknetCommand -ErrorAction SilentlyContinue
if (-not $darknetExists) {
    Write-Host "Darknet 명령을 찾지 못했습니다. 설치 후 아래 명령을 실행하세요."
    Write-Host "$DarknetCommand detector test cfg/coco.data cfg/yolov4-tiny.cfg `"$weightsPath`" `"$ImagePath`""
    exit 0
}

& $DarknetCommand detector test cfg/coco.data cfg/yolov4-tiny.cfg $weightsPath $ImagePath
