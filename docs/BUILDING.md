# Building

Everything here assumes PowerShell in the repository root.

## Prerequisites

| | |
|---|---|
| CMake | **3.21 or newer** for the supplied schema-v3 presets. The project's declared 3.15 minimum alone does not support them. |
| Compiler | MSVC from Visual Studio 2019/2022, or the standalone build tools. |
| Architecture | **x86 (32-bit) only.** See below. |
| Ninja | Only if you use the presets unchanged, because they set `"generator": "Ninja"`. Override it in your own preset if you'd rather use Visual Studio. |
| 7-Zip | Only for packaging. `tools/release/make-c2-package.ps1` looks for `7z.exe` on `PATH` and in the usual install locations and throws if it can't find one. |

Python **3.10 or newer** is required when building the default test configuration
(`Carnivores2Tests.ConfigDefaults`) and by the VS Code deployment task's
non-destructive config completion helper. Put `python` on `PATH` for deployment.
The engine executable itself does not require Python; configure with
`-DC2_BUILD_TESTS=OFF` to skip the test targets (and this requirement), as the
CI release legs do.

### Why 32-bit

`Hunt/Renderer/renderasm.cpp` contains inline x86 assembly in the hot
transform paths. There is no 64-bit configuration and adding one would mean
rewriting that file. If your Visual Studio install is 64-bit-only, add the x86
build tools, otherwise the compile fails on the assembly long before anything
useful is reported.

## Presets

| Preset | What you get |
|---|---|
| `ogl-release-shipping` | OpenGL engine, Release, perf hooks compiled out. The one to build, and the one the packaging script uses. |
| `ogl-release` | The same build with `GL_PERF_HOOKS=ON`. Build this if you want `F11` capture or `glperf_logging`. |
| `ogl-debug` | OpenGL, Debug, `MEM_DEBUG=ON`, renderer logging on. |
| `soft-release` | Software renderer, Release. |
| `soft-debug` | Software renderer, Debug, `MEM_DEBUG=ON`, renderer logging on. |
| `menu-release` | Menu only. No game executable. Fastest build in the repo. |
| `clang-tidy` | Inherits `ogl-debug`, exports `compile_commands.json`. |
| `ogl-pgo-instr` | Experimental instrumented PGO build. See below. |
| `ogl-pgo-opt` | Experimental PGO build that links against a trained profile. |

```powershell
cmake --preset ogl-release-shipping
cmake --build build/ogl-release-shipping
```

Output goes to `build/<preset>/bin/`: `Carnivores1_GL.exe`,
`Carnivores1_SOFT.exe`, `Carnivores2Menu.exe`, and a copy of `shaders/`.

The renderer is a compile-time choice (`-DRENDERER=GL` or `-DRENDERER=SOFT`, set
by the presets). Each engine binary has a fixed backend. The `renderer` key in
`config.cfg` **does select which binary the menu launches**: 0 Software,
1 OpenGL. It does not turn a running engine into the other backend.

## Tests

Twelve native test executables, two PowerShell validator suites and two Python
suites register with CTest (sixteen default suites in a GL build, fifteen
without the hidden-GPU one), plus two integration smoke tests that are off
by default. `-DC2_BUILD_TESTS=OFF` skips building the test targets entirely.

```powershell
cmake --preset ogl-release-shipping
cmake --build build/ogl-release-shipping
ctest --test-dir build/ogl-release-shipping --output-on-failure
```

Turning on the smoke tests requires a real installation with an MEE data set:

```powershell
cmake --preset ogl-release-shipping -DENABLE_DEPLOYED_SMOKE_TEST=ON -DCARNIVORES_LEGACY_DIR=E:/Games/CarnivoresLegacy
```

Those two tests run the deployed game from `CARNIVORES_LEGACY_DIR`, not the
build output, because the engine loads `HUNTDAT`, shaders, `config.cfg` and its
logs by relative path, so the installed directory is the process working directory.
A missing file or directory fails the test on purpose; don't paper over it by
pointing it at an empty folder.

`Carnivores2Tests.HuntdatValidator` shells out to `tools/check_huntdat.ps1`,
which is described in [DEBUGGING.md](DEBUGGING.md).
`Carnivores2Tests.ShopAssetValidator` exercises `tools/check_mod_assets.ps1`;
see [MODDING-SHOP.md](MODDING-SHOP.md).

CI runs on every push; see `.github/workflows/build.yml`.

## Packaging

The packaging script doesn't compile anything. Build the two release presets
first, then assemble:

```powershell
cmake --preset ogl-release-shipping
cmake --build build/ogl-release-shipping
cmake --preset soft-release
cmake --build build/soft-release
```

```powershell
pwsh tools/release/make-c2-package.ps1 `
  -OpenAlDir E:/Games/CarnivoresLegacy `
  -ReleaseNotesPath ./release-notes.md
```

Both arguments are required and the script throws without them. `-OpenAlDir`
points at any folder holding `OpenAL32.dll` and `alsoft.ini`. Those aren't
redistributable from this repository, so they have to come from your own
install. `-ReleaseNotesPath` is a markdown file shipped as `RELEASE-NOTES.md`,
and it has to sit outside the staging folder.

It also needs 7-Zip on `PATH`, or `-SevenZipPath`, and Python 3.10+ on `PATH`,
or `-PythonPath`, for the config template.

