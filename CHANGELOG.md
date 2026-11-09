# Changelog

Notable changes to Carnivores 2 - Modder's Engine v1.11 (Modernized)
are listed here.
Based on [Keep a Changelog](https://keepachangelog.com/).

Upstream Modder's Engine v1.11 is the base, not the modernization release
number. ModDB V1 through V6 correspond to GitHub v1.1.4-modernized
through v1.1.9-modernized.
Windows executable resources use major.minor.patch.build (v1.1.9.2 beta:
1.1.9.2). v1.1.9.2 is a beta pre-release built from local `main`; v1.1.9.1
(`81fe90b`) remains the last tagged patch. The beta is not a finished release
and has not been published to ModDB.
Copy-paste release summaries are in `release-notes/`.

## [v1.1.9.2-modernized-beta] - 2026-11-09

**Beta pre-release.** Changes since `v1.1.9.1-modernized` (`81fe90b`). The
automated five-preset build and CTest matrix, warning gate and release-flag
checks pass. Owner acceptance and broad mod validation are still partial. See
[`release-notes/v1.1.9.2-modernized-beta.md`](release-notes/v1.1.9.2-modernized-beta.md)
for the user-facing notes and known limits. The v1.1.9.1 items at the end of
this entry are kept for context, not announced as new beta features.

### Water sounds and footsteps

Dry decks and objects above water now use land footsteps. The old `ONWATER`
test only checked whether the tile was flooded, so walking on a bridge,
causeway or other object above a river played the water footstep set and
spawned ripple circles on the water surface below. The check now also requires
the player's feet to be at or below the water surface (`678352a`). Wading in
shallow water and surface swimming keep their water footsteps.

Dry-land footsteps no longer become water splashes after long uptime. After
roughly 24.9 days of Windows uptime, `timeGetTime()` becomes negative and
`RealTime % 3` indexed before the dry-step array, selecting loaded water impact
samples on dry terrain. Landing, dry and wet footsteps now use one unsigned,
bounded sample index (`1593709`).

Level reloads cannot return the previous level's cached sound. OpenAL buffers
are keyed by sample pointer, length and a resource generation, and retired
buffers are swept only after their voices and ambient sources release them
(`308ac66`). That removes another route to wrong water or ambient audio after
leaving and re-entering a level.

### Workbench

The OpenGL F10 panel is now a native Dear ImGui inspector. It replaces the old
GL panel and keeps the same thirteen Fog, Waves and Sun controls.

- Fog, Waves and Sun controls have sliders, validated exact entry, loaded
  resets and comparison, grouped undo/redo, help text and a change review.
- Night and Beam tabs expose the existing night grading, moon controls,
  flashlight intensity, reach and cone angles, plus a session-only RGB tint.
- The Water group exposes `water_mipmap` and `water_wave_fade_rings`.
- The Audio panel exposes 32 reverb fields across banks 0-6 and 8.
  Environment 7 stays dry and read-only.
- The collision inspector shows a bounded nearby-placement list, copied loaded
  `ofBOUND` slots, selected and resolved floor/ceiling contributions, and a
  default-off X-ray of the actual oriented geometry. A selection now lasts for
  the object's lifetime instead of four cells around the player (`b1c1b4b`).
- A review step can write changed, recognized keys to `config.cfg`. The merge
  preserves comments and line endings, updates only the value token, keeps a
  numbered `.before-workbench.bak` snapshot and replaces the file atomically.
  A refused save leaves the original file untouched (`2297a41`, `329eaba`,
  `c80702d`, `6a17733`, `0f336db`, `9b4e04d`).
- The panel labels each field's persistence and can emit a pasteable dump of
  the registry and recognized keys (`0b27fae`, `14d9d3a`).
- F8 hands input to and from the hunt. Escape returns from Test to the UI.
  Opening the panel does not pause the simulation.
- The software renderer now draws its legacy F10 panel instead of accepting
  keys with nothing on screen (`c2dcbcc`). The native Dear ImGui workbench is
  still OpenGL-only.

The performance tooling moved into `tools/perf`. It covers isolated scripted
captures, strict bracketed reports and the CSV statistics helper. See
`tools/perf/README.md` and `docs/LIVE-WORKBENCH.md`.

### Dynamic character roster

Character definitions are no longer limited to a fixed 128-entry array.
Storage is sized from the `characters` entries in `_RES.TXT`; model records are
frozen before placement so live character pointers stay valid (`30b4ef3`).

Spawn links and trophy-group membership grow as needed. Live-character,
AI-family, trophy-display, pack-member and per-definition spawn-info limits
remain separate and enforced.

Saved trophy `.sav`/`.sab` layouts are unchanged. Definition IDs remain
script-order IDs, so append variants instead of inserting or reordering entries
if old saves must keep referring to the same animals.

On x86 the metadata budget allows up to 1,833 definitions, and 1,024 fits
inside it. This does not mean a 1,024+ roster can spawn every definition in one
hunt. The 1,024-entry pack-type table, the 16 MiB `_RES.TXT` input budget, 256
live-character slots, 128 AI families, 128 trophy display slots and the menu's
32-bit launch flags still apply. Automated roster tests currently reach 513
definitions.

See `docs/MODDING-ROSTER.md` for authoring and capacity details.

### Shop, night and flashlight

The shop supports named prices and icon/text redirects for existing equipment
rows, plus a read-only shop checker. Legacy positional prices still work.
Dynamic shop rows are not implemented (`d86068e`, `037f137`, `85c654f`,
`147f156`, `7864e32`).

Moonlit night lighting has coordinated sky and fog grading, a Classic fallback,
and independent moon-disc RGB and halo-strength controls (`c3690ed`,
`ec9c6a6`, `bd92e19`). The flashlight restores brightness and color inside its
beam; night vision wins when both are active. The flashlight is a seventh
hunt-screen item with its own price fallback and configurable key, cone and
range (`543048e`, `7330249`, `108ba44`).

### Software renderer

- Sky modes 0 and 1 are implemented in software (`4785059`). The renderers use
  separate keys, `sky_mode` for OpenGL and `sky_mode_soft` for software, so the
  shipped defaults no longer force one renderer onto the other's sky
  (`8619c8e`, `2b987a2`).
- The legacy software sky keeps its original time-scrolled texture phase and
  unwrapped scanline step. The mode refactor had dropped `SKYDTime` and applied
  the level-plane step clamp to legacy mode, which froze and darkened the
  canopy into hard horizontal bands. Legacy mode is the original 1998 path
  again; mode 1 still uses the bounded level-plane step.
- Water tiles no longer backface-reject one half of a non-planar wave quad,
  which showed terrain through a diagonal tear (`e1c5804`).
- A hardware reload now recreates the software renderer instead of leaving a
  black 3D scene (`f7f7d70`).

### Collision and terrain

- `boundbox_fit 0` keeps legacy axis-aligned boxes and remains the default.
  `boundbox_fit 1` enables oriented fitting at any baked angle. The former
  value `2` is no longer accepted (`c909700`, `4a4a032`, `6f525e8`).
- `boundbox_slope 1` fits the underside independently from the top, so wall
  head-clearance and ceiling queries follow the accepted lower plane instead
  of filling the space beneath a slab down to its lowest vertex (`754fb00`,
  `598924f`).
- `terrain_mip_level` controls the GL terrain mip cap. The default keeps the
  stock 128+64 behaviour; higher values expose the full chain.
- `water_wave_fade_rings` ramps wave amplitude to zero instead of using the
  legacy hard cutoff. `0` reproduces the old gate exactly (`80cb01a`).
- The software water path honours its mip level again, and the shared
  camera-distance divide is guarded against a zero-distance tile.
- Terrain precache read footprints and cached wave expressions are covered by
  tests (`0d8027c`, `f666072`, `c9c94f7`).

### Audio

- Backward footsteps match forward loudness. Environment 7 stays dry. Ambient
  transitions keep their live loop positions and reach their exact gain
  endpoints. Moving-loop buffer replacement stops before binding.
- Failed OpenAL uploads and setup roll back instead of claiming success.
  Optional EFX uses the device-scoped capability and complete entry-point
  checks. Listener pitch follows aiming. Shutdown joins the worker before
  teardown, and OpenAL skips unused terrain geometry (`66e5f9e`).
- The game and menu share bounded RIFF parsing. PCM8/16 mono/stereo is
  normalized to mono16/22050, and all 73 deployed WAVs remain bit-exact.
  Unsupported, compressed or malformed WAVs fail cleanly, and a failed
  replacement keeps the existing samples.
- Saved OpenAL driver values 4 and 5 keep their legacy mapping (`fefc4b3`).

### Safety and correctness

- The hunter-death body append is guarded at live-character capacity. Callers
  receive the appended body, and a body-independent camera fallback prevents
  indexing past `Characters[]` or following a removed demo target (`9b25ba4`).
- The trophy pickup queue is bounded at its real capacity. A full queue drops
  the extra pickup request and its acknowledgement sound while still recording
  the trophy (`922c1b8`).
- Pack, killer and demo pointers are rebuilt after a character is removed.
  Stale pickup entries are dropped, and leader follow uses a bounded accessor
  (`eb1621c`).
- Bullets, particles, blood trails and ambient sounds use `memmove` and active
  counts for compaction instead of overlapping `memcpy` and fixed over-copies
  (`4235183`).
- Empty or oversized spawn-region lists fall back to the default map region.
  Zero or over-capacity idle-group counts are rejected before indexing their
  animation arrays (`06e0e10`).
- The MSVC `/analyze` pass fixed the caps-lock HUD gate, weapon morph phase,
  fish range defaults, trophy-save reads, the empty weapon selection and
  remaining Win32 result checks (`9448c8d`, `ef4a525`, `f531b76`, `0b1a59a`,
  `8ffd6e3`, `50a2744`, `3a13ca9`, `37665d2`, `ecdf39f`).
- Trophy exhibits no longer take bullet damage, rewrite their exhibit state or
  award score. The trophy room follows the profile view-distance setting
  (`20f8bbb`, `d26fbf4`).
- Weapon viewmodel fog is applied once, which removes the pocket-fog specular
  seam (`db58700`).
- The exit countdown and survival note no longer draw on top of each other
  (`54ecb4e`).
- Two-digit trophy names work at profile 10+ (`fb2061a`). Engine TGA validation
  is strict about the 16-bit format (`cec1877`). Menu config keys are
  case-insensitive (`37cabec`). F10 PgDn steps backwards (`433fa7f`).

### Menu, loaders and data

- Missing model files name the referring `_RES.TXT` entry. The menu refuses an
  incomplete area or trophy-room pair instead of launching into a file-open
  halt.
- Normal exits log `SESSION_EXIT` instead of `ABNORMAL_HALT`.
- The model loader checks `ReadFile`. The picture loader closes its file on
  every halt path. The empty weapon selection halts instead of reading
  `WeapInfo[-1]`.
- `config.cfg` gains missing supported defaults without replacing existing
  bytes or values. The engine and package defaults share one owner
  (`ca4e674`, `17ac7df`, `20fd3a1`).

### Build, tests and tooling

- `ScriptParser.cpp` split into a driver plus Common, Tables, Spawn and
  Character translation units. The parser contract suite grew from 20 to 34
  cases before the extraction (`12bfc54`, `4a14a03`, `cec0a86`, `ab05c53`,
  `dd4b438`).
- The menu was split into `MenuInput.cpp`, `MenuDraw.cpp`, `ConfigIO.cpp`,
  script parsing, asset loading and profile IO units (`530c939`, `99574ea`,
  `b8c4f82`, `20c5381`, `426d6e4`, `9cc6bd8`).
- Dead functions, stale comments and unused globals were removed. The test
  suite was consolidated from 686 to 487 cases while keeping every contract
  assertion (`d7c95a7`, `d74dfd2`, `af72cb2`, `429a0e5`).
- Per-preset MSVC warning baselines, OpenCppCoverage reporting and scoped LTO
  are in the CI/build path (`e381dfb`, `9b5c420`, `163e621`, `7c40f6b`).
- CMake exports `C2_BUILD_TESTS` so release legs can skip test targets.

### Performance

- Identical full UBO uploads are skipped, and unchanged draw uniforms are
  cached per GL program (`e81bd86`, `f59676b`).
- Sun occlusion sampling moved to frame start, before the depth clear. The
  measured area4_spawn run dropped frame CPU from 20.27 ms to 18.61 ms
  (`450ec73`).
- The terrain precache sweep is bounded to its read set (`c9c94f7`).

### Retained from v1.1.9.1

The v1.1.9.1 line already covered the lenient and strict script load policy,
the awareness resolver and timer ownership, the main pack and flee fixes, the
fish wander radius, trophy-room view distance, bullet-inert trophy exhibits and
weapon fog. It also added per-species `agressMulti` and the optional
`ai_logging` trace. Those changes remain in this build. See the v1.1.9.1
package notes and `CarnivoresDoc/architecture/creature-awareness.md` for the
detailed behavior.

## [v1.1.9-modernized] - 2026-09-20

ModDB label: V6. Changes since the published v1.1.8-modernized release
(`362eecd`). The published tag is `8ff8b70`; the refreshed v1.1.9 archive
also contains the post-tag fixes through `9db51ec`. Those fixes are **not**
part of the tag. See `release-notes/v1.1.9-modernized.md` for short-form notes.

### Summary
- Fix menu-launched hunts, improve mod compatibility and data validation,
  correct trophy-room and rendering issues, introduce finite creature
  awareness, and add configurable sky mapping.

### Hotfixed (after the tag; through `9db51ec`)
- Scale heard-shot and direct-hit reactions by authored aggression, restore
  the recent-damage range bypass, and search near the remembered event rather
  than running past it (`622e50b`, `c0e7299`, `927aa03`).
- Accept legacy numeric prefixes, C-style suffixes, and trailing tokens in
  `_RES.TXT` without accepting non-numeric or unsafe values (`de84813`).
- Correct hunt-submenu dinosaur pictures and descriptions for list entries
  sharing an AI slot (`9db51ec`).
- Accept zero-weight pack leaders and disabled/empty spawn data used by mods
  without accepting negative or non-finite weights (`91421cb`, `ea8ded5`,
  `91fe861`).
- Restore T-Rex shot/hit pursuit and sight/scent promotion, immediate charge
  on hit, and avoid repeated alert-animation resets (`ea8ded5`, `946b6b7`,
  `5fb1e23`, `ac1ad1e`, `30c01ad`).

### Fixed
- Disable the hunting map in the trophy room. Map availability now follows
  stable trophy-room session identity rather than the replaceable game-mode
  slot, and rendering also rejects a stale map mode defensively.
- Keep the first trophy-room plaque linked to body slot zero. Static mounts
  are now excluded from hunter-perception updates, which previously rewrote
  the first mount's slot-valued state from 0 to AI state 2 before plaque
  lookup and consequently prevented both its information panel and removal.
- Erase transient map-circle pixels reliably after closing the area map. HUD
  dirty regions are retained even when a full texture upload is pending, the
  midpoint circle's inclusive final row and column are tracked, and loading
  cleanup clears the full runtime height instead of only 768 rows.
- Keep large scenery and animated characters visible while their origins are
  outside the viewport. Oversized map placements now use a coarse per-level
  spatial index and extent-aware frustum checks instead of depending entirely
  on terrain-origin collection. Scenery extents include base geometry and all
  map-object animation frames rather than trusting understated RSC height
  metadata. Character files cache a conservative sphere across every animation
  frame; OpenGL culling uses that scaled model extent,
  the configured FOV, and the authored gameplay radius as a minimum. The
  original engine used only the authored radius (for example, stock
  Brontosaurus specifies 400 despite an approximately 1,414-unit animation
  extent), which made long animals disappear at screen edges.
- Keep terrain edge rows visible while strafing and crossing the renderer's
  snapped camera grid. The optimized OpenGL row sweep now uses the camera's
  exact residual position and expands its frustum half-planes before rejecting
  a row, while retaining the existing per-tile culls and distance fade.
- Decouple creature perception from graphical view distance. Ordinary hearing,
  sight, aggression, flying, aquatic, and hunting-dog sensing retain the
  legacy 72-cell gameplay ceiling while rendering can extend farther.
- Replace live-position knowledge from distant shots with finite awareness
  states. Creatures investigate or flee from the shot position, retaliate or
  flee from a direct hit for 60 seconds, and acquire the hunter's current
  position only through independent detection.
- Make species that fear hunter calls flee from the call-time position instead
  of continuously tracking the hunter. Call reactions now expire normally and
  cannot replace stronger awareness from sight, scent, or direct damage.
- Prevent pack alerts from granting every member the hunter's live position.
  Members that have not independently detected the hunter now follow or flee
  from their pack leader; only independent detection enables attacks and kills.
- Keep scrolled hunt lists aligned with mouse selection. Dinosaur rows were
  always drawn from the start of the list while clicks included the scroll
  offset, so selecting a visible late-roster creature could show or toggle a
  different creature. Mouse-wheel offsets now clamp without unsigned wrap.
- Accept modded `_RES.TXT` name/file lines that v1.1.8 rejected. The script
  parser matched keys by searching the whole line for "file" or "name" and
  stripped the quotes in place, so a path containing "name"
  (`models/modname/x.car`), a `filename =` key, or a name value like
  `'Profile'` was read as the model-file field and halted with "Characters
  file missing, too long, or malformed". Keys are now matched by assignment
  name, the value is read without editing the line, and spaces or `//`
  comments after the closing quote are ignored, matching
  `reference/res-txt-format.md`. The game and standalone menu now share these
  parsing rules, and the game halt message names the offending line.
- Raise `_RES.TXT` asset-path fields from 48 to 96 bytes (`SCRIPT_PATH_MAX` in
  `Hunt/Core/GameTypes.h`) while keeping display names at their legacy size.
  Mods ship model paths past the old field width
  (`models/maphuntables/mauvev/_ostafrikosaurusNIGHTM.car` is 53 characters);
  v1.1.7 overflowed into the next struct member and v1.1.8 halted on the
  line. The `TDinoInfo`/`TWeapInfo` layout assertions and the
  `dispSighting`/parser buffers that print those fields are updated.
- Keep modded `_RES.TXT` text values away from the numeric and block
  dispatch. The character and weapon line readers now recognize `name`,
  `file`, `gunshot`, `pic1`, `picc`, and `bModel` before the numeric/flag
  checks and before the spawn/trophy/kill/death/idle block openers, so a
  value containing a field key or block name as a substring
  (`models/massive/x.car`, `models/spawninfo/x.car`, `'crossbow'`) can no
  longer halt with a numeric-format error or consume the rest of the
  character section as a block body.
- Stop the projected sky's cloud pattern from aliasing into a woven band near
  the horizon. The sky texture now generates a mip chain when it is uploaded,
  so the extreme minification at low view elevation resolves to the texture's
  local average instead of sampling the base level.
- Stop the standalone menu from rejecting legacy `_RES.TXT` files it used to
  load. The menu's character reader matched field keys by substring, so stock
  values selected the wrong field: the `ai` check matched the
  `file = 'models/main_hunt/para.car'` line ("main" contains "ai") and the
  strict integer read rejected a decimal `health = 13.5`; both threw and
  aborted the menu whenever no `_MENU.TXT` was present. Numeric and flag keys
  are now matched by whole assignment key (letter case relaxed, with both the
  `smell` and `smellK` spellings accepted) and `health` keeps the legacy
  decimal truncation the engine already used.
- Reject `_MENU.TXT`/legacy price blocks with more dinosaur or weapon entries
  than the loaded roster. `ReadPrices` indexed `g_DinoInfo`/`g_WeapInfo` past
  the end of the vectors on a malformed script; it now throws a script error
  naming the line. Stock `_MENU.TXT` matches the roster exactly (10 dinos,
  7 weapons), so shipped data is unaffected.
- Stop the standalone menu from launching hunts and trophy rooms with a
  corrupted command line. The base `reg=/prj=/din=/wep=/dtm=` arguments were
  used to construct a `std::stringstream`, and the accessory, score-mod, and
  display-mode appends then overwrote that prefix from position 0 while
  `.str()` kept the old length; the surviving tail replaced the project
  argument, so every menu-launched hunt halted with "Error opening resource
  file .rsc" regardless of the mod. The base arguments are now written into
  an empty stream through `MakeLaunchParamStream()`, with a regression test
  for the append contract.
- Keep a mod character file's explicit `BLANK` animation placeholder from
  crashing the renderer. The loader accepts the zero-frame record so later
  animation indices keep their file-defined meaning, but the morph path
  dereferenced its empty frame buffer; a phase without payload now leaves the
  model's current vertices alone, including a partial morph from or into a
  blank phase.
- Reject malformed or oversized launch values before they reach fixed engine
  buffers: `prj=`/`server=` copies are bounded and logged, and `res=`,
  `x=`/`y=`, `din=`, `wep=` and `dtm=` are parsed as whole values instead of
  matching anywhere in the argument. Release builds enable `/GS` stack
  protection again (`/GS-` had disabled it).
- Parse `_RES.TXT` scalar values strictly. Indices, counts, weapon animation
  references and spawn/pack references are validated against their fixed
  arrays, an invalid value halts with the offending line instead of silently
  reading as zero, and the legacy decimal spellings for `health` and
  `killdist` are preserved. Weapon `recoil` is a float again, restoring the
  stock X-Bow's `0.35`.
- Preserve the configured night fog colours. Night mode no longer zeroes the
  red/blue fog and sky channels; the night-vision tint is the separate
  overlay.
- Harden the loaders and spawn setup: animation/resource counts, character
  morph bounds, and WAV/BMP/picture inputs are validated before use, and a
  failed survival or pack spawn no longer publishes a partially written
  character.
- Replace the fixed-depth `{}` skipper in the game's `_RES.TXT` parser with a
  nested-block parser that ignores braces inside quoted values and `//`
  comments, so pack overrides containing a directly nested `region` block no
  longer desynchronise the rest of the file.

### Added
- Configure the sky cloud mapping from `config.cfg`. `sky_mode` selects the
  legacy camera-coupled pitch offset (`0`), a world-level projected plane
  (`1`, default), or a direction-based stereographic dome (`2`);
  `sky_horizon_drop` (degrees, default 12) lowers the plane's compression
  singularity below the true horizon the way the C1 offset did, and
  `sky_dome_scale` (texels per radian at the horizon, default 384) sizes the
  dome canopy. Values are parsed strictly (whole token, inclusive range); an
  invalid entry keeps the default instead of silently selecting a valid mode.
- Apply `_RES.TXT` `common {}` survival defaults for `-survival` launches:
  `survivalArea` (1-10) selects the area, `survivalWeapon` (1-10, one-based)
  selects the weapon, and `survivalDTM` (0-2) selects day/night. Explicit
  command-line `prj=`, `wep=` and `dtm=` take precedence, and an out-of-range
  value halts with a clear message instead of silently selecting a default.

### Changed
- The sky cloud plane is now level with the world instead of following camera
  pitch, so cloud rows no longer lean while turning. Fog, pocket fog, and sun
  glow are identical in every mode; `sky_mode 0` restores the previous
  altitude-derived offset.
- Survival spawns use the shared 40-cell safety radius instead of
  `ctViewR + 1`. A large view-distance setting made the old radius exceed the
  authored region, so every survival placement failed; ambients can now
  appear closer to the hunter at the default view distance.
- The standalone menu passes `din=0 wep=0` when launching the trophy room,
  matching the creature/weapon selection it actually resolved. Previously the
  engine kept its startup defaults for that launch.

## [v1.1.8-modernized]

ModDB label: V5. Changes since the published v1.1.7-modernized release.

### Fixed
- Keep camera pocket-fog depth restricted to the camera's own pocket. A
  foreign pocket fog volume previously inherited the camera's in-fog
  depth, saturating distant objects into flat, fully fogged silhouettes
  the moment the player stepped into any pocket. Cross-pocket objects
  now keep their authored density and distance term.
- Restore weapon specular and env-map overlays. The viewmodel depth
  range introduced in v1.1.7 left the weapon body in a near slice but
  drew the specular and env-map passes at full range, so their window
  depth was ~20× the stored value and `GL_LEQUAL` discarded every
  fragment — the reflections disappeared silently.
- Scale HUD box text with resolution and unmirror the radar question
  mark. Box artwork scaled with the resolution but the text inside it
  did not, jamming into the panel corner at 1440p; the GL radar
  question-mark was also mirrored.
- Stop culling map objects with buried origin cells. The origin-cell
  burial test removed whole roof sections in Manya's Paradise cave,
  exposing the sky from inside — the original D3D/3DFX renderers
  deliberately left this test disabled.
- Hide the ammo counter for the whole weapon put-away animation (state 3).
  The counter gate (`Weapon.state`) kept the bullet icons up until the
  holster animation finished and the state reached 0 — after the gun had
  already left the screen and, on scoped weapons, after the FOV had already
  snapped back. Stock holster animations run 323-700 ms, so the leftover
  window was clearly visible on every weapon; the counter now hides as
  soon as the put-away begins and reappears when the next weapon starts
  raising.
- Launch slot six with the assets it validates: the menu now records which
  slot-six basename resolved (`external` on vanilla, `area6` on mods) and
  launches that name, and the engine's script parser aliases `external`→`area6`
  for `_RES.TXT` area filtering. Previously the menu launched `area6` even when
  it had validated `external.map` (clean abnormal resource halt on stock data),
  and a direct `external` launch applied every area's overwrite/addition blocks
  and crashed with an access violation after entering the game. File-open paths
  keep loading `external.map/.rsc`; menu log reports the resolved launch name.
- Release every persistent model/character owner and raw model buffer on
  shutdown; repeated in-process restarts keep level-arena use flat and clean
  shutdown reports no tracked leaks.
- Preserve heap-backed Level allocations in MEM_DEBUG reports instead of
  erasing them during an arena reset; allocation reports now include source,
  backend, generation, current-level peak, and session peak.
- Reject truncated or out-of-range model, character, resource, map, picture,
  sound, and `_RES.TXT` data before it can overflow fixed arrays or allocation
  arithmetic. Zero-byte arena allocations retain distinct ownership. Animated
  resource vertex counts and durations are validated,
  one-frame character animation interpolation is kept in bounds, and morph
  frame math no longer overflows 32-bit intermediates.
- Validate map texture/object/water references and the 64-entry landing list;
  fix the Software renderer's far-edge diagonal height-map read.
- Release all 256 model/sound slots, including valid zero-length sound entries.
- Make `-DMEM_DEBUG=ON` effective for non-Debug CMake configurations.

### Added
- Accessory prices in the hunt selection menu: the equipment list now
  shows each item's credit cost right-aligned next to its name (same
  style as the area/dinosaur/weapon lists), and accessories the account
  cannot cover are greyed out. The displayed value is the same price the
  selection click charges (`UtilInfo::m_Price` from `_RES.TXT` acces
  lines or the built-in defaults), so the list, the click gate, and the
  debit all stay consistent.
