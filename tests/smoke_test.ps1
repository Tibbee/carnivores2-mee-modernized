param(
    [string]$ExePath = "",
    [string]$LogFile = "",
    [ValidateRange(1, 2147483)]
    [int]$Duration = 5,
    [switch]$AllowMissingLog,
    [string]$WorkingDir = "",
    [string]$Preset = "hunt",
    [string]$Renderer = "GL",
    [string]$GameDir = "",
    [switch]$SkipFrameCheck,
    [string]$FrameDir = "",
    [ValidateRange(0, 255)]
    [double]$MinMeanLuminance = 1.0,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$GameArgs = @(
        "reg=0",
        "prj=huntdat\areas\area1",
        "area=1",
        "din=1",
        "wep=1",
        "dtm=1"
    )
)

$ErrorActionPreference = 'Stop'

if (-not $Renderer) {
    $Renderer = "GL"
}
$Renderer = $Renderer.ToUpperInvariant()
if ($Renderer -notin @("GL", "SOFT")) {
    throw "Unsupported renderer '$Renderer'. Expected GL or SOFT."
}

# Named launch shapes for the deployed-asset smoke tier. Explicit -GameArgs (or
# trailing arguments) always win; a preset only replaces the defaults.
if (-not $PSBoundParameters.ContainsKey('GameArgs')) {
    switch ($Preset.ToLowerInvariant()) {
        'hunt' { }
        'trophy' {
            # The trophy room is an observer-style loadout: no creatures and no
            # weapons, which is what the standalone menu now emits.
            $GameArgs = @("reg=0", "prj=huntdat/areas/trophy", "din=0", "wep=0", "dtm=1")
        }
        default {
            throw "Unsupported preset '$Preset'. Expected hunt or trophy."
        }
    }
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

if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) {
    throw "Deployed game executable not found: $ExePath. Run the VS Code copy task first."
}
if (-not (Test-Path -LiteralPath $WorkingDir -PathType Container)) {
    throw "Game working directory not found: $WorkingDir. Run the deployment task first."
}

# Resolve ExePath
$ExePath = (Resolve-Path -LiteralPath $ExePath -ErrorAction Stop).Path

Write-Host "=== Smoke Test ==="
Write-Host "  Exe:      $ExePath"
Write-Host "  Duration: $($Duration)s"
Write-Host "  Log:      $LogFile"
Write-Host "  WorkDir:  $WorkingDir"
Write-Host "  Renderer: $Renderer"

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

# ---------------------------------------------------------------------------
# Framebuffer assertions
#
# Log inspection alone is not enough. The software reload black screen
# produced a completely clean carnivor.log, so the historical smoke test
# passed it: the renderer was destroyed and never recreated, and nothing
# complained. The only symptom was a black window, so it has to be asserted
# on the pixels.
#
# This uses the same window discovery and client capture as
# tools/c2_capture_matrix.ps1 (both dot-source tools/c2_native_windows.ps1),
# so they agree on what the engine client is by construction.
# ---------------------------------------------------------------------------

Add-Type -AssemblyName System.Windows.Forms, System.Drawing
. (Join-Path $PSScriptRoot '../tools/c2_native_windows.ps1')
try { [C2NativeWindows]::SetProcessDpiAwarenessContext([IntPtr]::new(-4)) | Out-Null } catch { }

$SW_MINIMIZE = 6
$SW_RESTORE = 9

function Resolve-RenderHwnd {
    param([System.Diagnostics.Process]$Process)
    # C2 keeps a separate loading/progress window that can also be
    # MainWindowHandle. Only the renderer class owned by this process counts.
    return [C2NativeWindows]::FindHuntWindowForProcess($Process.Id)
}

function Wait-RenderHwnd {
    param([System.Diagnostics.Process]$Process, [int]$TimeoutSeconds)
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        if ($Process.HasExited) { throw "Game exited before its renderer window appeared (exit $($Process.ExitCode))." }
        $hwnd = Resolve-RenderHwnd $Process
        if ($hwnd -ne [IntPtr]::Zero) {
            $rect = New-Object C2NativeWindows+RECT
            if ([C2NativeWindows]::GetClientRect($hwnd, [ref]$rect) -and ($rect.R - $rect.L) -gt 1 -and ($rect.B - $rect.T) -gt 1) {
                return [pscustomobject]@{ Hwnd = $hwnd; Width = $rect.R - $rect.L; Height = $rect.B - $rect.T }
            }
        }
        Start-Sleep -Milliseconds 250
    }
    throw "Process-owned HuntRenderWindow did not become usable within $TimeoutSeconds seconds."
}

