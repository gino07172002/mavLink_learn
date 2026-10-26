param(
    [string]$QgcExe = 'D:\QGroundControl\bin\QGroundControl.exe',
    [ValidateRange(1, 3600)]
    [int]$DurationSeconds = 3600
)

$ErrorActionPreference = 'Stop'
$demoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$bridgeExe = Join-Path $demoRoot 'build/lab04_bridge.exe'
if (-not (Test-Path -LiteralPath $bridgeExe)) {
    throw 'Build the examples first: cmake --build build --parallel 1'
}
if (-not (Test-Path -LiteralPath $QgcExe)) {
    throw "QGroundControl executable not found: $QgcExe"
}

# Reuse a bridge already listening on the demo port; never terminate existing processes.
$demoEndpoints = @(Get-NetUDPEndpoint -LocalPort 14560 -ErrorAction SilentlyContinue)
if ($demoEndpoints.Count -gt 0) {
    foreach ($demoEndpoint in $demoEndpoints) {
        $demoOwner = Get-Process -Id $demoEndpoint.OwningProcess
        if ($demoOwner.Path -ne $bridgeExe) {
            throw "UDP 14560 is occupied by $($demoOwner.ProcessName); choose the manual port setup."
        }
    }
    Write-Host 'Reusing the running lab04_bridge on UDP 14560. Its original peer/duration still apply.'
} else {
    $demoBridge = Start-Process -FilePath $bridgeExe `
        -ArgumentList @('normal', '14560', '127.0.0.1', '14550', "$DurationSeconds") `
        -WorkingDirectory $demoRoot -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $demoRoot 'build/qgc-bridge.log') `
        -RedirectStandardError (Join-Path $demoRoot 'build/qgc-bridge-error.log') -PassThru
    Write-Host "Started teaching bridge PID=$($demoBridge.Id), duration=$DurationSeconds seconds."
}

$demoQgc = @(Get-Process -Name QGroundControl -ErrorAction SilentlyContinue)
if ($demoQgc.Count -gt 0) {
    Write-Host 'QGroundControl is already running; keeping the existing instance.'
} else {
    # Only the launched child inherits this renderer setting; no persistent OS setting changes.
    $demoPreviousBackend = $env:QT_QUICK_BACKEND
    try {
        $env:QT_QUICK_BACKEND = 'software'
        $demoStartedQgc = Start-Process -FilePath $QgcExe `
            -ArgumentList @('--logging:Comms.LinkManager,Vehicle.MultiVehicleManager', '--log-output') `
            -WorkingDirectory (Split-Path -Parent $QgcExe) -WindowStyle Normal `
            -RedirectStandardOutput (Join-Path $demoRoot 'build/qgc-software.log') `
            -RedirectStandardError (Join-Path $demoRoot 'build/qgc-software-error.log') -PassThru
        Write-Host "Started QGroundControl PID=$($demoStartedQgc.Id), Qt Quick software rendering."
    } finally {
        $env:QT_QUICK_BACKEND = $demoPreviousBackend
    }
}
Write-Host 'QGC: Analyze -> MAVLink Inspector. Expected vehicle 1 / component 1.'
Write-Host 'This is the teaching bridge, not PX4 SITL. Not Ready / missing parameters are expected.'
