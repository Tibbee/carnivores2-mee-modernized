<#
.SYNOPSIS
    Checks a Carnivores 2 MEE game folder for shop-asset problems a modder
    cannot see in-game: broken icon/text overrides, wrong icon formats, and
    rows that will render blank or with the empty icon box.

.DESCRIPTION
    The hunt-screen equipment shop reads its rows from HUNTDAT/_MENU.TXT (with
    HUNTDAT/_RES.TXT as the legacy fallback). Optional shop blocks customize
    existing rows. Assignments must follow the header on separate lines:

        accessory_prices
        {
            flashlight = 30
        }
        .

    See docs/MODDING-SHOP.md for the complete stock-tail replacement, including
    the score-block closer and final file dot. Icon and description blocks use
    the same multiline shape, with single-quoted game-relative file paths.

    This walks the same script the menu reads and reports, before the modder
    ships:

      - every override key that matches no shop row (a typo'd key silently
        changes nothing in-game)
      - every override whose file does not exist, or whose icon is not the
        menu art format (16-bit uncompressed truecolor TGA, no palette)
      - icons exceeding the visible canvas at (38,73): the menu now clips
        them safely, but does not scale them; non-stock cell sizes (741x214)
        may also look wrong
      - description lines longer than ~48 characters (the info panel draws
        them unwrapped at x=424, so long lines run off the panel)
      - rows whose default art/text is missing from this install (the row
        still shows: empty icon box, and the built-in blurb where one exists)
      - accessory blocks placed in _RES.TXT: the engine reads that file too,
        does not know these sections, and stops at the block's '.' closer --
        silently skipping every section after it. Accessory blocks belong in
        _MENU.TXT only
      - a section header swallowed by a still-open block: accessory blocks
        close at a '.' at column 0, not at their '}' line. The legacy
        accessories score block uses that rule too: insert a new '.' after
        its '}' before adding new blocks to the shipped _MENU.TXT
      - blocks placed after the script's terminating '.' line: the menu stops
        reading at the first '.' at column 0 outside a block, so anything
        appended below it never loads

    Keys are normalized the way the menu normalizes them: case-insensitive,
    spaces/hyphens/underscores ignored, so 'Cover scent', 'cover-scent' and
    'coverscent' are the same key. A row accepts its score-block key
    ('camo', 'radar', 'scent', 'double', 'tranq', 'nightvision',
    'flashlight', 'observe'), its display name, and the legacy full-command
    spelling.

    Nothing is modified; the report can be pasted into a release note.
    Errors exit 1; -Strict also fails on warnings.

.PARAMETER GameDir
    The game folder, i.e. the one containing HUNTDAT. Defaults to the current
    directory.

.PARAMETER Strict
    Treat warnings as failures as well.

.EXAMPLE
    .\check_mod_assets.ps1 'C:\Games\Carnivores 2'

.EXAMPLE
    .\check_mod_assets.ps1 -Strict -GameDir 'E:\Games\CarnivoresLegacy' > shop-check.txt
#>
param(
    [string]$GameDir = '.',
    [switch]$Strict
)

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

function Get-RelativePath {
    param([string]$Value)
    # Keep authored paths: the runtime does not rewrite them either.
    return $Value
}

# One pair of surrounding single quotes, the way Menu/Resources.cpp strips
# script values. Trim("'") would eat runs of quotes; the menu eats exactly one.
function Get-Unquoted {
    param([string]$Value)
    $v = $Value.Trim([char[]]" `t`r`n")
    if ($v.Length -ge 2 -and $v[0] -eq "'" -and $v[-1] -eq "'") {
        return $v.Substring(1, $v.Length - 2)
    }
    return $v
}

# The menu's key normalization: lowercase, spaces/tabs/hyphens/underscores
# ignored (Menu/Resources.cpp NormalizeAccessoryKey).
function Get-NormalizedKey {
    param([string]$Key)
    return ((Get-Unquoted $Key) -replace '[ \t_-]', '').ToLowerInvariant()
}

