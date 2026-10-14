# Consolidation Pass — C2 MEE

> **Last verified:** 2026-10-14 — WS1–WS4 complete. WS4: twenty-one slices across tooling, build/release and shaders; the owner visually verified the night-hunt flashlight unchanged after the GLSL include refactor. WS1: dead code removed; WS2: 686 → 469 cases in 17 clusters; WS3: config scanner unification plus thirteen unified-implementation slices and one owner-approved engine TGA validation fix. WS4 landed the shared Win32 helper, asset-checker prologue, runnable sky-mode harness, a Windows PowerShell-safe clang-tidy runner with pinned LLVM, the c2_test_support library, per-target LTO, the C2_BUILD_TESTS option, the engine-template-based package config, the trophy naming fix and the shared flashlight GLSL fragment. Twelfth four-preset checkpoint green (suite 487 cases); hygiene clean; clang-tidy exit 0 on the six batch TUs (local LLVM 23.1.0; CI pinned to choco 22.1.8).
> **Branch:** `refactor/consolidation-pass`, worktree `Carnivores2MEE1.11/build/consolidation-pass`, based on `feat/modder-workbench` @ `4d05faa`.
> **Status:** complete. Only the Progress log and the task list below are authoritative for what is done; the ranked estimates come from the Phase 1 hunts.

## Purpose

Remove LLM-grown bloat (duplicated implementations, dead code, over-abstraction,
redundant tests, tooling clutter) and leave leaner, better-engineered solutions.
Behavior is preserved by default; small local quality/robustness fixes are allowed
only when isolated in their own `fix(...)`/`change(...)` commit and listed in the
report. External contracts (file formats, `_RES.TXT`/`_MENU.TXT` semantics, config
keys, command-line flags, deployed layout) require owner sign-off, as do dependency
additions, CI-contract changes and deletions of files with unclear provenance.

## Baseline (Phase 0, all green)

| Preset | Configure | Build |
|---|---:|---:|
| ogl-debug | 4.5s | 14.9s (ctest 16/16, 20.2s) |
| ogl-release | 3.5s | 19.7s |
| soft-release | 3.8s | 15.1s |
| menu-release | 4.2s | 11.6s |

- Tests: 686 gtest cases in 12 executables + 4 script targets (ConfigDefaults, PerfTools, HuntdatValidator, ShopAssetValidator); `ctest` registers 16 default suites.
- Size: ~81.8k handwritten lines / ~248 files — Hunt/Game 21.8k, Hunt/Renderer 21.2k, tests 12.0k, Menu 7.4k, Hunt/Loaders 7.0k, Hunt/Core 5.5k, Hunt/Workbench 2.3k, rest 4.6k.
- The `feat/modder-workbench` diff vs `main` is dominated by ~65k vendored `deps/imgui/` lines — out of scope (same class as glad/KHR).

## Method and per-slice protocol

1. Re-verify every candidate at slice time (file:line), then make the minimal diff for one logical slice.
2. Every commit compiles in `ogl-debug` and passes the full registered suite; the four-preset matrix, clang-tidy, hygiene and release-flags checks run at slice-group checkpoints. The branch is never left red.
3. Before every commit, inspect `git diff --cached --stat`. Some files are stored with non-standard line endings (see Defects), so stage with filters disabled when needed and never let a commit contain whole-file renormalization churn.
4. Commit style: Conventional Commit, flat single-line bullets, trailing `Files:` line, no verification footers (canonical: `CarnivoresDoc/reference/commit-style-guide.md`).
5. Long tail work is delegated to read-only subagent hunts; the main session spot-checks cited evidence before acting on it.

## Ranked plan (Phase 1 inventory)

### WS1 — Dead code (~2,400–2,500 LOC)

| # | Item | Est. | State |
|---|---|---:|---|
| 1a | Disabled-species animation bundle: `AnimateMicro/Titan/Poacher/Huntdog/huntDogSearch` + CMake entries + fwd decls | ~1,774 | **done** — `9ff0122` |
| 1b | 12 functions with no caller (`ModelLoader.cpp:72,103,126,175,187,583`; `Resources.cpp:1065,1072`; `Interface.cpp:48,63`; `TerrainQueries.cpp:12`; `SoftWater.cpp:34`; `SoftTerrain.cpp:538`) | ~196 | **done** — `f3b60a3` |
| 1c | `_d3d`/`_3dfx`/`__rus` dead branches + no-op alpha-flag scaffold | ~110 | **done** — `376daeb` |
| 1d | Dead targeting/pack helpers (`CharacterTargeting.cpp:80,119,404`; `CharacterInternal.h:67,236,247`) | ~120 | **done** — `ca4c57e` |
| 1e | Commented-out "Superseded" state defines (`Characters.cpp`) | ~159 | **done** — `183fcc1` |
| 1f | Unused globals (`GameState.h:54,205,316,368,374,376,378,419`), dead declarations (`EngineAPI.h:95`, dup `:210/:304`; `Menu/Hunt.h:828`; `SoftInternal.h:88`; `NetworkManager.h:64-65`), split leftovers, unused `Hunt/Debug/Assert.h`, `Network.cpp` forwarding layer | ~120 | **done** — `a6ea274` |
| 1g | Stale comments/docs (`Resources.cpp:1244` 1999 build string, `README.md:141` F10 claims, `CHANGELOG.md:20,84`, `docs/ARCHITECTURE.md:13,143-146`, `Projectiles.cpp:1-4` header) | docs | **done** — `944ec20`, `3182616`, `a506ea4` |

### WS2 — Test-suite consolidation (686 → ~480 cases, ~20 clusters)

`test_map_scaling.cpp` tests test-local code (6→1, delete; production `GLHUD.cpp:450` never called) ·
`test_math.cpp` micro-cases (26→5) · ConfigMerge line-shape (29→5, keep `\v/\f` parity row) ·
source-string genre (23→4) · load-validate (19→9) · workbench persistence refusals (21→12) ·
bound-fit geometry (45→28) · workbench input (16→6) · WAV triple coverage (20→13) · FogSampling (14→6) ·
plus AI tables, water color/waves, menu resource/price, terrain fog, uniform cache, CLI helpers,
workbench three-layer (≈15) — combined ≈ −79.

Landed (2026-10-13): `test_map_scaling.cpp` deleted (−6, `d252d78`) · `test_math.cpp` 26→5 (`5259a6c`) · ConfigMerge 29→5 (`674b4c6`) · source-string genre: 23 source-only cases merged into four grouped `SourceContracts` tests, the hybrid workbench case stripped to its runtime checks (`bfb301e`). Suite: 686 → 616 cases.

Landed (2026-10-14): load-validate 19→9 (`a218fea`) · workbench persistence 21→12 across `test_workbench_config.cpp` and `test_workbench_gpu.cpp` (`bfa544d`) · bound-fit geometry 44→28 across `test_bound_fit.cpp`, `test_object_bounds.cpp` and the `ConfigParseBoundBoxFit` pair (`26e1222`; re-verification counted 44, not the estimated 45). Suite: 616 → 581 cases. Four-preset checkpoint green; hygiene re-run surfaced 2 pre-existing findings (see Defects 5-6).

Landed (2026-10-14, second batch): workbench input 14→6 (`ff26580`; re-verification counted 14, not the estimated 16) · WAV triple coverage 20→13 across `test_wave_reader.cpp`, `test_sound_loader_entry.cpp` and `test_menu_wave_entry.cpp` (`bd985e4`) · FogSampling 14→6 (`91d2e51`). Suite: 581 → 558 cases. Second four-preset checkpoint green.

