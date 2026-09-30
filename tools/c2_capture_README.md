# C2 performance capture matrix

`c2_capture_matrix.ps1` runs fixed C2 scenarios through a renderer capture
harness and preserves a self-contained evidence set for each run. It is based
on C1's proven capture runner, adapted for C2's configuration, trophy layout,
asset roster, executable names, native-resolution target, and stricter output
validation.

## Renderers

`-Renderer gl` (the default) is the original OpenGL 3.3 performance matrix and
behaves exactly as it always has. `-Renderer soft` reuses the same scenarios,
resolutions, landing coordinates, window discovery, and client capture against
`build/soft-release`, producing `frame.png` plus the runtime logs.

C2 selects its renderer **at compile time**, not from `config.cfg`, so the
executable identity is authoritative; the `renderer` key is written into the
seeded config only to keep the captured settings self-describing. A `soft` run
deliberately omits the GL-only machinery rather than emitting zeros for it:

| | `gl` | `soft` |
|---|---|---|
| Preset / executable | `ogl-release` / `Carnivores1_GL.exe` | `soft-release` / `Carnivores1_SOFT.exe` |
| `GL_PERF_HOOKS=ON` required | yes | no |
| F11 capture, 120-frame CSV, `glperf.log` | yes | no — and a stray GLPerf file is a run failure |
| ReShade/`opengl32.dll` removal + module check | yes | no |
| Deployed/build shader parity | yes | no (no shader stage) |
| `summary.json` | CPU distribution + counters | frame geometry and renderer label only |

Session directories are suffixed `-gl-` / `-soft-` so the two never mix. This
exists because the software renderer work (sky modes, water mipmapping, wave
falloff) needs repeatable before/after frames, and the matrix previously
pinned `renderer = 1` and could not produce them.

## Measurement policy

The primary baseline targets renderer performance rather than presentation:

- `ogl-release`, with `GL_PERF_HOOKS=ON`
- ReShade and local graphics wrappers rejected at runtime
- unlimited engine FPS (`fps_limit 0`)
- FOV 62
- maximum object detail 96
- maximum view option 255 (render radius 230)
- all GPU feature flags enabled
- fixed landing tile and default camera heading
- observer mode with no selected huntable dinosaur or weapon for area baselines
- 10-second warm-up, then one 120-render-frame F11 capture

GLPerf CPU time ends before `SwapBuffers`; it excludes VSync, variable-refresh,
compositor, and presentation waits. CSV GPU columns are `-1` by design. Resolved
GPU measurements are in `glperf.log`. CPU and GPU measurements overlap and must
not be added together.

Observer mode prevents player death and close-range selected-dinosaur occlusion,
but ambient actors, wind, and the animation clock are not frozen. Fixed landing
coordinates stabilize terrain composition. Use repeated samples for performance
claims rather than expecting byte-identical screenshots.

## Requirements

- x86 `build/ogl-release`, generated from the `ogl-release` preset
- `GL_PERF_HOOKS:BOOL=ON` in its CMake cache (GL runs only)
- legally obtained C2 deployment, defaulting to
  `E:/Games/CarnivoresLegacy`
- deployed shaders byte-identical to `build/ogl-release/bin/shaders` (GL only)
- no changed or untracked engine/build inputs
- game and standalone menu closed before starting

`CARNIVORES2_LEGACY_DIR` overrides the deployed directory.

The runner launches the build executable directly with the deployed directory
as its working directory. C2 resolves `config.cfg` beside the executable, so the
runner seeds a temporary executable-local copy from the deployed configuration.
The deployment's ReShade proxy, `opengl32.dll`, is removed before process creation
and restored byte-for-byte afterward. Loaded modules provide an additional check.
The deployed game executable is not replaced.

## Usage

From the C2 repository root:

