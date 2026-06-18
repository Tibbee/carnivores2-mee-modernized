# Current Status — C2 ME v1.11

> **Generated:** 2026-06-18
> **Branch:** `feature/opengl-improvements` (active integration branch)
> **Source of truth:** git log of C2 ME repo + CarnivoresPort/ planning docs.
> **This file is the single source of truth for "where are we right now".**
> If a claim in another doc conflicts with this file, this file wins.

---

## Build state

| Preset | Status | Source file(s) | Notes |
|--------|--------|----------------|-------|
| `d3d-debug` | ✅ | `Hunt/RendererD3D.cpp` (6,238 lines) | Unmodified from baseline |
| `d3d-release` | ✅ | same | |
| `soft-debug` | ✅ | `Hunt/RenderSoft.cpp` + `Hunt/renderasm.cpp` (4,769 lines, x86-only) | |
| `soft-release` | ✅ | same | |
| `glide-debug` | ✅ | `Hunt/Render3DFX.cpp` (5,054 lines) | Requires `deps/Glide2/` (gitignored) |
| `glide-release` | ✅ | same | |
| `ogl-debug` | ✅ | `Hunt/GLRenderer.{h,cpp}` + `GLShader.{h,cpp}` + `GLUI.cpp` + `IRenderer.h` | **Verified in this session** — `ninja: no work to do.` |
| `ogl-release` | ✅ | same | |
| `menu-release` | ✅ | `Menu/*` (renderer-free) | |

`Carnivores2Menu.exe` is the menu target output. `Carnivores1_<RENDERER>.exe` is the full game target output. See `AGENTS.md` § "Build System" for paths.

---

## Modernization port coverage

Comprehensive plan: `CarnivoresPort/docs/porting-plan.md`. Status of each phase:

| Phase | Description | Status | Notable commits |
|-------|-------------|--------|-----------------|
| 0 | Baseline verification | ✅ | `d39c7be` |
| 1.1 | `constexpr` for `#define`s (101 sites) | ✅ | `913e616` |
| 1.2 | `typedef struct` → plain `struct` (71 sites) | ✅ | `d6d9198` |
| 1.2b | Local plain structs | ✅ | `20f944d` |
| 1.3 | `nullptr` sweep | ✅ | `3805727` |
| 1.4 | `static_cast` sweep | ✅ | `aa87c59` |
| 1.5 | `bool`/`true`/`false` | ✅ | `d506fff` |
| 1.6 | `shl edi, 11` → `imul edi, VideoPitchB` in 31 sites | ✅ | (rolled into Phase 1.6 implicit fixes; `VideoPitchB` computed in `SetVideoMode`, used in `renderasm.cpp`) |
| 1.7 | `using` aliases | ✅ | `c9687d2` |
| 2.1 | `FastInvSqrt` | ✅ | `c163e79` |
| 2.2 | `atan2f` for `FindVectorAlpha` | ✅ | `9636d7c` |
| 2.3 | Squared-distance comparisons (math helpers + AI) | ✅ | `061d22e`, `aba1879`, `97db71a`–`fd59660` |
| 2.4 | Terrain distance fade (precompute, `<<10` row stride) | ✅ | `ff9c502`, `9f92b39` |
| 2.5 | Trace function optimizations | ✅ | `061d22e`, `aba1879` |
| 3.1 | OpenAL audio | ✅ | `5e4d647`, `1b751de` |
| 4.1 | FOV system (`OptFov`, `FovScaleFromDegrees`) | ✅ | `3173ff7`–`cc8c637` (15 commits, see `progress/2026-06-07-widescreen-fov.md`) |
| 4.2 | View distance (`OptViewR` extension, full terrain ring traversal) | ✅ | `82772e8` |
| 4.3 | F1 in-game settings overlay | ⛔ **Cancelled** — FOV/distance went into `Carnivores2Menu` instead | n/a |
| 4.8 | `EnumerateResolutions` + dynamic resolution list | ✅ | `800a6c0`–`83ec888`, `9136e76` |
| 5A | `Memory.h` tagged allocator types | ✅ | `c43c16b` |
| 5B.1 | `TSFX::lpData` → `std::vector` | ✅ | `9ad1e84` |
| 5B.2 | `Hunt.h` + `Resources.cpp` smart pointers | ✅ | `296fd6c` |
| 5C.1 | `ReleaseGlobalResources` + `ShutDownEngine` calls | ✅ | `6b7ea68` |
| 5C.2 | `LevelArena` construction + tag per-level allocs | ✅ | `3163571` |
| 5D | `Characters.cpp` (bundled into 5B.2; no separate commit needed) | ✅ | (in 5B.2) |
| 5E | Tag remaining `_HeapAlloc` calls | ✅ | `572adb1` |
| 5F.1 | `MEM_DEBUG` infrastructure | ✅ | `2d735f4` |
| 5F.2 | Wire `MEM_DEBUG` into lifecycle | ✅ | `13bcf59` |
| 6.1 | Ambient creatures teleport fix | ✅ | `95ae1d2` |
| 6.2 | Ambient sound suppression (trophy/observation modes) | ✅ | `b651421` |
| 6.3 | Trophy text layout UI scaling | ✅ | `4c17ef7` |
| 6.4 | `kViewGridCenter` (replace hardcoded `64`) | ✅ | `ab06bf2` |
| 7.1 | Picture registration member buffer | ✅ | `746b6ab` |
| 7.2 | Terrain rendering trim/short-circuit | ✅ | `06c93ec`, `3b4391f`, `bc8e23f` |
| 7.3 | Hot path optimizations (characters) | ✅ | `7755819` |
| 8.1 | Windowed mode (`-windowed`, `-nofullscreen`) | ✅ | `6ec6c2e` (soft), `57d5c85` (GL) |
| 8.2 | Resolution change without restart | ❌ | (no commit; `OptRes` is set at startup) |
| 9.1–9.5 | OpenGL scene (terrain, water, models, UI, HUD) | ✅ | see `progress/2026-06-10-opengl-terrain-water.md`, `2026-06-12-opengl-scene-completion.md` |
| 9.6 | OpenGL fog system rewrite (camera-centric) | ✅ | see `progress/2026-06-14-opengl-fog-system.md` |
| 9.7 | OpenGL hardware minimap (`DrawHMap` via `lpVideoBuf` path) | ✅ | `0597760` |
| 9.8 | OpenGL weapon viewmodel phong + env maps | ✅ | `b5d193a`, `d58d48d`, `34832a8` |
| 9.9 | OpenGL projected character shadows | ✅ | `3f55c28` |
| 9.10 | OpenGL `CopyHARDToDIB` screenshots | ✅ | `0dd25a6` |
| 9.11 | `RenderFSRect` texture leak fix | ✅ | `0dd25a6` (uses `m_whiteTexture`) |
| 9.12 | Health bar color quantization match | ✅ | `0dd25a6` |