- Deterministic OpenGL performance-capture tooling with manifest-driven C2
  scenarios, repeated 120-frame samples, exact configuration/save/ReShade
  restoration, screenshots, hashes, machine provenance, and fail-closed
  CSV/GPU/runtime-log validation. Generated evidence remains ignored.
- Unit coverage for arena alignment, reset/restart lifecycle, loader arithmetic,
  animation timing, map references, exact reads, and script helper boundaries.
- Allocator-matched `make_heap_object<T>()` and `make_heap_array<T>()` helpers.

### Changed
- Instance synchronized animated map objects on the GPU. Shared-pose
  VBO refresh plus ordinary instancing replaces the per-placement CPU
  geometry build for animated scenery, keeping legacy lighting and
  asset formats. Transparent, water-intersecting, and oversized
  ground-lit cases stay on the legacy path.
- Restore the original double-click-a-name shortcut on the player registration
  screen: double-clicking a profile in the name list enters the menu directly
  (the same commit path as the GO button and the Enter key), so loading an
  existing player no longer requires a separate OK click. Single clicks still
  just select a profile.
- Precompute frame-invariant terrain-grid axis coordinates and yaw products in
  `PreCashGroundModel`; matched five-run captures reduce that CPU scope by
  5.4% without changing terrain or animated-water calculations.