The result in `pkg/`:

```
Carnivores2Menu.exe      launcher and settings
v_gl.ren                 OpenGL engine
v_soft.ren               software engine
shaders/                 GLSL
OpenAL32.dll, alsoft.ini audio
config.cfg               clean defaults
THIRD-PARTY-LICENSES/    licence texts
RELEASE-NOTES.md         your -ReleaseNotesPath file
```

The `.ren` extension looks like a leftover but it isn't optional. The menu
launches those two names because that's what the upstream engine's menu did, so
the executables have to be named that way for the launcher to find them. The
script renames them for you; if you're doing it by hand, rename them yourself.

Two things worth knowing about what it does:

- **It refuses stale binaries.** Every executable's file version is checked
  against `VERSION_STRING` in `Hunt/Core/Version.h`, and a mismatch throws
  rather than shipping old code under a new label. If you get that error,
  rebuild the release presets.
- **`config.cfg` is regenerated, not copied.** The script runs
  `tools/c2_config_defaults.py`, which emits the engine's first-run template
  (`CreateDefaultConfig()` in `Hunt/Game/EngineInit.cpp`), so the package and
  the engine cannot drift apart. The deployed folder's dev-tweaked `config.cfg`
  is deliberately never shipped.

The package contains no data set. Drop your MEE 1.11 `HUNTDAT` next to the
menu. `OpenAL32.dll` is there because the engine loads it by name rather than
linking it; without it you get silence rather than an error.

There's also `tools/check_release_flags.ps1 -BuildDir <dir>` if you want to
confirm a release binary was linked with the flags it was supposed to be.

## Profile-guided optimization (experimental)

Two presets exist, both labelled experimental, and neither is used by a normal
build. They require an MSVC GL Release configuration and a shared `.pgd` file
(configured through `C2_PGO_MODE`/`C2_PGO_PGD_FILE`). `ogl-pgo-instr` links
with `/PGOINSTRUMENT`, `ogl-pgo-opt` with `/PGO` against a profile database.

**I have not measured this.** The instrumented build compiles and produces a
`.pgd`, but no gameplay has been played through it, no `.pgc` merged, and no
optimised build benchmarked against a plain Release build. Treat it as wired up
and unproven. If it turns out slower, that's a real possibility and nobody would
be surprised.

The shape, if you want to try it:

```powershell
cmake --preset ogl-pgo-instr
cmake --build --preset ogl-pgo-instr --target Carnivores1
```

Deploy `build/ogl-pgo-instr/bin/Carnivores1_GL.exe` into an install that has your
MEE data set, play something representative, meaning a real session and not a
launch-to-menu smoke test, then quit normally. Find the `.pgc` files it leaves
in that directory and merge each one into the profile:

```powershell
pgomgr /merge 'E:/Games/CarnivoresLegacy/<name>.pgc' 'build/ogl-pgo-instr/bin/Carnivores1_GL.pgd'
pgomgr /summary 'build/ogl-pgo-instr/bin/Carnivores1_GL.pgd'
```

Check the run counts are non-zero before you trust the database, then:

```powershell
cmake --preset ogl-pgo-opt
cmake --build --preset ogl-pgo-opt --target Carnivores1
```

Both presets point at the same `.pgd`, so keep the instrumented build and every
collected `.pgc` until the optimised link finishes. Rebuilding the instrumented
executable or deleting the database means training again. To profile without
the GL perf hooks in the binary, configure both presets with
`-DGL_PERF_HOOKS=OFF` first, because they inherit `ON` from `ogl-release`.

## Memory debugging

The debug presets set `MEM_DEBUG=ON`, which turns on the arena's leak
attribution: every leaked block reports the subsystem and generation that
allocated it rather than just an address. It's noisy in a Release build and
mostly useless, so it stays off outside the debug presets.

## Static analysis

```powershell
cmake --preset clang-tidy
pwsh tools/run_clang_tidy.ps1
```

Configuration for the rules is in `.clang-tidy` and notes on running it are in
`tools/clang-tidy.md`.

### MSVC warning baseline

A global `/WX` is not enabled: the legacy tree still has hundreds of unique
warning sites, mostly in headers compiled into many translation units. CI
instead captures each preset's build log and checks it with
`tools/check_msvc_warnings.ps1` against
`tools/warning-baseline/<preset>.txt`. A new file/warning-code pair, or a
higher unique-line count for an existing pair, fails the build. Fixing
warnings is always allowed; intentionally accepting new ones requires
regenerating the baseline with `-UpdateBaseline` and reviewing the diff.

The gate is a measured baseline, not a zero-warning target. At introduction the
`ogl-debug` baseline was 553 unique sites (mostly C4459 and C4244) and the
`menu-release` baseline was 22 (tests disabled, as in CI).
`-DC2_WARNINGS_AS_ERRORS=ON` remains available for focused local work but is not
enabled globally.

## Third-party build dependencies

Vendored under `deps/`: `glad` (OpenGL 3.3 loader) and the Khronos
`khrplatform.h`. googletest is downloaded by `FetchContent` on first configure,
so that step needs network access. `OpenAL32.dll` is loaded by name at runtime
and isn't a build input.

`deps/Glide2/` is gitignored and nothing in the build refers to it any more. The
Glide backend was removed, and it only ever mattered for building the base
engine.