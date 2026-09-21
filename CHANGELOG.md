# Changelog

Notable changes to Carnivores 2 - Modder's Engine v1.11 (Modernized)
are listed here.
Based on [Keep a Changelog](https://keepachangelog.com/).

Upstream Modder's Engine v1.11 is the base, not the modernization release
number. ModDB V1 through V6 correspond to GitHub v1.1.4-modernized
through v1.1.9-modernized.
Windows executable resources use major.minor.patch.0 (currently 1.1.9.0).

## [Unreleased]

### Changed
- Route all hunter awareness through one resolver and navigator
  (`Hunt/Game/CharacterAwareness.*`). Heard shots, direct hits, hunter calls
  and contact-range promotion share one eligibility, priority and timer path;
  the navigator owns every hunter-directed destination and the single
  reaction timer, and animators only read the response.
- Unify the hunter attack geometry (per-family look offsets) and the kill
  gate. Every kill now requires exact tracking and the family attack reach;
  fixed reactions, remembered event positions, awareness-less proximity and
  expired locks never authorize a kill.
- Tracking timers always expire now. A creature that loses the hunter
  returns to ordinary behavior when its reaction time runs out instead of
  chasing the live position indefinitely; its reaction time is refreshed
  normally while sight or scent keeps working.

### Removed
- The legacy `awareHunter` boolean. `hunterAwareness` is the single awareness
  state and `IsHunterAware()` derives from it; `TCharacter` remains 344 bytes,
  so save compatibility is unchanged.

### Fixed
- Let a fleeing creature crush the hunter at contact range. A huge sauropod
  that panics and runs over the player no longer passes through harmlessly:
  the kill gate accepts a fixed flee reaction at the species' authored
  attack reach, while the reach check keeps a remembered event point from
  ever authorizing a kill at a distance. The detached observer camera stays
  exempt; debug mode is still lethal.
- Restore the threat of species that have no active sight or scent. A
  creature in a fixed pursuit (shot investigation or hit retaliation) now
  treats a hunter who physically enters its authored attack reach as detected
  and promotes the reaction to exact tracking for 60 seconds. Defending
  sauropods (Amphicoelias, Brontosaurus) and wounded aquatic predators are
  dangerous again without gaining perception they were not authored to have; a
  remembered event position at a distance still never authorizes a kill, and
  flee reactions remain non-lethal.
- Let a reacting dangerous fish (Mosasaurus, Lacunepa) use its ordinary
  in-water proximity sense during a shot or hit reaction instead of only while
  idle. A wounded aquatic predator no longer loses the ability to hunt a
  hunter it can sense, and re-shooting it no longer refreshes a harmless
  pursuit instead of a real one.
- Hunter events (heard shots and direct hits) now use the species' authored
  aggression range scaled by `kHunterEventRangeScale` (2.5) instead of the
  binary rules that preceded it. A predator whose range covers the event
  (Carnotaurus, 72 x 200) charges it at any distance it can hear, while a
  low-aggression species (Pachycephalosaurus, 72 x 60 -- which reuses the
  Allosaurus AI clone) still flees from a genuinely distant event instead of
  charging the source. Authored `fearHearShot`/`fearShot`/`defensive` and
  passivity always flee. The scale is a single documented tuning value.
- Restore the recent-damage bypass on the ordinary acquisition range: a
  creature that was just shot keeps engaging beyond its normal range for 90
  seconds instead of immediately fleeing once the hunter moves out of range.
- Fixed reactions no longer run past the stored event position. When a
  pursuit reaches the event area without detecting the hunter, it stays alert
  and searches locally (`kShotSearchRadius`, 2048) until the reaction timer
  expires instead of sprinting forward or dropping straight to normal wander.
  Flee and search targets are clamped to the map so an extension cannot park a
  creature against the world edge.
- Give distant shot reactions enough time to reach the stored position. The
  proximity-based investigation time is floored at the species travel time
  plus the minimum search window (capped at 60 seconds), so a slow creature
  no longer times out mid-route and starts wandering far from the event.
- Permit zero-weight pack members used by legacy mods for leader-only creature
  types. Zero-ratio entries are excluded when positive follower weights exist;
  all-zero packs retain the legacy first-member follower fallback. Negative and
  non-finite ratios remain rejected.
- Preserve the T-Rex's dedicated aggressive response to audible shots and
  direct hits. Its intentionally omitted `aggress` value no longer puts its
  specialized, non-fleeing state machine into an unsupported flee reaction.
  A T-Rex following the fixed source of a shot or hit can now upgrade to
  continuous hunter tracking when it actually sees or smells the hunter.
  A direct hit cancels any pending look/roar notice and starts the charge
  immediately, matching the original game's response. Additional hits preserve
  that tracking and no longer restart its notice/roar sequence on every bullet.
  Repeated hits likewise avoid reinitializing alert animations for other
  dinosaurs that are already aware of the hunter.
- Parse `_RES.TXT` scalars with legacy `atoi`/`atof` semantics again: a
  valid numeric prefix wins and trailing text is ignored, so decimal literals
  on integer fields (`scale0 = 1000.0`), C-style suffixes (`runspd = 1.5f`),
  and stray trailing tokens load instead of aborting the hunt. Non-numeric,
  overflowing, and non-finite values are still rejected.
- Allow zero-weight spawn-group entries and skip pack groups with no members.
  Legacy mods disable a spawn entry with `spawnratio = 0`, and unused
  "template" pack groups may reference spawn groups without defining members;
  both now behave as no-ops instead of aborting the hunt. All-zero spawn
  groups keep the first-entry fallback, and negative or non-finite ratios
  remain rejected.
- Number hunt-submenu dinosaur pictures and descriptions by list position
  instead of the AI slot. The stock `_MENU.TXT` roster gives Iguanodon and
  Carnotaurus the same AI (17), so the old `ai - 9` lookup showed Carnotaurus
  the Iguanodon picture and T-Rex the Carnotaurus picture (and the matching
  INFO text). Each entry now resolves to its own `dinoN` asset, and an
  explicit `pic` line overrides the default thumbnail.

## [v1.1.9-modernized] - 2026-09-20

ModDB label: V6. Changes since the published v1.1.8-modernized release.

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