- Profile terrain and water GPU draws directly around `glDrawArrays`, after
  stream synchronization and upload, so pass timings no longer attribute
  command-submission gaps to draw execution.

## [v1.1.7-modernized]

ModDB label: V4. Changes since the published v1.1.6-modernized release.

### Fixed
- Mount trophy-room kills as static exhibits instead of live animals; the
  room no longer bounces back to the menu, and invalid saved species are
  skipped with a log line instead of crashing the load.
- Restore trophy-room collisions, hunt-info plaques, removal, the exit
  banner, and Escape save-and-quit so they survive game-mode slot changes
  (swimming, binoculars, map, pause).
- Match modifier-key bindings side-aware (default Left Shift sprint works
  on fresh saves) and fire toggles once per press instead of retriggering
  on Windows key auto-repeat.
- Read config.cfg past NUL padding: the engine no longer silently keeps
  its 60 FPS default when the file starts with NUL bytes; effective
  fps_limit/fov/object_detail are logged at startup.
- Keep world depth under viewmodels by drawing weapons in a near depth
  slice (depth-based post-processing and sun occlusion see the world
  again) while the gun stays always-on-top.
- Unity-magnification optics (red-dot sights) no longer take the
  scope-mask path: no viewmodel pop and no vanishing wind/compass
  overlays at 16:9.
