# Carnivores 2 Menu

Standalone launcher / menu executable for **Carnivores 2 Modder’s Engine**, replacing `StartLegacy.exe`.

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

## Known compatibility notes

- The bundled `Resources.cpp` is the original C2MenuAttempt parser.
  It handles vanilla Carnivores 2 `_RES.TXT`, but may fail on C2ME-era
  `_RES.TXT` files because it does not understand `overwrite{…}` blocks
  inside `characters`.
- The long-term plan is to reuse the game’s own parser from `Hunt/Resources.cpp`
  so the menu and the engine read the same format.

## Targets / outputs

- Executable name: `Carnivores2Menu.exe`
- Produced by the `Carnivores2Menu` cmake target
- 32-bit (`Win32`) only

---

See **CarnivoresPort/docs/design/menu-system-analysis.md** and
**CarnivoresPort/docs/issues/menu-recreation-status.md** for broader context.
