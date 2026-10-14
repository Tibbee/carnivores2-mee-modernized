# Carnivores 2 - Modder's Engine v1.11 (Modernized)

A maintained fork of the community Modder's Engine for Carnivores 2, with the
old rendering and audio backends replaced and a lot of the engine's behaviour
cleaned up. It still reads the same `HUNTDAT` scripts an MEE install uses, and
drops into the same directory layout.

Release history lives in [CHANGELOG.md](CHANGELOG.md). The current source tree
is `1.1.9.1`, which is a local package candidate and has not been published.

## What you need to run it

A Carnivores 2 Modder's Engine v1.11 data set: the `HUNTDAT` directory from an
MEE install, or a mod built on one. Not a stock retail copy of Carnivores 2.
The engine reads MEE's extended script and menu blocks, survival options, shop
accessories, the per-species aggression override, so the data set it expects is
the one MEE ships, not the one the 2000 CD installed.

None of that is bundled here and none of it ever will be, which is the cleanest
way to distribute this engine. The engine opens `HUNTDAT` from the directory it
runs in, exactly like the original did, so the data set goes next to the
executables. Everything else in this repository is source.

Owning Carnivores 2 is a separate matter and a licensing one rather than a
technical one. See [NOTICE.md](NOTICE.md).

The renderer is picked when you compile, not at runtime, so you get one engine
executable per backend. The menu launches them under their classic names,
`v_gl.ren` and `v_soft.ren`.

## Written for this fork

The rendering and audio stack is the biggest change:

- **Native OpenGL 3.3 renderer.** The base engine's Glide and Direct3D backends
  are gone, along with the need to install either. Software rendering is also
  still there and was reworked rather than replaced. It is a real fallback, not
  a stub.
- **Shaders live on disk** in `shaders/`, twelve shader programs plus one
  shared include. You can edit fog, water
  or sky behaviour without recompiling.
- **OpenAL audio** loaded by name at runtime, so swapping the DLL is enough.
  Reverb presets are tunable per environment from `config.cfg`.
- **Memory arena** with leak attribution that tells you which subsystem
  allocated the block that leaked.

Then the parts that were about making the game look and behave better:

- Pocket fog with per-tile colour blending, a floor that follows terrain relief,
  sun forward-scatter through it, and a camera-inside-the-volume envelope so
  standing in fog doesn't look like a flat wall of grey.
- Underwater rendering as its own system: per-water-body colour, depth-dependent
  density, vertical gradients, and a full-screen overlay while submerged.
- Three sky modes, cloud placement that doesn't swim when you move, and a
  night sky that keeps its fog colours.
- Water with three summed wave components, wavelength attenuation underwater,
  Fresnel from below, and a fade so displacement reaches zero without a seam at
  the shore.
- A flashlight you can buy in the shop, and night vision gear. Both are
  accessories you select at hunt setup.
- Breath-hold scope zoom and a proper crouch stance.
- Terrain mip policy in one place, with a config key for people who want the
  old aggressive filtering back.

The AI work is mostly about making the behaviour legible and finite:

- Every hunter reaction now routes through one awareness resolver and one
  navigator instead of a dozen handlers each with their own idea of what a
  creature knows. A creature reacts to where it *heard* something, not to where
  the hunter currently is, and tracking locks expire on a timer.
- A per-species aggression multiplier you can set in `_RES.TXT`, so a mod can
  retune a species without inventing a clone.
- An awareness trace you can turn on without a debugger (see below).

Data loading is deliberately forgiving. Old and modded scripts parse the way the
original `atoi`/`atof` did, broken values recover to something safe with a
diagnostic, and there is a strict mode for when you actually want it to stop.

## Building and running

### Prerequisites

- CMake **3.21 or newer** for the supplied presets (schema version 3).
  `CMakeLists.txt` declares 3.15, but that does not make presets usable on 3.15.
- Visual Studio 2019/2022 or the MSVC build tools.
- **32-bit x86.** `Hunt/Renderer/renderasm.cpp` contains inline x86 assembly, so
  there is no x64 build. If your toolset is 64-bit-only you need the x86
  cross-tools installed.
- Ninja, if you want to use the presets as written. They select the Ninja
  generator; override it if you'd rather use Visual Studio.
- 7-Zip, but only for `make-c2-package.ps1`. It refuses to run without `7z.exe`.

### Build

```powershell
cmake --preset ogl-release-shipping
cmake --build build/ogl-release-shipping
```

That gives you the OpenGL engine, and it's the preset the packaging script
builds from. For the software renderer, `soft-release`; for the menu on its own,
`menu-release`, which is the fastest thing to build.

