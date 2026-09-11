# Changelog

Notable changes to Carnivores 2 - Modder's Engine v1.11 (Modernized)
are listed here.
Based on [Keep a Changelog](https://keepachangelog.com/).

Upstream Modder's Engine v1.11 is the base, not the modernization release
number. ModDB V1/V2/V3/V4/V5 correspond to GitHub v1.1.4/v1.1.5/v1.1.6/v1.1.7/v1.1.8-modernized.
Windows executable resources use major.minor.patch.0 (currently 1.1.8.0).

## [Unreleased]

## [v1.1.8-modernized]

ModDB label: V5. Changes since the published v1.1.7-modernized release.

### Fixed
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
