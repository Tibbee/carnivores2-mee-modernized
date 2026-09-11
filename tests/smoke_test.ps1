# tests/smoke_test.ps1 — Launch C2 game, run N seconds, verify clean exit
param(
    [string]$ExePath = "",
    [string]$LogFile = "",
    [int]$Duration = 5,
    [string]$WorkingDir = "",
    [string]$Renderer = "GL",
    [string]$GameDir = "",
    [string[]]$GameArgs = @(
        "reg=0",
        "prj=huntdat\areas\area1",
        "area=1",
        "din=1",
        "wep=1",
        "dtm=1"
    )
)

if (-not $Renderer) {
    $Renderer = "GL"
}
$Renderer = $Renderer.ToUpperInvariant()
if ($Renderer -notin @("GL", "SOFT")) {
    throw "Unsupported renderer '$Renderer'. Expected GL or SOFT."
}

# By default smoke-test the deployed game, not the build artifact. The
# deployment task uses CARNIVORES_LEGACY_DIR and defaults to this location.
if (-not $GameDir) {
    $GameDir = $env:CARNIVORES_LEGACY_DIR
}
if (-not $GameDir) {
    $GameDir = "E:\Games\CarnivoresLegacy"
}
if (-not $ExePath) {
    $ExeName = if ($Renderer -eq "GL") { "v_gl.exe" } else { "v_soft.ren" }
    $ExePath = Join-Path $GameDir $ExeName
}
if (-not $WorkingDir) {
    $WorkingDir = Split-Path -Parent $ExePath
}
if (-not $LogFile) {
    $LogFile = Join-Path $WorkingDir "carnivor.log"
}

if (-not (Test-Path $ExePath)) {
    throw "Deployed game executable not found: $ExePath. Run the VS Code copy task first."
}
if (-not (Test-Path $WorkingDir)) {
    throw "Game working directory not found: $WorkingDir. Run the deployment task first."
}

# Resolve ExePath
$ExePath = Resolve-Path $ExePath -ErrorAction Stop

Write-Host "=== Smoke Test ==="
Write-Host "  Exe:      $ExePath"
Write-Host "  Duration: $($Duration)s"
Write-Host "  Log:      $LogFile"
Write-Host "  WorkDir:  $WorkingDir"

# A game instance that is already running holds carnivor.log open and keeps
# writing to it. This test would then delete the file out from under it, read
# back the other session's log, and report whatever it finds there as a
# failure. Say so plainly instead of failing mysteriously later.
$aliases = @(
    [System.IO.Path]::GetFileNameWithoutExtension($ExePath),
    'v_gl', 'v_soft', 'v_d3d', 'v_3dfx', 'Carnivores1_GL', 'Carnivores1_SOFT'
) | Select-Object -Unique
$running = Get-Process -Name $aliases -ErrorAction SilentlyContinue
if ($running) {
    throw "A game instance is already running (PID $(($running.Id) -join ', ')). Close it and re-run - this test needs carnivor.log to itself."
}

# Clean previous log
if (Test-Path $LogFile) {
    try {
        Remove-Item $LogFile -Force -ErrorAction Stop
    } catch {
        throw "Could not delete $LogFile - $($_.Exception.Message)"
    }
    Write-Host "  Removed old log file"
}

Write-Host "Launching deployed game..."
Write-Host "  Args:     $($GameArgs -join ' ')"
$proc = Start-Process -FilePath $ExePath -ArgumentList $GameArgs -WorkingDirectory $WorkingDir -PassThru -NoNewWindow
Write-Host "  PID: $($proc.Id)"
Write-Host "  Waiting $($Duration) seconds..."

$sw = [System.Diagnostics.Stopwatch]::StartNew()
$exitedEarly = $false

while ($sw.Elapsed.TotalSeconds -lt $Duration) {
    Start-Sleep -Milliseconds 200
    if ($proc.HasExited) {
        $exitedEarly = $true
        break
    }
}

if ($exitedEarly) {
    $elapsed = $sw.Elapsed.TotalSeconds.ToString('F1')
    $exitCode = $proc.ExitCode
    Write-Host "Game exited early after ${elapsed}s (exit code: $exitCode)"
    if ($exitCode -ne 0 -and $exitCode -ne -1 -and $exitCode -ne 1) {
        Write-Host "FAILED: Game exited with unexpected code $exitCode"
        exit 1
    }
} else {
    Write-Host "Time's up - killing process..."
    $proc.Kill()
    Wait-Process -Id $proc.Id -ErrorAction SilentlyContinue
    Write-Host "Process terminated (exit code undefined after kill)"
}

# Check log for errors
if (Test-Path $LogFile) {
    $logContent = Get-Content $LogFile -Raw
    $errorPattern = 'ABNORMAL_HALT|ERROR|FATAL|assertion'
    if ($logContent -match $errorPattern) {
        Write-Host "FAILED: Found error markers in log:"
        $logContent | Select-String -Pattern $errorPattern | ForEach-Object { Write-Host "  $_" }
        exit 1
    }
    $logSize = (Get-Item $LogFile).Length
    Write-Host "  Log: $logSize bytes (no errors)"
} else {
    Write-Warning "No log file found at $LogFile"
}

Write-Host "=== Smoke Test PASSED ==="
exit 0
