<# Explicitly invoked asset-backed probe. Never launches as part of CTest. #>
param(
    [Parameter(Mandatory=$true)][string]$Fixture,
    [Parameter(Mandatory=$true)][string]$Evidence,
    [string]$GameDir = 'E:/Games/CarnivoresLegacy',
    [int]$ExpectedCount = 513,
    [switch]$ExpectOldFailure
)
$ErrorActionPreference = 'Stop'
if (Test-Path $Evidence) { throw 'Evidence directory must be new' }
if (Get-Process -Name v_gl,Carnivores1_GL,Carnivores2Menu -ErrorAction SilentlyContinue) {
    throw 'Close game/menu before probing; no process will be stopped'
}
New-Item -ItemType Directory -Path $Evidence | Out-Null
$Evidence = (Resolve-Path $Evidence).Path
$files = @(Get-ChildItem $GameDir -File)
$hashes = @{}
foreach ($file in $files) {
    $hashes[$file.Name] = (Get-FileHash $file.FullName).Hash
    Copy-Item $file.FullName (Join-Path $Evidence $file.Name)
}
$res = Join-Path $GameDir 'HUNTDAT/_RES.TXT'
Copy-Item $res (Join-Path $Evidence '_RES.original.txt')
$resHash = (Get-FileHash $res).Hash
$process = $null
try {
    Copy-Item $Fixture $res
    # Restored byte-for-byte below; the capture scheduler requires this opt-in.
    Add-Content -LiteralPath (Join-Path $GameDir 'config.cfg') -Value "`nglperf_logging 1" -Encoding ascii
    $output = Join-Path $Evidence 'capture.bmp'
    $arguments = @('reg=0','prj=huntdat\areas\area1','area=1','din=1','wep=1','dtm=1',
        '-windowed','-res=800x600','-capture-background','-capture-warmup-ms=250',
        "-capture-output=$output")
    $process = Start-Process -FilePath (Join-Path $GameDir 'v_gl.exe') -WorkingDirectory $GameDir `
        -ArgumentList $arguments -PassThru
    if (-not $process.WaitForExit(90000)) {
        throw "Probe PID $($process.Id) did not exit. Close it manually; backup is $Evidence"
    }
    $process.Refresh()
    foreach ($name in @('carnivor.log','render.log')) {
        $path = Join-Path $GameDir $name
        if (Test-Path $path) { Copy-Item $path (Join-Path $Evidence "probe-$name") }
    }
    $log = Get-Content (Join-Path $GameDir 'render.log') -Raw
    if ($ExpectOldFailure) {
        if ($log -notmatch 'characters capacity exceeded.*index=128') {
            throw 'Old executable did not reproduce the expected capacity error; inspect evidence'
        }
    } else {
        if ($process.ExitCode -ne 0 -or -not (Test-Path $output)) {
            throw "Game probe failed: exit=$($process.ExitCode), capture missing or incomplete"
        }
        if ($log -notmatch "Character roster finalized: $ExpectedCount definitions") {
            throw 'Game did not finalize the expected roster'
        }
        if ($ExpectedCount -gt 128 -and $log -notmatch "_RES.TXT character 'Roster probe [1-5][0-9][0-9]'") {
            throw 'No high-ID model load found in game log'
        }
    }
    "Probe PASS; exit=$($process.ExitCode); evidence=$Evidence"
} finally {
    if (-not $process -or $process.HasExited) {
        Copy-Item (Join-Path $Evidence '_RES.original.txt') $res
        if ((Get-FileHash $res).Hash -ne $resHash) { throw 'Script restoration hash mismatch' }
        foreach ($file in $files) {
            $target = Join-Path $GameDir $file.Name
            if (-not (Test-Path $target) -or (Get-FileHash $target).Hash -ne $hashes[$file.Name]) {
                Copy-Item (Join-Path $Evidence $file.Name) $target
            }
        }
        # Remove only new root output files generated during this owned probe.
        foreach ($new in Get-ChildItem $GameDir -File) {
            if (-not $hashes.ContainsKey($new.Name) -and
                $new.Name -match '^(carnivor\.log|render\.log|glperf.*\.(log|csv)|trophy00\.(sav|sab))$') {
                Copy-Item $new.FullName (Join-Path $Evidence "generated-$($new.Name)")
                Remove-Item -LiteralPath $new.FullName
            }
        }
        foreach ($file in $files) {
            if ((Get-FileHash (Join-Path $GameDir $file.Name)).Hash -ne $hashes[$file.Name]) {
                throw "Restoration mismatch: $($file.Name)"
            }
        }
        'Original script and all existing root files restored and hash-verified'
    } else {
        Write-Warning "Owned PID $($process.Id) still running; files not restored underneath it. Close it, then restore from $Evidence."
    }
}