if (-not (Test-Path -LiteralPath $GameDir -PathType Container)) {
    Write-Output "Game folder not found: $GameDir"
    exit 1
}
$GameDir = (Resolve-Path -LiteralPath $GameDir).Path
$huntdat = Join-Path $GameDir 'HUNTDAT'

if (-not (Test-Path -LiteralPath $huntdat -PathType Container)) {
    Write-Output "No HUNTDAT folder in: $GameDir"
    Write-Output "Run this from the game folder (the one holding the .ren/.exe files)."
    exit 1
}

# --- the shop rows ------------------------------------------------------------
# Mirrors the addUtil() table in Menu/Resources.cpp: the score-block key (the
# canonical spelling), the display name, the launch command, the default art
# and blurb paths, and whether a built-in blurb covers a missing .nfo.
# Observer has no shop row and no icon; only its text is redirectable.
$rows = @(
    [pscustomobject]@{ ScoreKey = 'camo';        Name = 'Camouflage';   Cmd = '-camo';                        Icon = 'huntdat/menu/pics/equip1.tga';   Text = 'huntdat/menu/txt/camoflag.nfo'; BuiltinText = $false },
    [pscustomobject]@{ ScoreKey = 'radar';       Name = 'Radar';        Cmd = '-radar';                       Icon = 'huntdat/menu/pics/equip2.tga';   Text = 'huntdat/menu/txt/radar.nfo';    BuiltinText = $false }
    [pscustomobject]@{ ScoreKey = 'scent';       Name = 'Cover scent';  Cmd = '';                             Icon = 'huntdat/menu/pics/equip3.tga';   Text = 'huntdat/menu/txt/scent.nfo';   BuiltinText = $false }
    [pscustomobject]@{ ScoreKey = 'double';      Name = 'Double ammo';  Cmd = '-double';                      Icon = 'huntdat/menu/pics/equip4.tga';   Text = 'huntdat/menu/txt/double.nfo';  BuiltinText = $false }
    [pscustomobject]@{ ScoreKey = 'nightvision'; Name = 'Night vision'; Cmd = '-nightvision';                 Icon = 'huntdat/menu/pics/equip_nv.tga'; Text = 'huntdat/menu/txt/nightvis.nfo'; BuiltinText = $true }
    [pscustomobject]@{ ScoreKey = 'tranq';       Name = 'Tranquilizers'; Cmd = '-tranq -tranquilizer';        Icon = 'huntdat/menu/pics/equip6.tga';   Text = 'huntdat/menu/txt/tranq.nfo';    BuiltinText = $false }
    [pscustomobject]@{ ScoreKey = 'flashlight';  Name = 'Flashlight';   Cmd = '-flashlight';                  Icon = 'huntdat/menu/pics/equip_fl.tga'; Text = 'huntdat/menu/txt/flashlight.nfo'; BuiltinText = $true }
    [pscustomobject]@{ ScoreKey = 'observe';     Name = 'Observer';     Cmd = '-observe -observer';           Icon = $null;                            Text = 'huntdat/menu/txt/observe.nfo'; BuiltinText = $false }
)

# Accepted spellings per row, in probe order: score key, display name, then
# the legacy command-derived key (the command without its leading dash).
$rowKeys = @{}
foreach ($row in $rows) {
    $keys = New-Object System.Collections.Generic.List[string]
    $keys.Add((Get-NormalizedKey $row.ScoreKey))
    $keys.Add((Get-NormalizedKey $row.Name))
    $legacy = $row.Cmd
    if ($legacy.Length -gt 0 -and $legacy[0] -eq '-') { $legacy = $legacy.Substring(1) }
    $legacy = (Get-NormalizedKey $legacy)
    if ($legacy -ne '') { $keys.Add($legacy) }
    $rowKeys[$row.ScoreKey] = $keys
}

function Get-RowForKey {
    param([string]$Key)
    foreach ($row in $rows) {
        if ($rowKeys[$row.ScoreKey] -contains (Get-NormalizedKey $Key)) { return $row }
    }
    return $null
}

