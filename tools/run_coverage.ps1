<#
.SYNOPSIS
Runs the debug CTest suite under OpenCppCoverage and writes Cobertura + HTML.

.DESCRIPTION
The engine and tests are 32-bit. OpenCppCoverage's x64 collector can drive the
x64 ctest process and still instrument the 32-bit test executables when
--cover_children is set (verified locally). CI installs the pinned 0.9.9.0
Chocolatey package and invokes this script; developers can use the same script
with OpenCppCoverage on PATH.

OpenCppCoverage is archived upstream, but it remains the practical dynamic
coverage option for MSVC PDBs on Windows. The CI pin keeps the report stable.

.PARAMETER BuildDir
CTest build directory. Defaults to build/ogl-debug relative to the repo root.

.PARAMETER Output
Cobertura XML path. Defaults to coverage.xml in the repo root.

.PARAMETER Html
HTML report directory. Defaults to coverage-html in the repo root.

.PARAMETER TestRegex
Optional CTest -R filter for focused runs.
#>
param(
    [string]$BuildDir = 'build/ogl-debug',
    [string]$Output = 'coverage.xml',
    [string]$Html = 'coverage-html',
    [string]$TestRegex = '',
    [string]$OpenCppCoverage = '',
    [string]$RepoRoot = ''
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = Split-Path $PSScriptRoot -Parent
}
$repo = (Resolve-Path -LiteralPath $RepoRoot -ErrorAction Stop).Path.TrimEnd([char[]]@('\', '/'))
if (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $repo $BuildDir
}
if (-not [System.IO.Path]::IsPathRooted($Output)) {
    $Output = Join-Path $repo $Output
}
if (-not [System.IO.Path]::IsPathRooted($Html)) {
    $Html = Join-Path $repo $Html
}

if ([string]::IsNullOrWhiteSpace($OpenCppCoverage)) {
    $coverageCommand = Get-Command OpenCppCoverage.exe -ErrorAction SilentlyContinue
    if (-not $coverageCommand) {
        throw 'OpenCppCoverage.exe was not found on PATH. Install it with: choco install opencppcoverage --version=0.9.9.0'
    }
    $coveragePath = $coverageCommand.Source
}
else {
    if (-not (Test-Path -LiteralPath $OpenCppCoverage -PathType Leaf)) {
        throw "OpenCppCoverage.exe was not found: $OpenCppCoverage"
    }
    $coveragePath = (Resolve-Path -LiteralPath $OpenCppCoverage).Path
}

# The tests are 32-bit, but ctest is 64-bit. The x64 collector drives ctest and
# instruments the 32-bit children through --cover_children; the x86 collector
# cannot reliably drive a 64-bit parent. Fail early with a clear message.
$peBytes = [System.IO.File]::ReadAllBytes($coveragePath)
if ($peBytes.Length -lt 0x40) {
    throw "OpenCppCoverage.exe is too small to be a PE image: $($coveragePath)"
}
$peOffset = [BitConverter]::ToInt32($peBytes, 0x3C)
if ($peOffset -lt 0 -or ($peOffset + 6) -ge $peBytes.Length) {
    throw "OpenCppCoverage.exe has an invalid PE header: $($coveragePath)"
}
$peMachine = [BitConverter]::ToUInt16($peBytes, $peOffset + 4)
if ($peMachine -ne 0x8664) {
    throw ("OpenCppCoverage must be the x64 collector (PE machine 0x{0:X4}); install the default package, not the x86 build." -f $peMachine)
}

if (-not (Test-Path -LiteralPath $BuildDir -PathType Container)) {
    throw "CTest build directory not found: $BuildDir"
}

$ctest = Get-Command ctest -ErrorAction SilentlyContinue
if (-not $ctest) {
    throw 'ctest was not found on PATH.'
}

$ctestArgs = @('--test-dir', $BuildDir, '--output-on-failure')
if (-not [string]::IsNullOrWhiteSpace($TestRegex)) {
    $ctestArgs += @('-R', $TestRegex)
}

Write-Host "OpenCppCoverage: $($coveragePath)"
Write-Host "CTest: $($ctest.Source)"
Write-Host "Build: $BuildDir"
Write-Host "Cobertura: $Output"
Write-Host "HTML: $Html"

& $coveragePath `
    --cover_children `
    --sources (Join-Path $repo 'Hunt') `
    --sources (Join-Path $repo 'Menu') `
    --sources (Join-Path $repo 'tests') `
    --working_dir $repo `
    --export_type "cobertura:$Output" `
    --export_type "html:$Html" `
    -- $ctest.Source @ctestArgs
$coverageExit = $LASTEXITCODE

if (Test-Path -LiteralPath $Output -PathType Leaf) {
    [xml]$document = Get-Content -LiteralPath $Output
    $lineRate = [double]$document.coverage.GetAttribute('line-rate')
    $covered = $document.coverage.GetAttribute('lines-covered')
    $valid = $document.coverage.GetAttribute('lines-valid')
    $summary = 'Coverage: {0:P1} lines ({1}/{2})' -f $lineRate, $covered, $valid
    Write-Host $summary
    if (-not [string]::IsNullOrWhiteSpace($env:GITHUB_STEP_SUMMARY)) {
        $summary | Out-File -FilePath $env:GITHUB_STEP_SUMMARY -Append -Encoding utf8
    }
}

exit $coverageExit
