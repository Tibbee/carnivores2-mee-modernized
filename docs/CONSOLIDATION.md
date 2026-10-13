# Consolidation Pass — C2 MEE

> **Last verified:** 2026-10-13 — WS1 complete (1a-1g); four-preset checkpoint green (see Progress log).
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

### WS3 — Unified implementations (~1,300 LOC)

Foundational first: config path resolution ×4 (~45) · load-policy wrapper ×2 (~15) · ConfigPersistence classifier (~20) ·
full config scanner unification ×4 (~135, staged, needs parity harness) · shadow-TextOut ×6 (~40) ·
menu constants vs `Core/Constants.h` (~50) · resolution list ×2 (~70) · FPS limiter ×2 (~20) ·
DPI/VK helpers ×2 (~22) · ship spawn ×2 (~12) · script-key predicate (~30) · F10 overlay GL+Soft (~150-200) ·
bound-fit merge (~75) · idle-group selector across 8 animation files (~150-200) · menu OpenAL plumbing (~100) ·
TGA reader ×2 (~60) · env presets triple-source (~30) · test fixtures/helpers (~55).

### WS4 — Build & tooling (~900 LOC)

`make-c2-package.ps1` embedded 188-line config template (~200; owner: Python dependency) ·
`tools/comments.py` tracked orphan (366; owner) · asset-checker prologue (~85) · Win32 native helper ×3 (~100) ·
`sky_modes` doc/dead (~59) · `.gitignore` negations · dead preset knobs · `C2_BUILD_TESTS` for menu/soft legs (owner) ·
`c2_test_support` library · LTO per-target · clang-tidy LLVM pin · changed-file selection dedup ·
PGO presets (owner) · flashlight GLSL tri-copy (~90, high risk, needs owner visual verification).

### Owner decisions still open

Menu `SaveConfig` unification · trophy format/filename fix · packaging Python dependency ·
`C2_BUILD_TESTS` CI build-time trade · PGO removal or experimental labelling · `comments.py` delete vs document ·
GLSL include refactor · config-scanner staging policy.

## Defects logged (not fixed without an owner call)

1. **Trophy filename divergence at profile ≥10** — menu `trophy10.sav` (`Menu/Resources.cpp:1508`) vs engine `trophy010.sav` (`Trophy.cpp:383`); reachable via the `reg=` command-line option (menu UI is profiles 0-7). Classified: partial-launch/latent.
2. `g_KeyNames[256]` definition vs `extern char g_KeyNames[128]` (`Menu/Hunt.h:857/998`) — declaration-size mismatch; unverified, needs a look.
3. `CreateLog` prints "Build v2.04. Sep.24 1999" (`Hunt/Loaders/Resources.cpp:1244`) — intentional legacy or fix? **Resolved 2026-10-13:** owner chose replace; `3182616` now prints `VERSION_STRING` from `Core/Version.h`.
4. **Line-ending anomaly** — 7 files carry doubled-CR (`\r\r\n`) endings in committed blobs (CharacterCollision 682, CharacterTargeting 505, Trophy 444, Projectiles 397, Ships 359, CharacterMovement 173, Vector 150), and this environment runs `core.autocrlf=true` against mixed-normalization blobs. Handling for this pass: stage affected files byte-faithfully (`git hash-object -w --no-filters` + `git update-index --cacheinfo`) and verify `git diff --cached --stat` shows the intended minimal diff. A repo-wide normalization pass is proposed as a separate owner-approved item — not silently done here.

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
