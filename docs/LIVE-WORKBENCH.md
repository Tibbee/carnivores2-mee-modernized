# Live modder workbench — local preview

OpenGL only. Overlay began at `af2aee3` (2026-10-06); atmosphere controls at
`2b53b5d` and combined collision preview at `38efa89` (2026-10-08). The feature
is merged to `main` (fast-forward via `feat/modder-workbench` and the
consolidation pass, `5deb8b0` on 2026-10-16; MSVC-analysis fixes on top,
`6c51c70`). It is a tested local Shipping preview, not a published release.
Software keeps the original F10 text panel. The separate MEE 1.2 migration is
not required.

The current deployed preview at `E:/Games/CarnivoresLegacy` is the analysis-fix
build of `main` and includes the workbench. The earlier October 9 combined build
(`598924f` collision checkpoint, Audio `75db99a`, Collision inspection `e931f13`,
lifecycle isolation `201ffd8`) preserved all 639 protected files, and its
[delivery record](../../CarnivoresDoc/working/sessions/c2-workbench-audio-collision-2026-10-09.md)
remains the integration/deployment evidence. New listening/real-map visual
acceptance remains separate from the merged state. Source and builds live in
`Carnivores2MEE1.11/` on `main`; the temporary sibling worktree was removed at
owner request and its evidence is archived.

## Quick start

- **F10:** open/close the compact floating inspector.
- **UI mode:** mouse and keyboard belong to the tool, including outside its panel.
- **F8:** switch to Test scene for normal movement/look; F8 or Escape returns to UI.
- Choose Fog, Waves, Water, Sun, Night, Beam, Audio or Collision. Drag a slider, or type an
  exact number and press Enter. Rejected text never replaces the previous valid runtime value.
- Reset restores the captured loaded value. Compare loaded previews the baseline
  without deleting edits; Return to edited restores them. One drag is one undo.
- Undo/Redo buttons or Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z when no widget is active.
  A focused text box keeps its own text-editing undo.
- Help shows actual parameter keys, bounds, units and effects; Changes lists edits.
- Dump runtime to log or D (when no widget is active) keeps the legacy dump workflow.
- UI scale changes readability; the window can move/collapse and scroll.

**The hunt is not paused.** Creatures, water, physics, recoil and underwater damage
continue. Edit in a safe scene. Test mode does not automatically unpause a hunt.
Scope/map/pause remain game states, not workbench modes. Escape in UI ends an
active interaction/focus first, otherwise closes the panel.

Held keys/buttons must be released before they can become fresh gameplay actions
following ownership changes. Focus loss returns an open Test session to UI;
closing and renderer recreation retain applied edits. Hunt restart restores the
original legacy tuning defaults and starts a new loaded baseline/history generation.
Invalid diagnostic state cannot prevent these ordinary legacy hunt resets.
Atmosphere previews return to their captured configured values when the snapshot
is still owned; new external/config values are not overwritten during restart.

## Night, beam and scene context

Night adds world brightness/desaturation, sky brightness, moon RGB brightness and
halo strength. These existing settings apply during night hunts; night vision
bypasses world/sky grading. Beam adds intensity, reach and ordered cone angles,
plus a linear-RGB tint picker (Ctrl-click a channel for exact entry).

Integer settings reject fractions, rather than silently rounding. Inner cone
must stay at least one degree below outer; sliders respect the pair and exact
entry rejects invalid pairs. Tint edits/reset/compare/history cover RGB together.
Beam preview requires purchased equipment, switch on and a living hunter; the
workbench never grants equipment. No obsolete Classic/Moonlit selector is added.

Water exposes the two existing config-backed water settings:
`water_wave_fade_rings` (distance over which the wave displacement fades to
zero before the legacy cutoff; 0 restores the hard cutoff) and `water_mipmap`
(software renderer only; 1 uses the distance mip chain, 0 keeps legacy
full-resolution water). Both are integer settings and both save through the
workbench review; `water_mipmap` has no visible effect under OpenGL.

Expand Scene context for read-only project/time/renderer, underwater/pocket-fog,
requested audio environment and equipment state. Environment is the control-state
request, not confirmation from the audio backend. Booleans/time are not editable.

## Audio / Environment (2026-10-09 source checkpoint)