function Get-ClientLuminance {
    <#
      Grabs the engine client and reduces it to mean/max luminance. A
      destroyed renderer reads as a solid black client, which is exactly the
      failure this exists to catch.
    #>
    param([IntPtr]$Hwnd, [int]$Width, [int]$Height, [string]$OutPng = "")
    $origin = New-Object C2NativeWindows+POINT
    if (-not [C2NativeWindows]::ClientToScreen($Hwnd, [ref]$origin)) { throw "ClientToScreen failed." }
    $screen = [System.Windows.Forms.SystemInformation]::VirtualScreen
    if ($origin.X -lt $screen.Left -or $origin.Y -lt $screen.Top -or
        ($origin.X + $Width) -gt $screen.Right -or ($origin.Y + $Height) -gt $screen.Bottom) {
        throw "The ${Width}x${Height} client is not fully visible on the virtual desktop; a screen grab would capture something other than the game."
    }
    $bitmap = New-Object System.Drawing.Bitmap $Width, $Height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen($origin.X, $origin.Y, 0, 0,
            [System.Drawing.Size]::new($Width, $Height),
            [System.Drawing.CopyPixelOperation]::SourceCopy)
        if ($OutPng) { $bitmap.Save($OutPng, [System.Drawing.Imaging.ImageFormat]::Png) }
        # Sample on a grid; a full per-pixel pass is needlessly slow for a
        # black/not-black test and the client is large.
        $sum = 0L; $max = 0.0; $samples = 0
        $stepX = [Math]::Max(1, [int]($Width / 160))
        $stepY = [Math]::Max(1, [int]($Height / 120))
        for ($y = 0; $y -lt $Height; $y += $stepY) {
            for ($x = 0; $x -lt $Width; $x += $stepX) {
                $c = $bitmap.GetPixel($x, $y)
                $l = 0.2126 * $c.R + 0.7152 * $c.G + 0.0722 * $c.B
                $sum += $l
                if ($l -gt $max) { $max = $l }
                $samples++
            }
        }
        if ($samples -eq 0) { throw "No pixels were sampled from the ${Width}x${Height} client." }
        return [pscustomobject]@{ Mean = $sum / $samples; Max = $max; Samples = $samples }
    } finally {
        $graphics.Dispose(); $bitmap.Dispose()
    }
}

function Assert-FrameNotBlack {
    # The default threshold is a deliberate near-zero: a black client is 0.0,
    # and a legitimately dark frame is orders of magnitude above 1.0, so
    # night scenes will not trip it. Raised only by -MinMeanLuminance, which
    # exists so the assertion can be proven able to fail.
    param($Window, [string]$Label, [string]$OutDir, [double]$Threshold = 1.0)
    $png = if ($OutDir) { Join-Path $OutDir ("frame-$Label.png") } else { "" }
    $stats = Get-ClientLuminance $Window.Hwnd $Window.Width $Window.Height $png
    $where = if ($png) { " (saved $png)" } else { "" }
    Write-Host ("  {0,-12} mean luminance {1,7:F2}  max {2,6:F1}{3}" -f $Label, $stats.Mean, $stats.Max, $where)
    if ($stats.Mean -lt $Threshold) {
        throw "Framebuffer is black after $Label (mean luminance $($stats.Mean.ToString('F3'))/255, required $Threshold)$where. The renderer window exists but is not drawing."
    }
    return $stats
}

function Wait-ForLogCount {
    param([string]$Path, [string]$Pattern, [int]$Target, [int]$TimeoutSeconds, [System.Diagnostics.Process]$Process)
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        if ($Process.HasExited) { throw "Game exited while waiting for '$Pattern' (exit $($Process.ExitCode))." }
        if (Test-Path -LiteralPath $Path -PathType Leaf) {
            $count = @(Select-String -LiteralPath $Path -Pattern $Pattern -SimpleMatch -ErrorAction SilentlyContinue).Count
            if ($count -ge $Target) { return $count }
        }
        Start-Sleep -Milliseconds 250
    }
    return -1
}

