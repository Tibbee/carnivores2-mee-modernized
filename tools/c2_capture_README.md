# C2 unattended correctness and performance captures

`c2_capture_matrix.ps1` defaults to a **single max-view terrain stress test**:
Area 4 rotation + translation, 2560×1440, view option 255 (radius 230),
three-second warm-up, 120 captured frames, and automatic exit. OpenGL uses a
**hidden, non-activating window**: no focus grab, mouse capture, simulated
keys, display-mode change, audio, or desktop screenshot. You can keep using
other applications while it runs. Correctness runs are capped at 60 FPS to
reduce desktop contention; they are not performance benchmarks.

Verified on the owner's NVIDIA RTX 3070 on 2026-10-02: the default run completed
in about eight seconds including harness setup (incremental build had no work),
with a non-black 2560×1440 framebuffer, 120 CSV rows, no GPU-query drops, no
precache-read failures, and normal automatic shutdown. This is driver-specific
validation, not a guarantee for every OpenGL ICD.

## Renderers

`-Renderer gl` (the default) uses `ogl-release` with GLPerf enabled. Background
capture requires that instrumented GL build; Shipping rejects the capture flag
before creating any window. `-Foreground` explicitly opts into the old visible
window/F11/desktop-capture workflow. There is no automatic foreground fallback.

Software capture remains available only with **`-Renderer soft -Foreground`**,
using `build/soft-release` and producing `frame.png` plus runtime logs.

C2 selects its renderer **at compile time**, not from `config.cfg`, so the
executable identity is authoritative; the `renderer` key is written into the
seeded config only to keep the captured settings self-describing. A `soft` run
deliberately omits the GL-only machinery rather than emitting zeros for it:

| | `gl` | `soft` |
|---|---|---|
| Preset / executable | `ogl-release` / `Carnivores1_GL.exe` | `soft-release` / `Carnivores1_SOFT.exe` |
| `GL_PERF_HOOKS=ON` required | yes | no |
| Automatic capture, 120-frame CSV, `glperf.log` | yes (F11 only with `-Foreground`) | no — and a stray GLPerf file is a run failure |
| ReShade/`opengl32.dll` removal + module check | yes | no |
| Deployed/build shader parity | yes | no (no shader stage) |
| `summary.json` | CPU distribution + counters | frame geometry and renderer label only |

Session directories are suffixed `-gl-` / `-soft-` so the two never mix. This
exists because the software renderer work (sky modes, water mipmapping, wave
falloff) needs repeatable before/after frames, and the matrix previously
pinned `renderer = 1` and could not produce them.

## Measurement policy

Use **`-Purpose benchmark`** deliberately for performance evidence. It defaults
to the fixed-camera `area4_spawn` scenario and a ten-second warm-up. Keep the
machine idle for comparable timings; background capture avoids interruption,
not CPU/GPU competition. Match capture mode across control and candidate:
hidden-window driver behavior, normal process priority, and disabled audio mean
background timings cannot be compared directly with foreground gameplay.
Metadata records purpose, mode, framebuffer source, and timing caveats.

The benchmark baseline targets renderer performance rather than presentation:

- `ogl-release`, with `GL_PERF_HOOKS=ON`
- ReShade and local graphics wrappers rejected at runtime
- unlimited engine FPS (`fps_limit 0`)
- FOV 62
- maximum object detail 96
- maximum view option 255 (render radius 230)
- all GPU feature flags enabled
- fixed landing tile; Area 4 benchmark uses `-camfixed` to pin its full pose
- observer mode with no selected huntable dinosaur or weapon for area baselines
- 10-second warm-up, then one automatic 120-render-frame capture

GLPerf CPU time ends before `SwapBuffers`; it excludes VSync, variable-refresh,
compositor, and presentation waits. CSV GPU columns are `-1` by design. Resolved
GPU measurements are in `glperf.log`. CPU and GPU measurements overlap and must
not be added together.

Observer mode prevents player death and close-range selected-dinosaur occlusion,
but ambient actors, wind, and the animation clock are not frozen. Fixed landing
coordinates stabilize terrain composition. Use repeated samples for performance
claims rather than expecting byte-identical screenshots.

Routine correctness runs test **only max view**. Lower view distances require
an explicit `-Purpose benchmark -ViewOption N` invocation; no automatic
distance sweep exists. The fast asset-free native coverage tests still check
several radii and camera poses in less than a second without opening the game.

`area4_camtravel` implies `-camspin` and includes pitch changes and horizontal
motion across the camera-grid snap boundaries. Background motion advances one
full yaw revolution per 120 rendered frames, independent of FPS. Every terrain
and water grid-position read is checked against the refreshed precache rows.
A failure is logged without a modal dialog. This catches stale reads, not every
possible visual defect; manual walking/swimming and pocket-fog boundary review
remain separate acceptance checks.

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
rem Routine stress test: max view only, one hidden run, no desktop interruption.
powershell -NoProfile -File tools/c2_capture_matrix.ps1

rem Performance comparison: fixed camera, unlimited FPS; keep the machine idle.
powershell -NoProfile -File tools/c2_capture_matrix.ps1 -Purpose benchmark -Repeats 3

rem Visible testing is explicit, never a background-mode fallback.
powershell -NoProfile -File tools/c2_capture_matrix.ps1 -Renderer soft -Foreground -Scenario area1_spawn -Resolution 1024x768
```

Useful controls:

```text
-Renderer gl|soft
-Purpose correctness|benchmark
-Foreground
-ViewOption N                 (non-max values require benchmark purpose)
-WarmupSeconds N
-PostCaptureSeconds N         (foreground only; default 0)
-Repeats N
-SkipBuild
-DryRun
-OutDir PATH
-All                         (explicit, expensive scenario/resolution matrix)
-AllowDirtyBuildInputs       (diagnostic runs preserve the source patch)
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

Those window modes apply to foreground captures. Background runs use an
exact-size hidden popup at any listed resolution. The harness verifies the
engine client size; it never externally resizes the window. The engine writes
a bottom-up BGRA BMP from the final OpenGL back buffer, after the last measured
frame and before swapping. The harness validates dimensions/non-black pixels,
converts it to `frame.png`, and removes the intermediate BMP. Other windows
cannot obscure the image.

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
6. `Game normal shutdown` after automatic completion (or foreground `WM_CLOSE`);
7. background mode: framebuffer saved, correct dimensions, non-black image,
   and no observed visible/focused renderer window.

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

## Sky mode probe

`c2_capture_sky_modes.ps1` is a behavioural harness for the sky cloud-texture
placement. It seeds `sky_mode` (OpenGL) or `sky_mode_soft` (software) into the
deployed `config.cfg`, runs the fixed `area1_spawn` scenario once per mode
through the capture matrix, and copies each `frame.png` into
`tools/artifacts/sky-current/staging-<renderer>/`. Modes 0 and 1 are captured
for both renderers; the dome (2) is OpenGL-only. The original `config.cfg`
bytes are restored afterwards.

The matrix is invoked with `-Foreground`: unattended software captures are
rejected, and the desktop-pixel frame is the image the modes are compared on,
so the game window is visible while the probe runs. Judge a change by
comparing the mode folders of one run side by side.