Audio has a prominent read-only live scene/backend status card, separate from the
**PRESET EDITOR / manual selection** section. The preset selector never follows
scene changes; **Edit scene-requested preset** is the explicit shortcut to that bank.
It changes the editor only, not the gameplay environment.
Audio shows backend request/application/capability separately from Scene context.
Select a bank to edit decay, HF decay ratio, diffusion and integer late-reverb mB;
this does not force the gameplay environment. Bank 7 is dry/read-only and inactive/
unsupported backends disable tuning. Reset bank/group, history and comparison use
loaded config values. Active-bank edits refresh checked EFX without restarting loops.
Expand Voices/ambient for <=10 Hz observations: playback validity, configured gain,
relative/world position, listener distance and fade. Gain is not final audible volume.
Legacy/inactive states stay explicitly unknown. No automatic audition; a file
write happens only through the panel's explicit save review below.

Four build/test presets pass; new listening/owner acceptance remains pending.
Collision inspection is implemented in the following independent source checkpoint.
New panels require their own listening/visual acceptance; October 8 acceptance remains
for the previous visual controls/input and combined collision engine.

## Collision inspection

Select a nearby placement from the clamped 9 x 9 list (maximum 81 candidates).
Placements validate the actual loaded model slot, not `OCount` (which is scratch
state for internal records of the last loaded model). The inspector copies up to
eight actual loaded ofBOUND slots and displays IDs,
flags, placement/rotation, extents and independent plane gradients. Legacy circle/
square policies are labelled without invented fitted boxes. Nothing is refitted.
Selected eligible planes are separate from resolved neighborhood floor/ceiling
queries; map-edge/reload-unsafe queries are unavailable. Ceiling includes the
engine terrain +20480 fallback, not proof of a physical roof.

Selected X-ray is default-off: green footprint at object origin, gold top, blue
underside, grey connecting edges. Optional magenta is the collision wall +2 margin.
Loaded padding is already present; no second +72 is added. Projection follows the
existing camera and clips near plane/viewport. It is explicitly not depth-tested.
F8 Test may retain the overlay; stale session/cell/object/nearby selection clears.
Closed/collapsed skips scans/projection. No physics, picking, pause or map writes.

## Saving to config.cfg

The panel's **SAVE TO CONFIG.CFG** section writes the changed fields that have a
recognized config key: the nine night/beam scalars, the 32 effective `envN_*`
reverb fields and the two water keys `water_wave_fade_rings` and
`water_mipmap` (software renderer only; OpenGL ignores it, but the value is
saved for the software renderer). It is a two-step action — **Review changes** shows the resolved
destination, the exact lines and their old/new values, then **Confirm write**
performs the save — so closing the panel or moving a slider writes nothing. Only
the changed keys' own value tokens are rewritten; comments, unknown keys,
duplicate earlier lines and every terminator survive byte for byte. A numbered
`config.cfg.before-workbench.bak` snapshot is kept beside the file, and a
refused save leaves the original untouched and no litter behind. The values
apply on the **next launch**; the running scene is unchanged and there is no
in-engine reload yet. The legacy 13 fields and the RGB tint have no loader key,
so the review lists them as unpublishable rather than writing a setting the
loader ignores.

## What is not saved

All 59 scalar/channel descriptors are global runtime tuning, not per-area
definitions. Editing a value changes the running session; closing keeps applied
values until the hunt's ordinary reset. The review save above is the only path
that writes a file, and it writes only `config.cfg`; no profile, trophy or
resource file is rewritten. ImGui layout is session-only. Legacy dump/tint
identifiers are not config keys. D deliberately keeps the original 13-field log
dump unchanged. No picking, pause/step, docking or creature/weapon/region
editing. Spatial occlusion remain outside this implementation; collision
inspection is strictly read-only.

## Owner acceptance and performance

On 2026-10-08 the owner confirmed all the discussed gameplay acceptance checks
were completed, including editing/focus/lifecycle and Night/Beam/collision checks.
This is owner-reported acceptance, not assistant-observed testing. The
[performance tools](../tools/perf/README.md) were subsequently exercised in an
owner-authorized hidden/silent series on 2026-10-09 against collision/audio main
without the workbench. Captures/modes validate, but several-ms closed drift exceeds
the estimated +0.311 ms UI cost; stable overhead/foreground timing remain unresolved.
Precache stays ~1.9 ms. That series did not replace the installed preview. Hidden GPU/widget tests
and hidden captures are not foreground gameplay FPS/input-latency evidence.

[Canonical behavior](../../CarnivoresDoc/architecture/live-modder-workbench.md)
· [combined verification/deployment](../../CarnivoresDoc/working/sessions/c2-workbench-integration-2026-10-08.md)