# Match the runtime's quote-aware comments in new shop blocks and dispatch.
function Get-CodeLine {
    param([string]$Line)
    $quoted = $false
    for ($i = 0; $i -lt $Line.Length; $i++) {
        if ($Line[$i] -eq "'") { $quoted = -not $quoted }
        if (-not $quoted -and ($Line[$i] -eq '#' -or $Line[$i] -eq ';' -or
            ($Line[$i] -eq '/' -and $i + 1 -lt $Line.Length -and $Line[$i + 1] -eq '/'))) {
            return $Line.Substring(0, $i)
        }
    }
    return $Line
}

# Recognize complete, case-sensitive header tokens, not substring mentions.
function Get-AccessoryBlockKind {
    param([string]$Line)
    if ($Line.StartsWith('#') -or $Line.StartsWith(';')) { return $null }
    if ($Line -cmatch '^\s*accessory_icons(?:\s|\{|$)') { return 'icons' }
    if ($Line -cmatch '^\s*accessory_descriptions(?:\s|\{|$)') { return 'texts' }
    if ($Line -cmatch '^\s*accessory_prices(?:\s|\{|$)') { return 'prices' }
    return $null
}

# The menu tries a row's score key, display name and legacy command in order.
function Get-EffectiveOverride {
    param([hashtable]$Overrides, $Row)
    foreach ($key in $rowKeys[$Row.ScoreKey]) {
        if ($Overrides.ContainsKey($key)) { return $Overrides[$key] }
    }
    return $null
}

# --- locate the menu script ---------------------------------------------------
$menuTxt = Join-Path $huntdat '_MENU.TXT'
$resTxt  = Join-Path $huntdat '_RES.TXT'
$script = $menuTxt
$scriptName = '_MENU.TXT'
if (-not (Test-GameFile $menuTxt)) {
    if (-not (Test-GameFile $resTxt)) {
        $errors.Add("missing file   $menuTxt and $resTxt   (no menu script to check)")
    } else {
        $warnings.Add("missing file   $menuTxt   (menu falls back to _RES.TXT)")
        $script = $resTxt
        $scriptName = '_RES.TXT'
    }
}

# The engine also reads _RES.TXT and does not know the accessory sections:
# its main loop stops at the block's '.' closer, dropping every section after
# it. Catch that placement even when the menu itself read _MENU.TXT.
if (Test-GameFile $resTxt) {
    $lineNo = 0
    foreach ($line in [System.IO.File]::ReadAllLines($resTxt)) {
        $lineNo++
        if ($null -ne (Get-AccessoryBlockKind $line)) {
            $errors.Add("bad placement  _RES.TXT:$lineNo has an accessory block; the engine stops at its '.' closer and skips the rest of the file -- put accessory blocks in _MENU.TXT only")
        }
    }
}

