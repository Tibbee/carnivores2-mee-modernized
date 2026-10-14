# Config default completion

`c2_config_defaults.py` makes supported config controls discoverable without
resetting owner settings. Requires Python 3.10 or later. The VS Code deployment
script calls it automatically after building and before copying binaries, and
the release packaging script uses it to emit the packaged `config.cfg`.
It refuses deployment while the game/menu is running; it never stops them.

```powershell
# Inspect only; also checks source/template coverage.
python tools/c2_config_defaults.py E:/Games/CarnivoresLegacy/config.cfg

# Append missing defaults and create a non-overwriting backup.
python tools/c2_config_defaults.py E:/Games/CarnivoresLegacy/config.cfg --apply

# Source parity/coverage gate only.
python tools/c2_config_defaults.py --check
```

Defaults are extracted from `EngineInit.cpp::CreateDefaultConfig`, using the
compiled FOV/object-detail constants and GPU feature mask. Checks cover the
current engine and menu config readers, all effective reverb presets and the
first-run buffer capacity. A future key must be added to the template before
the source gate passes. The check intentionally scopes the menu config parser,
not its unrelated resource-script parsers.

## Preservation rules

- Existing content is kept byte-for-byte; new lines append at the end.
- Present keys are never replaced, including custom values, unknown keys,
  duplicates and invalid values. This is not a sanitizer or repair tool.
- Comments, BOM, existing line endings and non-ASCII bytes are preserved.
- Repeated completion is idempotent.
- A missing file is created using the full engine template.
- Before modifying an existing file, save `config.cfg.before-defaults.bak`.
  If it exists, use `.before-defaults.1.bak`, `.2.bak`, etc.; never overwrite it.
- Oversized, UTF-16 and NUL-containing files are rejected without modification.
- Conditional `resolution` and `hunt_*` entries remain commented examples when
  missing: enabling them would override profile/current-menu selections.
- Reverb `env0..6` and `env8` expose decay, decayhf, diffusion and reverb with
  their exact compiled defaults. Environment 7 is a deliberate no-op, so no
  misleading effective controls are added for it.
- Obsolete `night_lighting` is not added. Existing obsolete keys are preserved.
- `night_moon_glow_strength` is supported and is added at default 100 if missing,
  without changing an existing custom glow value.
- F10 runtime tuning such as `SunGlare_Disc` is not a config reader key; this
  tool does not turn debug variables into new config options.

This deployment convenience does not automatically migrate a config on ordinary
game/menu startup. The engine first-run template and release package include
the complete defaults. The menu's independent first-run writer can still create
its own smaller set; running the deployment/completion tool fills missing keys.

## Tests

```powershell
python tests/test_config_defaults.py -v
ctest --test-dir build/ogl-release-shipping --output-on-failure
```

`Carnivores2Tests.ConfigDefaults` is registered with CTest. Tests cover
production reader/audio parity, source-key negative control, custom
values, duplicate keys, byte preservation, conditional examples, idempotence,
unsupported encodings, first-run creation and backup safety. The build/test
configuration now requires the Python interpreter. Full package assembly is
not run by these tests; only template values and PowerShell syntax are checked.
