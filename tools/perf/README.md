# C2 performance tools

> **Last verified:** 2026-10-09
> **Applies to:** [C2], OpenGL `main`
> **Status:** Asset-backed hidden captures validated; 25 offline cases/four matrices pass. Old/new comparison is noise-limited, not a definitive overhead or foreground FPS result.

## CSV helper

From the engine repository:

```powershell
perl tools/perf/glperf_stats.pl path/to/capture.csv
perl tools/perf/glperf_stats.pl baseline.csv changed.csv
python tests/test_perf_tools.py
```

The helper moved from the non-Git workspace root. It now fails visibly for
missing files/columns, malformed rows, nonnumeric/nonfinite or negative values.
It summarizes every current scope/workload column, including previous frame
interval/swap. Median averages the middle pair; p95/p99 use nearest rank.
GPU and previous-frame `-1` sentinels are unavailable, not timings. The current
engine always writes GPU CSV fields as `-1`; resolved rolling-log GPU samples
cannot be substituted into these per-frame comparisons.

Perl uses core modules only. Python 3.10+ uses the standard library. CTest runs
`tests/test_perf_tools.py`; Perl-specific tests explicitly skip if Perl is absent.
The local verification ran with Perl present and no skips.

## Workbench overhead protocol

`workbench_perf.py` separates preparation, explicit execution, and offline
analysis. It does **not** replace the installed preview or require desktop input.
Normal launches/Shipping behavior are unchanged. An instrumented GL Release
launch can explicitly select `-capture-workbench=closed|ui|test` together with
`-capture-background`, warm-up and framebuffer-output arguments. The engine
sets the real F10/F8 surface before warm-up via its semantic message handler,
then verifies its mode on every frame. F11 is not injected or exempted from UI
input ownership. Initialization/fallback/mode failures must not count as timings.

The default series is three repeated **closed / full UI / closed** triples.
`--include-test` adds separate closed / Test strip / closed triples, never mixing
the strip into full-inspector results. Each process uses the same copied binary,
assets, configuration, profile, Area 4 observer fixture, fixed camera and daytime.
The full UI is the default inspector, not expanded Help/Changes/RGB popups.

- Resolution **2560x1440**, view option **255**, derived radius **230**.
- Warm-up **3 seconds**, then **120 measured frames**, automatic normal exit.
- CPU renderer time includes workbench submission but excludes SwapBuffers.
- Previous frame interval includes simulation/presentation/pacing; previous swap
  is lagged. Neither is display/input latency.
- Report individual median/mean/p95/p99/spread, before/after closed drift,
  bracketed median deltas and engine workload counters. Precache reference is
  approximately **1.9 ms**, not an automatic universal PASS threshold.
- Ambient actors/wind/animation remain live. Inspect workload and drift before
  attributing a difference to the UI. Counters do not include ImGui draw work.
- **Hidden/no-audio renderer measurements are not foreground gameplay FPS.**
  This estimates open-surface cost on the current feature, not every historical
  renderer change or a feature-vs-main closed-engine comparison.

## Prepare without launching

First checkpoint source changes and rebuild `ogl-release` from an x86 MSVC shell:

```powershell
cmake --preset ogl-release
cmake --build build/ogl-release
python tools/perf/workbench_perf.py prepare --game-dir E:/Games/CarnivoresLegacy --output build/workbench-perf/session-01 --include-test
```

Output must be a **new** ignored directory under this checkout's `build/`.
Preparation refuses an existing game/menu, Shipping/arbitrary binaries, dirty
engine/build inputs and binaries older than source inputs. It makes a full
independent `HUNTDAT` copy (~1.4 GiB for the current deployment), optional
`MULTIPLAYER` assets (the hitbox CAR is loaded in offline hunts too), plus build
shaders, executable and cloned config/profiles. No writable junctions/hardlinks,
ReShade proxy, deployment, original config edits or original trophy edits.
Only copied measurement keys/view bytes change. Original lighting customization
and other settings survive in the cloned baseline. Each run restores the same
copied inputs; original file inventory/hashes are checked after the series.
Fixtures contain licensed/private material: do not commit or redistribute output.

## Run only in an agreed testing session

```powershell
# Explicit launch command; preparation/analyze/help never launch the game.
python tools/perf/workbench_perf.py run build/workbench-perf/session-01/session.json
# Offline revalidation/report; no engine or computer use required.
python tools/perf/workbench_perf.py analyze build/workbench-perf/session-01/session.json
```

The runner checks that owned windows stay hidden and do not take foreground.
There is no SendInput, focus activation, cursor warp, automatic process kill,
profile cleanup or overwrite/resume of completed evidence. On a timeout/failure,
it records the owned PID; if still alive, **close it yourself** before another run.
No assertion of foreground behavior follows from these checks.

The session retains source/PE/config/profile/asset hashes, cache identity,
CPU/GPU/driver/OS metadata, per-run commands/PIDs, CSVs, logs and final BMPs.
Analysis requires complete alternating 120-frame captures, unchanged evidence,
mode/completion confirmation and original-file preservation. `report.json` keeps
Test/UI separate and exposes drift rather than granting an automatic speedup.
Review the final framebuffers to confirm the intended inspector/scene.

**Live validation (2026-10-09):** 58 complete 120-frame captures compare native
closed/UI/Test and pre-workbench collision/audio main. The first real attempts
exposed missing offline-required MULTIPLAYER assets and a hidden backend-attachment
skip; both are corrected, with failure evidence retained. Five-repeat confirmation
estimates UI minus new-closed at +0.311 ms median, but repeat range -2.376..+3.020 ms
and closed drift up to 5.407 ms prevent a reliable small-cost/regression claim.
Precache remains ~1.9 ms. See the
[measurement record](../../../CarnivoresDoc/working/sessions/c2-workbench-performance-2026-10-09.md).
Computer use was unnecessary; foreground timing remains a separate agreed session.

[Workbench behavior](../../docs/LIVE-WORKBENCH.md)
· [canonical acceptance](../../../CarnivoresDoc/working/sessions/c2-workbench-acceptance-2026-10-07.md)
