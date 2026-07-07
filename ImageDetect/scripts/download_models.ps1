param(
    [ValidateSet("mediapipe_lite0", "yolox_tiny", "yolox_s", "rtdetrv2_s", "all-small")]
    [string]$Model = "mediapipe_lite0",
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $root "assets\models"
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

function Get-ModelFile {
    param(
        [Parameter(Mandatory = $true)][string]$Url,
        [Parameter(Mandatory = $true)][string]$FileName
    )

    $outFile = Join-Path $OutputDir $FileName
    if (Test-Path $outFile) {
        Write-Host "skip existing $FileName"
        return
    }

    Write-Host "download $FileName"
    curl.exe -L $Url -o $outFile
}

if ($Model -eq "mediapipe_lite0" -or $Model -eq "all-small") {
    Get-ModelFile `
        -Url "https://storage.googleapis.com/mediapipe-models/object_detector/efficientdet_lite0/int8/latest/efficientdet_lite0.tflite" `
        -FileName "efficientdet_lite0_int8.tflite"
}

if ($Model -eq "yolox_tiny" -or $Model -eq "all-small") {
    Get-ModelFile `
        -Url "https://github.com/Megvii-BaseDetection/YOLOX/releases/download/0.1.1rc0/yolox_tiny.onnx" `
        -FileName "yolox_tiny.onnx"
}

if ($Model -eq "yolox_s") {
    Get-ModelFile `
        -Url "https://github.com/Megvii-BaseDetection/YOLOX/releases/download/0.1.1rc0/yolox_s.onnx" `
        -FileName "yolox_s.onnx"
}

if ($Model -eq "rtdetrv2_s") {
    Get-ModelFile `
        -Url "https://github.com/lyuwenyu/storage/releases/download/v0.2/rtdetrv2_r18vd_120e_coco_rerun_48.1.pth" `
        -FileName "rtdetrv2_r18vd_120e_coco_rerun_48.1.pth"
}

Write-Host "done: $OutputDir"
