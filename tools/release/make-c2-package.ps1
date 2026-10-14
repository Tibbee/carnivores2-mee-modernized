<#
.SYNOPSIS
  Builds the Carnivores 2 MEE Modernized release zip (version from Version.h).

.DESCRIPTION
  Assembles the package folder (pkg/Carnivores2_MEE_v<Version>_Modernized)
  from the shipping build outputs, the runtime OpenAL Soft files, a
  generated clean default config.cfg, the MIT/LGPL license texts, the
  end-user README.txt, CHANGELOG and caller-supplied release notes,
  and produces the versioned zip.

  The game exe is distributed under its classic MEE names (v_gl.ren /
  v_soft.ren) because the menu launches those names (Menu/Menu.cpp).

  Requires the shipping builds to exist (see release-guide.md 5.1).
  The ogl-release-shipping tree supplies both the menu and GL engine;
  soft-release supplies the software engine. menu-release is a separate tree.
  Python 3.10+ (or -PythonPath) emits the clean default config.cfg through
  tools/c2_config_defaults.py.

.EXAMPLE
  powershell -NoProfile -File tools/release/make-c2-package.ps1 -OpenAlDir "C:\Games\CarnivoresLegacy" -ReleaseNotesPath "C:\release\RELEASE-NOTES.md"
#>
[CmdletBinding()]
param(
    [string]$RepoRoot = '',
    [string]$PkgRoot  = '',
    [string]$OpenAlDir = '',          # dir containing OpenAL32.dll + alsoft.ini (from a game install)
    [string]$SevenZipPath = '',       # path to 7z.exe (auto-detected if empty)
    [string]$LgplPath = '',           # path to LGPL 2.1 license text (fetched if empty)
    [string]$ReleaseNotesPath = '',   # explicit notes file outside the staging folder
    [string]$PythonPath = '',         # python.exe used to emit config.cfg (auto-detected if empty)
    [string]$ShaOut = ''
)

$ErrorActionPreference = 'Stop'
if (-not $RepoRoot) { $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path }
if (-not $PkgRoot)  { $PkgRoot  = Join-Path $RepoRoot 'pkg' }
if (-not $ShaOut)   { $ShaOut   = Join-Path $PkgRoot 'SHA256.txt' }
# Read the shared executable version to keep archive and binary labels aligned.
$versionHeader = Get-Content (Join-Path $RepoRoot 'Hunt\Core\Version.h') -Raw
if ($versionHeader -notmatch '#define\s+VERSION_STRING\s+"(\d+\.\d+\.\d+\.\d+)"') {
    throw 'Expected VERSION_STRING in major.minor.patch.build format'
}
$Version = $Matches[1]
$ZipName   = "Carnivores2_MEE_v${Version}_Modernized.zip"
$PkgName   = "Carnivores2_MEE_v${Version}_Modernized"
$PkgDir    = Join-Path $PkgRoot $PkgName
if (-not $ReleaseNotesPath) {
    throw 'Pass -ReleaseNotesPath to a release-note file outside the package staging folder; historical CarnivoresDoc release-note drafts were removed.'
}

# Resolve external tools without hardcoding a developer's machine paths:
#  - 7-Zip: search common install locations / PATH
#  - OpenAL runtime: caller must supply -OpenAlDir (a stock game folder), since
#    the DLLs are not redistributable from this repo
#  - LGPL text: fetch from the official OpenAL-Soft source once (cached), so no
#    repo copy of the license is needed and no local path is embedded
#  - Python: emits the clean config.cfg from the engine's first-run template
#    (tools/c2_config_defaults.py); 3.10+ on PATH, or pass -PythonPath
if (-not $SevenZipPath) {
    $candidates = @(
        "$env:ProgramFiles\7-Zip\7z.exe",
        "${env:ProgramFiles(x86)}\7-Zip\7z.exe",
        "$env:LOCALAPPDATA\Programs\7-Zip\7z.exe"
    ) | Where-Object { Test-Path $_ }
    $cmd7z = Get-Command 7z.exe -ErrorAction SilentlyContinue
    $SevenZipPath = if ($candidates) { $candidates[0] } elseif ($cmd7z) { $cmd7z.Source }
}
if (-not $SevenZipPath) { throw '7z.exe not found - pass -SevenZipPath or install 7-Zip' }

if (-not $PythonPath) {
    $cmdPython = Get-Command python.exe -ErrorAction SilentlyContinue
    $PythonPath = if ($cmdPython) { $cmdPython.Source }
}
if (-not $PythonPath) { throw 'python.exe not found - pass -PythonPath or put Python 3.10+ on PATH' }
$Python = $PythonPath