Want the in-engine performance counters? Build `ogl-release` instead. It is the
same thing with `GL_PERF_HOOKS` compiled in, which is what `F11` and
`glperf_logging 1` need.

`docs/BUILDING.md` lists all of them and explains what the rest are for.

Built executables land in `build/<preset>/bin/`:

| File | What it is |
|---|---|
| `Carnivores1_GL.exe` | OpenGL 3.3 engine |
| `Carnivores1_SOFT.exe` | software engine |
| `Carnivores2Menu.exe` | launcher and settings menu |

### Package

`tools/release/make-c2-package.ps1` turns binaries you've already built into a
playable folder in `pkg/`, including the renaming:

```powershell
pwsh tools/release/make-c2-package.ps1 `
  -OpenAlDir E:/Games/CarnivoresLegacy `
  -ReleaseNotesPath ./release-notes.md
```

It renames `Carnivores1_GL.exe` to `v_gl.ren` and `Carnivores1_SOFT.exe` to
`v_soft.ren` so the menu can find them, copies the shaders across, and writes a
clean `config.cfg`. Both arguments are required: `-OpenAlDir` points at a folder
with `OpenAL32.dll` and `alsoft.ini`, and `-ReleaseNotesPath` is a markdown file
shipped as `RELEASE-NOTES.md`.

If you're assembling a folder by hand instead, the `.ren` names are the part
that matters: the menu looks for exactly those two and nothing else.

## Keys and logs worth knowing

| Key | |
|---|---|
| `F10` | Tuning inspector. OpenGL builds open the native workbench (Fog/Waves/Sun/Night/Beam/Audio/Collision tabs); software builds keep the legacy three-tab text menu. `D` dumps the current values to the log. |
| `F11` | One second of per-frame performance data to a CSV. OpenGL builds only. |
| `F12` | Screenshot. |
| `F9` | Quit cleanly, which is the polite way to end a session. |

Logs are written next to the executable: `carnivor.log` for the structured
runtime log, `render.log` for the older channel and the AI trace, and
`glperf-*.log` when performance logging is enabled.

If you are chasing a creature that does something you didn't expect, turn on
`ai_logging 1` in `config.cfg` first. It writes one line per hunter event plus a
per-second state line for every reacting animal: species, position, where it
thinks you're going, how far away you actually are, and how much reaction time
it has left. Most AI questions get answered from that file.

`docs/DEBUGGING.md` lists keys, important `config.cfg` options, and the
data validation tools worth using before filing a bug.

## Customizing the equipment shop

Existing rows accept named prices and icon/description redirects in
`HUNTDAT/_MENU.TXT`, without recompiling. Prices reserve the current hunt's
setup budget; they are not permanently deducted from profile score.
[docs/MODDING-SHOP.md](docs/MODDING-SHOP.md) has a copyable example, the required
block terminators, supported keys, and the read-only asset checker. These hooks
do not add new rows or gameplay abilities.

## Documentation

- [CHANGELOG.md](CHANGELOG.md): what changed in each release
- [docs/BUILDING.md](docs/BUILDING.md): presets, packaging, PGO, deployment
- [docs/DEBUGGING.md](docs/DEBUGGING.md): keys, logs, config, data validation
- [docs/MODDING-SHOP.md](docs/MODDING-SHOP.md): equipment prices, art, text and validation
- [docs/NIGHT-LIGHTING.md](docs/NIGHT-LIGHTING.md): Moonlit night, flashlight color restoration and Classic fallback (unmerged feature)
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md): source layout and how it got
  that way
- [NOTICE.md](NOTICE.md): upstream credits, base commit, third-party licences

## Credits and licence

Carnivores 2 and its assets are © 2000 Action Forms / WizardWorks /
Infogrames. This repository contains none of them.

The engine this fork is based on is the **Carnivores 2 Modder's Edition
Engine** by [Ornithomimid1 (Oli)](https://github.com/Ornithomimid1) and the
upstream community, whose work is the whole modding foundation this project
stands on. The standalone menu comes from
[Carn2-Menu](https://github.com/carnivores-cpe/Carn2-Menu). Please credit them
and point people at the upstream project:
[carnivores-cpe/Carnivores-CPE](https://github.com/carnivores-cpe/Carnivores-CPE/tree/Map-Amb-Demo-2).
That branch is still evolving on its own; this fork is not tracking it and
shouldn't be assumed to contain anything new from it.

The modernization work in this repository is © 2026 Tibor Harsányi
(StriderTibe) and is MIT licensed, see [LICENSE](LICENSE). That licence covers
the modernization only. It does not extend to the original game, to the
upstream engine code, or to the third-party components listed in
[NOTICE.md](NOTICE.md).