---

## What's still open

### Menu
- **TGA slider art** (`Menu/Menu.cpp:619 DrawSliderBar` is GDI-procedural). 4–5 hours. Modder-reskin support.

### OpenGL renderer
- **Water fog per-body color** — `WaterList[].fogRGB` is not wired. Only `VMap2[].ALPHA` is currently used.
- **OpenGL Phase 0 deferred tooling:**
  - `--glperf <map> <duration> <output.csv>` batch mode (F11 capture is the current workflow).
  - RenderDoc cross-validation guide.
- **OpenGL Phase 2 perf (8 of 24 tasks done):**
  - 2.7, 2.8 — shader consolidation, back-face cull
  - 2.10–2.19 — independent optimizations (clipping, shadows, elements, sun, GPU frustum cull)
  - 2.21–2.24 — texture array for models, single `RenderMappedObject` path, element cull on GPU, cheap `GetSkyK` proxy
  - See `CarnivoresPort/docs/design/opengl-gpu-migration.md` §2 for the full task list.
- **OpenGL Phase 3 refactors (all evidence-gated, none started):**
  - 3.1 multi-threaded command buffer recording
  - 3.2 async asset streaming
  - 3.3 scene graph overhaul (octree)
  - 3.4 GL 4.x feature adoption (compute, SSBOs)
  - 3.5 OpenGL ES / ANGLE / WebGL — explicit no-go.

### Other
- **Resolution change without restart** (Phase 8.2). Reinit all renderers, late-phase item per `porting-plan.md`.
- **`SoftRenderer` adapter** wrapping D3D/3DFX/Soft under `IRenderer`. Low priority; legacy renderers un-wrapped.

### Hygiene
- **C1 working tree has 3 uncommitted `// TEST:` files** (`.gitignore`, `Hunt/GLRenderer.cpp`, `Hunt/Game.cpp`). These are the *source of truth* for future ports; commit with proper scope or revert.

---

## Active branches in C2 ME

| Branch | Commits ahead of `main` | Notes |
|--------|------------------------|-------|
| `main` | 0 | Clean baseline (`44ca16d chore(repo): initialize repository`) |
| `port/modernization-c2` | 145 | Historical integration branch. **No unique commits** (zero in `git log feature/opengl-improvements..port/modernization-c2`). |
| `feature/opengl-improvements` | 151 | **Active**. Strict superset of `port/modernization-c2` + 6 GL polish commits: `302698c` (water quick wins + terrain cache), `3122e74` (fog color), `4ab70c8` (shared water wave offsets), `ec07f40` (water alpha fade to GPU), `0d4b2e5` (water wave cache lookup wire-up), `8d02aed` (water-pass used-layer mask cache). |

---

## Documentation drift

The following `CarnivoresPort/docs/` files are stale as of 2026-06-18. Read with skepticism; cross-check against git log.

- `reports/2026-06-06-c2-me-audit.md` — claims C2 ME lacks `IRenderer`, `GLRenderer`, `Memory.h`, `FastInvSqrt`, `atan2f`, FOV system, etc. **All false now** — those features exist.
- `reports/c1-modernization-reference.md` — same false claims.
- `reports/opengl-phase0-baseline.md` and `opengl-phase1-results.md` — predate the 2026-06-14 fog rewrite. Re-baseline after the rewrite.
- `design/community-menu-mee-analysis.md` — claims our menu lacks audio, save-on-quit, `.c2map`, score multipliers. **All false now** — see `issues/menu-feature-gaps.md` for the current list.
- `design/memory-system-migration.md` — describes the migration plan as "not started". **All done** (5A–5F).
- `analysis/c1-debt.md` — several items now obsolete (e.g., §1 claims GL presets are broken, §2/§3 claim `IRenderer`/`GLRenderer` are missing, §4 says OpenGL renderer is "deferred" — none of which is true).
- `progress/*.md` — most are accurate as of their date but the timeline is incomplete past 2026-06-14.
- `porting-plan.md` — "Recommended Start Sequence" lists every done phase as "Not started" — see the table above for the actual state.

For an audit of all stale claims with file:line evidence, see `CarnivoresPort/docAnalysis.txt` (generated 2026-06-17).
