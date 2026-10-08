# Live modder workbench — local preview

OpenGL only, on `feat/modder-workbench`. Overlay began at `af2aee3` (2026-10-06);
atmosphere controls at `2b53b5d` and combined collision preview at `38efa89` (2026-10-08).
This is a tested local Shipping preview, not a published release. Software keeps
the original F10 text panel. The separate MEE 1.2 migration is not required.

The deployed combined build includes main's reviewed collision checkpoint `598924f`.
Source/builds live in sibling `Carnivores2Workbench/`; main itself remains collision-only.
Future main updates need integration/testing here before another combined deployment.

## Quick start

- **F10:** open/close the compact floating inspector.
- **UI mode:** mouse and keyboard belong to the tool, including outside its panel.
- **F8:** switch to Test scene for normal movement/look; F8 or Escape returns to UI.
- Choose Fog, Waves, Sun, Night or Beam. Drag a slider, or type an exact number and press Enter.
  Rejected text never replaces the previous valid runtime value.
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

Expand Scene context for read-only project/time/renderer, underwater/pocket-fog,
requested audio environment and equipment state. Environment is the control-state
request, not confirmation from the audio backend. Booleans/time are not editable.

## What is not saved

All 25 scalar/channel descriptors are global runtime tuning, not per-area definitions.
Closing keeps applied values until the hunt's ordinary reset, **not a file save**.
No config, profile, trophy or resource files are rewritten by editing. ImGui layout
is session-only. The nine night/beam scalar keys already exist in config and are
labelled as such, but the tool never writes them. Legacy dump/tint identifiers
are not config keys. D deliberately keeps the original 13-field log dump unchanged.
No picking, pause/step, docking, creature/weapon/region editing or audio tuning.

## Acceptance still needed

Please check readability, slider/exact entry, comparison/history, UI/Test/focus
transitions, held buttons and map/scope/pause combinations in the real hunt.
Hidden native GPU/widget tests are not full in-game acceptance or performance
benchmarks. No game/menu was automatically launched for this preview.

[Canonical behavior](../../CarnivoresDoc/architecture/live-modder-workbench.md)
· [combined verification/deployment](../../CarnivoresDoc/working/sessions/c2-workbench-integration-2026-10-08.md)