```bat
powershell -NoProfile -File tools/c2_capture_matrix.ps1 -Scenario area1_spawn -Resolution 1024x768
powershell -NoProfile -File tools/c2_capture_matrix.ps1 -Renderer soft -Scenario area1_spawn -Resolution 1024x768
powershell -NoProfile -File tools/c2_capture_matrix.ps1 -All -Repeats 3
```

Useful controls:

```text
-Renderer gl|soft
-WarmupSeconds N
-PostCaptureSeconds N
-Repeats N
-SkipBuild
-DryRun
-OutDir PATH
```

The runner builds once before the matrix unless `-SkipBuild` is supplied. It
freezes and records the resulting executable hash for the complete invocation.

## Scenarios and resolutions

`c2_capture_manifest.json` is the canonical matrix. Initial ready scenarios
cover the deployed Areas 1–5, 7, 8, and the Trophy room; the owner's Area 4 is a
replacement modded map and is identified by its artifact hash. The slot-six
`external` path stays excluded (`ready: false`): its launch crash was fixed at
`f2c1700` (2026-09-10), but this deployment never shipped `hunter1_a6.car`, so a
full slot-six load still halts on the missing asset and the map remains an Area 1
clone — not a meaningful performance fixture.
Water-edge and character-heavy fog scenarios remain deferred until their camera
composition is reviewed and frozen.

Ready resolutions are:

| Resolution | Mode | Role |
|---|---|---|
| 1024×768 | Windowed | Legacy-resolution CPU and visual control |
| 1920×1080 | Windowed | Modern scaling control |
| 2560×1440 | Borderless | Owner's native workload; keeps the full client visible |

The harness verifies the physical engine client size. It never externally
resizes the window because that would not update C2's gameplay GL viewport.

## Artifact layout

Each invocation creates a timestamped session and one directory per
scenario/resolution/repeat:

```text
tools/artifacts/perf-current/session-<UTC>-<SHA>/
  manifest.json
  area1_spawn__1024x768__r01/
    frame.png
    capture.csv
    glperf.log
    glperf-startup-01.log  # only if a slow load initialized an earlier renderer
    render.log
    carnivor.log
    summary.json
    metadata.json
```

`metadata.json` records source and executable hashes, exact command line,
settings, loaded OpenGL module, machine/driver identity, scenario-asset hashes,
and artifact hashes. `summary.json` contains CPU distribution, counter means,
and per-scope CPU summaries.

For every successful **GL** run, the runner requires:

1. exactly 120 CSV rows numbered 0–119;
2. the expected counter columns and intentional `-1` GPU sentinels;
3. resolved GPU samples in the rolling log;
4. `gpu_drop=0` throughout the run;
5. no `ABNORMAL_HALT`; and
6. `Game normal shutdown` after `WM_CLOSE`.

Items 1–4 are GL-only. Items 5 and 6 apply to **both** renderers, and a
`soft` run additionally fails if it produced any GLPerf file at all, which
would mean the wrong executable or a stale harness was picked up.

The original executable-local `config.cfg`, deployed `trophy00.sav`,
`trophy00.sab`, `opengl32.dll`, `render.log`, and `carnivor.log` bytes are
restored in `finally`, including after a failed run.
A failed run keeps its isolated directory, `failure.txt`, and any attributable
GLPerf files for diagnosis, then removes those generated files from the deployment.
Generated `tools/artifacts/` files remain ignored and must not be committed.

## Flashlight beam probe

`c2_capture_flashlight.ps1` is a behavioural (not performance) harness for the
night-hunt flashlight. It launches area1 at `dtm=2` with `-flashlight`, captures
the frame before and after posting the `F` toggle, then pitches the camera to
the ground by parking the cursor below the client centre (the game reads the
cursor delta each frame and re-centres it) and captures again. It prints mean
luminance per screen region for each state.

The region deltas are the point: with the beam working, `pitched/crosshair`
must rise when the light is switched on. It fell (`-2.6`) while the beam was
evaluated from per-vertex scalars, and rose to `+28.0` once the shaders derived
the beam from the interpolated view position. Treat a negative crosshair delta
as a beam regression, not as noise.
