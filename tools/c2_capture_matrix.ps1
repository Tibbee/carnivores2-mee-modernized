<#
.SYNOPSIS
Capture reproducible C2 OpenGL performance scenarios.

.EXAMPLE
powershell -NoProfile -File tools/c2_capture_matrix.ps1 -Scenario area1_spawn -Resolution 1024x768
powershell -NoProfile -File tools/c2_capture_matrix.ps1 -Scenario area4_spawn -Resolution 2560x1440 -Repeats 5
powershell -NoProfile -File tools/c2_capture_matrix.ps1 -All -Repeats 3
#>
[CmdletBinding()]
param(
    [string]$Scenario = "area1_spawn",
    [string]$Resolution = "1024x768",
    [switch]$All,
    [ValidateRange(1, 20)][int]$Repeats = 1,
    [ValidateRange(1, 120)][int]$WarmupSeconds = 10,
    [ValidateRange(0, 30)][int]$PostCaptureSeconds = 2,
    [string]$GameDir = $env:CARNIVORES2_LEGACY_DIR,
    [string]$BuildExe = "",
    [string]$OutDir = "tools/artifacts/perf-current",
    [switch]$SkipBuild,
    [switch]$AllowDirtyBuildInputs,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"
$Invariant = [System.Globalization.CultureInfo]::InvariantCulture
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $GameDir) { $GameDir = "E:/Games/CarnivoresLegacy" }
$GameDir = (Resolve-Path -LiteralPath $GameDir).Path
if (-not [System.IO.Path]::IsPathRooted($OutDir)) { $OutDir = Join-Path $repoRoot $OutDir }
if (-not $BuildExe) { $BuildExe = Join-Path $repoRoot "build/ogl-release/bin/Carnivores1_GL.exe" }

$manifestPath = Join-Path $repoRoot "tools/c2_capture_manifest.json"
$manifest = Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
$deployedConfigPath = Join-Path $GameDir "config.cfg"
$configPath = $null # Resolved beside BuildExe after preflight; C2 loads config from the executable directory.
$trophyPath = Join-Path $GameDir "trophy00.sav"
$trophyAuxPath = Join-Path $GameDir "trophy00.sab"
$WM_CLOSE = 0x0010

function Get-Sha256 {
    param([Parameter(Mandatory=$true)][string]$Path)
    $stream = [System.IO.File]::OpenRead($Path)
    $algorithm = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = $algorithm.ComputeHash($stream)
        return ([BitConverter]::ToString($bytes)).Replace('-', '').ToLowerInvariant()
    } finally {
        $algorithm.Dispose()
        $stream.Dispose()
    }
}

function Save-FileState {
    param([string]$Path)
    $exists = Test-Path -LiteralPath $Path -PathType Leaf
    return [pscustomobject]@{
        Path = $Path
        Existed = $exists
        Bytes = if ($exists) { [System.IO.File]::ReadAllBytes($Path) } else { $null }
    }
}

function Restore-FileState {
    param($State)
    if ($State.Existed) {
        [System.IO.File]::WriteAllBytes($State.Path, $State.Bytes)
    } elseif (Test-Path -LiteralPath $State.Path) {
        Remove-Item -LiteralPath $State.Path -Force
    }
}

function Set-ConfigValues {
    param([string]$Path, [hashtable]$Values)

    $text = if (Test-Path -LiteralPath $Path) {
        [System.Text.Encoding]::ASCII.GetString([System.IO.File]::ReadAllBytes($Path)).Replace("`0", "")
    } else { "" }
    $lines = @($text -split "`r?`n")
    foreach ($key in $Values.Keys) {
        $found = $false
        for ($i = 0; $i -lt $lines.Count; $i++) {
            if ($lines[$i] -match ("^\s*" + [regex]::Escape($key) + "\s+")) {
                $lines[$i] = "$key $($Values[$key])"
                $found = $true
            }
        }
        if (-not $found) { $lines += "$key $($Values[$key])" }
    }
    $newText = (($lines -join "`r`n").TrimEnd("`r", "`n")) + "`r`n"
    [System.IO.File]::WriteAllText($Path, $newText, [System.Text.Encoding]::ASCII)
}

function Get-ConfigValues {
    param([string]$Path)
    $wanted = @('renderer','fov','object_detail','fps_limit','resolution','display_mode','gpufeatures','glperf_logging')
    $values = [ordered]@{}
    $text = [System.Text.Encoding]::ASCII.GetString([System.IO.File]::ReadAllBytes($Path)).Replace("`0", "")
    foreach ($line in ($text -split "`r?`n")) {
        if ($line -match '^\s*([^#;\s]+)\s+([^\s#;]+)') {
            $key = $Matches[1].ToLowerInvariant()
            if ($wanted -contains $key) { $values[$key] = $Matches[2] }
        }
    }
    return $values
}

