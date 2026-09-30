# Debugging, logging and data validation

Written for the two kinds of person who end up here: someone whose hunt crashed
or behaved strangely, and someone who just changed a map or `_RES.TXT` and wants
to know whether the problem is their data or the engine.

## Keys

| Key | |
|---|---|
| `F10` | Debug menu. Toggle with `F10`, pick a value with the arrow keys, change it with `+`/`-`, `PgUp`/`PgDn` to change tab, `D` to print the current values, `F10` again to close. |
| `F11` | Capture one second of per-frame CPU/GPU counters to a CSV. OpenGL builds compiled with `GL_PERF_HOOKS` only. |
| `F12` | Screenshot. |
| `F9` | Quit the hunt cleanly. |
| `Enter` | Toggle fullscreen. Disabled in survival mode. |

The `F10` menu is live: the values it changes take effect on the next frame, so
you can stand in the water and walk the fog density down until you can see. It
works in both renderers. It does not persist, so `D` prints what you found and you
can write it into `config.cfg` or a shader by hand.

Three tabs:

| Tab | Values |
|---|---|
| **FOG** | vertical range, vertical strength, curve exponent, soft cap base, cap camera boost, base density multiplier, camera depth multiplier |
| **WAVES** | primary swell, secondary cross-wave, fine detail amplitudes, speed |
| **SUN** | master glare strength, sun/moon disc alpha (multipliers on the computed values; `0` disables the layer) |

## Log files

All written next to the executable, which is also why the deployed directory is
the working directory when you test.

| File | |
|---|---|
| `carnivor.log` | Structured runtime log. Session start and clean exits are recorded here. |
| `render.log` | The older `PrintLog` channel, plus the AI trace when that's enabled. |
| `menu.log` | The standalone menu. |
| `glperf-*.log` | Rolling one-second averages, only with `GL_PERF_HOOKS` and `glperf_logging 1`. |
| `glperf-capture-*.csv` | The `F11` capture. |

Enable the verbose channel with `verbose_logging 1` in `config.cfg`.

## Tracing creature behaviour

If a dinosaur does something you didn't ask for, this is the first thing to
turn on:

```
ai_logging 1
```

It writes to `render.log` and costs nothing when idle. Two kinds of line:

- **One per hunter event**: sight, scent, a heard shot, a hit, a call,
  contact-range promotion, aquatic proximity, startle scatters. Every awareness
  acquisition is logged, so the trigger for any reaction is always visible.
  Perception refreshes stay silent, otherwise the file drowns.
- **One per second per reacting creature**: species name and id, state,
  position, the destination it has actually stored, the live distance to the
  hunter, and how much reaction time is left.

The second kind is the one that answers questions. When an animal walks away
from you instead of toward you, the line tells you whether it thinks you're
somewhere else, whether its timer already expired, or whether it never
registered you at all. You do not need a debugger for any of that.

`verbose_logging 1` also turns this on, but the separate switch is clearer about
what you did.

## Validating mod data

The engine opens data by paths recorded in its own scripts, and a missing file
is a fatal halt rather than something it skips past. So before you file a bug
about a map that won't load, check the data.

```
pwsh tools/check_huntdat.ps1 -GameDir E:/Games/CarnivoresLegacy
```

It walks the same paths the engine does and reports:

- every model path in `HUNTDAT/_RES.TXT` that doesn't exist, with the entry that
  names it
- every area slot in `_MENU.TXT` (or its `_RES.TXT` fallback) whose `.map`/`.rsc`
  pair is incomplete, and that combination halts the game at map load
- `.c2map` descriptors whose launch path has no complete pair
- the trophy room pair the menu opens
- missing description and thumbnail files

It changes nothing, the output is meant to be pasted into a bug report, and it
exits `1` on errors. `-Strict` also fails on warnings. Textures and sounds are
deliberately not checked, because the engine reports those differently and
listing them would be noise.

For equipment prices, icons and descriptions, also run:

```powershell
pwsh tools/check_mod_assets.ps1 -GameDir E:/Games/CarnivoresLegacy
```