if (-not $LgplPath) {
    # The staging folder is created later, but the cached license text lands
    # directly in the package root, so it has to exist before the fetch.
    if (-not (Test-Path -LiteralPath $PkgRoot)) {
        New-Item -ItemType Directory -Force -Path $PkgRoot | Out-Null
    }
    $LgplPath = Join-Path $PkgRoot 'COPYING.OpenAL-Soft.txt'
    if (-not (Test-Path $LgplPath)) {
        Write-Host 'Fetching LGPL 2.1 license text (OpenAL-Soft official copy)...'
        Invoke-WebRequest -UseBasicParsing -Uri 'https://raw.githubusercontent.com/kcat/openal-soft/master/COPYING' -OutFile $LgplPath
    }
}

if (-not $OpenAlDir) { throw 'Pass -OpenAlDir pointing at a folder containing OpenAL32.dll and alsoft.ini (e.g. a stock Carnivores 2 install)' }
$SevenZip  = $SevenZipPath
$lgpl      = $LgplPath

# --- inputs -------------------------------------------------------------
$menuExe = Join-Path $RepoRoot 'build\ogl-release-shipping\bin\Carnivores2Menu.exe'
$glExe   = Join-Path $RepoRoot 'build\ogl-release-shipping\bin\Carnivores1_GL.exe'
$softExe = Join-Path $RepoRoot 'build\soft-release\bin\Carnivores1_SOFT.exe'
$shaders = Join-Path $RepoRoot 'build\ogl-release-shipping\bin\shaders'
$openal  = Join-Path $OpenAlDir 'OpenAL32.dll'
$alsoft  = Join-Path $OpenAlDir 'alsoft.ini'
# A clean default config is generated by the engine on first launch; we ship
# the same content, emitted by the shared tool from the engine template. The
# deployed config.cfg is dev-tweaked - never ship it.
$configTool = Join-Path $RepoRoot 'tools\c2_config_defaults.py'
foreach ($f in @($menuExe, $glExe, $softExe, $openal, $alsoft, $lgpl, $configTool, $ReleaseNotesPath)) {
    if (-not (Test-Path -LiteralPath $f -PathType Leaf)) { throw "Missing required file: $f" }
}
# The staging folder is deleted before assembly. Do not accept its existing
# release-notes file as an input: it would vanish before Copy-Item reads it.
$notesFullPath = (Resolve-Path -LiteralPath $ReleaseNotesPath).Path
$stagingFullPath = [System.IO.Path]::GetFullPath($PkgDir).TrimEnd('\', '/')
if ($notesFullPath.StartsWith($stagingFullPath + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Release notes must be outside the package staging folder (extract the published ZIP elsewhere or supply a new draft).'
}
if (-not (Test-Path $shaders)) { throw "Missing shaders dir: $shaders" }

# Never package a new version label around older executable build outputs.
foreach ($exe in @($menuExe, $glExe, $softExe)) {
    $builtVersion = (Get-Item -LiteralPath $exe).VersionInfo.FileVersion
    if ($builtVersion -ne $Version) {
        throw "Stale executable: $exe reports $builtVersion; expected $Version. Rebuild the shipping presets."
    }
}

# --- clean staging ------------------------------------------------------
if (Test-Path $PkgDir) { Remove-Item $PkgDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $PkgDir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $PkgDir 'THIRD-PARTY-LICENSES') | Out-Null

# --- binaries -----------------------------------------------------------
Copy-Item $menuExe $PkgDir
# game exes go under their classic MEE names (the menu launches these)
Copy-Item $glExe   (Join-Path $PkgDir 'v_gl.ren')
Copy-Item $softExe (Join-Path $PkgDir 'v_soft.ren')
Copy-Item -Recurse -Force $shaders (Join-Path $PkgDir 'shaders')
Copy-Item $openal  $PkgDir
Copy-Item $alsoft  $PkgDir
Copy-Item (Join-Path $RepoRoot 'LICENSE')   $PkgDir
Copy-Item (Join-Path $RepoRoot 'NOTICE.md') $PkgDir
Copy-Item (Join-Path $RepoRoot 'deps\imgui\LICENSE.txt') (Join-Path $PkgDir 'THIRD-PARTY-LICENSES\LICENSE.Dear-ImGui.txt')
Copy-Item (Join-Path $RepoRoot 'CHANGELOG.md') $PkgDir
Copy-Item $ReleaseNotesPath (Join-Path $PkgDir 'RELEASE-NOTES.md')
Copy-Item $lgpl (Join-Path $PkgDir 'THIRD-PARTY-LICENSES\COPYING.OpenAL-Soft.txt')

# --- clean default config.cfg ------------------------------------------
# The engine's first-run template is the single owner; emit it through the
# shared defaults tool rather than embedding a second copy here.
& $Python $configTool (Join-Path $PkgDir 'config.cfg') --apply | Out-Null
if ($LASTEXITCODE -ne 0) { throw "config.cfg generation failed ($LASTEXITCODE)" }

# --- end-user README.txt ------------------------------------------------
@"
================================================================
  Carnivores 2 - Modder's Engine v1.11 (Modernized v$Version)
================================================================

VERSIONING
  Project release candidate: Modernized v$Version (not published).
  Upstream Modder's Engine v1.11 is the base; modernization releases
  are numbered separately. ModDB V6 was the previous v1.1.9 release.
  Windows executable version: $Version

WHAT'S INCLUDED
  Carnivores2Menu.exe - Launcher / settings menu (entry point)
  v_gl.ren            - OpenGL 3.3 renderer (launched by menu)
  v_soft.ren          - Software renderer (launched by menu)
  shaders/            - GL shader programs
  config.cfg          - Default configuration
  RELEASE-NOTES.md    - This package's release notes
  CHANGELOG.md        - Full change history
  OpenAL32.dll        - OpenAL Soft runtime (audio)
  alsoft.ini          - OpenAL configuration for this game
  LICENSE, NOTICE.md, THIRD-PARTY-LICENSES/ - legal texts

REQUIREMENTS
  - Windows PC (32-bit / x86)
  - OpenGL 3.3 capable GPU for the GL renderer
  - Original game installation (HUNTDAT folder with game assets)
  - Microsoft Visual C++ Redistributable 2019-2022 (x86)

INSTALLATION
  1. Use your compatible original game/mod installation.
     Back up config.cfg and saves before updating.
  2. Copy the files INSIDE the extracted package folder into the
     game folder next to HUNTDAT. Keep your existing config.cfg
     when updating if you want to preserve personal settings.
  3. Launch Carnivores2Menu.exe.

NIGHT VISION (modders)
  Night vision is added by this modernization and works on every
  game/mod data set. If your HUNTDAT has no night-vision art, the
  shop row shows without an icon (and uses a built-in description).
  To add your own art/description, drop these into your HUNTDAT:
    HUNTDAT/menu/pics/equip_nv.tga   - shop icon (16-bit TGA)
    HUNTDAT/menu/txt/nightvis.nfo    - description text
  When present they are used automatically.

FLASHLIGHT (modders)
  The flashlight is added by this modernization and works on every
  game/mod data set for the OpenGL renderer. With no flashlight art
  the shop row shows without an icon and uses a built-in
  description. To add your own art/description, drop these into
  your HUNTDAT:
    HUNTDAT/menu/pics/equip_fl.tga   - shop icon (16-bit TGA)
    HUNTDAT/menu/txt/flashlight.nfo  - description text
  When present they are used automatically. Keep .nfo lines under
  about 48 characters: the info panel draws them unwrapped.

LEGAL
  This package contains MODIFIED EXECUTABLES and supporting
  libraries. It does NOT include any original game assets. You
  MUST own the original game to use this mod. Assets are
  copyrighted by their owners and are NOT redistributed.
  See LICENSE (MIT) and NOTICE.md. OpenAL Soft is distributed
  under the LGPL - see THIRD-PARTY-LICENSES/ and
  https://openal-soft.org/

UPSTREAM CREDITS
  Built on Modder's Edition Engine by Ornithomimid1 (Oli) and
  upstream contributors. Original Carnivores 2 by Action Forms.
  https://github.com/Ornithomimid1
  https://github.com/carnivores-cpe/Carnivores-CPE/tree/Map-Amb-Demo-2
  See NOTICE.md for this fork's exact base snapshot and menu credits.

MODERNIZED v$Version HIGHLIGHTS (since v1.1.9)
  - Lenient-by-default script recovery with strict mod-validation mode.
  - Fixed creature awareness, contact-range threats, fleeing and packs.
  - Restored fish wander radius and safe trophy-room exhibit damage.
  - Corrected weapon highlights in pocket fog.
  - Added per-species aggression tuning and opt-in AI awareness logging.
  See RELEASE-NOTES.md for details and config instructions.

REPORTING ISSUES
  https://github.com/Tibbee/carnivores2-mee-modernized/issues
  Include Modernized v$Version, your mod/map, renderer,
  and reproduction steps.
"@ | Set-Content -Path (Join-Path $PkgDir 'README.txt') -Encoding Ascii

# --- zip ----------------------------------------------------------------
$zipPath = Join-Path $PkgRoot $ZipName
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
Push-Location $PkgRoot
try {
    & $SevenZip a -tzip -bd $ZipName "$PkgName\" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "7z failed with exit code $LASTEXITCODE" }
} finally {
    Pop-Location
}

# --- SHA-256 (portable; Get-FileHash may be absent on stripped PS 5.1) --
$sha = [System.BitConverter]::ToString(
    [System.Security.Cryptography.SHA256]::Create().ComputeHash(
        [System.IO.File]::OpenRead($zipPath))).Replace('-', '').ToLower()
"$sha  $ZipName" | Set-Content -Path $ShaOut -Encoding Ascii

Write-Host "Created: $zipPath"
Write-Host "SHA-256: $sha"
