param(
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $root "assets\images"
}

New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$images = @(
    @{
        Name = "coco_000000000139.jpg"
        Url = "http://images.cocodataset.org/val2017/000000000139.jpg"
        Note = "living room, person, tv, chair, vase"
    },
    @{
        Name = "coco_000000039769.jpg"
        Url = "http://images.cocodataset.org/val2017/000000039769.jpg"
        Note = "cats, sofa, remotes"
    },
    @{
        Name = "coco_000000397133.jpg"
        Url = "http://images.cocodataset.org/val2017/000000397133.jpg"
        Note = "kitchen, person, dining table"
    }
)

foreach ($image in $images) {
    $outFile = Join-Path $OutputDir $image.Name
    if (Test-Path $outFile) {
        Write-Host "skip existing $($image.Name)"
        continue
    }

    Write-Host "download $($image.Name) - $($image.Note)"
    curl.exe -L $image.Url -o $outFile
}

Write-Host "done: $OutputDir"
