# tools/verify-local.ps1
#
# Local replacement for the checks that are deliberately not part of the
# automatic CI gate. Run this from the repository root before pushing.
#
# It builds the debug configuration, checks the MSVC warning baseline, runs
# CTest, checks comment hygiene, and exercises the smoke-script contracts.
# The clean-checkout part of CI still has to happen in a fresh clone or
# worktree; an incremental local build cannot prove that.

param(
    [switch]$SkipBuild,
    [switch]$SkipCommentHygiene,
    [switch]$SkipSmokeContracts
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $repoRoot
try {
    function Invoke-Checked {
        param(
            [Parameter(Mandatory = $true)]
            [scriptblock]$Command,
            [Parameter(Mandatory = $true)]
            [string]$Description
        )

        & $Command
        if ($LASTEXITCODE -ne 0) {
            throw "$Description failed with exit code $LASTEXITCODE."
        }
    }

    if (-not $SkipCommentHygiene) {
        Write-Host '==> comment hygiene'
        Invoke-Checked { ./tools/check_comment_hygiene.ps1 } 'Comment hygiene'
    }

    if (-not $SkipBuild) {
        Write-Host '==> configure ogl-debug'
        Invoke-Checked { cmake --preset ogl-debug -DC2_BUILD_TESTS=ON } 'CMake configure'

        Write-Host '==> build ogl-debug'
        $log = Join-Path $repoRoot 'build/verify-local-build.log'
        cmake --build --preset ogl-debug 2>&1 | Tee-Object -FilePath $log
        if ($LASTEXITCODE -ne 0) {
            throw "Debug build failed with exit code $LASTEXITCODE."
        }
        Invoke-Checked { ./tools/check_msvc_warnings.ps1 -Log $log -Preset 'ogl-debug' } 'MSVC warning baseline'

        Write-Host '==> CTest'
        Invoke-Checked { ctest --test-dir build/ogl-debug --output-on-failure } 'CTest'
    }

    if (-not $SkipSmokeContracts) {
        $dotnet = Get-Command dotnet -ErrorAction Stop
        $fixture = Join-Path ([System.IO.Path]::GetTempPath()) "c2-smoke-fixture-$PID"
        New-Item -ItemType Directory -Path $fixture -Force | Out-Null
        try {
            Write-Host '==> smoke-script contracts'
            Invoke-Checked {
                & $dotnet.Source build tests/smoke_fixture.csproj --configuration Release --output $fixture --nologo
            } 'Smoke fixture build'
            Invoke-Checked {
                ./tests/test_smoke_script.ps1 -FixtureDll (Join-Path $fixture 'smoke_fixture.dll')
            } 'Smoke-script contracts'
        }
        finally {
            Remove-Item -LiteralPath $fixture -Recurse -Force -ErrorAction SilentlyContinue
        }
    }

    Write-Host 'Local verification passed.'
}
finally {
    Pop-Location
}
