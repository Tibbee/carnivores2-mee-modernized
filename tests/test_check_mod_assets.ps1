$ErrorActionPreference = 'Stop'

$checker = Join-Path $PSScriptRoot '../tools/check_mod_assets.ps1'
$root = Join-Path ([System.IO.Path]::GetTempPath()) ("c2-shop-check-" + [guid]::NewGuid().ToString('N'))
$huntdat = Join-Path $root 'HUNTDAT'
$menuFile = Join-Path $huntdat '_MENU.TXT'
$resFile = Join-Path $huntdat '_RES.TXT'
$icon = Join-Path $huntdat 'menu/pics/camo.tga'

function Invoke-Check {
    param([int]$ExpectedExit, [string]$ExpectedText, [string]$ForbiddenText = '')
    $output = @(& $checker -GameDir $root)
    $code = $LASTEXITCODE
    $text = $output -join "`n"
    if ($code -ne $ExpectedExit -or -not $text.Contains($ExpectedText) -or
        ($ForbiddenText -ne '' -and $text.Contains($ForbiddenText))) {
        throw "Expected exit $ExpectedExit and '$ExpectedText' but not '$ForbiddenText'; got exit $($code):`n$text"
    }
    return $text
}

try {
    New-Item -ItemType Directory -Path (Join-Path $huntdat 'menu/pics') -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $huntdat 'menu/txt') -Force | Out-Null
    [System.IO.File]::WriteAllText($resFile, "# accessory_icons mentioned in a comment`n.")
    [System.IO.File]::WriteAllText($menuFile, @'
# accessory_descriptions is not a section header
accessory_icons
{
camo = 'huntdat/menu/pics/camo.tga'
}
.
accessory_descriptions
{
camo = 'huntdat/menu/txt/camo.nfo'
}
.
.
# accessory_icons is only a comment after the terminator
'@)
    [System.IO.File]::WriteAllText((Join-Path $huntdat 'menu/txt/camo.nfo'), 'Mod blurb')
    [byte[]]$tga = @(0,0,2,0,0,0,0,0,0,0,0,0,1,0,1,0,16,0,255,255)
    [System.IO.File]::WriteAllBytes($icon, $tga)

    $valid = Invoke-Check -ExpectedExit 0 -ExpectedText '0 error(s)' -ForbiddenText "no icon        'Camouflage'"
    if ($valid.Contains("no blurb file  'Camouflage'") -or $valid.Contains('bad placement') -or
        $valid.Contains('swallowed') -or $valid.Contains('after end')) {
        throw "A valid override or a comment was treated as a missing asset/block:`n$valid"
    }

    [System.IO.File]::WriteAllBytes($icon, $tga[0..17])
    Invoke-Check -ExpectedExit 1 -ExpectedText 'truncated 16-bit pixel data' | Out-Null
    $tga[17] = 32 # top-origin art renders upside down in this menu
    [System.IO.File]::WriteAllBytes($icon, $tga)
    Invoke-Check -ExpectedExit 0 -ExpectedText 'top-origin icon' | Out-Null

    [System.IO.File]::WriteAllText($menuFile, "accessory_icons`n{`ncamo = 'huntdat/menu/pics/camo.tga'`n}`n")
    Invoke-Check -ExpectedExit 1 -ExpectedText 'unterminated' | Out-Null
    [System.IO.File]::WriteAllText($menuFile, "accessory_icons`n{`ncamo = 'huntdat/menu/pics/camo.tga'`n}`naccessory_descriptions`n{`n}`n.`n.`n")
    Invoke-Check -ExpectedExit 0 -ExpectedText 'swallowed' | Out-Null
    [System.IO.File]::WriteAllText($menuFile, ".`naccessory_icons`n{`n}`n.`n")
    Invoke-Check -ExpectedExit 1 -ExpectedText 'after end' | Out-Null
    [System.IO.File]::WriteAllText($resFile, "accessory_icons`n{`n}`n.`n")
    Invoke-Check -ExpectedExit 1 -ExpectedText 'bad placement' | Out-Null

    Write-Output 'Shop asset checker contract tests passed.'
}
finally {
    if (Test-Path -LiteralPath $root) {
        Remove-Item -LiteralPath $root -Recurse -Force
    }
}
