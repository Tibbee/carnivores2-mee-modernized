param(
    [string]$GameDir = "E:/Games/CarnivoresLegacy",
    [string]$OutDir  = "tools/artifacts/flashlight-pitch",
    [string]$ExtraArgs = "-flashlight"
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Windows.Forms, System.Drawing
. (Join-Path $PSScriptRoot 'c2_native_windows.ps1')

function Save-Frame([IntPtr]$Hwnd, [int]$W, [int]$H, [string]$Png) {
    $o = New-Object C2NativeWindows+POINT
    [void][C2NativeWindows]::ClientToScreen($Hwnd, [ref]$o)
    $b = New-Object System.Drawing.Bitmap $W, $H
    $g = [System.Drawing.Graphics]::FromImage($b)
    $g.CopyFromScreen($o.X, $o.Y, 0, 0, [System.Drawing.Size]::new($W, $H),
        [System.Drawing.CopyPixelOperation]::SourceCopy)
    $b.Save($Png, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $b.Dispose()
}
function Send-Key([IntPtr]$Hwnd, [int]$Vk, [int]$Scan) {
    [void][C2NativeWindows]::SetForegroundWindow($Hwnd)
    $d = [IntPtr](1 -bor ($Scan -shl 16))
    $u = [IntPtr](1 -bor ($Scan -shl 16) -bor (1 -shl 30) -bor (1 -shl 31))
    [void][C2NativeWindows]::PostMessage($Hwnd, 0x0100, [IntPtr]$Vk, $d)
    Start-Sleep -Milliseconds 100
    [void][C2NativeWindows]::PostMessage($Hwnd, 0x0101, [IntPtr]$Vk, $u)
    Start-Sleep -Seconds 2
}
function Region-Stats([string]$Png, [int]$W, [int]$H) {
    $img = [System.Drawing.Bitmap]::new($Png)
    $out = @{}
    $boxes = [ordered]@{
        'crosshair' = @([int]($W*0.44),[int]($W*0.56),[int]($H*0.44),[int]($H*0.56))
        'lower'     = @([int]($W*0.40),[int]($W*0.60),[int]($H*0.62),[int]($H*0.78))
        'bottom'    = @([int]($W*0.36),[int]($W*0.64),[int]($H*0.80),[int]($H*0.96))
    }
    foreach ($k in $boxes.Keys) {
        $r = $boxes[$k]; $sum = 0.0; $n = 0
        for ($y=$r[2]; $y -lt $r[3]; $y+=2) { for ($x=$r[0]; $x -lt $r[1]; $x+=2) {
            $c = $img.GetPixel($x,$y); $sum += 0.2126*$c.R + 0.7152*$c.G + 0.0722*$c.B; $n++ } }
        $out[$k] = $sum / $n
    }
    $img.Dispose()
    return $out
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$launch = "reg=0 prj=huntdat" + [char]92 + "areas" + [char]92 + "area1 area=1 din=1 wep=1 dtm=2"
if ($ExtraArgs -ne "") { $launch += " " + $ExtraArgs }
Write-Host "Launch: $launch"

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = Join-Path $GameDir "v_gl.exe"
$psi.Arguments = $launch
$psi.WorkingDirectory = $GameDir
$proc = [System.Diagnostics.Process]::Start($psi)

try {
    Start-Sleep -Seconds 9
    $hwnd = [C2NativeWindows]::FindHuntWindowForProcess($proc.Id)
    $rect = New-Object C2NativeWindows+RECT
    [void][C2NativeWindows]::GetClientRect($hwnd, [ref]$rect)
    $w = $rect.R - $rect.L; $h = $rect.B - $rect.T
    Write-Host "Client ${w}x${h}"
    $centre = New-Object C2NativeWindows+POINT
    $centre.X = [int]($w/2); $centre.Y = [int]($h/2)
    [void][C2NativeWindows]::ClientToScreen($hwnd, [ref]$centre)

    Start-Sleep -Seconds 2
    Save-Frame $hwnd $w $h (Join-Path $OutDir "level_off.png")
    Send-Key $hwnd 0x46 0x21
    Save-Frame $hwnd $w $h (Join-Path $OutDir "level_on.png")
    Write-Host "level captured"

    # Pitch down: the game reads the cursor delta each frame and resets it, so
    # park the pointer below centre repeatedly to accumulate PlayerBeta.
    [void][C2NativeWindows]::SetForegroundWindow($hwnd)
    for ($i = 0; $i -lt 90; $i++) {
        [void][C2NativeWindows]::SetCursorPos($centre.X, $centre.Y + 70)
        Start-Sleep -Milliseconds 16
    }
    Start-Sleep -Seconds 1
    Save-Frame $hwnd $w $h (Join-Path $OutDir "pitch_on.png")
    Write-Host "pitched-down ON captured"

    Send-Key $hwnd 0x46 0x21
    Save-Frame $hwnd $w $h (Join-Path $OutDir "pitch_off.png")
    Write-Host "pitched-down OFF captured"

    foreach ($pair in @(@('level_off','level_on'), @('pitch_off','pitch_on'))) {
        $a = Region-Stats (Join-Path $OutDir ($pair[0] + '.png')) $w $h
        $b = Region-Stats (Join-Path $OutDir ($pair[1] + '.png')) $w $h
        $label = if ($pair[0].StartsWith('level')) { 'LEVEL ' } else { 'PITCHED' }
        foreach ($k in $a.Keys) {
            "{0} {1,-10} off {2,6:N1}  on {3,6:N1}  delta {4,6:N2}" -f $label, $k, $a[$k], $b[$k], ($b[$k]-$a[$k])
        }
    }
} finally {
    if (-not $proc.HasExited) { $proc.Kill() }
}
