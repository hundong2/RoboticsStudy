param(
    [string]$Root = (Resolve-Path "$PSScriptRoot\..").Path,
    [switch]$AsWslPaths
)

$ErrorActionPreference = "Stop"

$imageExtensions = @("*.jpg", "*.jpeg", "*.png", "*.bmp", "*.webp")
$projectDir = Join-Path $Root "project"
$trainImageDir = Join-Path $Root "data\images\train"
$validImageDir = Join-Path $Root "data\images\valid"
$trainList = Join-Path $projectDir "train.txt"
$validList = Join-Path $projectDir "valid.txt"

function Get-ImageList {
    param([string]$Directory)

    $files = foreach ($extension in $imageExtensions) {
        Get-ChildItem -LiteralPath $Directory -Filter $extension -File -ErrorAction SilentlyContinue
    }

    $files | Sort-Object FullName | ForEach-Object {
        $path = $_.FullName.Replace("\", "/")
        if ($AsWslPaths -and $path -match "^([A-Za-z]):/(.*)$") {
            $drive = $matches[1].ToLowerInvariant()
            "/mnt/$drive/$($matches[2])"
        }
        else {
            $path
        }
    }
}

New-Item -ItemType Directory -Force -Path $projectDir | Out-Null

$trainImages = @(Get-ImageList -Directory $trainImageDir)
$validImages = @(Get-ImageList -Directory $validImageDir)

if ($trainImages.Count -gt 0) {
    $trainImages | Set-Content -LiteralPath $trainList -Encoding UTF8
}
else {
    Clear-Content -LiteralPath $trainList -ErrorAction SilentlyContinue
}

if ($validImages.Count -gt 0) {
    $validImages | Set-Content -LiteralPath $validList -Encoding UTF8
}
else {
    Clear-Content -LiteralPath $validList -ErrorAction SilentlyContinue
}

$trainCount = (Get-Content -LiteralPath $trainList -ErrorAction SilentlyContinue | Where-Object { $_.Trim() }).Count
$validCount = (Get-Content -LiteralPath $validList -ErrorAction SilentlyContinue | Where-Object { $_.Trim() }).Count

Write-Host "train.txt 생성 완료: $trainCount images"
Write-Host "valid.txt 생성 완료: $validCount images"
if ($AsWslPaths) {
    Write-Host "경로 형식: WSL (/mnt/...)"
}
else {
    Write-Host "경로 형식: Windows (D:/...)"
}
Write-Host "다음 단계: project/robot_parts.cfg를 준비한 뒤 학습 스크립트를 실행하세요."