Landed (2026-10-14, third batch): AI tables 35→15 (`1c678f4`; the behavior-math file became fifteen grouped tests with all 171 assertion statements preserved) · water color/waves 19→11 across `test_water_color.cpp`, `test_water_wave_cache.cpp` and `test_water_wave_fade.cpp` (`d5dd1bf`; 27+15+17 statements preserved) · menu resource/price 41→20 (`b16be7b`; 197 statements preserved). Suite: 558 → 509 cases. Third four-preset checkpoint green; clang-tidy now runs locally against `build/clang-tidy` and reported no blocking findings on the five changed files.

Landed (2026-10-14, fourth batch): terrain fog 18→8 across `test_terrain_fog.cpp` and `test_terrain_fog_smoothing.cpp` (`f838e27`; 33+3 statements and all 8 SameColor call sites preserved) · uniform cache 6→3 (`daa2172`; 31 statements preserved) · CLI helpers 11→5 across `test_command_line_parse.cpp` and `test_command_line_entry.cpp` (`13c0b28`; 33+52 statements preserved). Suite: 509 → 490 cases. Fourth four-preset checkpoint green.

Landed (2026-10-14, fifth batch — WS2 close): workbench session/runtime layers 52→31 across `test_workbench_session.cpp` and `test_workbench_runtime.cpp` (`429a0e5`; 182+192 statements preserved; merged scenarios keep fixture continuity by ordering or explicit seam resets). Suite: 490 → 469 cases. Fifth four-preset checkpoint green. WS2 closed at 469 cases (17 clusters).

### WS3 — Unified implementations (~1,300 LOC)

Foundational first: config path resolution ×4 (~45) · load-policy wrapper ×2 (~15) · ConfigPersistence classifier (~20) ·
full config scanner unification ×4 (~135, staged, needs parity harness) · shadow-TextOut ×6 (~40) ·
menu constants vs `Core/Constants.h` (~50) · resolution list ×2 (~70) · FPS limiter ×2 (~20) ·
DPI/VK helpers ×2 (~22) · ship spawn ×2 (~12) · script-key predicate (~30) · F10 overlay GL+Soft (~150-200) ·
bound-fit merge (~75) · idle-group selector across 8 animation files (~150-200) · menu OpenAL plumbing (~100) ·
TGA reader ×2 (~60) · env presets triple-source (~30) · test fixtures/helpers (~55).

Landed (2026-10-14): config path resolution ×4 → `Hunt/Core/ConfigPath.h` (`ResolveReadPath`/`ResolveWritePath`; `e38b060`) ·
load-policy pre-read ×2 → `ApplyLoadPolicyFromConfigFile` in `Loaders/LoadDiagnostics.h` (`f44c1fa`) ·
ConfigPersistence classifier → shared `kMaxConfigBytes` + `IsUtf16ConfigText` in `Core/ConfigText.h` (`ae57db8`).
Config-scanner unification (owner call: harness first, one consumer per commit): harness `e79b7d3` (`Core/ConfigScan.h` + parity corpus) ·
pre-reader `c4020f7` · menu key-case fix `37cabec` · menu reader + `SaveConfig` merge `ae2613a` · engine reader `9bb92da`.
Seventh four-preset checkpoint green; suite 472 cases.

Continued (2026-10-14): shadow-TextOut ×6 → `Core/ShadowText.h` (`f6f5942`) ·
legacy audio normalization fix (`fefc4b3`, owner call — Defect 7) ·
menu constants vs `Core/Constants.h` (`e1016c3`) ·
resolution list ×2 → `Core/ResolutionList.h` (`a8b60d4`).
Eighth four-preset checkpoint green; suite 473 cases.

Continued further (2026-10-14): FPS limiter ×2 → `Core/FrameLimiter.h` (`3b15544`) ·
DPI/VK helpers ×2 → `Core/DpiAwareness.h` + `Core/KeyBindings.h` (`a763217`) ·
ship spawn ×2 (`07118a1`).
Ninth four-preset checkpoint green; suite 473 cases.

Continued (2026-10-14, third batch): script-key predicate ×2 → `ScriptKeyMatches`/`ScriptKeyIsIgnoreCase` in `Loaders/LoadValidate.h` (`1c682b5`) ·
F10 overlay GL+Soft → `Workbench/TuningOverlay.h` (`09b6351`) ·
bound-fit merge re-verified: the production math is already unified in `Core/BoundFit.h` (`6f525e8` upstream), nothing to merge — item closed without a diff.
Tenth four-preset checkpoint green; suite 473 cases. WS3 continues with the remaining unified-implementation items.

### WS4 — Build & tooling (~900 LOC)

`make-c2-package.ps1` embedded 188-line config template (~200; owner: Python dependency) ·
`tools/comments.py` tracked orphan (366; owner) · asset-checker prologue (~85) · Win32 native helper ×3 (~100) ·
`sky_modes` doc/dead (~59) · `.gitignore` negations · dead preset knobs · `C2_BUILD_TESTS` for menu/soft legs (owner) ·
`c2_test_support` library · LTO per-target · clang-tidy LLVM pin · changed-file selection dedup ·
PGO presets (owner) · flashlight GLSL tri-copy (~90, high risk, needs owner visual verification).

Landed (2026-10-14): see the progress log for all twenty-one commits. Every WS4 item is closed except the flashlight visual gate, and all owner calls are recorded. Highlights: `tools/comments.py` deleted; the shared `C2NativeWindows` helper and the asset-checker prologue/report deduplicated; the sky-mode harness made runnable and documented; the clang-tidy runner fixed for Windows PowerShell and its LLVM pinned to choco 22.1.8 (local 23.1.0 documented); the changed-file filter unified in the runner; dead preset knobs dropped; `c2_test_support` added; LTO scoped to shipped targets; `C2_BUILD_TESTS` added with CI release legs off; PGO labelled experimental; the packaged config emitted from the engine template (Python requirement documented); trophy naming unified on the menu's two-digit form (Defect 1); the flashlight/night-lighting GLSL unified into `shaders/flashlight.glsl` through a minimal loader `#include`.

### Owner decisions

All resolved 2026-10-14 — delete `comments.py`; require Python 3.10+ for packaging and emit the package config
from the engine template; add `C2_BUILD_TESTS` with the CI release legs off; label the PGO presets experimental;
land the GLSL include refactor subject to visual verification; pin CI clang-tidy to choco LLVM 22.1.8 with the
local 23.1.0 documented; unify trophy naming on the menu's two-digit form. (The config-scanner staging policy,
the menu `SaveConfig` unification and the legacy audio 4–5 mapping resolved earlier — see the progress log and
Defect 7.) The flashlight visual gate closed 2026-10-14: the owner verified the night-hunt beam unchanged on the
`0955d87` deploy.

## Defects logged (not fixed without an owner call)