It checks shop keys, effective overrides, image formats, text and block
placement without changing the installation. `-Strict` also fails on warnings.
[MODDING-SHOP.md](MODDING-SHOP.md) explains the supported authoring syntax.

### Strict load mode

By default the engine is forgiving on purpose. Malformed scalars keep a
fallback, out-of-range indices clamp, overlong text truncates, and each recovery
writes a diagnostic naming the line it came from. Old mods and 25-year-old
scripts keep working.

When you're validating a mod and want the first problem to stop everything:

```
load_mode strict
```

Or set `C2_STRICT_DATA=1` in the environment, which is easier for CI. Strict
mode halts on recoverable scalar/index problems. Structurally malformed files
and values that can't be made safe at all stay fatal in both modes. Menu art
and description diagnostics remain nonfatal; use the shop checker's `-Strict`
option when validating those assets.

`tests/test_legacy_scalar_differential.cpp` pins every recovered value to what
the original `atoi`/`atof` would have returned, so if someone ever tightens a
rule enough to break an old mod, it fails in CI before it reaches your hunt.

## `config.cfg`

Generated with inline documentation on first launch if it doesn't exist. The
menu rewrites only the keys it owns; your comments and any keys it doesn't know
about survive. Values are validated on load.

| Key | Default | |
|---|---|---|
| `renderer` | `1` | Selects the binary launched by the menu: 0 Software, 1 OpenGL. Each engine binary's backend is fixed at compile time. |
| `fov` | `62` | Degrees, 35-90. |
| `object_detail` | `48` | View distance / object detail, 24-96. |
| `fps_limit` | `1` | 0 unlimited, 1 = 60, 2 = 120, 3 = 240. |
| `display_mode` | `2` | 0 windowed, 1 exclusive fullscreen, 2 borderless. |
| `resolution` | unset | e.g. `1920x1080`. Omit to use the profile's own resolution. |
| `verbose_logging` | `0` | Verbose runtime logging. |
| `ai_logging` | `0` | AI awareness trace. See above. |
| `load_mode` | `lenient` | `strict` halts on the first recoverable data error. |
| `nightvision_key` | `78` | VK code; 78 is `N`. |
| `flashlight_key` | `70` | VK code; 70 is `F`. Only works on hunts started with the flashlight accessory. |
| `flashlight_inner_angle` | `14` | Half-angle in degrees, full strength inside it. |
| `flashlight_outer_angle` | `26` | Half-angle where the beam fades out completely. |
| `flashlight_range` | `6000` | World units; 256 is one terrain cell. |
| `flashlight_intensity` | `500` | Percent. 500 multiplies lit surfaces by up to 6.0 before roll-off. |
| `sky_mode` | `1` | 0 legacy camera-coupled, 1 world-level plane, 2 direction-based dome. |
| `sky_mode_soft` | `0` | Same values for the software renderer, separate key because it bands on 1. |
| `sky_horizon_drop` | `12` | Degrees. Lowers mode 1's compression singularity below the horizon. `0` is a plain level plane. |
| `sky_dome_scale` | `384` | Texels per radian for the dome. Larger = smaller clouds. |
| `terrain_mip_level` | `3` | 0-3 over the 128/64/32/16 chain. `3` uses the whole chain; `1` reproduces the old D3D/3DFX behaviour; `0` disables filtering. |
| `water_mipmap` | `1` | Software renderer only. Distanced water vs. legacy full resolution. |
| `water_wave_fade_rings` | `8` | Rings over which wave displacement fades to zero before the legacy cutoff. `0` is the hard cutoff. |
| `gpufeatures` | all on | Bitmask. `0` disables every GPU optimisation. |
| `glperf_logging` | `0` | Requires a `GL_PERF_HOOKS` build. Writes `glperf-*.log`. |

Audio reverb is tuned with commented-out `env0_*` overrides (`decay`, `decayhf`,
`diffusion`, `reverb`). The `envN` preset selects the environment: 0 Generic,
1 Plate, 2 Forest, 3 Mountain, 4 Canyon, 5 Cave, 6 Special-1, 7 no-op,
8 Underwater. Needs OpenAL EFX and a restart. Uncomment one to override the
compiled default.

