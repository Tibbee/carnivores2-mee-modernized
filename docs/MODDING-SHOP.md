# Customizing the equipment shop

These hooks customize the **seven existing equipment rows**. They do not add
rows, rename them, or invent engine abilities. They are present on the
`feature/shop-externalization` source branch and are not part of an earlier
published release. Use a menu binary built from this branch.

Back up `HUNTDAT/_MENU.TXT` before editing. Do not put these blocks in
`_RES.TXT`: the engine reads that file too, and a menu-only block can cut its
resource parsing short.

## Copyable script tail

Keep the earlier weapons, characters and positional `prices` sections.
Replace the existing final `accessories` section and its ending dot with this
tail. Change the example paths to files you actually provide, or remove the
corresponding override block. Keep your mod's score multipliers if they differ
from these defaults.

```text
accessories
{
    camo = 0.85
    radar = 0.70
    scent = 0.80
    double = 1.0
    tranq = 1.25
    observe = 1.0
}
.
accessory_icons
{
    flashlight = 'huntdat/menu/pics/my_flashlight.tga'
    scent = 'huntdat/menu/pics/my_scent.tga'
}
.
accessory_descriptions
{
    flashlight = 'huntdat/menu/txt/my_flashlight.nfo'
}
.
accessory_prices
{
    flashlight = 25
    nightvision = 45
    tranq = 60
}
.
.
```

**Braces do not close accessory blocks.** The legacy score reader and each
new shop block consume through their own dot at **column zero**. The last two
dots above close the price block and then the file. A new block immediately
after the stock score block's `}` without a dot is swallowed by that reader.
Do not put assignments on the section-header line; those are diagnosed, not
loaded. Section names are lowercase and case-sensitive.

## Keys and precedence

| Canonical key | Display-name alias |
|---|---|
| `camo` | `Camouflage` |
| `radar` | `Radar` |
| `scent` | `Cover scent` |
| `double` | `Double ammo` |
| `nightvision` | `Night vision` |
| `tranq` | `Tranquilizers` |
| `flashlight` | `Flashlight` |

Keys ignore case, spaces/tabs, hyphens and underscores. Single-quoted keys work,
including `'Cover scent'`. Canonical keys win over display-name aliases, then
legacy command-derived aliases (including `tranq -tranquilizer`). A later
valid assignment to the same normalized key replaces the earlier one.
`observe` / `Observer` is accepted **only for descriptions**, not icons/prices.
The old score-modifier block still uses literal keys; it was not normalized.

## Prices are a setup budget

A named price overrides that row's positional `acces` price, which otherwise
falls back to its built-in default. Named entries do not consume or shift
positional slots. Five-line legacy tables retain the original NV/tranquilizer
fallback; six- and seven-line tables also keep their ordering.

Use unquoted whole-number values from 0 through 2147483647. Zero is free;
negative values, fractions, quoted numbers and out-of-range numbers are
reported and ignored in lenient mode, rejected in runtime strict mode. `-0`
is also zero. Selected costs and remaining-budget arithmetic use 64-bit totals;
restored and launched setups must remain affordable.

**Selecting equipment does not permanently subtract profile score.** Prices
reserve the current setup's available budget. Hunt-end score multipliers are a
separate system. These hooks do not change that economy.

## Art, text and comments

- Icons: uncompressed truecolor 16-bit TGA, no palette, bottom-left origin.
  Stock equipment art is 741x214. Pictures are drawn at native size, with no
  scaling; portions beyond the 800x600 canvas at (38,73) are clipped safely.
  Full-screen menu backgrounds must be exactly 800x600.
- Icon redirects replace default art even when missing/invalid; that row then
  has an empty icon box. Missing default icons are optional.
- Descriptions: plain game-compatible text, ideally ASCII, one panel line per
  file line, about 48 characters or fewer. No automatic wrapping. Missing or
  unreadable redirects fall back to the normal `.nfo`, then built-in text for
  night vision/flashlight. Other rows may remain blank. An empty readable file
  intentionally supplies a blank description.
- Use single-quoted game-relative paths. `#`, `;` and `//` begin comments
  outside quotes in the new blocks and top-level dispatch; quoted filenames
  containing `#` are preserved. The legacy score block keeps its old grammar.
- New-block and top-level logical lines may contain at most 4096 characters;
  longer lines are consumed whole and reported/ignored, or rejected strictly.
  This does not expand the older character/weapon/score field readers' limits.

Runtime asset diagnostics are nonfatal even under `load_mode strict`. That
mode rejects bad numeric script values, not missing shop artwork.

## Validate before distributing

From the source repository:

```powershell
pwsh tools/check_mod_assets.ps1 -GameDir 'C:/Games/MyMod'
pwsh tools/check_mod_assets.ps1 -GameDir 'C:/Games/MyMod' -Strict
```

The checker changes no files. Errors return exit 1; `-Strict` also fails on
warnings. It checks effective redirects, prices, supported targets, TGA
payload/origin/dimensions, text length, malformed headers and block placement.
Shadowed aliases are notes rather than false missing-asset failures. Clipping
and unusual sizes are warnings about appearance, not a claim that art scales.
The checker does not run a hunt or certify unrelated `_RES.TXT` content; use
`check_huntdat.ps1` for map/model path validation too.

Test a mod in a backed-up or disposable installation before release. Launching
the real menu/game can update config and trophy files, unlike these checkers.
