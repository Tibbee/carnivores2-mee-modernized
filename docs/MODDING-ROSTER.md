# Character definitions and the dynamic roster

> Included in the v1.1.9.2 Beta. The published v1.1.9/v1.1.9.1 builds still have
> the 128-definition limit; this guide describes the beta source/package.

The engine now sizes its character-definition storage to the entries read from
`HUNTDAT/_RES.TXT`, rather than using a fixed 128-element array. This is not a
replacement limit of 256 or 512; regression tests include 513 definitions.

The local unreleased source doubles the beta's 64 MiB metadata budget to
**128 MiB**. On x86 Release/Shipping this admits **3,667 definitions**
(128 MiB divided by 2 x 18,300 bytes per definition, rounded down).
The 3,668th is rejected; x86 Debug admits 3,616 because its metadata is larger.
The published beta retains its 1,833-definition budget ceiling.
This is a metadata limit, not a
promise that every definition can spawn in one hunt: the independent 1,024-entry
pack-type table, 16 MiB `_RES.TXT` input budget, 256 live-character slots, 128
AI families, 128 trophy display slots and the menu's 32-bit launch flags still
apply. A roster where every definition generates a pack can therefore hit the
pack-type table before the metadata budget.

## What counts as an entry?

Each outer definition block inside a `characters` section counts once. The
hunter, ambient animals, fish, dinosaur variants and other character-based
entities all count. Duplicate names/model paths are still separate definitions.
Nested `overwrite`, `addition`, animation and spawn blocks are part of their
parent definition, not extra roster entries. Multiple animals spawned from one
definition do not create more definitions.

Definitions retain zero-based script-order IDs. Keep the hunter first and append
variants if existing saves need to keep referring to the same animals. Inserting,
deleting or rearranging entries can change the meaning of saved trophy IDs;
dynamic storage does not convert old saves to name-based identity.

## Safety and lifetime

The roster grows while loading the script, then allocates and freezes its model
record array before placing animals. It cannot be resized or reloaded during a
hunt: live characters retain pointers into that storage. Resources are loaded
for used entries and released during normal shutdown.

Safety checks remain enabled in both loading modes. Lenient loading can recover
some invalid field values; it cannot safely ignore invalid memory references or
failed allocations. `_RES.TXT` has a 16 MiB input budget. Roster metadata has a
conservative 128 MiB estimated budget including growth allowance; model textures,
animations and sounds are additional allocations, not covered by that estimate.
The 32-bit process and independent model-format limits still apply.
The budget is a ceiling, not a startup allocation: a stock-sized roster does not
allocate 128 MiB. At the Release ceiling the records themselves total about
64 MiB before vector spare capacity and other overhead. Raising this policy
does not guarantee that a mod's assets fit in the remaining address space.
Parser/finalization tests cover the full metadata ceiling and rejection of the
next entry; asset-backed gameplay remains tested to 513 definitions, not 3,667.

## What has not been expanded?

- Live animals: the existing 256-element instance array and placement guards.
- Pack types (1024), members per pack (32), and per-definition spawn-info lists (32).
- Trophy display/save slots: 128; trophy group *membership* lists are dynamic.
- Menu selection masks, category mappings, unique call sounds/icons and AI families.
- Survival list size and wave progression. Existing slots can reference IDs above 127.

Spawn-group link lists and selection scratch storage are dynamic, so many
variants sharing one group no longer hit the old 128-link limit. This does not
make all the separate pack/population limits unlimited.

## Testing a large roster

`tools/c2_roster_fixture.py SOURCE OUTPUT --count 513` generates a new synthetic
script using the reference install's Moschops assets. It never edits the source,
retains all original IDs and randomises the copied ambient group so high IDs can
spawn in a short test. Do not distribute the generated file or treat it as a
finished mod.

`tests/roster_deployed_probe.ps1` is an explicitly launched, asset-backed hidden
OpenGL probe, not an automatic CTest. It temporarily substitutes a fixture,
enables capture logging, and backs up/restores root files, config, saves and the
resource script. It requires an instrumented OpenGL build (`GL_PERF_HOOKS=ON`),
not Shipping. Read the script and engine `AGENTS.md` before running; a timed-out
owned process must be closed manually before restoring files underneath it.

The ordinary parser-contract suite contains deterministic large-roster tests.
Its opt-in `RosterIntegration.InstalledResourceScript` test uses the production
loader from the game working directory. Select it using the `GTEST_FILTER`
environment variable together with `C2_ROSTER_PROBE=1`, not command-line filtering
that can mutate CRT arguments before the legacy command-line reader uses them.