function Set-TrophyViewOption {
    param([string]$Path, [int]$Value)
    # C2's guarded binary layout is TTrophyRoom[1516], seven int32 options,
    # then OptViewR at byte offset 1540 (Hunt/Game/Trophy.cpp).
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 1544) { throw "Trophy save is too short to set OptViewR: $Path" }
    [BitConverter]::GetBytes($Value).CopyTo($bytes, 1540)
    [System.IO.File]::WriteAllBytes($Path, $bytes)
}

function Get-TrophyViewOption {
    param([string]$Path)
    $bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 1544) { throw "Trophy save is too short to read OptViewR: $Path" }
    return [BitConverter]::ToInt32($bytes, 1540)
}

function Get-SourceSha {
    $sha = & git -C $repoRoot rev-parse HEAD 2>$null
    if ($LASTEXITCODE -ne 0 -or -not $sha) { throw "Could not resolve repository HEAD." }
    return $sha.Trim()
}

function Assert-CleanBuildInputs {
    $dirty = @(& git -C $repoRoot status --porcelain --untracked-files=all -- Hunt shaders deps CMakeLists.txt CMakePresets.json 2>$null)
    if ($LASTEXITCODE -ne 0) { throw "Could not inspect engine source status." }
    if ($dirty.Count -gt 0 -and -not $AllowDirtyBuildInputs) {
        throw "Engine/build inputs have changes; use -AllowDirtyBuildInputs only for diagnostic captures:`n  $($dirty -join "`n  ")"
    }
    if (@($dirty | Where-Object { $_ -match '^\?\?' }).Count) {
        throw 'Untracked build inputs cannot be preserved by the diagnostic patch; refusing capture.'
    }
    if ($dirty.Count -gt 0 -and $SkipBuild) { throw 'Dirty diagnostic captures require a build; omit -SkipBuild.' }
    $script:buildInputChanges = $dirty
}

function Assert-X86Pe {
    param([string]$Path)
    $stream = [System.IO.File]::OpenRead($Path)
    $reader = New-Object System.IO.BinaryReader($stream)
    try {
        $stream.Position = 0x3c
        $peOffset = $reader.ReadInt32()
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) { throw "Not a PE executable: $Path" }
        $machine = $reader.ReadUInt16()
        if ($machine -ne 0x014c) { throw ("Expected x86 PE machine 0x014c, got 0x{0:x4}: {1}" -f $machine, $Path) }
    } finally {
        $reader.Dispose()
        $stream.Dispose()
    }
}

function Assert-PerfBuild {
    $cache = Join-Path $repoRoot "build/ogl-release/CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cache -PathType Leaf)) { throw "Missing ogl-release CMake cache." }
    $cacheText = Get-Content -Raw -LiteralPath $cache
    if ($cacheText -notmatch '(?m)^GL_PERF_HOOKS:BOOL=ON\s*$') {
        throw "build/ogl-release does not have GL_PERF_HOOKS=ON."
    }
    Assert-X86Pe $BuildExe
}

function Assert-ShadersMatch {
    $buildShaders = Join-Path (Split-Path -Parent $BuildExe) "shaders"
    $gameShaders = Join-Path $GameDir "shaders"
    if (-not (Test-Path -LiteralPath $buildShaders -PathType Container)) { throw "Missing build shader directory: $buildShaders" }
    if (-not (Test-Path -LiteralPath $gameShaders -PathType Container)) { throw "Missing deployed shader directory: $gameShaders" }

    $buildFiles = @(Get-ChildItem -LiteralPath $buildShaders -File | Sort-Object Name)
    if ($buildFiles.Count -eq 0) { throw "Build shader directory is empty." }
    foreach ($source in $buildFiles) {
        $deployed = Join-Path $gameShaders $source.Name
        if (-not (Test-Path -LiteralPath $deployed -PathType Leaf)) { throw "Missing deployed shader: $deployed" }
        if ((Get-Sha256 $source.FullName) -ne (Get-Sha256 $deployed)) {
            throw "Deployed shader differs from ogl-release build: $($source.Name)"
        }
    }
}

function Get-GLPerfSnapshot {
    $snapshot = @{}
    Get-ChildItem -LiteralPath $GameDir -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like 'glperf-*.log' -or $_.Name -like 'glperf-capture-*.csv' } |
        ForEach-Object { $snapshot[$_.FullName] = '{0}:{1}' -f $_.LastWriteTimeUtc.Ticks, $_.Length }
    return $snapshot
}

function Get-ChangedGLPerfFiles {
    param($Before)
    $changed = @()
    Get-ChildItem -LiteralPath $GameDir -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like 'glperf-*.log' -or $_.Name -like 'glperf-capture-*.csv' } |
        ForEach-Object {
            $identity = '{0}:{1}' -f $_.LastWriteTimeUtc.Ticks, $_.Length
            if (-not $Before.ContainsKey($_.FullName) -or $Before[$_.FullName] -ne $identity) { $changed += $_ }
        }
    return $changed
}

Add-Type -AssemblyName System.Windows.Forms, System.Drawing
Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class C2CaptureNative {
    public delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr lParam);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
    [DllImport("user32.dll")] private static extern bool EnumWindows(EnumWindowsProc callback, IntPtr lParam);
    [DllImport("user32.dll")] private static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint processId);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] private static extern int GetClassName(IntPtr hwnd, StringBuilder className, int maxCount);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hwnd, out RECT rect);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hwnd, ref POINT point);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern void keybd_event(byte virtualKey, byte scanCode, uint flags, UIntPtr extraInfo);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr dpiContext);
    public static IntPtr FindHuntWindowForProcess(int processId) {
        IntPtr found = IntPtr.Zero;
        EnumWindows(delegate(IntPtr hwnd, IntPtr ignored) {
            uint owner; GetWindowThreadProcessId(hwnd, out owner);
            if (owner != (uint)processId) return true;
            StringBuilder name = new StringBuilder(128); GetClassName(hwnd, name, name.Capacity);
            if (name.ToString() != "HuntRenderWindow") return true;
            found = hwnd; return false;
        }, IntPtr.Zero);
        return found;
    }
}
"@
try { [C2CaptureNative]::SetProcessDpiAwarenessContext([IntPtr]::new(-4)) | Out-Null } catch { }

function Resolve-GameHwnd {
    param([System.Diagnostics.Process]$Process)
    # C2 keeps a small loading/progress window that may become
    # MainWindowHandle. Only the renderer's process-owned class is valid.
    return [C2CaptureNative]::FindHuntWindowForProcess($Process.Id)
}

function Wait-GameHwnd {
    param(
        [System.Diagnostics.Process]$Process,
        [int]$Width,
        [int]$Height,
        [int]$TimeoutSeconds = 120
    )
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    $lastSize = 'not created'
    while ($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        if ($Process.HasExited) { throw "Game exited before its renderer window became ready (exit $($Process.ExitCode))." }
        $hwnd = Resolve-GameHwnd $Process
        if ($hwnd -ne [IntPtr]::Zero) {
            $rect = New-Object C2CaptureNative+RECT
            if ([C2CaptureNative]::GetClientRect($hwnd, [ref]$rect)) {
                $actualWidth = $rect.R - $rect.L
                $actualHeight = $rect.B - $rect.T
                $lastSize = "${actualWidth}x${actualHeight}"
                if ($actualWidth -eq $Width -and $actualHeight -eq $Height) { return $hwnd }
            }
        }
        Start-Sleep -Milliseconds 250
    }
    throw "Process-owned HuntRenderWindow did not reach ${Width}x${Height} within $TimeoutSeconds seconds; last size was $lastSize."
}

function Assert-CaptureWindowSize {
    param([IntPtr]$Hwnd, [int]$Width, [int]$Height)
    [C2CaptureNative]::SetForegroundWindow($Hwnd) | Out-Null
    $rect = New-Object C2CaptureNative+RECT
    if (-not [C2CaptureNative]::GetClientRect($Hwnd, [ref]$rect)) { throw "GetClientRect failed." }
    $actualWidth = $rect.R - $rect.L
    $actualHeight = $rect.B - $rect.T
    if ($actualWidth -ne $Width -or $actualHeight -ne $Height) {
        throw "Requested ${Width}x${Height}, but the engine client is ${actualWidth}x${actualHeight}."
    }
}

function Assert-ReShadeDisabled {
    param([System.Diagnostics.Process]$Process)
    $Process.Refresh()
    $openglPath = $null
    foreach ($module in @($Process.Modules)) {
        $name = $module.ModuleName.ToLowerInvariant()
        $path = $module.FileName
        if ($name -eq 'opengl32.dll') { $openglPath = $path }
        if ($name -like 'reshade*' -or (($name -eq 'opengl32.dll' -or $name -eq 'dxgi.dll') -and $path.StartsWith($GameDir, [System.StringComparison]::OrdinalIgnoreCase))) {
            throw "ReShade or a local graphics wrapper is loaded: $path"
        }
    }
    # Some 32-bit proxy/OpenGL combinations are omitted by Process.Modules.
    # The local wrapper is removed before process creation, so a missing
    # module-path observation is recorded but is not treated as proof by itself.
    return $openglPath
}

function Trigger-GLPerfCapture {
    param([IntPtr]$Hwnd)
    [C2CaptureNative]::SetForegroundWindow($Hwnd) | Out-Null
    Start-Sleep -Milliseconds 150
    [C2CaptureNative]::keybd_event(0x7A, 0, 0, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds 350
    [C2CaptureNative]::keybd_event(0x7A, 0, 2, [UIntPtr]::Zero)
}

function Wait-GLPerfCapture {
    param($Before, [System.Diagnostics.Process]$Process, [int]$TimeoutSeconds = 30)
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    while ($timer.Elapsed.TotalSeconds -lt $TimeoutSeconds) {
        if ($Process.HasExited) { throw "Game exited while waiting for the GLPerf CSV." }
        $csv = @(Get-ChangedGLPerfFiles $Before | Where-Object { $_.Name -like 'glperf-capture-*.csv' } |
            Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1)
        if ($csv.Count -gt 0) {
            try {
                $lineCount = [System.IO.File]::ReadAllLines($csv[0].FullName).Length
                if ($lineCount -ge 121) { return $csv[0] }
            } catch { }
        }
        Start-Sleep -Milliseconds 200
    }
    throw "A complete 120-frame GLPerf CSV was not produced within $TimeoutSeconds seconds."
}

function Capture-GameClientPng {
    param([IntPtr]$Hwnd, [string]$OutPng, [int]$Width, [int]$Height)
    $origin = New-Object C2CaptureNative+POINT
    if (-not [C2CaptureNative]::ClientToScreen($Hwnd, [ref]$origin)) { throw "ClientToScreen failed." }
    $screen = [System.Windows.Forms.SystemInformation]::VirtualScreen
    if ($origin.X -lt $screen.Left -or $origin.Y -lt $screen.Top -or
        ($origin.X + $Width) -gt $screen.Right -or ($origin.Y + $Height) -gt $screen.Bottom) {
        throw "The ${Width}x${Height} client is not fully visible on the virtual desktop; screenshot would be clipped."
    }
    $bitmap = New-Object System.Drawing.Bitmap $Width, $Height
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.CopyFromScreen($origin.X, $origin.Y, 0, 0,
            [System.Drawing.Size]::new($Width, $Height),
            [System.Drawing.CopyPixelOperation]::SourceCopy)
        $bitmap.Save($OutPng, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $graphics.Dispose(); $bitmap.Dispose()
    }
}

function Stop-CaptureProcess {
    param([System.Diagnostics.Process]$Process)
    if (-not $Process -or $Process.HasExited) { return }
    if (-not $Process.CloseMainWindow()) {
        $hwnd = Resolve-GameHwnd $Process
        if ($hwnd -ne [IntPtr]::Zero) { [C2CaptureNative]::PostMessage($hwnd, $WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null }
    }
    if (-not $Process.WaitForExit(10000)) {
        Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
        throw "Game did not exit through WM_CLOSE within ten seconds."
    }
}

function Get-Percentile {
    param([double[]]$Values, [double]$Percentile)
    $sorted = @($Values | Sort-Object)
    if ($sorted.Count -eq 0) { return $null }
    $index = [Math]::Ceiling($Percentile * $sorted.Count) - 1
    $index = [Math]::Max(0, [Math]::Min($sorted.Count - 1, $index))
    return [double]$sorted[$index]
}

function Test-AndSummarizeCsv {
    param([string]$Path)
    $header = ([System.IO.File]::ReadLines($Path) | Select-Object -First 1) -split ','
    foreach ($required in @('frame','cpu_ms','gpu_ms','draw_calls','triangles','state_changes','texture_binds','texture_switches','unique_textures')) {
        if ($header -notcontains $required) { throw "GLPerf CSV is missing column '$required'." }
    }
    $rows = @(Import-Csv -LiteralPath $Path)
    if ($rows.Count -ne 120) { throw "Expected 120 CSV rows, found $($rows.Count)." }
    for ($i = 0; $i -lt $rows.Count; $i++) {
        if ([int]$rows[$i].frame -ne $i) { throw "CSV frame sequence is invalid at row $i." }
    }
    foreach ($column in @($header | Where-Object { $_ -eq 'gpu_ms' -or $_ -like '*_gpu_ms' })) {
        foreach ($row in $rows) {
            if ([double]::Parse($row.$column, $Invariant) -ne -1.0) { throw "CSV GPU sentinel changed in column '$column'." }
        }
    }
    $cpu = [double[]]@($rows | ForEach-Object { [double]::Parse($_.cpu_ms, $Invariant) })
    $scopes = [ordered]@{}
    foreach ($column in @($header | Where-Object { $_ -like '*_cpu_ms' })) {
        $values = [double[]]@($rows | ForEach-Object { [double]::Parse($_.$column, $Invariant) })
        $scopes[$column.Substring(0, $column.Length - 7)] = [ordered]@{
            mean_ms = [Math]::Round(($values | Measure-Object -Average).Average, 4)
            p95_ms = [Math]::Round((Get-Percentile $values 0.95), 4)
            max_ms = [Math]::Round(($values | Measure-Object -Maximum).Maximum, 4)
        }
    }
    $counters = [ordered]@{}
    foreach ($column in @('draw_calls','triangles','state_changes','texture_binds','texture_switches','unique_textures')) {
        $values = [double[]]@($rows | ForEach-Object { [double]::Parse($_.$column, $Invariant) })
        $counters[$column] = [Math]::Round(($values | Measure-Object -Average).Average, 3)
    }
    return [ordered]@{
        frames = $rows.Count
        cpu_ms = [ordered]@{
            mean = [Math]::Round(($cpu | Measure-Object -Average).Average, 4)
            median = [Math]::Round((Get-Percentile $cpu 0.50), 4)
            p95 = [Math]::Round((Get-Percentile $cpu 0.95), 4)
            p99 = [Math]::Round((Get-Percentile $cpu 0.99), 4)
            min = [Math]::Round(($cpu | Measure-Object -Minimum).Minimum, 4)
            max = [Math]::Round(($cpu | Measure-Object -Maximum).Maximum, 4)
        }
        counters_mean = $counters
        scopes = $scopes
        note = "GLPerf CPU excludes SwapBuffers/presentation wait. CSV GPU fields are -1 by design; use glperf.log for resolved GPU timings."
    }
}

function Assert-GpuLog {
    param([string]$Path)
    $text = Get-Content -Raw -LiteralPath $Path
    $drops = @([regex]::Matches($text, 'gpu_drop=(\d+)') | ForEach-Object { [int]$_.Groups[1].Value })
    if ($drops.Count -eq 0) { throw "GLPerf log contains no GPU drop counters." }
    if (($drops | Measure-Object -Maximum).Maximum -gt 0) { throw "GLPerf log reports dropped GPU samples." }
    $samples = @([regex]::Matches($text, 'gpu_n=(\d+)') | ForEach-Object { [int]$_.Groups[1].Value })
    if ($samples.Count -eq 0 -or ($samples | Measure-Object -Maximum).Maximum -le 0) { throw "GLPerf log contains no resolved GPU samples." }
}

function Get-SystemMetadata {
    $cpu = @(Get-CimInstance Win32_Processor -ErrorAction SilentlyContinue | Select-Object -First 1)
    $gpu = @(Get-CimInstance Win32_VideoController -ErrorAction SilentlyContinue | Where-Object { $_.Name -notmatch 'Virtual|Remote' } | Select-Object -First 1)
    $os = @(Get-CimInstance Win32_OperatingSystem -ErrorAction SilentlyContinue | Select-Object -First 1)
    $screen = [System.Windows.Forms.Screen]::PrimaryScreen
    return [ordered]@{
        cpu = if ($cpu.Count) { $cpu[0].Name.Trim() } else { $null }
        gpu = if ($gpu.Count) { $gpu[0].Name.Trim() } else { $null }
        gpu_driver = if ($gpu.Count) { $gpu[0].DriverVersion } else { $null }
        reported_refresh_hz = if ($gpu.Count) { $gpu[0].CurrentRefreshRate } else { $null }
        os = if ($os.Count) { "$($os[0].Caption) $($os[0].Version) build $($os[0].BuildNumber)" } else { $null }
        primary_display = "$($screen.Bounds.Width)x$($screen.Bounds.Height)"
        owner_display_note = "2560x1440, 180 Hz variable-refresh FreeSync display; normal gameplay limit 90 or 120 FPS"
    }
}

function Get-ScenarioAssetMetadata {
    param($ScenarioEntry)
    $result = [ordered]@{}
    if (-not $ScenarioEntry.project) { return $result }
    $relative = ([string]$ScenarioEntry.project).Replace('/', '\')
    foreach ($extension in @('.prj','.map','.rsc')) {
        $path = Join-Path $GameDir ($relative + $extension)
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing scenario asset: $path" }
        $result[$extension.TrimStart('.')] = [ordered]@{ path = $path; sha256 = Get-Sha256 $path }
    }
    return $result
}

function Run-Capture {
    param($ScenarioEntry, $ResolutionEntry, [int]$RepeatIndex)
    $resolutionId = [string]$ResolutionEntry.id
    $parts = $resolutionId -split 'x'
    if ($parts.Count -ne 2) { throw "Invalid resolution '$resolutionId'." }
    $width = [int]$parts[0]; $height = [int]$parts[1]
    $mode = [string]$ResolutionEntry.mode
    $displayMode = if ($mode -eq 'borderless') { 2 } else { 0 }

    $arguments = @()
    if ($ScenarioEntry.project) { $arguments += "prj=$($ScenarioEntry.project)" }
    foreach ($argument in @($ScenarioEntry.extra_args)) { $arguments += [string]$argument }
    $arguments += if ($mode -eq 'borderless') { '-borderless' } else { '-windowed' }
    $arguments += "-res=$resolutionId"

    $baseName = "$($ScenarioEntry.id)__${resolutionId}__r$('{0:d2}' -f $RepeatIndex)"
    $runDir = Join-Path $sessionDir $baseName
    New-Item -ItemType Directory -Path $runDir -Force | Out-Null
    $png = Join-Path $runDir "frame.png"
    $metaPath = Join-Path $runDir "metadata.json"
    $summaryPath = Join-Path $runDir "summary.json"
    $failurePath = Join-Path $runDir "failure.txt"

    if ($DryRun) {
        Write-Host "[dry-run] $baseName :: $BuildExe $($arguments -join ' ')"
        return
    }

    $beforeGLPerf = Get-GLPerfSnapshot
    $localOpenGLWrapper = Join-Path $GameDir 'opengl32.dll'
    $states = @(
        (Save-FileState $configPath)
        (Save-FileState $trophyPath)
        (Save-FileState $trophyAuxPath)
        (Save-FileState (Join-Path $GameDir 'render.log'))
        (Save-FileState (Join-Path $GameDir 'carnivor.log'))
        (Save-FileState $localOpenGLWrapper)
    )
    $reshadeWrapperHash = if ($states[5].Existed) { Get-Sha256 $localOpenGLWrapper } else { $null }
    $process = $null
    $runError = $null
    $openglModule = $null
    $effectiveConfigHash = $null
    $effectiveTrophyHash = $null
    $effectiveConfig = $null
    $assets = $null

    try {
        # Seed the executable-local config from the owner's deployed settings,
        # then override only the controlled measurement keys.
        [System.IO.File]::WriteAllBytes($configPath, [System.IO.File]::ReadAllBytes($deployedConfigPath))
        Set-ConfigValues $configPath @{
            renderer = 1
            fov = [int]$manifest.settings.fov
            object_detail = [int]$manifest.settings.object_detail
            fps_limit = [int]$manifest.settings.fps_limit
            resolution = $resolutionId
            display_mode = $displayMode
            gpufeatures = [string]$manifest.settings.gpufeatures
            glperf_logging = 1
        }
        Set-TrophyViewOption $trophyPath ([int]$manifest.settings.view_option)
        # The deployed opengl32.dll is ReShade's proxy. Removing it only for
        # this run forces the system OpenGL ICD; the exact original bytes are
        # restored in finally.
        Remove-Item -LiteralPath $localOpenGLWrapper -Force -ErrorAction SilentlyContinue
        Remove-Item -LiteralPath (Join-Path $GameDir 'render.log'), (Join-Path $GameDir 'carnivor.log') -Force -ErrorAction SilentlyContinue
        $effectiveConfigHash = Get-Sha256 $configPath
        $effectiveTrophyHash = Get-Sha256 $trophyPath
        $effectiveConfig = Get-ConfigValues $configPath
        $assets = Get-ScenarioAssetMetadata $ScenarioEntry

        $process = Start-Process -FilePath $BuildExe -ArgumentList $arguments -WorkingDirectory $GameDir -PassThru -WindowStyle Normal
        Write-Host "[$baseName] pid=$($process.Id); waiting for renderer readiness"
        $hwnd = Wait-GameHwnd $process $width $height
        Write-Host "[$baseName] renderer ready; warming for $WarmupSeconds s"
        Start-Sleep -Seconds $WarmupSeconds
        if ($process.HasExited) { throw "Game exited during warm-up (exit $($process.ExitCode))." }
        Assert-CaptureWindowSize $hwnd $width $height
        $openglModule = Assert-ReShadeDisabled $process

        Trigger-GLPerfCapture $hwnd
        $null = Wait-GLPerfCapture $beforeGLPerf $process
        if ($PostCaptureSeconds -gt 0) { Start-Sleep -Seconds $PostCaptureSeconds }
        Capture-GameClientPng $hwnd $png $width $height
        Stop-CaptureProcess $process

        $changed = @(Get-ChangedGLPerfFiles $beforeGLPerf)
        $allLogs = @($changed | Where-Object { $_.Name -like 'glperf-*.log' })
        $csvs = @($changed | Where-Object { $_.Name -like 'glperf-capture-*.csv' })
        # A slow area load can initialize GLPerf twice and both logs may
        # accumulate rolling frames. The latest log belongs to the renderer
        # window that reached its final requested size and produced the CSV;
        # preserve any earlier initialization logs separately.
        $logs = @($allLogs | Sort-Object Name -Descending | Select-Object -First 1)
        if ($allLogs.Count -lt 1 -or $logs.Count -ne 1 -or $csvs.Count -ne 1) {
            throw "Expected at least one attributable GLPerf log and exactly one CSV; found total logs=$($allLogs.Count), selected logs=$($logs.Count), csvs=$($csvs.Count)."
        }
        $logCopy = Join-Path $runDir 'glperf.log'
        $csvCopy = Join-Path $runDir 'capture.csv'
        Copy-Item -LiteralPath $logs[0].FullName -Destination $logCopy
        Copy-Item -LiteralPath $csvs[0].FullName -Destination $csvCopy
        $startupLogCopies = @()
        $startupIndex = 0
        foreach ($startupLog in @($allLogs | Where-Object { $_.FullName -ne $logs[0].FullName })) {
            $startupIndex++
            $startupCopy = Join-Path $runDir ('glperf-startup-{0:D2}.log' -f $startupIndex)
            Copy-Item -LiteralPath $startupLog.FullName -Destination $startupCopy
            $startupLogCopies += $startupCopy
        }
        foreach ($runtimeName in @('render.log','carnivor.log')) {
            $runtimePath = Join-Path $GameDir $runtimeName
            if (Test-Path -LiteralPath $runtimePath -PathType Leaf) { Copy-Item -LiteralPath $runtimePath -Destination (Join-Path $runDir $runtimeName) }
        }

        $runtimeText = @('render.log','carnivor.log') | ForEach-Object {
            $p = Join-Path $runDir $_
            if (Test-Path -LiteralPath $p) { Get-Content -Raw -LiteralPath $p }
        }
        $runtimeText = $runtimeText -join "`n"
        if ($runtimeText -match 'ABNORMAL_HALT') { throw "Runtime logs report ABNORMAL_HALT." }
        if ($runtimeText -notmatch 'Game normal shutdown') { throw "Runtime logs do not confirm normal shutdown." }

        Assert-GpuLog $logCopy
        $summary = Test-AndSummarizeCsv $csvCopy
        $summary | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $summaryPath -Encoding UTF8

        $artifacts = [ordered]@{}
        foreach ($artifact in @($png,$logCopy,$csvCopy,$summaryPath,(Join-Path $runDir 'render.log'),(Join-Path $runDir 'carnivor.log')) + $startupLogCopies) {
            if (Test-Path -LiteralPath $artifact -PathType Leaf) {
                $artifacts[[System.IO.Path]::GetFileName($artifact)] = [ordered]@{
                    sha256 = Get-Sha256 $artifact
                    bytes = (Get-Item -LiteralPath $artifact).Length
                }
            }
        }

        $metadata = [ordered]@{
            schema_version = 1
            capture_timestamp_utc = (Get-Date).ToUniversalTime().ToString('o')
            build_sha = $sourceSha
            build_inputs_dirty = ($buildInputChanges.Count -gt 0)
            build_input_changes = $buildInputChanges
            build_input_diff_sha256 = $sourceDiffHash
            build_exe = $BuildExe
            build_exe_sha256 = $buildExeHash
            build_preset = $manifest.build_preset
            scenario = $ScenarioEntry.id
            scenario_name = $ScenarioEntry.name
            project = $ScenarioEntry.project
            repeat = $RepeatIndex
            resolution = $resolutionId
            aspect = $ResolutionEntry.aspect
            window_mode = $mode
            game_args = $arguments
            landing_tile = $ScenarioEntry.landing_tile
            camera = $ScenarioEntry.camera
            settings = [ordered]@{
                fov = [int]$manifest.settings.fov
                object_detail = [int]$manifest.settings.object_detail
                view_option = Get-TrophyViewOption $trophyPath
                fps_limit = [int]$manifest.settings.fps_limit
                gpufeatures = [string]$manifest.settings.gpufeatures
                reshade = $false
            }
            config_source = $deployedConfigPath
            config_source_sha256 = Get-Sha256 $deployedConfigPath
            config_effective = $effectiveConfig
            config_effective_path = $configPath
            config_effective_sha256 = $effectiveConfigHash
            reshade_wrapper_removed_for_run = $states[5].Existed
            reshade_wrapper_sha256 = $reshadeWrapperHash
            trophy_effective_sha256 = $effectiveTrophyHash
            executable_opengl_module = $openglModule
            scenario_assets = $assets
            system = $systemMetadata
            artifacts = $artifacts
            notes = $ScenarioEntry.notes
        }
        $metadata | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $metaPath -Encoding UTF8
        Remove-Item -LiteralPath @($changed.FullName) -Force
        Write-Host "[$baseName] complete -> $runDir"
    } catch {
        $runError = $_
        $_ | Out-String | Set-Content -LiteralPath $failurePath -Encoding UTF8
        foreach ($runtimeName in @('render.log','carnivor.log')) {
            $runtimePath = Join-Path $GameDir $runtimeName
            if (Test-Path -LiteralPath $runtimePath -PathType Leaf) { Copy-Item -LiteralPath $runtimePath -Destination (Join-Path $runDir $runtimeName) -Force }
        }
        # Preserve and remove only files created or changed by this failed run;
        # pre-existing GLPerf evidence from another session remains untouched.
        $failureGLPerf = @(Get-ChangedGLPerfFiles $beforeGLPerf)
        foreach ($failureArtifact in $failureGLPerf) {
            Copy-Item -LiteralPath $failureArtifact.FullName -Destination (Join-Path $runDir ("failure-" + $failureArtifact.Name)) -Force
        }
        if ($failureGLPerf.Count) {
            Remove-Item -LiteralPath @($failureGLPerf.FullName) -Force
        }
    } finally {
        try { Stop-CaptureProcess $process } catch { if (-not $runError) { $runError = $_ } }
        foreach ($state in $states) { Restore-FileState $state }
    }
    if ($runError) { throw $runError }
}