The engine only writes `config.cfg` when the file doesn't exist, so a config
that predates a new feature keeps silently running that key on its default. To check effective settings, read the startup `Config effective:` entries in
`render.log`, then compare omitted keys with the documented defaults and
`CreateDefaultConfig()` in `Hunt/Game/EngineInit.cpp`. A local development-only
config comparison script is not part of this repository's distributed tools.

## Performance

`GL_PERF_HOOKS` is on in `ogl-release` and `ogl-debug`, off in
`ogl-release-shipping`. With it compiled in, `glperf_logging 1` writes rolling
one-second averages and `F11` captures 120 frames of per-frame counters to CSV.

For anything you intend to keep, use the capture harness rather than a manual
run. `tools/c2_capture_matrix.ps1` plays fixed scenarios at fixed coordinates and
resolutions and preserves a self-contained evidence set per run: `frame.png`,
the runtime logs, and a `summary.json`:

```powershell
pwsh tools/c2_capture_matrix.ps1 -Renderer gl
pwsh tools/c2_capture_matrix.ps1 -Renderer soft
```

Sessions are suffixed `-gl-` and `-soft-` so they never mix. A `soft` run omits
the GL-only machinery instead of emitting zeros for it, and a stray GLPerf file
in a software run is treated as a failure. `tools/c2_capture_README.md` has the
full comparison. There are dedicated capture scripts for the flashlight and for
the sky modes.

## Tests

Ten default CTest suites (eight native executables and two PowerShell
validator suites) run without an MEE data set present:

| Test | Covers |
|---|---|
| `Carnivores2Tests` | The maths and parsing layer: vectors, spawn maths, billboards, projectile maths, score mods, AI behaviour constants, the memory arena, game modes, trophy modes, key bindings, config parse and text, load validation and diagnostics, map scaling, flashlight, sky modes, sky/fog projection, terrain fog and mip policy, water colour, water visibility, wave fade, menu slider and launch args, and the legacy scalar differential |
| `Carnivores2Tests.FogSampling` | Fog sampling |
| `Carnivores2Tests.UIText` | HUD text layout |
| `Carnivores2Tests.LoaderContracts` | Links the real `SoundLoader.cpp` and checks its entry point |
| `Carnivores2Tests.PictureLoaderContracts` | Links the real `PictureLoader.cpp` so dimension and truncation checks run on production code |
| `Carnivores2Tests.CommandLineContracts` | `ProcessCommandLine()` against real global state, including `prj=` / `server=` bounds |
| `Carnivores2Tests.MenuResourceContracts` | The menu's `_MENU.TXT` reader with the `_RES.TXT` fallback, both grammars through the real `ReadCharacters()` / `ReadWeapons()` |
| `Carnivores2Tests.ScriptParserContracts` | The real `_RES.TXT` line readers, checking that text fields can't select the numeric dispatch or a block opener |
| `Carnivores2Tests.HuntdatValidator` | Runs `tools/check_huntdat.ps1` against fixtures |
| `Carnivores2Tests.ShopAssetValidator` | Runs the shop checker against placement, alias, price, comment and asset fixtures |

The `*Contracts` tests deliberately link the production source file rather than
reimplementing it. If one of those breaks, it's because the real entry point
changed behaviour.

Two more, `SmokeTest` and `TrophySmokeTest`, run the deployed game and need
`-DENABLE_DEPLOYED_SMOKE_TEST=ON` plus `CARNIVORES_LEGACY_DIR` set. See
[BUILDING.md](BUILDING.md).

## Before filing a bug

It saves a round trip if the report includes these:

1. `carnivor.log` from the session, including the clean exit line if you got one
2. Your `config.cfg`
3. Which renderer (`v_gl.ren` or `v_soft.ren`) and the resolution
4. The map or area name, and whether it loads at all or loads and then misbehaves
5. Output of `tools/check_huntdat.ps1` if a map is involved
6. For a creature that won't do what you expect: a slice of `render.log` with
   `ai_logging 1` on, covering the moment it went wrong

The first four cover most of it. The last one turns an AI argument into a
readable log.