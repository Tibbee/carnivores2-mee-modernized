# Shared prologue and report for the asset checkers (check_huntdat.ps1 and
# check_mod_assets.ps1). Dot-source it after param(): it runs in the caller's
# scope, so $errors/$warnings/$notes, $GameDir and $huntdat stay script-local
# and every existing collection call site keeps working.
$ErrorActionPreference = 'Stop'

$errors   = New-Object System.Collections.Generic.List[string]
$warnings = New-Object System.Collections.Generic.List[string]
$notes    = New-Object System.Collections.Generic.List[string]

function Test-GameFile {
    param([string]$Path)
    # Literal, wildcard-free check: data-supplied names may contain [ ] which
    # Test-Path would treat as a pattern.
    return [System.IO.File]::Exists($Path)
}

function Resolve-GameRoot {
    # Resolves the caller's game folder and exposes the HUNTDAT root. Exits 1
    # with a support-friendly message when either is missing, exactly as the
    # two checkers used to do inline.
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        Write-Output "Game folder not found: $Path"
        exit 1
    }
    $script:GameDir = (Resolve-Path -LiteralPath $Path).Path
    $script:huntdat = Join-Path $script:GameDir 'HUNTDAT'

    if (-not (Test-Path -LiteralPath $script:huntdat -PathType Container)) {
        Write-Output "No HUNTDAT folder in: $($script:GameDir)"
        Write-Output "Run this from the game folder (the one holding the .ren/.exe files)."
        exit 1
    }
}

function Write-CheckReport {
    # The shared report shape: detail lines, then ERRORS/WARNINGS/NOTES
    # sections, then the summary and the -Strict exit rule.
    param([string]$Title, [string[]]$Details, [switch]$Strict, [switch]$WithNotes)

    Write-Output $Title
    foreach ($detail in $Details) { Write-Output "  $detail" }

    foreach ($section in @(
            @{ Label = 'ERRORS'; Entries = $errors }
            @{ Label = 'WARNINGS'; Entries = $warnings }
            @{ Label = 'NOTES'; Entries = $notes })) {
        if ($section.Entries.Count -gt 0) {
            Write-Output ""
            Write-Output "$($section.Label) ($($section.Entries.Count))"
            foreach ($entry in $section.Entries) { Write-Output "  $entry" }
        }
    }
    if ($errors.Count -eq 0 -and $warnings.Count -eq 0 -and $notes.Count -eq 0) {
        Write-Output "  No problems found."
    }

    Write-Output ""
    $summary = "$($errors.Count) error(s), $($warnings.Count) warning(s)"
    if ($WithNotes) { $summary += ", $($notes.Count) note(s)" }
    Write-Output $summary

    if ($errors.Count -gt 0 -or ($Strict -and $warnings.Count -gt 0)) { exit 1 }
    exit 0
}