$required = @($manifestPath, $deployedConfigPath, $trophyPath, (Join-Path $GameDir 'HUNTDAT/_MENU.TXT'), (Join-Path $GameDir 'HUNTDAT/_RES.TXT'))
$missing = @($required | Where-Object { -not (Test-Path -LiteralPath $_ -PathType Leaf) })
if ($missing.Count) { throw "Missing capture prerequisites:`n  $($missing -join "`n  ")" }
Assert-CleanBuildInputs
if (-not $SkipBuild) {
    & cmake --build --preset ogl-release
    if ($LASTEXITCODE -ne 0) { throw "ogl-release build failed." }
}
$BuildExe = (Resolve-Path -LiteralPath $BuildExe).Path
$configPath = Join-Path (Split-Path -Parent $BuildExe) 'config.cfg'
Assert-PerfBuild
Assert-ShadersMatch
$sourceSha = Get-SourceSha
$buildExeHash = Get-Sha256 $BuildExe
$systemMetadata = Get-SystemMetadata
$sessionStamp = (Get-Date).ToUniversalTime().ToString('yyyyMMdd-HHmmssZ')
$sessionDir = Join-Path $OutDir ("session-{0}-{1}" -f $sessionStamp, $sourceSha.Substring(0, 7))
New-Item -ItemType Directory -Path $sessionDir -Force | Out-Null
Copy-Item -LiteralPath $manifestPath -Destination (Join-Path $sessionDir 'manifest.json')
$sourceDiffHash = $null
if ($buildInputChanges.Count -gt 0) {
    $patchPath = Join-Path $sessionDir 'build-input-changes.diff'
    & git -C $repoRoot diff --binary --output=$patchPath HEAD -- Hunt shaders deps CMakeLists.txt CMakePresets.json
    if ($LASTEXITCODE -ne 0) { throw 'Could not preserve diagnostic source patch.' }
    $sourceDiffHash = Get-Sha256 $patchPath
}

if ($All) {
    $selectedScenarios = @($manifest.scenarios | Where-Object { $_.ready })
    $selectedResolutions = @($manifest.resolutions | Where-Object { $_.ready })
} else {
    $selectedScenarios = @($manifest.scenarios | Where-Object { $_.id -eq $Scenario } | Select-Object -First 1)
    if ($selectedScenarios.Count -eq 0) { throw "Unknown scenario '$Scenario'." }
    if (-not $selectedScenarios[0].ready) { throw "Scenario '$Scenario' is deferred." }
    $selectedResolutions = @($manifest.resolutions | Where-Object { $_.id -eq $Resolution } | Select-Object -First 1)
    if ($selectedResolutions.Count -eq 0) { throw "Unknown resolution '$Resolution'." }
    if (-not $selectedResolutions[0].ready) { throw "Resolution '$Resolution' is deferred." }
}

Write-Host "C2 capture session: $sessionDir"
Write-Host "Build: $sourceSha ($buildExeHash)"
Write-Host "Settings: ReShade off, FPS unlimited, FOV $($manifest.settings.fov), object detail $($manifest.settings.object_detail), view option $($manifest.settings.view_option)"
foreach ($scenarioEntry in $selectedScenarios) {
    foreach ($resolutionEntry in $selectedResolutions) {
        for ($repeat = 1; $repeat -le $Repeats; $repeat++) {
            Run-Capture $scenarioEntry $resolutionEntry $repeat
        }
    }
}
Write-Host "Capture session complete: $sessionDir"