- Menu: the sixth hunt slot falls back to area6.map for mods; ambient
  music continues across submenus and is silenced only around external
  processes (hunts and the trophy room).

### Notes
- Windows executable resources now report 1.1.7.0.
- config.cfg files padded with NUL bytes are recovered automatically on
  load (logged); resaving from the menu writes a clean file.

## [v1.1.6-modernized]

ModDB label: V3. Changes since the published v1.1.5-modernized release.

### Fixed
- Retain camera-pocket fog on clear-cell terrain, characters, and shadows with the appropriate pocket colour.
- Clamp camera fog opacity to prevent inverted skies and white blowout.
- Restore dinosaur call responses and fish splash particles by correcting distance-calculation overflow.
- Preserve scope/binocular zoom through menu and pause transitions; night vision no longer breaks scope zoom.
- Close the area map when drawing a weapon and prevent reopening it while the weapon is raised.
- Eliminate weapon sheen shimmer from mismatched overlay/base clipping.
- Recalculate equipment debit immediately and refuse unaffordable selections; safely fall back from overdrawn saved setups.
- Preserve comments and settings not owned by the menu when saving config.cfg.

### Added
- Remember the last hunt's area, creatures, weapons, accessories, and time of day.
- OpenAL EFX preset overrides: envN_decay, envN_decayhf, envN_diffusion, and envN_reverb, with validated numeric input and safe rejection logs.
- F10 SUN tab with independent glare and sun/moon disc controls (0-2, 1=normal), reset per hunt; PgUp/PgDn cycle tabs and D dumps values.