1. **Trophy filename divergence at profile ≥10** — menu `trophy10.sav` (`Menu/Resources.cpp:1508`) vs engine `trophy010.sav` (`Trophy.cpp:383`); reachable via the `reg=` command-line option (menu UI is profiles 0-7). Classified: partial-launch/latent. **Resolved 2026-10-14:** owner chose the menu's two-digit naming; `fb2061a` formats the four engine sites with `%02d`, so profiles 0-9 are unchanged and ≥10 now agree across both EXEs.
2. `g_KeyNames[256]` definition vs `extern char g_KeyNames[128]` (`Menu/Hunt.h:857/998`) — declaration-size mismatch; unverified, needs a look.
3. `CreateLog` prints "Build v2.04. Sep.24 1999" (`Hunt/Loaders/Resources.cpp:1244`) — intentional legacy or fix? **Resolved 2026-10-13:** owner chose replace; `3182616` now prints `VERSION_STRING` from `Core/Version.h`.
4. **Line-ending anomaly** — 7 files carry doubled-CR (`\r\r\n`) endings in committed blobs (CharacterCollision 682, CharacterTargeting 505, Trophy 444, Projectiles 397, Ships 359, CharacterMovement 173, Vector 150), and this environment runs `core.autocrlf=true` against mixed-normalization blobs. Handling for this pass: stage affected files byte-faithfully (`git hash-object -w --no-filters` + `git update-index --cacheinfo`) and verify `git diff --cached --stat` shows the intended minimal diff. A repo-wide normalization pass is proposed as a separate owner-approved item — not silently done here.
5. **Comment hygiene finding (pre-existing)** — `Hunt/Core/AIBehavior.h:64` references `hunter-awareness-review.md`, which exists in both `CarnivoresDoc/architecture/` and `CarnivoresDoc/archive/`; `tools/check_comment_hygiene.ps1` rejects the ambiguity. **Resolved 2026-10-14:** owner chose the `architecture/` copy; `e7de326` qualifies the reference and the hygiene scan now reports 1 problem (Defect 6 only).
6. **Comment hygiene finding (pre-existing)** — `tools/release/make-c2-package.ps1:93` mentions `RELEASE-NOTES.md`, which the current doc tree does not contain; the script is a listed WS4 owner item (embedded config template / packaging), so the reference is left for that work rather than reworded here. **Resolved 2026-10-14:** `38d47f9` names the artifact in prose; the hygiene scan (278 files + the 211-file doc tree) reports no problems.
7. **Legacy audio backend normalization divergence (regression)** — `Core/Constants.h`'s `NormalizeAudioBackend` mapped saved driver values 4–5 (old OpenAL / XAudio2) to DirectSound, contradicting its own comment and the menu's still-original copy; the rewrite happened silently during the `db96ce7` Constants.h extraction, so a legacy profile selected a different backend per EXE. Reachable via legacy `trophy0N.sav` (engine, `Trophy.cpp:442`) and menu profile saves (`Resources.cpp:1595`). **Resolved 2026-10-14:** owner chose the original menu behavior (4–5 → OpenAL Soft); `fefc4b3` restores the pre-extraction body and pins the mapping table in `tests/test_audio_backend.cpp`.
8. **clang-only frontend error at `AddMessage` call sites (pre-existing)** — `Hunt.cpp:1225,1231` pass a ternary of string literals into `AddMessage(LPSTR)`; clang rejects it (`no matching function for call to 'AddMessage'`), MSVC accepts the permissive form, so the product builds. Introduced `250163bc` (2026-09-06), found when clang-tidy first ran over the TU during the ninth checkpoint. **Resolved 2026-10-14:** `22ca2b3` const-qualifies the parameter (`LPSTR` → `LPCSTR`; the body only reads it), so clang parses the TU and clang-tidy exits 0 on it.
9. **F10 debug menu PgDn behaves like PgUp (pre-existing)** — `Hunt.cpp:1222-1232`: both branches set `UnderwaterDebugTab = (UnderwaterDebugTab + 1) % 3`, so PgDn never steps the tab list backwards. Reachable via the F10 underwater debug menu (dev-facing overlay); the WS3 F10-overlay slice is the natural place to confirm the intended cycle.

## Progress log

### 2026-10-13 — baseline + inventory + WS1 slices 1-4

- Phase 0 baseline recorded (above); four read-only inventory hunts run; ~12 top claims spot-checked against source.
- `376daeb` refactor(hunt): remove dead D3D/3DFX branches and no-op alpha-flag scaffold — 8 files, −140 lines; all four presets build, ctest 16/16.
- `183fcc1` refactor(hunt): drop superseded commented-out state defines — `Characters.cpp`, −161 lines; ogl-debug build and ctest 16/16.
- `9ff0122` refactor(ai): remove disabled-species animations — 5 unreachable files plus call-site cleanup, −1,812 lines; all four presets build, ctest 16/16.
- `f3b60a3` refactor(hunt): remove twelve functions with no remaining callers — 7 files, −211 lines; all four presets build, ctest 16/16.
- Group checkpoint: comment hygiene clean (270 files); release flags clean (`/GS`, no `/GS-`); clang-tidy not installed locally (CI-only check).
- Owner decisions recorded: plan approved; disabled-species bundle deleted (owner confirmed it was upstream in-progress content to be reimplemented differently if ever needed); pass record persisted as this file.
- Next slices: 1d (dead targeting/pack helpers — note `CharacterTargeting.cpp` is one of the doubled-CR files, stage byte-faithfully), then 1f, 1g, then WS2 clusters, then WS3/W4.

### 2026-10-13 — WS1 slices 5-6 (1d, 1f)

- `ca4c57e` refactor(hunt): remove dead targeting and pack helpers — deleted SetNewTargetPlace_IcthOld, SetNewTargetPlaceVanilla and SetNewTargetPlaceMosasaurus, the unused inline GetAngleDifference/AnimatePackFollow/AnimatePackFollowSq helpers and their orphaned comments; `CharacterTargeting.cpp` staged byte-faithfully (doubled-CR preserved, deletions only). ogl-debug build and ctest 16/16.
- `a6ea274` refactor(hunt): remove unused globals, dead declarations and network wrappers — 11 unused `GameState.h` globals, 20 duplicate + 1 redundant `EngineAPI.h` declarations, the empty split-scaffolding sections (Textures/ScoreMod/FiringMode notes moved onto their declarations in `GameState.h`), the `Network.cpp` forwarding layer replaced by direct `g_Network` calls, and the unused `Hunt/Debug/Assert.h`, `NetworkGet/NetworkPost`, `RenderBMPModel2` and `StartupServer/StartupClient` declarations; `docs/ARCHITECTURE.md` tree updated. 12 files, −228 lines. ogl-debug build and ctest 16/16.
- Next slices: 1g (stale comments/docs), then WS2 clusters, then WS3/W4. Four-preset checkpoint pending once 1g completes WS1.

### 2026-10-13 — WS1 slice 7 (1g) + WS1 checkpoint

