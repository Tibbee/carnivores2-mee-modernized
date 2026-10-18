<#
.SYNOPSIS
Checks an MSVC build log against the checked-in warning baseline for a preset.

.DESCRIPTION
The legacy tree is not clean enough for a global /WX. This gate keeps the
measured baseline instead: it fails when a project-owned source file gains a
warning code it did not have before, or when the number of unique warning
lines for an existing file/code pair increases. Fixing warnings is always
allowed; the baseline can be tightened later.

Warnings are deduplicated by file + line + code, so a header included by many
translation units counts once per warning line. The baseline stores unique-line
counts per file + code, which tolerates line-number shifts and changes in the
number of translation units that include a header.

.PARAMETER Log
MSVC build log captured from `cmake --build`.

.PARAMETER Preset
CMake preset name, used for tools/warning-baseline/<Preset>.txt.

.PARAMETER BaselineDir
Optional baseline directory override (used by tests and initial setup).

.PARAMETER UpdateBaseline
Rewrite the baseline from the supplied log instead of checking it. Review the
diff before committing: this is the only way the gate can accept new warnings.

.PARAMETER RepoRoot
Optional repository root override. Defaults to the parent of tools/.
#>
param(
    [Parameter(Mandatory = $true)]
    [string]$Log,

    [Parameter(Mandatory = $true)]
    [string]$Preset,

    [string]$BaselineDir = '',

    [switch]$UpdateBaseline,

    [string]$RepoRoot = ''
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = Split-Path $PSScriptRoot -Parent
}
$repoPath = (Resolve-Path -LiteralPath $RepoRoot -ErrorAction Stop).Path.TrimEnd([char[]]@('\', '/'))
$logPath = (Resolve-Path -LiteralPath $Log -ErrorAction Stop).Path

if ([string]::IsNullOrWhiteSpace($BaselineDir)) {
    $BaselineDir = Join-Path $repoPath 'tools\warning-baseline'
}
$baselinePath = Join-Path $BaselineDir "$Preset.txt"

# Parse project-owned warnings from the log. Compiler diagnostics use
# path(line[,column]): warning Cxxxx: message. Some command-line diagnostics
# have no line number; keep them grouped under line 0.
$sites = @{}
$messages = @{}
$ansi = [char]27 + '\[[0-9;]*m'
$linePattern = '^(?<file>.*?)\((?<line>\d+)(?:,\d+)?\)\s*:\s*warning\s+(?<code>C\d+)\s*:\s*(?<message>.*)$'
$barePattern = '^(?<file>.*?)\s*:\s*warning\s+(?<code>C\d+)\s*:\s*(?<message>.*)$'

foreach ($raw in Get-Content -LiteralPath $logPath) {
    $line = $raw -replace $ansi, ''
    $match = [regex]::Match($line, $linePattern)
    if (-not $match.Success) {
        $match = [regex]::Match($line, $barePattern)
    }
    if (-not $match.Success) {
        continue
    }

    $file = $match.Groups['file'].Value.Trim().Trim('"')
    if ([string]::IsNullOrWhiteSpace($file)) {
        continue
    }
    if (-not [System.IO.Path]::IsPathRooted($file)) {
        $file = Join-Path $repoPath $file
    }
    try {
        $fullPath = [System.IO.Path]::GetFullPath($file)
    }
    catch {
        continue
    }

    $prefix = $repoPath + [System.IO.Path]::DirectorySeparatorChar
    if (-not $fullPath.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        continue
    }
    $relativePath = $fullPath.Substring($prefix.Length).Replace('\', '/')
    $relativeLower = $relativePath.ToLowerInvariant()
    if ($relativeLower -like 'deps/*' -or
        $relativeLower -like 'build/*' -or
        $relativeLower -like 'install/*' -or
        $relativeLower -like '_deps/*') {
        continue
    }

    $code = $match.Groups['code'].Value
    $lineNumber = $match.Groups['line'].Value
    if ([string]::IsNullOrWhiteSpace($lineNumber)) {
        $lineNumber = '0'
    }

    $siteKey = "$relativePath|$lineNumber|$code"
    $fileCodeKey = "$relativePath|$code"
    $sites[$siteKey] = $fileCodeKey
    if (-not $messages.ContainsKey($fileCodeKey)) {
        $messages[$fileCodeKey] = $match.Groups['message'].Value.Trim()
    }
}

# Count unique warning lines per file + code.
$current = @{}
foreach ($fileCodeKey in $sites.Values) {
    if ($current.ContainsKey($fileCodeKey)) {
        $current[$fileCodeKey]++
    }
    else {
        $current[$fileCodeKey] = 1
    }
}

function Get-SiteCount {
    param([hashtable]$Map)
    if ($Map.Count -eq 0) {
        return 0
    }
    $sum = ($Map.Values | Measure-Object -Sum).Sum
    return [int]$sum
}

$currentSiteCount = Get-SiteCount -Map $current

# Read the baseline. A missing baseline is an error unless we are creating it.
$baseline = @{}
if (Test-Path -LiteralPath $baselinePath -PathType Leaf) {
    foreach ($raw in Get-Content -LiteralPath $baselinePath) {
        $entry = $raw.Trim()
        if ($entry -eq '' -or $entry.StartsWith('#')) {
            continue
        }
        $parts = $entry -split '\|'
        if ($parts.Count -ne 3) {
            throw "Invalid baseline entry in ${baselinePath}: $entry"
        }
        $count = 0
        if (-not [int]::TryParse($parts[2], [ref]$count)) {
            throw "Invalid warning count in ${baselinePath}: $entry"
        }
        $baseline["$($parts[0])|$($parts[1])"] = $count
    }
}
elseif (-not $UpdateBaseline) {
    throw "Warning baseline not found: $baselinePath. Run with -UpdateBaseline to create it."
}

$baselineSiteCount = Get-SiteCount -Map $baseline

if ($UpdateBaseline) {
    $source = ''
    try {
        $source = (& git -C $repoPath rev-parse --short HEAD 2>$null)
    }
    catch {
        $source = ''
    }

    New-Item -ItemType Directory -Force -Path $BaselineDir | Out-Null
    $output = New-Object System.Collections.Generic.List[string]
    $output.Add("# MSVC warning baseline for preset: $Preset")
    $output.Add('# format: relative/path|code|unique_warning_line_count')
    if (-not [string]::IsNullOrWhiteSpace($source)) {
        $output.Add("# source: $source")
    }
    $output.Add("# updated: $(Get-Date -Format 'yyyy-MM-dd')")
    $output.Add('# Regenerate intentionally with tools/check_msvc_warnings.ps1 -UpdateBaseline.')
    foreach ($key in ($current.Keys | Sort-Object)) {
        $output.Add("$key|$($current[$key])")
    }
    $text = ($output -join "`n") + "`n"
    [System.IO.File]::WriteAllText($baselinePath, $text, (New-Object System.Text.UTF8Encoding($false)))
    Write-Host "Wrote warning baseline: $baselinePath ($currentSiteCount unique sites, $($current.Count) file/code pairs)."
    exit 0
}

# A non-empty baseline with no parsed warnings usually means the log format
# changed or the wrong log was passed; fail closed rather than reporting a
# misleading pass.
if ($current.Count -eq 0 -and $baseline.Count -gt 0) {
    throw "No project-owned MSVC warnings were parsed from $logPath, but the baseline is not empty. Check the build log."
}

$newKeys = @()
$grownKeys = @()
$shrunkKeys = @()
foreach ($key in $current.Keys) {
    if (-not $baseline.ContainsKey($key)) {
        $newKeys += $key
    }
    elseif ($current[$key] -gt $baseline[$key]) {
        $grownKeys += $key
    }
    elseif ($current[$key] -lt $baseline[$key]) {
        $shrunkKeys += $key
    }
}
$removedKeys = @($baseline.Keys | Where-Object { -not $current.ContainsKey($_) })

$newKeys = @($newKeys | Sort-Object)
$grownKeys = @($grownKeys | Sort-Object)
$shrunkKeys = @($shrunkKeys | Sort-Object)
$removedKeys = @($removedKeys | Sort-Object)

Write-Host "MSVC warning baseline: preset=$Preset unique_sites=$currentSiteCount baseline_sites=$baselineSiteCount file_code_pairs=$($current.Count)"
Write-Host "new=$($newKeys.Count) increased=$($grownKeys.Count) reduced_or_removed=$($shrunkKeys.Count + $removedKeys.Count)"

foreach ($key in $newKeys) {
    $parts = $key -split '\|'
    Write-Host ("  NEW  {0} {1} x{2}  {3}" -f $parts[0], $parts[1], $current[$key], $messages[$key])
}
foreach ($key in $grownKeys) {
    $parts = $key -split '\|'
    Write-Host ("  GREW {0} {1} {2} -> {3}  {4}" -f $parts[0], $parts[1], $baseline[$key], $current[$key], $messages[$key])
}

if ($newKeys.Count -gt 0 -or $grownKeys.Count -gt 0) {
    Write-Host ''
    Write-Host 'New MSVC warnings are blocked by the baseline.'
    Write-Host "If these warnings are intentional, review and update ${baselinePath}:"
    Write-Host "  ./tools/check_msvc_warnings.ps1 -Log '$Log' -Preset $Preset -UpdateBaseline"
    throw "MSVC warning baseline check failed for $Preset ($($newKeys.Count) new, $($grownKeys.Count) increased)."
}

Write-Host 'MSVC warning baseline check passed.'