function Invoke-FrameChecks {
    <#
      Boot, then a full renderer teardown/recreate, asserting non-black
      pixels after each.

      The restart is driven by the window losing and regaining activation:
      MainWndProc recomputes `A = (GetActiveWindow() == hWnd)` on every
      message, and on the transition it calls ShutDown3DHardware() and sets
      NeedRVM, which routes through Activate3DHardware() -- the same
      shutdown/recreate path as a gameplay reload, and the one that used to
      leave the software renderer destroyed. Unlike the in-game reload (the
      'R' key) it needs no particular game state, so it is deterministic
      here.
    #>
    param([System.Diagnostics.Process]$Process, [string]$LogPath, [string]$OutDir, [int]$BudgetSeconds, [double]$Threshold = 1.0)

    $window = Wait-RenderHwnd $Process ([Math]::Max(5, [int]($BudgetSeconds * 0.5)))
    Write-Host "  Renderer client: $($window.Width)x$($window.Height)"
    [C2NativeWindows]::SetForegroundWindow($window.Hwnd) | Out-Null
    # Let the first world frame land before judging it.
    Start-Sleep -Seconds 2
    $null = Assert-FrameNotBlack $window 'boot' $OutDir $Threshold

    $before = Wait-ForLogCount $LogPath 'Activate3DHardware' 1 10 $Process
    Write-Host "  Driving a renderer restart (minimize/restore)..."
    [C2NativeWindows]::ShowWindow($window.Hwnd, $SW_MINIMIZE) | Out-Null
    Start-Sleep -Milliseconds 1200
    [C2NativeWindows]::ShowWindow($window.Hwnd, $SW_RESTORE) | Out-Null
    [C2NativeWindows]::SetForegroundWindow($window.Hwnd) | Out-Null

    $after = Wait-ForLogCount $LogPath 'Activate3DHardware' ($before + 1) 20 $Process
    if ($after -lt 0) {
        # Do not degrade silently: a post-reload assertion that never ran
        # would report the boot result as if it covered the reload.
        throw "The renderer restart never ran (no second 'Activate3DHardware' in $LogPath within 20s), so the post-reload frame was never actually verified."
    }
    Write-Host "  Restart confirmed in log; settling..."
    Start-Sleep -Seconds 2
    $window = Wait-RenderHwnd $Process 10
    [C2NativeWindows]::SetForegroundWindow($window.Hwnd) | Out-Null
    $null = Assert-FrameNotBlack $window 'reload' $OutDir $Threshold
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

try {
    $timedOut = $false
    # The framebuffer assertions need the process alive and its window up, so
    # they run inside the lifetime window rather than after the kill. Their
    # cost comes out of the requested duration.
    if (-not $SkipFrameCheck) {
        if (-not $FrameDir) {
            $FrameDir = Join-Path $WorkingDir "smoke-frames"
        }
        New-Item -ItemType Directory -Path $FrameDir -Force | Out-Null
        Write-Host "Frame checks enabled (frames -> $FrameDir)"
        Invoke-FrameChecks $proc $LogFile $FrameDir ([Math]::Max(10, $Duration)) $MinMeanLuminance
    } else {
        Write-Host "Frame checks skipped (-SkipFrameCheck)"
    }

    while ($sw.Elapsed.TotalSeconds -lt $Duration) {
        Start-Sleep -Milliseconds 200
        if ($proc.HasExited) {
            break
        }
    }

    if (-not $proc.HasExited) {
        $timedOut = $true
        Write-Host "Time's up - killing process..."
        $proc.Kill()
        $null = $proc.WaitForExit(2000)
        Write-Host "Process terminated after the requested timeout"
    }

    if (-not $timedOut) {
        $elapsed = $sw.Elapsed.TotalSeconds.ToString('F1')
        $exitCode = $proc.ExitCode
        throw "Game exited early after ${elapsed}s (exit code: $exitCode)"
    }
}
finally {
    if ($proc -and -not $proc.HasExited) {
        try {
            $proc.Kill()
            $null = $proc.WaitForExit(2000)
        }
        catch [System.InvalidOperationException] {
            # The process can exit between HasExited and Kill.
        }
    }
}

# Check log for errors
if (Test-Path -LiteralPath $LogFile -PathType Leaf) {
    $logContent = Get-Content -LiteralPath $LogFile -Raw
    if ([string]::IsNullOrWhiteSpace($logContent)) {
        throw "Log file is empty: $LogFile"
    }
    $errorPattern = 'ABNORMAL_HALT|ERROR|FATAL|assertion'
    if ($logContent -match $errorPattern) {
        Write-Host "Found error markers in log:"
        $logContent | Select-String -Pattern $errorPattern | ForEach-Object { Write-Host "  $_" }
        throw "Smoke log contains error markers: $LogFile"
    }
    $logSize = (Get-Item $LogFile).Length
    Write-Host "  Log: $logSize bytes (no errors)"
} else {
    if ($AllowMissingLog) {
        Write-Warning "No log file found at $LogFile (allowed by -AllowMissingLog)"
    } else {
        throw "Required log file was not created: $LogFile"
    }
}

Write-Host "=== Smoke Test PASSED ==="
