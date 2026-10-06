# Live modder workbench — local preview

OpenGL only, on `feat/modder-workbench`, implemented at `af2aee3` (2026-10-06).
This is a tested local Shipping preview, not a published release. Software keeps
the original F10 text panel. The separate MEE 1.2 migration is not required.

## Quick start

- **F10:** open/close the compact floating inspector.
- **UI mode:** mouse and keyboard belong to the tool, including outside its panel.
- **F8:** switch to Test scene for normal movement/look; F8 or Escape returns to UI.
- Choose Fog, Waves or Sun. Drag a slider, or type an exact number and press Enter.
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
original tuning defaults and starts a new loaded baseline/history generation.

## What is not saved

All thirteen fields are global runtime tuning, not per-area authored definitions.
Closing keeps applied values until the hunt's ordinary reset, **not a file save**.
No config, profile, trophy or resource files are rewritten by editing. ImGui layout
is session-only. Existing dump names are not recognized config keys; copying them
into config does not create supported persistence. No picking, pause/step,
docking, creature/weapon/region editing or audio/night adapters are included.

## Acceptance still needed

Please check readability, slider/exact entry, comparison/history, UI/Test/focus
transitions, held buttons and map/scope/pause combinations in the real hunt.
Hidden native GPU/widget tests are not full in-game acceptance or performance
benchmarks. No game/menu was automatically launched for this preview.

[Canonical behavior](../../CarnivoresDoc/architecture/live-modder-workbench.md)
· [verification and deployment](../../CarnivoresDoc/working/sessions/c2-workbench-overlay-2026-10-06.md)
