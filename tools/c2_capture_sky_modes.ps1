# Sky mode evidence: capture one software frame per mode at a fixed viewpoint.
# Uses the capture matrix's scenario/resolution so the frames are comparable.
param(
    [string]$GameDir = $env:CARNIVORES2_LEGACY_DIR,
    [ValidateSet("gl", "soft")][string]$Renderer = "soft",
    [string]$Resolution = "1024x768",
    [int]$WarmupSeconds = 6,
    [string]$OutDir = "tools/artifacts/sky-current",
    [int]$HorizonDrop = 0
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $GameDir) { $GameDir = "E:/Games/CarnivoresLegacy" }
if (-not [System.IO.Path]::IsPathRooted($OutDir)) { $OutDir = Join-Path $repoRoot $OutDir }
$matrix = Join-Path $repoRoot "tools/c2_capture_matrix.ps1"

# sky_mode values to capture. The dome mode is OpenGL-only. The software
# renderer reads sky_mode_soft, so the key follows the renderer.
$modes = if ($Renderer -eq "gl") { @(0, 1, 2) } else { @(0, 1) }
$skyKey = if ($Renderer -eq "gl") { "sky_mode" } else { "sky_mode_soft" }
$staging = Join-Path $OutDir ("staging-" + $Renderer)
New-Item -ItemType Directory -Path $staging -Force | Out-Null

$configPath = Join-Path $GameDir "config.cfg"
$backup = [System.IO.File]::ReadAllBytes($configPath)
try {
    foreach ($mode in $modes) {
        Write-Host "=== sky_mode $mode ($Renderer) ==="
        # Seed sky_mode into the deployed config the matrix copies from.
        $text = [System.Text.Encoding]::ASCII.GetString($backup).Replace("`0", "")
        $lines = @($text -split "`r?`n")
        $found = $false
        for ($i = 0; $i -lt $lines.Count; $i++) {
            if ($lines[$i] -match "^\s*$skyKey\s+") { $lines[$i] = "$skyKey $mode"; $found = $true }
        }
        if (-not $found) { $lines += "$skyKey $mode" }
        if ($HorizonDrop -gt 0) {
            $foundDrop = $false
            for ($i = 0; $i -lt $lines.Count; $i++) {
                if ($lines[$i] -match "^\s*sky_horizon_drop\s+") { $lines[$i] = "sky_horizon_drop $HorizonDrop"; $foundDrop = $true }
            }
            if (-not $foundDrop) { $lines += "sky_horizon_drop $HorizonDrop" }
        }
        [System.IO.File]::WriteAllText($configPath, (($lines -join "`r`n").TrimEnd("`r","`n") + "`r`n"), [System.Text.Encoding]::ASCII)

        $tag = if ($HorizonDrop -gt 0) { "sky_mode$mode" + "drop$HorizonDrop" } else { "sky_mode$mode" }
        $runOut = Join-Path $OutDir $tag
        # -Foreground is required: unattended software runs are rejected by the
        # matrix, and the desktop-pixel capture is what the modes compare.
        & $matrix -Renderer $Renderer -Scenario area1_spawn -Resolution $Resolution `
            -WarmupSeconds $WarmupSeconds -PostCaptureSeconds 1 -Foreground -OutDir $runOut `
            -AllowDirtyBuildInputs | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "Capture failed for $tag" }
        $frame = Get-ChildItem -LiteralPath $runOut -Recurse -Filter "frame.png" |
            Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
        if (-not $frame) { throw "No frame.png produced for $tag" }
        $dest = Join-Path $staging ("{0}_{1}.png" -f $tag, $Renderer)
        Copy-Item -LiteralPath $frame.FullName -Destination $dest -Force
        Write-Host "  -> $dest"
    }
} finally {
    [System.IO.File]::WriteAllBytes($configPath, $backup)
    Write-Host "Restored $configPath"
}
Write-Host "Done: $staging"