- `944ec20` docs(repo): fix stale F10 and layout descriptions — README/DEBUGGING now describe the native OpenGL workbench and the software legacy panel; `docs/ARCHITECTURE.md` drops the "Core/ headers only" claim (StateDefs.cpp) and refreshes the split counts (Characters 427, Hunt 1,944, 13 animation files, 360-line dispatcher, 9 Soft*.cpp, 3,906 lines); `Projectiles.cpp` gets a normal three-line banner (byte-faithful staging, mixed endings preserved). ogl-debug build and ctest 16/16.
- `3182616` fix(log): report the modernized build version — `CreateLog` prints `VERSION_STRING` (1.1.9.1) instead of "Build v2.04. Sep.24 1999" (owner call, closes defect #3). ogl-debug build and ctest 16/16.
- `a506ea4` docs(repo): refresh the stale changelog refs — origin/main pinned as `81fe90b`, workbench preview branch head as `4d05faa` (owner call); historical post-tag `9db51ec` references left alone. CHANGELOG:20,84 re-verified as dated history otherwise.
- WS1 checkpoint (completes 1c-1g group): all four presets configure and build clean (ogl-release, soft-release, menu-release re-run; ogl-debug current). ctest: ogl-debug 16/16, ogl-release 16/16, soft-release 15/15, menu-release 15/15. Comment hygiene clean (263 files scanned). Release flags clean (`/GS`, no `/GS-`) in ogl-release and soft-release. clang-tidy not installed locally (CI-only check).
- Next slices: WS2 test-suite clusters, then WS3 unified implementations, then WS4 build/tooling. Owner decisions still open: menu `SaveConfig` unification, trophy format/filename fix, packaging Python dependency, `C2_BUILD_TESTS` trade, PGO removal/labelling, `comments.py` delete vs document, GLSL include refactor, config-scanner staging policy.

### 2026-10-13 — WS2 clusters 1-4

- `d252d78` test(hud): delete the test-local map-scaling simulations — all six cases exercised helpers copied into the test file; re-verification corrected the inventory note (ClearStaleHUDRegions is live, GLUI.cpp:343) and only the file was removed. −6.
- `5259a6c` test(math): consolidate the vector micro-cases into five grouped tests — every production function keeps at least one assertion; 26→5. −21.
- `674b4c6` test(config): consolidate the ConfigMerge line-shape cases — 29→5, byte-exact expectations and the \v/\f parity row preserved. −24.
- `bfb301e` test(renderer): consolidate the source-string contract cases into four tests — a read-only hunt inventoried 24 source-text cases (22 pure, 1 hybrid, 2 registry-driven); 23 merged into `SourceContracts` (new `tests/test_source_contracts.cpp` for three groups + one in `test_workbench_config.cpp`), exact duplicates deduplicated, and the hybrid workbench-runtime case kept as runtime-only. −19 net (23 cases out, 4 in).
- Suite count measured with `--gtest_list_tests` across the 12 executables: 686 → 616 (each commit ogl-debug + ctest 16/16).
- Next WS2 clusters: load-validate (19→9), workbench persistence refusals (21→12), bound-fit (45→28), workbench input (16→6), WAV (20→13), FogSampling (14→6), then the remainder.

### 2026-10-14 — WS2 clusters 5-7 + four-preset checkpoint

- `a218fea` test(loaders): consolidate the load-validate cases into nine tests — 19→9; every production helper keeps its boundary rows and the tool-checked assertion set is identical (157 statements before/after). −10.
- `bfa544d` test(workbench): consolidate the persistence cases into twelve tests — the 15 `ConfigPersistence` cases became 8 and the 6 `WorkbenchPersistGPU` cases became 4; only duplicated setup assertions were absorbed, no expectation value lost. 21→12. −9.
- `26e1222` test(collision): consolidate the bound-fit geometry cases into twenty-eight tests — 13 `BoundFit` + 14 `ObjectBounds`/`BoundSurfaces` + 1 `ConfigParseBoundBoxFit`; re-verification counted the cluster at 44 cases (hunt estimated 45) and the assertion/token sets are identical. 44→28. −16.
- Suite count measured across the 12 executables: 616 → 581 (each commit ogl-debug + ctest 16/16, staged-diff checked).
- Checkpoint: all four presets configure and build clean. ctest: ogl-debug 16/16, ogl-release 16/16, soft-release 15/15, menu-release 15/15. Release flags clean (`/GS`, no `/GS-`) in ogl-release and soft-release. clang-tidy not installed locally (CI-only check).
- Comment hygiene re-run: 263 files scanned, 2 findings logged as Defects 5-6 — both code references predate this pass and the doc tree state is unchanged since 2026-10-03, so the earlier "clean" record missed them; no fix without an owner/doc-tree call.
- Next WS2 clusters: workbench input (16→6), WAV triple coverage (20→13), FogSampling (14→6), then AI tables, water color/waves, menu resource/price, terrain fog, uniform cache, CLI helpers, workbench three-layer (≈15).

### 2026-10-14 — WS2 clusters 8-10 + second checkpoint

- `ff26580` test(workbench): consolidate the input-boundary cases into six tests — 14→6 (hunt estimated 16); closed/unavailable transparency, backend availability, closing/transition fencing, press quarantine, Test-mode transitions and focus loss. −8.
- `bd985e4` test(audio): consolidate the WAV coverage into thirteen tests — `WaveReader` 9→6, `SoundLoaderEntry` 6→4, `MenuWaveTest` 5→3; every assertion and literal value preserved (29/26/26 statements). 20→13. −7.
- `91d2e51` test(renderer): consolidate the fog-sampling cases into six tests — 14→6; the inline/public equivalence sweep and the underwater reference formula stay intact, the routing/skip/variant cases are grouped with counter resets between scenarios. −8.
- Suite count measured across the 12 executables: 581 → 558 (each commit ogl-debug + ctest 16/16, staged-diff checked).
- Checkpoint (second): all four presets build clean. ctest: ogl-debug 16/16, ogl-release 16/16, soft-release 15/15, menu-release 15/15.
- Defect 5 resolved by owner call: `e7de326` docs(ai) qualifies the AIBehavior.h review reference with `architecture/`; the hygiene scan now reports only the WS4-owned packaging reference (Defect 6).
- Next WS2 clusters: AI tables, water color/waves, menu resource/price, terrain fog, uniform cache, CLI helpers, workbench three-layer (≈15).

### 2026-10-14 — WS2 clusters 11-13 + third checkpoint

- `1c678f4` test(ai): consolidate the behavior-math cases into fifteen tests — the 35 AIBehaviorMathTest cases became fifteen grouped tests (ranges/noise, shot-investigation timers, reaction selection, notice/alert gating, classification and kill/pack authority, aggression boundaries, event flee rules, authored threat, contact/reach, untimed ticks, aggression sources, stimulus eligibility, pack anchors, look-offset table, flee legs); all 171 assertion statements preserved. 35→15; suite 558→538.
- `d5dd1bf` test(water): consolidate the colour and wave cases into eleven tests — 19→11 across the three water test files; all 59 assertions preserved; merged fade cases ordered so no C4456 shadowing warning is introduced. Suite 538→530.
- `b16be7b` test(menu): consolidate the resource and price cases into twenty tests — 41→20; all 197 assertions preserved; scenarios grouped in block scopes for the same warning-free build. Suite 530→509.
- Checkpoint (third): all four presets build clean. ctest: ogl-debug 16/16, ogl-release 16/16, soft-release 15/15, menu-release 15/15. Release flags clean (`/GS`, no `/GS-`) in ogl-release and soft-release. Comment hygiene: 263 files scanned, only the WS4-owned Defect 6 finding (unchanged). The `Menu/Hunt.h` C4100 warning seen while recompiling the menu tests is pre-existing (reproduced on HEAD, not from this pass).
- clang-tidy is now runnable locally: the VS Code C++ extension bundles LLVM 23.1.0, so `tools/run_clang_tidy.ps1 -BuildDir build/clang-tidy -Files <changed files>` was run against the five changed files — exit 0, no errors and no blocking-check hits (advisory legacy diagnostics only). Prior checkpoints' "not installed locally (CI-only check)" no longer applies to this environment.
- Next WS2 clusters: terrain fog, uniform cache, CLI helpers, workbench three-layer (≈15).

### 2026-10-14 — WS2 clusters 14-16 + fourth checkpoint

- `f838e27` test(terrain): consolidate the fog cases into eight tests — 18→8 across `test_terrain_fog.cpp` and `test_terrain_fog_smoothing.cpp`; all 33+3 assertions preserved and every SameColor call site retained (the smoothing assertions live in the helper, so call-site counts were checked separately). Suite 509→499.
- `daa2172` test(renderer): consolidate the uniform-cache cases into three tests — 6→3; first-use/repeat collapse, ULP/signed-zero sensitivity, and the night/flashlight production transitions; all 31 assertions preserved. Suite 499→496.
- `13c0b28` test(cli): consolidate the command-line cases into five tests — 11→5 across the parse and entry files; all 33+52 assertions preserved. Suite 496→490.
- Checkpoint (fourth): all four presets build clean. ctest: ogl-debug 16/16, ogl-release 16/16, soft-release 15/15, menu-release 15/15. Release flags clean (`/GS`, no `/GS-`). Comment hygiene: 263 files scanned, only the WS4-owned Defect 6 finding (unchanged).
- clang-tidy: ran again on the five changed files — exit 0, no errors and no blocking-check hits.
- Next WS2 cluster: workbench three-layer (≈15 est.), which closes WS2; then WS3 unified implementations and WS4 build/tooling.

### 2026-10-14 — WS2 closer + fifth checkpoint

- `429a0e5` test(workbench): consolidate the session and runtime cases into thirty-one tests — 52→31 across `test_workbench_session.cpp` (27→14) and `test_workbench_runtime.cpp` (25→17); all 182+192 assertions preserved. Merged scenarios keep fixture continuity either by ordering (external-write invalidation relative to sync cases, dump inventory before the byte-exact legacy block) or by explicit seam resets (`workbench_test::ResetAudio()` plus `ResetRuntimeForHunt()` in the audio-fault pair) where the per-test fixture previously supplied them. Suite 490→469.
- Note: the "workbench three-layer" cluster was interpreted as the two remaining layers of the tuning stack (session + runtime) because the persistence layer was already consolidated as the workbench-persistence cluster; collision/GPU layers are separate features and were left alone.
- Checkpoint (fifth): all four presets build clean. ctest: ogl-debug 16/16, ogl-release 16/16, soft-release 15/15, menu-release 15/15. Release flags clean (`/GS`, no `/GS-`). Comment hygiene: 263 files scanned, only the WS4-owned Defect 6 finding (unchanged). clang-tidy: exit 0, no errors and no blocking-check hits on the two changed files. The C4459 warnings in the workbench runtime translation unit (`NightLighting.h` ×3 plus the `v`/`nv` locals) reproduce on HEAD and are pre-existing.
- WS2 closes at 686 → 469 cases in 17 clusters (plan target ~480). Next: WS3 unified implementations — foundational first (config path resolution ×4, load-policy wrapper ×2, ConfigPersistence classifier, then the staged config-scanner unification), then WS4 build/tooling.

### 2026-10-14 — WS3 foundational slices + sixth checkpoint

- `e38b060` refactor(config): share the config path resolution between the EXEs — new `Hunt/Core/ConfigPath.h` carries the exe-dir-first policy for both the read path (falls back to the working directory when no file exists) and the write path (exe directory authoritative); the four copies in `Menu/Resources.cpp` and `Hunt/Game/EngineInit.cpp` collapse onto it, `GetEngineConfigPath` keeps its char-buffer API on top, and the stale resolution comment left in `Hunt/Game/Config.cpp` is dropped. 4 files, −93/+67 lines; ogl-debug + ctest 16/16.
- `f44c1fa` refactor(config): share the load-policy pre-read between the EXEs — `ApplyLoadPolicyFromConfigFile` in `Loaders/LoadDiagnostics.h` (read the body, scan `load_mode`, then the `C2_STRICT_DATA` override) replaces the engine's `LoadLoadPolicy` and the menu's `InitMenuLoadPolicy`; the shared read uses binary mode, matching the engine copy, and the pre-read still runs before `_RES.TXT`/`_MENU.TXT` are parsed. 3 files, −39/+32 lines; ogl-debug + ctest 16/16.
- `ae57db8` refactor(workbench): share the config text classification with the loader — `Core/ConfigText.h` exposes `kMaxConfigBytes` and `IsUtf16ConfigText` (also used inside `ReadConfigText`); `ConfigPersistence::Classify` keeps only its refuse policy (the loader recovers NUL padding, the writer refuses it). 2 files, −12/+24 lines; ogl-debug + ctest 16/16.
- Observation: `Hunt/Game/Config.cpp` now holds only the `ShowFaces` definition plus blank padding — the dropped comment was its last real content. A file-level cleanup is a candidate for owner review; not touched here.
- Checkpoint (sixth): all four presets configure and build clean. ctest: ogl-debug 16/16, ogl-release 16/16, soft-release 15/15, menu-release 15/15. Release flags clean (`/GS`, no `/GS-`) in ogl-release and soft-release. Comment hygiene: 264 files scanned, only the WS4-owned Defect 6 finding (unchanged). clang-tidy (LLVM 23.1.0): exit 0 on the four changed TUs, advisory legacy diagnostics only.
- Next: the staged config-scanner unification (×4, ~135 est.) — owner call pending on the staging policy; the parity harness lands before any engine/menu reader is touched.

### 2026-10-14 — config-scanner owner call

- Staging policy: harness first — a pure `Core/ConfigScan.h` plus a parity corpus pinning today's engine/menu/pre-read behavior with no behavior change — then one consumer per commit: the load-policy pre-read, the menu (reader + `SaveConfig` merge onto `configmerge::Merge`), the engine reader.
- Grammar: both spellings stay as-is. `load_mode=strict` keeps working through the pre-reader's `=` separator while every other key keeps the whitespace form; the harness pins both grammars as intentional rather than a bug.

### 2026-10-14 — WS3 config-scanner unification + seventh checkpoint

- `e79b7d3` refactor(config): add the shared config scanner and its parity corpus — `Hunt/Core/ConfigScan.h` yields the key/value fields the engine reader, menu reader and pre-reader each scan for today; `tests/test_config_scan.cpp` pins the readers' whitespace grammar (column-0 comments, \v/\f separators, missing values, `key=value` as one token) and the pre-reader's equals grammar, with a differential over the real `InitLoadPolicyFromConfigText`; no consumer wired yet. Suite 469 → 472.
- `c4020f7` refactor(config): scan the load policy with the shared field scanner — the pre-reader drops its hand-rolled line/token loop for `configscan::Scan(WhitespaceOrEquals)`; `LoadKeyEquals` compares a scanner token. 1 file, −40/+17 lines.
- `37cabec` fix(config): match menu keys case-insensitively like the engine (owner call) — `configscan::KeyEquals` becomes the one ASCII case-insensitive rule, ConfigMerge's duplicate `EqualKey` loop is dropped, and the coverage tool plus its test follow the new reader shape; suite 472 → 473.
- `ae2613a` refactor(config): read and merge the menu config with the shared helpers — menu `LoadConfig` scans with `configscan::Scan`; `SaveConfig`'s merge branch moves onto `configmerge::Merge` (byte-preserving value-token updates, the file's own terminator for appends, earlier duplicates left inert), invalid owned entries are skipped with a notice (owner call), and the read/write turns binary with the first-run template's CRLF spelled out so terminators are never re-translated.
- `9bb92da` refactor(config): scan the engine config with the shared field scanner — `CopyConfigToken` reproduces the `%63s` copy, the dispatch chain is untouched, and the night-lighting oversized-token guard moves inline (its raw-line helper and test are removed; the invalid-enum assertion moves into the strict-settings test). Suite 473 → 472.
- Owner calls (all recorded above): staging — harness first, one consumer per commit; grammar — both spellings pinned; menu keys — case-insensitive like the engine; invalid save values — skip the entry, save the rest.
- Checkpoint (seventh): all four presets configure and build clean. ctest: ogl-debug 16/16, ogl-release 16/16, soft-release 15/15, menu-release 15/15. Suite 472 cases across the 12 executables. Release flags clean (`/GS`, no `/GS-`) in ogl-release and soft-release. Comment hygiene: 266 files scanned, only the WS4-owned Defect 6 finding (unchanged). clang-tidy (LLVM 23.1.0): exit 0 on the changed TUs, advisory legacy diagnostics only.
- Next: WS3 continues with the remaining unified-implementation items — shadow-TextOut ×6, menu constants vs `Core/Constants.h`, resolution list ×2, FPS limiter ×2, DPI/VK helpers ×2, ship spawn ×2, script-key predicate, F10 overlay GL+Soft, bound-fit merge, idle-group selector, menu OpenAL plumbing, TGA reader ×2, env presets triple-source, test fixtures/helpers.