### Changed
- Generic and Forest reverb decay defaults reduced to 1.49 seconds; other preset fields unchanged.
- Centralize menu/mode transitions to preserve overlay state consistently.
- Prominently credit Ornithomimid1 (Oli) and link the upstream Map-Amb-Demo-2 branch.
- Retain upstream MEE v1.11 in the product title and identify the project release as Modernized v1.1.6 (Windows resources: 1.1.6.0). V3 is only the ModDB label.

### Notes
- Audio overrides require a restart and OpenAL EFX. Preset 7 remains a no-op; the exposed controls do not fully disable early reflections.
- Back up config.cfg and saves before updating; preserve your config to keep personal settings.

## [v1.1.5-modernized] - 2026-09-05

ModDB label: V2. Summary of the published release.

### Fixed
- Correct wide-FOV terrain/water culling and horizon fog, including the software renderer.
- Apply the selected resolution in fullscreen/borderless modes and restore the desktop after exclusive fullscreen.
- Keep projectiles working at maximum view distance and guard against projectile-list overflow during sustained fire.
- Correct arrow/bolt orientation and prevent stuck bolts rotating with the camera.
- Disable weapons in the trophy room and restore usable key defaults for fresh or foreign saves.
- Fix shared trophy/resupply ship audio, ship spawn visibility, and menu music continuing into hunts.
- Render fully submerged scenery, animate map objects in OpenGL, and keep distant sprites upright when looking up/down.
- Preserve crouch/aim behavior and keep compass/wind HUD placement consistent while aiming.
- Make maximum object detail reachable; allow observer mode without a loadout.
- Correct accessory price mapping on five-line mods, retain the NV entry with fallback art, and reject malformed menu art.

