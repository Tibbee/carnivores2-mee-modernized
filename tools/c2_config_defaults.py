"""Append missing supported config defaults without rewriting existing settings.
Defaults come from the engine's first-run template. Conditional menu selections
remain commented examples: enabling them would override profile/live defaults.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
OPTIONAL = {"resolution", "hunt_area", "hunt_dinos", "hunt_weapons", "hunt_utils", "hunt_time"}
OBSOLETE = {"night_lighting"}


def engine_template(root: Path = ROOT) -> str:
    source = (root / "Hunt/Game/EngineInit.cpp").read_text(encoding="utf-8")
    start = source.index("int len2 = snprintf")
    end = source.index("  // A truncated template", start)
    text = "".join(json.loads(s) for s in re.findall(r'"(?:[^"\\]|\\.)*"', source[start:end]))
    constants = (root / "Hunt/Core/Constants.h").read_text(encoding="utf-8")
    for name in ("kFovDefault", "kObjectDetailDefault"):
        value = re.search(rf"\b{name}\s*=\s*(\d+)", constants)
        if not value:
            raise ValueError(f"Cannot read {name}")
        text = text.replace("%d", value[1], 1)
    state = (root / "Hunt/Core/GameState.h").read_text(encoding="utf-8")
    flags = {name: 1 << int(bit) for name, bit in re.findall(r"(GPUF_\w+)\s*=\s*1u\s*<<\s*(\d+)", state)}
    expression = re.search(r"kGpuFeaturesDefault\s*=\s*([^;]+);", state)
    if not expression:
        raise ValueError("Cannot read GPU default")
    mask = 0
    for name in re.findall(r"GPUF_\w+", expression[1]):
        mask |= flags[name]
    text = text.replace("%u", str(mask), 1)
    if "%d" in text or "%u" in text:
        raise ValueError("Unresolved template formatting")
    return text


def entries(text: str, commented: bool = False) -> dict[str, str]:
    result = {}
    for line in text.splitlines():
        stripped = line.lstrip()
        if commented:
            if not stripped.startswith("#"):
                continue
            if stripped[1:2].isspace():
                continue  # Prose such as '# resolution saved in ...' is not a key.
            stripped = stripped[1:]
        elif stripped.startswith("#"):
            continue
        parts = stripped.split()
        if len(parts) < 2 or not re.fullmatch(r"[a-zA-Z][\w]*", parts[0]):
            continue
        key = parts[0].lower()
        if commented and key not in OPTIONAL:
            continue
        if key in result and result[key] != parts[1]:
            raise ValueError(f"Conflicting default for {key}")
        result[key] = parts[1]
    return result


def check_coverage(root: Path = ROOT) -> None:
    template = engine_template(root)
    defaults = entries(template)
    examples = entries(template, commented=True)
    engine = (root / "Hunt/Game/EngineInit.cpp").read_text(encoding="utf-8")
    menu = (root / "Menu/Resources.cpp").read_text(encoding="utf-8")
    engine_reader = engine[engine.index("static void LoadConfig()\n{"):]
    menu_reader = menu[menu.index("static bool ParseConfigField("):menu.index("// Read config.cfg and override")]
    keys = set(re.findall(r'_stricmp\(key,\s*"([\w]+)"', engine_reader))
    # The menu reader matches keys through the shared configscan::KeyEquals.
    keys.update(re.findall(r'KeyEquals\(field\.key,\s*"([\w]+)"', menu_reader))
    keys.add("load_mode")  # Bootstrap parser runs before the main reader.
    missing = keys - OBSOLETE - defaults.keys() - examples.keys()
    if missing:
        raise ValueError(f"Missing supported defaults/examples: {sorted(missing)}")
    if defaults.keys() & OBSOLETE:
        raise ValueError("Obsolete selector in defaults")
    if defaults.keys() & OPTIONAL:
        raise ValueError("Conditional selection must not be forced by a default")
    # The compiled preset table lives in c2audio::Defaults(); the runtime copy
    # and the workbench registry are both derived from it.
    audio = (root / "Hunt/Audio/AudioInspection.h").read_text(encoding="utf-8")
    table = audio[audio.index("Environments Defaults()"):audio.index("EnvironmentLabel")]
    rows = re.findall(r"\{\s*(-?[\d.]+)f?,\s*(-?[\d.]+)f?,\s*(-?[\d.]+)f?,\s*(-?\d+)\s*\}", table)
    if len(rows) != 9:
        raise ValueError("Cannot read the nine reverb presets")
    for env, row in enumerate(rows):
        if env == 7:  # This environment intentionally does nothing; zero decay is not a valid override.
            continue
        for field, value in zip(("decay", "decayhf", "diffusion", "reverb"), row):
            key = f"env{env}_{field}"
            if key not in defaults or float(defaults[key]) != float(value):
                raise ValueError(f"Missing/wrong compiled audio default: {key}")
    capacity = int(re.search(r"char buf\[(\d+)\];\s*int len2 = snprintf", engine)[1])
    if len(template.encode("utf-8")) + 1 > capacity:
        raise ValueError("Engine template exceeds its write buffer")


def complete_config(data: bytes, template: str) -> tuple[bytes, list[str]]:
    if len(data) > 1024 * 1024 or data.startswith((b"\xff\xfe", b"\xfe\xff")) or b"\0" in data:
        raise ValueError("Refusing oversized, UTF-16 or NUL-containing config; resave it first")
    # Latin-1 is a byte-preserving inspection view, not a re-encoding of the input.
    inspected = data.removeprefix(b"\xef\xbb\xbf").decode("latin-1")
    present = set()
    commented = set()
    for line in inspected.splitlines():
        stripped = line.lstrip()
        comment = stripped.startswith("#")
        if comment:
            if stripped[1:2].isspace():
                continue
            stripped = stripped[1:]
        match = re.match(r"([a-zA-Z][\w]*)(?:\s|=|$)", stripped)
        if match:
            (commented if comment else present).add(match[1].lower())
    defaults = entries(template)
    optional = {k: v for k, v in entries(template, True).items() if k in OPTIONAL}
    missing = [(k, v) for k, v in defaults.items() if k not in present]
    examples = [(k, v) for k, v in optional.items() if k not in present and k not in commented]
    if not missing and not examples:
        return data, []
    newline = b"\r\n" if b"\r\n" in data else b"\n"
    lines = ["", "# Missing supported settings appended with compiled defaults."]
    lines.extend(f"{k} {v}" for k, v in missing)
    if examples:
        lines += ["", "# Optional menu/profile selections: examples only; uncomment to override."]
        lines.extend(f"#{k} {v}" for k, v in examples)
    suffix = newline.join(s.encode("ascii") for s in lines) + newline
    if data and not data.endswith((b"\n", b"\r")):
        suffix = newline + suffix
    result = data + suffix
    if len(result) > 1024 * 1024:
        raise ValueError("Completed config would exceed engine size limit")
    return result, [k for k, _ in missing] + ["#" + k for k, _ in examples]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("config", nargs="?", type=Path)
    parser.add_argument("--apply", action="store_true", help="Append missing defaults; otherwise dry-run")
    parser.add_argument("--check", action="store_true", help="Verify parser/template/audio coverage")
    args = parser.parse_args()
    check_coverage()
    if args.check:
        print("PASS: supported readers, reverb presets and template capacity agree")
    if args.config:
        exists = args.config.exists()
        original = args.config.read_bytes() if exists else b""
        if exists:
            completed, added = complete_config(original, engine_template())
        else:
            completed = engine_template().encode("utf-8")
            added = list(entries(engine_template())) + ["#" + key for key in sorted(OPTIONAL)]
        if args.apply and completed != original:
            if exists:
                # Keep successive backups; never overwrite an earlier owner snapshot.
                backup = args.config.with_name(args.config.name + ".before-defaults.bak")
                number = 0
                while backup.exists():
                    number += 1
                    backup = args.config.with_name(args.config.name + f".before-defaults.{number}.bak")
                with backup.open("xb") as file:
                    file.write(original)
                if args.config.read_bytes() != original:
                    raise ValueError("Config changed during inspection; refusing update")
                args.config.write_bytes(completed)
            else:
                with args.config.open("xb") as file:
                    file.write(completed)
        print(json.dumps({"config": str(args.config), "applied": args.apply, "added": added}, indent=2))
    elif not args.check:
        parser.error("Specify a config path or --check")


if __name__ == "__main__":
    main()
