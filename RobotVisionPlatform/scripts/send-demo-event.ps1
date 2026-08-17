param(
    # Base URL of the locally running ASP.NET Core ingest server.
    [string]$ServerUrl = 'http://localhost:5080',
    # Device ID shown on the monitoring page.
    [string]$DeviceId = 'beginner-demo-001'
)

$ErrorActionPreference = 'Stop'
# PowerShell object -> JSON avoids quote escaping mistakes in a handwritten JSON string.
$body = @{
    deviceId = $DeviceId
    sequence = 1
    modelVersion = 'demo/0.1.0'
    detections = @(
        @{ label = 'person'; confidence = 0.94; x = 0.10; y = 0.20; width = 0.30; height = 0.50 }
    )
} | ConvertTo-Json -Depth 4

# POST the same camelCase contract used by the C++ HTTP event sink.
$response = Invoke-WebRequest `
    -Uri "$($ServerUrl.TrimEnd('/'))/api/events/detections" `
    -Method Post `
    -ContentType 'application/json' `
    -Body $body

Write-Host "Demo event accepted: HTTP $($response.StatusCode), device=$DeviceId"