# --- parse the two blocks -----------------------------------------------------
# Same grammar the menu reads: 'key = value' lines inside the block, one pair
# of surrounding single quotes stripped, inline '#' comments cut, block ends
# at a line starting with '.'.
$iconOverrides = @{}       # normalized key -> @{ Value; Line }
$textOverrides = @{}
$priceOverrides = @{}
$block = $null
$scoreBlock = $false
$nestedSection = $false
$nestedDepth = 0
$nestedStarted = $false
$terminated = $false
$lineNo = 0
if (Test-GameFile $script) {
    foreach ($line in [System.IO.File]::ReadAllLines($script)) {
        $lineNo++
        if ($nestedSection) {
            $nestedDepth += ([regex]::Matches($line, '\{')).Count - ([regex]::Matches($line, '\}')).Count
            if ($line.Contains('{')) { $nestedStarted = $true }
            if ($nestedStarted -and $nestedDepth -le 0) { $nestedSection = $false }
            continue
        }
        if (-not $scoreBlock -and $line.Length -gt 4096) {
            $errors.Add("line too long  ${scriptName}:$lineNo exceeds 4096 characters; the runtime ignores the whole line")
            continue
        }
        # Preserve the score reader's old grammar; only dispatch/new blocks
        # use the improved comment rules.
        if (-not $scoreBlock) { $line = Get-CodeLine $line }
        $header = Get-AccessoryBlockKind $line
        if ($scoreBlock) {
            if ($line.Length -gt 0 -and $line[0] -eq '.') { $scoreBlock = $false; continue }
            if ($null -ne $header) {
                $errors.Add("swallowed       ${scriptName}:$lineNo '$($line.Trim())' is inside the legacy accessories score block -- add a '.' at column zero after that block's '}' before the new section")
            }
            continue
        }
        if ($terminated) {
            # The menu stops at the first top-level '.' at column 0.
            if ($null -ne $header) {
                $errors.Add("after end      ${scriptName}:$lineNo '$($line.Trim())' is below the script's terminating '.' line -- the menu stops reading there; insert the block before the final '.' line")
            }
            continue
        }
        if ($null -eq $block -and $line.Length -gt 0 -and $line[0] -eq '.') {
            $terminated = $true
            continue
        }
        if ($null -ne $block) {
            if ($line.Length -gt 0 -and $line[0] -eq '.') { $block = $null; continue }
            # A section header inside a still-open block: the block only ends
            # at a '.' line, not at its '}' line, so this header was consumed
            # as part of the previous block and never takes effect. The engine
            # reads it the same way. (Header lines carry no '='; values may.)
            if ($null -ne $header) {
                $warnings.Add("swallowed       ${scriptName}:$lineNo '$($line.Trim())' is inside the still-open '$block' block -- close each block with a line containing only a dot ('.'), not just its '}' line; the section never takes effect")
            }
            $eq = $line.IndexOf('=')
            if ($eq -lt 1) { continue }
            $key = $line.Substring(0, $eq).Trim([char[]]" `t`r`n")
            if ($key -eq '' -or $key[0] -eq '#') { continue }
            $value = $line.Substring($eq + 1)
            if ($block -eq 'prices') { $value = $value.Trim([char[]]@(32,9,10,13,11,12)) }
            else { $value = (Get-Unquoted $value) }
            if ($value -eq '' -and $block -ne 'prices') { continue }
            $entry = @{ Value = $value; Line = $lineNo }
            if ($block -eq 'icons') { $entry.Value = Get-RelativePath $value; $iconOverrides[(Get-NormalizedKey $key)] = $entry }
            elseif ($block -eq 'texts') { $entry.Value = Get-RelativePath $value; $textOverrides[(Get-NormalizedKey $key)] = $entry }
            else {
                $parsed = 0
                if ($value -notmatch '^[+-]?[0-9]+$' -or -not [int]::TryParse($value, [ref]$parsed) -or $parsed -lt 0) {
                    $errors.Add("bad price      '$value' (${scriptName}:$lineNo) must be a nonnegative whole number within the 32-bit range (runtime ignores this assignment)")
                } else { $priceOverrides[(Get-NormalizedKey $key)] = $entry }
            }
            continue
        }
        if ($line -cmatch '^\s*accessories(?:\s|\{|$)' -and -not $line.StartsWith('#') -and -not $line.StartsWith(';')) {
            $scoreBlock = $true
            continue
        }
        if ($null -ne $header) {
            if ($line.Contains('=')) {
                $errors.Add("bad header     ${scriptName}:$lineNo put assignments on separate lines after the shop section header")
            }
            $block = $header
            continue
        }
        # Production readers consume these brace-delimited sections before
        # dispatch resumes. A dot inside one is not the menu EOF.
        if ($line -cmatch '^\s*(weapons|characters|prices)(?:\s|\{|$)' -or
            ($line.Contains('{') -and $line -cmatch '^\s*(common|areatable|spawntable|packtable|trophytable|oldambients|corpseambients|mapambients|hunterinfo)(?:\s|\{|$)')) {
            $nestedDepth = ([regex]::Matches($line, '\{')).Count - ([regex]::Matches($line, '\}')).Count
            $nestedStarted = $line.Contains('{')
            $nestedSection = -not $nestedStarted -or $nestedDepth -gt 0
        }
    }
    if ($scoreBlock) {
        $errors.Add("unterminated   $scriptName ends inside the legacy accessories score block")
    }
    if ($null -ne $block) {
        $errors.Add("unterminated   $scriptName ends inside an accessory block; add a '.' closer before the script's final '.'")
    }
}

