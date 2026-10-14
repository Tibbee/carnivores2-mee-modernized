# Consolidation Pass — C2 MEE

> **Last verified:** 2026-10-14 — WS1 complete (1a-1g); WS2 complete (17 clusters, 686 → 469 cases); WS3 foundational slices, the staged config-scanner unification and six follow-on slices landed (shadow text, menu constants, resolution list, FPS limiter, DPI/VK helpers, ship spawn); ninth four-preset checkpoint green; hygiene has 1 open finding (Defect 6, WS4-owned); clang-tidy runs locally (VS Code cpptools LLVM 23.1.0) and is clean on the batch TUs except Hunt.cpp's pre-existing clang-only frontend error (Defect 8).
> **Branch:** `refactor/consolidation-pass`, worktree `Carnivores2MEE1.11/build/consolidation-pass`, based on `feat/modder-workbench` @ `4d05faa`.
> **Status:** in progress. Owner approved the plan and the deletion of the disabled-species animation bundle. Only the Progress log and the task list below are authoritative for what is done; the ranked estimates come from the Phase 1 hunts.

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
Ninth four-preset checkpoint green; suite 473 cases. WS3 continues with the remaining unified-implementation items.

### WS4 — Build & tooling (~900 LOC)

`make-c2-package.ps1` embedded 188-line config template (~200; owner: Python dependency) ·
`tools/comments.py` tracked orphan (366; owner) · asset-checker prologue (~85) · Win32 native helper ×3 (~100) ·
`sky_modes` doc/dead (~59) · `.gitignore` negations · dead preset knobs · `C2_BUILD_TESTS` for menu/soft legs (owner) ·
`c2_test_support` library · LTO per-target · clang-tidy LLVM pin · changed-file selection dedup ·
PGO presets (owner) · flashlight GLSL tri-copy (~90, high risk, needs owner visual verification).

### Owner decisions still open

Trophy format/filename fix · packaging Python dependency ·
`C2_BUILD_TESTS` CI build-time trade · PGO removal or experimental labelling · `comments.py` delete vs document ·
GLSL include refactor. (The `config-scanner staging policy`, the menu `SaveConfig` unification and the legacy
audio 4–5 mapping resolved 2026-10-14 — see the progress log and Defect 7.)

## Defects logged (not fixed without an owner call)

1. **Trophy filename divergence at profile ≥10** — menu `trophy10.sav` (`Menu/Resources.cpp:1508`) vs engine `trophy010.sav` (`Trophy.cpp:383`); reachable via the `reg=` command-line option (menu UI is profiles 0-7). Classified: partial-launch/latent.
2. `g_KeyNames[256]` definition vs `extern char g_KeyNames[128]` (`Menu/Hunt.h:857/998`) — declaration-size mismatch; unverified, needs a look.
3. `CreateLog` prints "Build v2.04. Sep.24 1999" (`Hunt/Loaders/Resources.cpp:1244`) — intentional legacy or fix? **Resolved 2026-10-13:** owner chose replace; `3182616` now prints `VERSION_STRING` from `Core/Version.h`.
4. **Line-ending anomaly** — 7 files carry doubled-CR (`\r\r\n`) endings in committed blobs (CharacterCollision 682, CharacterTargeting 505, Trophy 444, Projectiles 397, Ships 359, CharacterMovement 173, Vector 150), and this environment runs `core.autocrlf=true` against mixed-normalization blobs. Handling for this pass: stage affected files byte-faithfully (`git hash-object -w --no-filters` + `git update-index --cacheinfo`) and verify `git diff --cached --stat` shows the intended minimal diff. A repo-wide normalization pass is proposed as a separate owner-approved item — not silently done here.
5. **Comment hygiene finding (pre-existing)** — `Hunt/Core/AIBehavior.h:64` references `hunter-awareness-review.md`, which exists in both `CarnivoresDoc/architecture/` and `CarnivoresDoc/archive/`; `tools/check_comment_hygiene.ps1` rejects the ambiguity. **Resolved 2026-10-14:** owner chose the `architecture/` copy; `e7de326` qualifies the reference and the hygiene scan now reports 1 problem (Defect 6 only).
6. **Comment hygiene finding (pre-existing)** — `tools/release/make-c2-package.ps1:93` mentions `RELEASE-NOTES.md`, which the current doc tree does not contain; the script is a listed WS4 owner item (embedded config template / packaging), so the reference is left for that work rather than reworded here.
7. **Legacy audio backend normalization divergence (regression)** — `Core/Constants.h`'s `NormalizeAudioBackend` mapped saved driver values 4–5 (old OpenAL / XAudio2) to DirectSound, contradicting its own comment and the menu's still-original copy; the rewrite happened silently during the `db96ce7` Constants.h extraction, so a legacy profile selected a different backend per EXE. Reachable via legacy `trophy0N.sav` (engine, `Trophy.cpp:442`) and menu profile saves (`Resources.cpp:1595`). **Resolved 2026-10-14:** owner chose the original menu behavior (4–5 → OpenAL Soft); `fefc4b3` restores the pre-extraction body and pins the mapping table in `tests/test_audio_backend.cpp`.
8. **clang-only frontend error at `AddMessage` call sites (pre-existing)** — `Hunt.cpp:1225,1231` pass a ternary of string literals into `AddMessage(LPSTR)`; clang rejects it (`no matching function for call to 'AddMessage'`), MSVC accepts the permissive form, so the product builds. Introduced `250163bc` (2026-09-06), found when clang-tidy first ran over the TU during the ninth checkpoint. Not fixed here: the fix choice (const-qualify the parameter vs restructure the call) belongs in a dedicated `fix(...)` commit.
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
