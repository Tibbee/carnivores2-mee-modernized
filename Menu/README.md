# Carnivores 2 Menu

Standalone launcher / menu executable for **Carnivores 2 Modder's Engine**, replacing `StartLegacy.exe`.

**_Derived from [C2MenuAttempt](https://github.com/carnivores-cpe/Carn2-Menu)._**

---

## What this is

- A **Windows GDI** application
- Reads game data from `HUNTDAT/`
- Saves / loads **trophy profiles** (`trophy00.sav`, …)
- Launches the appropriate renderer exe (`.ren`) with command-line params

## How to run

1. Copy the menu exe beside your `HUNTDAT/` folder.
2. Make sure the chosen renderer `.ren` is either on PATH or in the same folder.
3. Run `Carnivores2Menu.exe`.

## Current feature state

The menu now matches or exceeds the `carnivores_menu_mee` reference in every tracked feature.

| Feature | Status | Since |
|---------|--------|-------|
| `_RES.TXT` C2ME block skipping (`overwrite{}`, etc.) | ✅ | `5a84f62` |
| Audio feedback (ambient, hover, click) | ✅ | `5e4d647` + follow-ups |
| Save-on-quit | ✅ | `5acc0e8` |
| `.c2map` custom map discovery | ✅ | `fe42409` |
| Accessory score multipliers (`accessories {}` in `_MENU.TXT`) | ✅ | `6b9c421` |
| FOV slider in video options | ✅ | `c37ed16` |
| `config.cfg` for extended settings | ✅ | `cc8c637` |
| TrophyLoad/TrophySave bool overflow fix | ✅ | `b958205` |
| Resolution index remapping (StartLegacy compat) | ✅ | `b958205` |
| Renderer persistence across launches | ✅ | `f2953ff` |
| TGA slider art | ❌ | (only remaining gap) |

## Known compatibility notes

- The bundled `Menu/Resources.cpp` parser handles vanilla Carnivores 2 `_RES.TXT` and all C2ME-era `_MENU.TXT` extensions (`overwrite{…}` blocks inside `characters`, `accessories{}`, `prices{}`). C2ME-specific sub-blocks in `characters` (`spawninfo`, `spawngroup`, `killtype`, `tropinfo`, `deathtype`, `idlegroup`, `packinfo`, `packgroup`, `waterIgroup`) are skipped via `SkipNestedBlock()`.
- Audio selection is trimmed to the two supported backends: DirectSound and OpenAL Soft.
- Menu audio feedback is enabled via OpenAL Soft: ambient music on main menu, hover and click sounds.
- `config.cfg` (text-based, in working directory) stores FOV and other extended settings not in the legacy binary trophy format. Written on change, read at menu startup.

## Targets / outputs

- Executable name: `Carnivores2Menu.exe`
- Produced by the `Carnivores2Menu` cmake target via the `menu-release` preset
- 32-bit (`Win32`) only
- Fastest build: `cmake --build --preset menu-release`

---

## See also

- **../CHANGELOG.md** — user-facing release history

The menu was originally adapted from `C2MenuAttempt/` (a community effort, gitignored and not part of the tracked source). The current `Menu/` directory is the live implementation.