# --- check the overrides ------------------------------------------------------
function Read-TgaHeader {
    param([string]$Path)
    $fs = [System.IO.File]::OpenRead($Path)
    try {
        $buf = New-Object byte[] 18
        $read = $fs.Read($buf, 0, 18)
        if ($read -lt 18) { return $null }
        return [pscustomobject]@{
            IdLength  = $buf[0]
            ColorMap  = $buf[1]
            ImageType = $buf[2]
            Width     = $buf[12] + 256 * $buf[13]
            Height    = $buf[14] + 256 * $buf[15]
            Bits      = $buf[16]
            Descriptor = $buf[17]
        }
    } finally { $fs.Dispose() }
}

foreach ($entry in $iconOverrides.GetEnumerator()) {
    $row = Get-RowForKey -Key $entry.Key
    $where = "${scriptName}:$($entry.Value.Line)"
    if ($null -eq $row -or $row.ScoreKey -eq 'observe') {
        $errors.Add("unknown key    '$($entry.Key)' ($where) matches no icon target -- the override silently changes nothing")
        continue
    }
    $effective = Get-EffectiveOverride -Overrides $iconOverrides -Row $row
    if ($effective.Line -ne $entry.Value.Line) {
        $notes.Add("shadowed icon  '$($entry.Key)' ($where) is unused; a higher-priority key wins for '$($row.Name)'")
        continue
    }
    $full = Join-Path $GameDir $entry.Value.Value
    if (-not (Test-GameFile $full)) {
        $errors.Add("missing icon   $($entry.Value.Value)   <- '$($row.Name)' override ($where); the row shows the empty icon box")
        continue
    }
    $header = Read-TgaHeader -Path $full
    if ($null -eq $header) {
        $errors.Add("bad icon       $full   <- '$($row.Name)' override ($where); too short for a TGA header")
        continue
    }
    if ($header.ImageType -ne 2 -or $header.ColorMap -ne 0 -or $header.Bits -ne 16) {
        $errors.Add("bad icon       $($entry.Value.Value)   <- '$($row.Name)' override ($where); menu art is 16-bit uncompressed truecolor TGA, this file is type=$($header.ImageType) cmap=$($header.ColorMap) bpp=$($header.Bits)")
        continue
    }
    if ($header.Width -eq 0 -or $header.Height -eq 0 -or
        ([System.IO.FileInfo]$full).Length -lt (18L + $header.IdLength + 2L * $header.Width * $header.Height)) {
        $errors.Add("bad icon       $($entry.Value.Value)   <- '$($row.Name)' override ($where); zero dimensions or truncated 16-bit pixel data")
        continue
    }
    if (($header.Descriptor -band 0x20) -ne 0) {
        $warnings.Add("top-origin icon $($entry.Value.Value)   has its top-origin bit set; the menu ignores the origin and will draw it upside down")
    }
    if ($header.Width -gt 762 -or $header.Height -gt 527) {
        $warnings.Add("clipped icon   $($entry.Value.Value)   is $($header.Width)x$($header.Height); the menu clips it to the canvas at (38,73), without scaling")
    } elseif ($header.Width -ne 741 -or $header.Height -ne 214) {
        $warnings.Add("odd icon size  $($entry.Value.Value)   is $($header.Width)x$($header.Height); the stock cell is 741x214 and nothing scales, so it renders 1:1 at the same spot")
    }
}

foreach ($entry in $priceOverrides.GetEnumerator()) {
    $row = Get-RowForKey -Key $entry.Key
    $where = "${scriptName}:$($entry.Value.Line)"
    if ($null -eq $row -or $row.ScoreKey -eq 'observe') {
        $errors.Add("unknown key    '$($entry.Key)' ($where) matches no priced shop row")
        continue
    }
}

