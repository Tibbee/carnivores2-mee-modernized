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
accessories
{
 camo = 0.85
}
.
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
accessory_prices
{
'Cover scent' = 12
flashlight = 0
}
.
.
# accessory_icons is only a comment after the terminator
'@)
    [System.IO.File]::WriteAllText((Join-Path $huntdat 'menu/txt/camo.nfo'), 'Mod blurb')
    [byte[]]$tga = @(0,0,2,0,0,0,0,0,0,0,0,0,1,0,1,0,16,0,255,255)
    [System.IO.File]::WriteAllBytes($icon, $tga)

    $valid = Invoke-Check -ExpectedExit 0 -ExpectedText 'price overrides: 2' -ForbiddenText "no icon        'Camouflage'"
    if ($valid.Contains("no blurb file  'Camouflage'") -or $valid.Contains('bad placement') -or
        $valid.Contains('swallowed') -or $valid.Contains('after end')) {
        throw "A valid override or a comment was treated as a missing asset/block:`n$valid"
    }

    $correctMenu = [System.IO.File]::ReadAllText($menuFile).Replace("`r`n", "`n")
    # The public recipe itself is a fixture, not just an illustrative snippet.
    $guide = [IO.File]::ReadAllText((Join-Path $PSScriptRoot '../docs/MODDING-SHOP.md'))
    $sample = [regex]::Match($guide, '```text\r?\n(.*?)```', [Text.RegularExpressions.RegexOptions]::Singleline).Groups[1].Value
    if (-not $sample) { throw 'The public guide has no copyable script tail.' }
    [IO.File]::WriteAllBytes((Join-Path $huntdat 'menu/pics/my_flashlight.tga'), $tga)
    [IO.File]::WriteAllBytes((Join-Path $huntdat 'menu/pics/my_scent.tga'), $tga)
    [IO.File]::WriteAllText((Join-Path $huntdat 'menu/txt/my_flashlight.nfo'), 'Mod flashlight')
    [IO.File]::WriteAllText($menuFile, $sample)
    Invoke-Check -ExpectedExit 0 -ExpectedText 'price overrides: 3' -ForbiddenText 'swallowed' | Out-Null
    [System.IO.File]::WriteAllText($menuFile, $correctMenu.Replace("}`n.`naccessory_icons", "}`naccessory_icons"))
    Invoke-Check -ExpectedExit 1 -ExpectedText 'legacy accessories score block' | Out-Null
    [System.IO.File]::WriteAllText($menuFile, $correctMenu.Replace('flashlight = 0', 'flashlight = -4'))
    Invoke-Check -ExpectedExit 1 -ExpectedText 'bad price' | Out-Null
    [System.IO.File]::WriteAllText($menuFile, $correctMenu.Replace('flashlight = 0', "flashlight = '4'"))
    Invoke-Check -ExpectedExit 1 -ExpectedText 'bad price' | Out-Null
    [System.IO.File]::WriteAllText($menuFile, $correctMenu.Replace("`naccessory_icons", "`n// accessory_descriptions is a comment`naccessory_icons"))
    Invoke-Check -ExpectedExit 0 -ExpectedText 'icon overrides: 1' -ForbiddenText 'bad header' | Out-Null
    [System.IO.File]::WriteAllText($menuFile, $correctMenu.Replace("camo = 'huntdat", "camo =" + (' ' * 300) + "'huntdat"))
    Invoke-Check -ExpectedExit 0 -ExpectedText 'icon overrides: 1' | Out-Null
    [System.IO.File]::WriteAllText($menuFile, $correctMenu.Replace("camo = 'huntdat", "camo =" + (' ' * 4100) + "'huntdat"))
    Invoke-Check -ExpectedExit 1 -ExpectedText 'line too long' | Out-Null
    [System.IO.File]::WriteAllText($menuFile, $correctMenu.Replace('flashlight = 0', "; flashlight = 900`n// radar = 800`nflashlight = -0"))
    Invoke-Check -ExpectedExit 0 -ExpectedText 'price overrides: 2' -ForbiddenText 'unknown key' | Out-Null
    [System.IO.File]::WriteAllText($menuFile, $correctMenu.Replace("camo = 'huntdat/menu/pics/camo.tga'", "Camouflage = 'missing.tga'`ncamo = 'huntdat/menu/pics/camo.tga'"))
    Invoke-Check -ExpectedExit 0 -ExpectedText 'shadowed icon' -ForbiddenText 'missing icon' | Out-Null
    [System.IO.File]::WriteAllText($menuFile, $correctMenu.Replace("camo = 'huntdat/menu/txt/camo.nfo'", "Camouflage = 'missing.nfo'`ncamo = 'huntdat/menu/txt/camo.nfo'"))
    Invoke-Check -ExpectedExit 0 -ExpectedText 'shadowed text' -ForbiddenText 'missing text' | Out-Null
    [System.IO.File]::WriteAllText($menuFile, $correctMenu.Replace("camo = 'huntdat/menu/pics/camo.tga'", "observe = 'huntdat/menu/pics/camo.tga'"))
    Invoke-Check -ExpectedExit 1 -ExpectedText 'matches no icon target' | Out-Null
    [System.IO.File]::WriteAllText($menuFile, "accessory_prices { flashlight = 7 }`n.`n.`n")
    Invoke-Check -ExpectedExit 1 -ExpectedText 'bad header' | Out-Null
    [System.IO.File]::WriteAllText($menuFile, "common {`n.`n}`n" + $correctMenu)
    Invoke-Check -ExpectedExit 0 -ExpectedText 'price overrides: 2' -ForbiddenText 'after end' | Out-Null
    $hashIcon = Join-Path $huntdat 'menu/pics/camo#1.tga'
    [System.IO.File]::WriteAllBytes($hashIcon, $tga)
    [System.IO.File]::WriteAllText($menuFile, $correctMenu.Replace("camo.tga'", "camo#1.tga' # outside quotes"))
    Invoke-Check -ExpectedExit 0 -ExpectedText 'icon overrides: 1' -ForbiddenText 'missing icon' | Out-Null
    [System.IO.File]::WriteAllText($menuFile, $correctMenu)
    [byte[]]$tall = New-Object byte[] (18 + 741 * 600 * 2)
    $tall[2] = 2; $tall[12] = 229; $tall[13] = 2; $tall[14] = 88; $tall[15] = 2; $tall[16] = 16
    [System.IO.File]::WriteAllBytes($icon, $tall)
    Invoke-Check -ExpectedExit 0 -ExpectedText 'clipped icon' | Out-Null

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
    [System.IO.File]::WriteAllText($resFile, "accessory_prices`n{`n}`n.`n")
    Invoke-Check -ExpectedExit 1 -ExpectedText 'bad placement' | Out-Null

    Write-Output 'Shop asset checker contract tests passed.'
}
finally {
    if (Test-Path -LiteralPath $root) {
        Remove-Item -LiteralPath $root -Recurse -Force
    }
}