### Added
- Rifle breath-aim support via optional breathaim = TRUE in the weapon's _RES.TXT block, alongside sniper breath-focus behavior.
- Persistent windowed/exclusive/borderless display selection and documented resolution/display_mode config keys.

## [v1.1.4-modernized] - 2026-09-04

Initial public release of the modernization. Built on the community
Carnivores 2 Modder's Engine v1.11 - huge credit to Ornithomimid1. This is a modernization of the engine source; it does **not**
include any original game assets.

### Added
- Native OpenGL 3.3 renderer (hardware terrain, models, sky, HUD/minimap, projected character shadows)
- Standalone settings menu (`Carnivores2Menu.exe`) - FOV, view distance, object detail, FPS limiter, keybindings
- Night vision equipment with bindable key and night desaturation
- Realistic water: waves, depth fog, underwater gradient and overlay
- OpenAL audio backend with EFX reverb (dynamic OpenAL Soft loader)
- New sky rendering: sun/moon halos, horizon gradient, cloud-aware lighting
- Dynamic resolution enumeration, widescreen support, borderless fullscreen
- Configurable view distance and per-pixel distance fog
- Memory arena allocators and stability/leak fixes

### Changed
- Modernized rendering, audio, and configuration paths (config.cfg text file replaces registry)
- Restructured monolithic sources into modular directories

### Notes
- Requires the original game's HUNTDAT (no copyrighted assets included)
- Windows x86, OpenGL 3.3 for the GL renderer; software renderer fallback included
- Source licensed MIT; see LICENSE and NOTICE.md