foreach ($entry in $textOverrides.GetEnumerator()) {
    $row = Get-RowForKey -Key $entry.Key
    $where = "${scriptName}:$($entry.Value.Line)"
    if ($null -eq $row) {
        $errors.Add("unknown key    '$($entry.Key)' ($where) matches no shop row -- the override silently changes nothing")
        continue
    }
    $effective = Get-EffectiveOverride -Overrides $textOverrides -Row $row
    if ($effective.Line -ne $entry.Value.Line) {
        $notes.Add("shadowed text  '$($entry.Key)' ($where) is unused; a higher-priority key wins for '$($row.Name)'")
        continue
    }
    $full = Join-Path $GameDir $entry.Value.Value
    if (-not (Test-GameFile $full)) {
        $errors.Add("missing text   $($entry.Value.Value)   <- '$($row.Name)' override ($where); the row keeps its default description")
        continue
    }
    $lineIndex = 0
    foreach ($textLine in [System.IO.File]::ReadAllLines($full)) {
        $lineIndex++
        if ($textLine.Length -gt 48) {
            $warnings.Add("long text line $($entry.Value.Value):$lineIndex is $($textLine.Length) chars; the info panel draws lines unwrapped at x=424, so past ~48 characters runs off the panel")
        }
    }
}

# --- effective row status ------------------------------------------------------
# A selected icon override replaces the default even if broken; a missing
# description override falls back to the default .nfo (then built-in text).
foreach ($row in $rows) {
    if ($row.ScoreKey -eq 'observe') { continue }  # no shop row, info panel only
    $iconOverride = Get-EffectiveOverride -Overrides $iconOverrides -Row $row
    $iconFull = Join-Path $GameDir (Get-RelativePath $row.Icon)
    if ($null -eq $iconOverride -and -not (Test-GameFile $iconFull)) {
        $notes.Add("no icon        '$($row.Name)' ($($row.Icon) absent) -- the row shows the empty icon box")
    }
    $textOverride = Get-EffectiveOverride -Overrides $textOverrides -Row $row
    $textFull = Join-Path $GameDir (Get-RelativePath $row.Text)
    if (($null -eq $textOverride -or -not (Test-GameFile (Join-Path $GameDir $textOverride.Value))) -and
        -not (Test-GameFile $textFull)) {
        if ($row.BuiltinText) {
            $notes.Add("no blurb file  '$($row.Name)' ($($row.Text) absent) -- the built-in text covers it")
        } else {
            $warnings.Add("no blurb file  '$($row.Name)' ($($row.Text) absent) -- the panel shows no text without an override or a shipped .nfo")
        }
    }
}

# --- report -------------------------------------------------------------------
Write-Output "Carnivores 2 shop-asset check: $GameDir"
Write-Output "  script: $scriptName; icon overrides: $($iconOverrides.Count); text overrides: $($textOverrides.Count); price overrides: $($priceOverrides.Count)"

if ($errors.Count -gt 0) {
    Write-Output ""
    Write-Output "ERRORS ($($errors.Count))"
    foreach ($entry in $errors) { Write-Output "  $entry" }
}
if ($warnings.Count -gt 0) {
    Write-Output ""
    Write-Output "WARNINGS ($($warnings.Count))"
    foreach ($entry in $warnings) { Write-Output "  $entry" }
}
if ($notes.Count -gt 0) {
    Write-Output ""
    Write-Output "NOTES ($($notes.Count))"
    foreach ($entry in $notes) { Write-Output "  $entry" }
}
if ($errors.Count -eq 0 -and $warnings.Count -eq 0 -and $notes.Count -eq 0) {
    Write-Output "  No problems found."
}

Write-Output ""
Write-Output "$($errors.Count) error(s), $($warnings.Count) warning(s), $($notes.Count) note(s)"

if ($errors.Count -gt 0 -or ($Strict -and $warnings.Count -gt 0)) { exit 1 }
exit 0