### 2026-10-14 — WS3 slices (shadow text, menu constants, resolution list) + audio fix + eighth checkpoint

- `f6f5942` refactor(ui): share the shadow text drawing across the EXEs and renderers — new `Hunt/Core/ShadowText.h` holds the one shadow-then-main TextOut rule (transparent background, shadow at (+1, +1), shadow colour as a parameter because the menu / GL life-info / soft HUD use black while the GL overlay and box text use the near-black 0x00101010); Menu `DrawTextShadow`, the GLHUD life-info and GLUI control-overlay lambdas, SoftHUD's `STTextOut` and UIText `DrawBox` collapse onto it (GLUI keeps its `ProtectHUDRect` extent pass at the call site), and the commented-out sixth copy in `Interface.cpp` is deleted. 7 files, +39/−25.
- Owner call (2026-10-14): legacy audio values 4–5 map to OpenAL Soft (the original `24c1620` behavior) — logged as Defect 7.
- `fefc4b3` fix(audio): restore the legacy OpenAL mapping for saved driver values 4-5 — `NormalizeAudioBackend` in `Core/Constants.h` had silently changed to DirectSound during the `db96ce7` extraction while the comment kept the old rule; the pre-extraction body is restored and the mapping table (0/2/3 → DirectSound, 1/4+ → OpenAL Soft, negatives → DirectSound) is pinned in `tests/test_audio_backend.cpp`. Suite 472 → 473.
- `e1016c3` refactor(menu): consume the shared option constants from Core/Constants.h — Menu/Hunt.h drops its FOV, view-distance and object-detail copies plus `ClampMenuViewOpt`, `ViewOptToCtViewR` (no callers) and `ClampMenuObjectDetail` (−87 lines); Resources.cpp calls the shared `ClampViewOpt`/`ClampObjectDetail`; the dead `AIIndexEnum` (no references, stale values that collided with Core's AI_* constants), the exact-duplicate `AudioSystemEnum`/`NormalizeAudioBackend` and the unused `HIRGB`/`TRURGB`/`HIRGBA`/`TRURGBA` macros go away (`HiColor` at the single call site).
- `a8b60d4` refactor(core): share the runtime resolution list between the EXEs — new `Hunt/Core/ResolutionList.h` owns `TRes`, `kMaxResolutions` and the `Add`/`Enumerate` rules both EXEs previously carried; each binary keeps its storage and a one-line `EnumerateResolutions` wrapper; `RenderTypes.h` and `Menu/Hunt.h` stop defining `TRes`, and both storages plus the menu test seam size themselves from `kMaxResolutions`.
- Checkpoint (eighth): all four presets configure and build clean. ctest: ogl-debug 16/16, ogl-release 16/16, soft-release 15/15, menu-release 15/15. Suite 473 cases across the 12 executables. Release flags clean (`/GS`, no `/GS-`) in ogl-release and soft-release. Comment hygiene: 268 files scanned plus a 211-file doc tree, only the WS4-owned Defect 6 finding. clang-tidy (LLVM 23.1.0): exit 0 on the eleven changed TUs, advisory diagnostics only — the two new include-cleaner hits on the audio test were silenced with a direct `Core/Constants.h` include before the checkpoint.
- Observation: `PrintLoad`'s `loadtxt` (`Hunt/Game/Interface.cpp`) is write-only now that the commented-out copy is gone — a candidate for a later dead-code pass. The menu's `kFpsLimit*` defines stay put for the upcoming FPS-limiter slice, which should decide their shared home.
- Next: WS3 continues — FPS limiter ×2, DPI/VK helpers ×2, ship spawn ×2, script-key predicate, F10 overlay GL+Soft, bound-fit merge, idle-group selector, menu OpenAL plumbing, TGA reader ×2, env presets triple-source, test fixtures/helpers.

### 2026-10-14 — WS3 slices (FPS limiter, DPI/VK helpers, ship spawn) + ninth checkpoint

- `3b15544` refactor(core): share the FPS limiter between the EXEs — the option indexes and the target table move into `Core/Constants.h` (`kFpsLimitOff/60/120/240/Count/Default`, `kFpsLimitValues`, `FpsLimitTarget`); the menu's `kFpsLimit*` defines, Hunt.cpp's `kFpsValues` and EngineInit.cpp's local `fpsValues` collapse onto them, and OptFpsLimit's default plus the effective-config log use the named values. New `Core/FrameLimiter.h` carries the queryPerformanceCounter pacer both loops spelled out (sleep whole milliseconds, spin the remainder, `timeBeginPeriod(1)` on first use), one instance per caller. The shared pacer advances the frame phase unconditionally (the menu's behavior); the engine copy skipped the phase update while Unlimited, which only affects the first frame after switching away from it.
- `a763217` refactor(core): share the DPI-awareness and VK-mapping helpers — new `Core/DpiAwareness.h` holds the guard macro plus the runtime-resolved per-monitor-v2 opt-in both EXEs spelled out; `MapVKKey` moves into `Core/KeyBindings.h` with the menu's full mouse-button mapping. The engine's `Interface.cpp` copy (L/R buttons only, no callers) is deleted rather than merged.
- `07118a1` refactor(hunt): share the ship spawn placement — file-local `PlaceShipBeyondView(TShip&)` in `Ship.cpp` replaces the two copies in AddShipSupply and InitShip (offset ctViewR+30 cells, floor 90, wrap past the left/top edge, lift to terrain + DeltaY + 1024).
- Checkpoint (ninth): all four presets configure and build clean. ctest: ogl-debug 16/16, ogl-release 16/16, soft-release 15/15, menu-release 15/15. Suite 473 cases. Release flags clean (`/GS`, no `/GS-`). Comment hygiene: 270 files scanned plus the 211-file doc tree, only the WS4-owned Defect 6.
- clang-tidy (LLVM 23.1.0): exit 0 on five of the six changed TUs; `Hunt.cpp` reports two clang-only frontend errors at the `AddMessage` ternary-of-literals calls (1225/1231), introduced `250163bc` on 2026-09-06 and accepted by MSVC — logged as Defect 8 and left for a dedicated fix rather than folded into this batch.
- Observation (logged as Defect 9): the F10 debug menu's PgUp and PgDn branches both advance the tab with `(tab + 1) % 3` (`Hunt.cpp:1222-1232`), so PgDn never steps backwards; the upcoming F10-overlay slice should confirm the intended cycle.
- Next: WS3 continues — script-key predicate, F10 overlay GL+Soft, bound-fit merge, idle-group selector, menu OpenAL plumbing, TGA reader ×2, env presets triple-source, test fixtures/helpers.

### 2026-10-14 — WS3 slices (script-key predicate, F10 overlay) + tenth checkpoint

- `1c682b5` refactor(loaders): share the script-key matcher between the engine and the menu — `LoadValidate.h` gains `ScriptKeyMatches` (the whole-key-before-'=' rule with an explicit case mode) plus the new `ScriptKeyIsIgnoreCase`; the menu's local `MenuScriptKeyIs` copy is deleted and its 28 call sites use the shared case-relaxed predicate. Case behavior is preserved on both sides (engine case-sensitive, menu's `689be6b` relaxation kept) and pinned in the existing load-validate test.
- `09b6351` refactor(workbench): draw the F10 overlay from the tuning model on both renderers — new `Workbench/TuningOverlay.h` builds the rows from `LegacyParameter`/`FindDescriptor` (labels and order) plus `ReadRuntimeValues` (live values), with the frame position, colours, header/hint text and row format moved over verbatim; GLUI's three copy-pasted tab blocks and SoftHUD's parallel arrays are gone, `STTextOut` takes `const char*`. The parameter list can no longer drift between renderers.
- Bound-fit merge re-verified at slice time: the oriented/sloped ofBOUND math already lives in `Core/BoundFit.h` (`6f525e8` upstream `refactor(collision): unify oriented box fitting`) with `ObjectBounds.cpp` and `CollisionGeometry.h` as consumers; no duplicate remains, item closed without a diff.
- Checkpoint (tenth): all four presets configure and build clean. ctest: ogl-debug 16/16, ogl-release 16/16, soft-release 15/15, menu-release 15/15. Suite 473 cases. Release flags clean (`/GS`, no `/GS-`).
- Comment hygiene: 271 files scanned plus the 211-file doc tree, only the WS4-owned Defect 6.
- clang-tidy (LLVM 23.1.0): exit 0 on the batch TUs (GLUI, SoftHUD, Menu/Resources, the load-validate test; TuningOverlay.h followed through GLUI). The provider header was polished from 9 to 5 advisory diagnostics; the remainder are the codebase-standard array-parameter bounds notes. Hunt.cpp's pre-existing clang-only frontend error (Defect 8) remains the one non-advisory finding and still needs its dedicated fix.
- Observation: `ShowControlElements` still carries per-renderer copies of the timer / message / exit-countdown text blocks around the now-shared overlay; consolidating those needs the dirty-rect callbacks and is not part of the current WS3 list. Defect 9 (PgUp/PgDn) is input code in `Hunt.cpp`, untouched by the overlay drawing merge.
- Next: WS3 continues — idle-group selector across the animation files, menu OpenAL plumbing, TGA reader ×2, env presets triple-source, test fixtures/helpers.

### 2026-10-14 — WS3 slice (idle-group selector)

- `f0d6c07` refactor(hunt): share the idle-group phase selector across the animators — new inline `SelectIdleGroupPhase` in `Game/CharacterInternal.h` owns the current group's end roll (end chance with `endOnAny`/last-animation, `instantRepeat`) plus the start-chance group roll and the first-vs-random animation pick; the seven inline copies in `AnimateBrahi`, `AnimateBrahiOld`, `AnimateClassicAmbient`, `AnimateHuntable`, `AnimateTRex` and `AnimateIcth` (idle + idle2 sets) collapse onto it, with callers keeping their own gate (corpse-carried / on-water) and fallback phase (`walkAnim` / `swimAnim`). Re-verification counted seven selector sites in six files (the hunt estimated eight files); the `rRand` call order and probabilities are unchanged. 9 files, +196/−217 lines.
- Branch behavior pinned in the new `tests/test_idle_group_select.cpp`: zero/certain start chance, in-group continuation until the end chance, ended-group release vs instant-repeat restart. Suite 473 → 477 cases.
- Slice verification: ogl-debug builds clean and the full registered suite passes 16/16; comment hygiene clean (272 files); clang-tidy (LLVM 23.1.0) exit 0 on the seven TUs, advisory diagnostics only (array indexing, int→float chance math, lowercase `f` suffixes) after bracing the new helper.
- No owner decisions were needed for this slice. The eleventh four-preset checkpoint follows the remaining batch: menu OpenAL plumbing, TGA reader ×2, env presets triple-source, test fixtures/helpers.

### 2026-10-14 — WS3 tail (OpenAL plumbing, TGA readers, env presets, test fixtures) + eleventh checkpoint

- `ccc3974` refactor(audio): share the OpenAL device and context lifecycle between the EXEs — new `Hunt/Audio/OpenALDevice.h` (`OpenOpenALDevice` with distinct `NoDevice`/`NoContext` results and half-open cleanup, `CloseOpenALDevice`); the engine `InitAudioSystem`/`Audio_Shutdown` and the menu `MenuAudioInit`/`MenuAudioShutdown` drop their two verbatim open/close copies. Re-verification found the menu's upload/source/destroy helpers are not engine duplicates (buffer cache, per-sound frequency and channel semantics differ), so only the lifecycle is shared. The AL seam gained openDevice/createContext/makeCurrent injection plus close/destroy counters; every open failure path and the idempotent close are pinned. Suite 477 → 479.
- `cec1877` fix(hunt): validate the engine TGA pictures against the 16-bit format (owner call) — `LoadPictureTGA` reads the 18-byte header, rejects palettes, non-truecolor image types and depths other than 16, and honors the image-ID field; malformed mod art now halts with a named error instead of rendering garbage. New `Hunt/Loaders/TgaFormat.h` owns the packed header, the image-type constants and the acceptance rule; six new picture-loader contracts (suite 479 → 485).
- `ce1f138` refactor(loaders): read the two TGA loaders from one format owner — the menu's `ReadTGAFile` switches to `CheckTgaPictureFormat` with its per-case diagnostics kept verbatim and the private `Menu/Targa.h` is deleted; the on-disk layout can no longer drift between the EXEs.
- `f1d8b04` refactor(audio): derive the reverb presets from one compiled table — `c2audio::Defaults()` becomes constexpr and the single owner; the engine's `EAX2ENV`/`g_EnvPresets` copy is gone (the runtime bank is a `c2audio::Environments` seeded from Defaults, same ranges and reverb truncation) and the workbench registry's 32 `envN` compiled defaults read `c2audio::Defaults()[env][field]`. `tools/c2_config_defaults.py` now reads its parity rows from the new owner; the first-run config template stays literal (deployed config contract) and is still checked against the table by the tool.
- `c13d15a` refactor(tests): share the loader-halt and temp-script fixtures — new `tests/test_support.h` owns `LoaderHalt` and `TempScript`; five copies are deleted across the picture, sound, script and menu contract tests and call sites keep their names via using-declarations. `f44b423` test(audio): includes the shared device header directly, clearing the only new-code include-cleaner notes.
- Checkpoint (eleventh): all four presets configure and build clean. ctest: ogl-debug 16/16, ogl-release 16/16, soft-release 15/15, menu-release 15/15. Suite 485 cases. Release flags clean (`/GS`, no `/GS-`) in ogl-release and soft-release. Comment hygiene: 274 files plus the 211-file doc tree, only the WS4-owned Defect 6.
- clang-tidy (LLVM 23.1.0): exit 0 on the ten batch TUs; advisory diagnostics only (the usual legacy categories; the new-code include-cleaner notes were silenced with a direct include before the checkpoint). Hunt.cpp's pre-existing clang-only frontend error (Defect 8) remains the one non-advisory finding.
- Observation: the picture/sound loader contract tests cannot delete their temp files when a halt path throws — the loader's open HANDLE keeps the file locked until process exit, so the post-`EXPECT_THROW` `remove()` fails and the file lands in the working directory. Pre-existing and harmless under ctest (gitignored build dir), noted for a later test-seam cleanup.
- WS3 closes with this batch (the ranked list is done; the bound-fit merge was already closed in the tenth batch). Next: WS4 build & tooling (~900 est.) in a fresh session — packaging config template/Python dependency, `comments.py` verdict, asset-checker prologue, Win32 native helpers, `sky_modes`, `.gitignore` negations, dead preset knobs, `C2_BUILD_TESTS`, `c2_test_support`, LTO per-target, clang-tidy pin, changed-file dedup, PGO presets, flashlight GLSL tri-copy.
### 2026-10-14 — WS4 owner calls and tooling slices

- Owner calls recorded: delete `tools/comments.py`; require Python 3.10+ for packaging and emit the package config from the engine template; add `C2_BUILD_TESTS` (default ON) with the CI release legs off; label the PGO presets experimental (keep the plumbing); land the GLSL include refactor subject to visual verification; pin CI clang-tidy to choco LLVM 22.1.8 while documenting the local cpptools 23.1.0; unify trophy naming on the menu's two-digit form; fix and document the sky-mode harness.
- `fdb0d8d` chore(tools): delete the orphaned comment accounting script — tools/comments.py (445 lines) had no caller or doc reference; its .gitignore negation goes with it.
- `fee3a60` chore(repo): negate the tracked config-defaults tooling in .gitignore — c2_config_defaults.py and its README were force-added without negations; the stale comment now names the reviewed tooling categories. Verified with git check-ignore.
- `22ca2b3` fix(hunt): accept const messages in AddMessage — the parameter becomes LPCSTR (the body only feeds lstrcpy into MessageList.mtext); closes Defect 8 and lets clang parse Hunt.cpp again.
- `a965d91` fix(ci): select translation units on Windows PowerShell in the clang-tidy runner — ConvertFrom-Json emits the parsed array as one object in 5.1, so the runner (and therefore the CI job, which uses that shell) silently selected nothing and exited 0; the database is now collected into a variable before wrapping, and the runner prints the analyzed unit count.
- `d40107d` ci: let the clang-tidy runner own the changed-file filter — the workflow's duplicate extension regex is dropped; only the empty-diff guard stays.
- `d5b1afe` ci: pin the clang-tidy LLVM version and report the resolved one — choco 22.1.8 in CI; the runner prints the executable and version; tools/clang-tidy.md documents both versions and keeps the 22.1.3 diagnostic count as history.
- `60d05bc` chore(build): drop the dead logging knobs from the debug presets — ENABLE_RENDERER_LOGGING/ENABLE_TERRAIN_LOGGING were read by no option or source.
- `915a887` fix(tools): make the sky-mode capture harness runnable — it always threw (the matrix rejects unattended software runs and requires foreground for PostCaptureSeconds) and it seeded sky_mode for the software renderer, which reads sky_mode_soft; the invocation now passes -Foreground, the seeded key follows the renderer, the unused skyDir variable is gone and the capture README documents the probe.
- `69a3fed` refactor(tools): share the Win32 window helper between the smoke test and the capture harnesses — one C2NativeWindows type in tools/c2_native_windows.ps1 (with a PSTypeName guard for repeated dot-sourcing) replaces the three embedded C2SmokeNative/C2CaptureNative/C2PitchNative classes; verified in Windows PowerShell 5.1 and PowerShell 7 against a synthetic HuntRenderWindow, including client-rect and client-to-screen.
- `9f4f10a` fix(ci): run the fixture smoke contracts without frame checks — the framebuffer checks added in 4302eb1 need a renderer window the .NET fixture does not create, so every runtime-contracts case had failed at the window wait since 2026-09-29; the fixture cases now pass -SkipFrameCheck and the deployed tier keeps the frame assertions. All seven contract cases pass locally.
- `40c2b7b` chore(repo): ignore dotnet intermediate output — the smoke fixture build leaves tests/obj/ next to the already-ignored bin/.
- `be70cb9` refactor(tools): share the asset-checker prologue and report — tools/asset_check_common.ps1 owns the error/warning/note lists, Test-GameFile, Resolve-GameRoot and Write-CheckReport with its -Strict exit rule; both checkers keep their own Get-RelativePath semantics. stdout, stderr and exit codes are byte-identical to HEAD for both tools against the deployed game folder, and both contract scripts pass.

### 2026-10-14 — WS4 build, release and shader slices + twelfth checkpoint

- `d7651ce` refactor(build): compile the shared test translation units once — new c2_test_support static library (StateDefs, ObjectBounds, TerrainQueries, Vector, CollisionInspection, ConfigPersistence, TuningRuntime, TuningSession, InputBoundary, OpenAL_Loader); target-local files keep their defines (WorkbenchUI with C2_WORKBENCH_UI_PROBE, the AL seams, glad.c, Menu/Resources.cpp) and C2_SOURCE_ROOT stays target-scoped.
- `163e621` refactor(build): scope LTO to the shipped targets — INTERPROCEDURAL_OPTIMIZATION_RELEASE on the game, the menu and the ImGui library; tests and GoogleTest no longer compile /GL or link /LTCG (verified from ninja command lines) while /GS stays in every release flag set.
- `7c40f6b` build(ci): gate the test targets behind C2_BUILD_TESTS — default ON; the CI ogl-release/soft-release/menu-release legs configure OFF (no GoogleTest fetch, no Python requirement), only ogl-debug builds and runs the suite; BUILDING.md documents the trade and corrects the suite inventory (twelve native executables, sixteen GL / fifteen non-GL registered suites). A scratch OFF configure+build produces only the game and menu.
- `1362f9d` docs(build): label the PGO presets experimental — displayNames and the BUILDING.md section state the requirement set (MSVC GL Release plus a shared .pgd).
- `17ac7df` refactor(release): emit the packaged config from the engine template — the 188-line embedded copy is gone; make-c2-package.ps1 runs tools/c2_config_defaults.py against the staged path, the tool's now-impossible package/engine comparison is removed, and Python 3.10+ (or -PythonPath) is documented as a packaging requirement. End-to-end packaging verified: fresh PkgRoot, license fetch, staging, zip and SHA; the packaged config.cfg is byte-identical to the engine's first-run template (6646 bytes).
- `21f6794` fix(release): create the package root before caching the license text — a fresh or custom -PkgRoot made the LGPL download fail with DirectoryNotFoundException; the template report is also piped to Out-Null.
- `fb2061a` fix(hunt): pad the trophy save names to two digits (owner call) — closes Defect 1; Trophy.cpp staged byte-faithfully (444 doubled-CR lines preserved, the cached diff shows only the four format strings).
- `0955d87` refactor(gl): include the shared flashlight and night-lighting GLSL from one file — Hunt/Renderer/ShaderIncludes.h gives the loader a minimal #include (relative resolution, depth-limited cycles, named failures for malformed/missing includes); shaders/flashlight.glsl owns the beam uniforms, ApplyFlashlight and ApplyNightLighting; the three world shaders keep only the include and their call sites. The source contract now pins the include and the restoration-before-illumination ordering, the new ShaderIncludes tests pin expansion and the error paths (suite 485 → 487), CMake globs *.glsl and lists the header, and the docs count the shared fragment. ogl-release-shipping built and deployed (menu + GL game + 13 shader files) for the owner's in-game check.
- `38d47f9` fix(tools): keep the tool comments inside the documented doc tree — closes Defect 6 and the newly added run_clang_tidy.ps1 reference; hygiene is clean.
- Checkpoint (twelfth): all four presets configure and build clean. ctest: ogl-debug 16/16, ogl-release 16/16, soft-release 15/15, menu-release 15/15. Suite 487 cases across the 12 executables. Release flags clean (/GS, no /GS-) in ogl-release and soft-release. Comment hygiene: 278 files plus the 211-file doc tree, 0 problems. clang-tidy (LLVM 23.1.0): exit 0 on the six batch TUs (Resources, Trophy, GLShader with the new header, the two changed test TUs and Hunt.cpp, which the AddMessage fix cleared).
- Local tooling note: the ignored .vscode/copy_build.ps1 now deploys *.glsl as well (both the main worktree copy and this worktree's copy); without that the deploy task would drop flashlight.glsl while the shaders require it. The rest of the deploy was verified (shaders copied to the deployed folder and the preset bin).
- Observation: the capture matrix's dirty-build-input check and tools/perf/workbench_perf.py's copy of it remain duplicated across PowerShell and Python by design of that evidence tool; the changed-file dedup item was interpreted as the CI/runner pair and closed. Defect 9 (F10 PgDn) stays open as recorded in WS3, and the line-ending normalization item stays a separate owner-approved proposal.
- Gate closed 2026-10-14: the owner verified the night-hunt flashlight unchanged on the 0955d87 deploy (shaders load, beam shape and ground boost intact); WS4 and the consolidation pass are complete.

