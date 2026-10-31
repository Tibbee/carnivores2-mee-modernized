#!/usr/bin/env python3
"""Read-only animation/sound reference audit for Carnivores 2 MEE assets.

Evaluates the effective _RES.TXT definition for each area/time and
character-preview override, reads each owning .car file's header and animation
table directly, and reports references outside that model's AniCount/SfxCount.
It does not launch the game or modify any asset.

Usage:
    python tools/audit_animation_refs.py --game-dir E:/Games/CarnivoresLegacy
    python tools/audit_animation_refs.py --game-dir ... --strict

Exit codes: 0 clean, 1 errors (or warnings with --strict), 2 tool error.
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable


MAX_MODEL_COUNT = 64
MAX_ANIMATION_FRAMES = (2**31 - 1) // 256
MAX_WAV_LENGTH = 16 << 20


@dataclass
class Node:
    fields: dict[str, list[str]] = field(default_factory=dict)
    blocks: list[tuple[list[str], "Node"]] = field(default_factory=list)


def _strip_comments(line: str) -> str:
    out: list[str] = []
    quoted = False
    i = 0
    while i < len(line):
        ch = line[i]
        if ch == "'":
            quoted = not quoted
            out.append(ch)
        elif not quoted and ch == "/" and i + 1 < len(line) and line[i + 1] == "/":
            break
        else:
            out.append(ch)
        i += 1
    return "".join(out)


def _unquote(value: str) -> str:
    value = value.strip()
    if len(value) >= 2 and value[0] == "'" and value[-1] == "'":
        return value[1:-1]
    return value


def parse_script(path: Path) -> Node:
    root = Node()
    stack: list[tuple[list[str], Node]] = [([], root)]
    pending_header: list[str] | None = None
    with path.open("r", encoding="utf-8", errors="replace") as handle:
        for raw in handle:
            line = _strip_comments(raw).strip()
            if not line:
                continue
            if line == "{":
                stack.append((pending_header or [], Node()))
                pending_header = None
                continue
            if line == "}":
                if len(stack) > 1:
                    header, node = stack.pop()
                    stack[-1][1].blocks.append((header, node))
                pending_header = None
                continue
            if line.endswith("{"):
                header_text = line[:-1].strip()
                stack.append((header_text.split() if header_text else [], Node()))
                pending_header = None
                continue
            if "=" in line:
                key, value = line.split("=", 1)
                key = key.strip().lower()
                if key:
                    stack[-1][1].fields.setdefault(key, []).append(_unquote(value))
                pending_header = None
                continue
            pending_header = line.split()
    return root


def _merge_override(target: Node, override: Node) -> None:
    for key, values in override.fields.items():
        target.fields[key] = list(values)
    for header, child in override.blocks:
        target.blocks.append((header, child))


def effective(node: Node, tokens: set[str]) -> Node:
    out = Node(
        fields={key: list(values) for key, values in node.fields.items()},
        blocks=[],
    )
    for header, child in node.blocks:
        resolved = effective(child, tokens)
        if header and header[0].lower() == "overwrite":
            if any(token.lower() in tokens for token in header[1:]):
                _merge_override(out, resolved)
        else:
            out.blocks.append((header, resolved))
    return out


def first(node: Node, key: str) -> str | None:
    values = node.fields.get(key.lower())
    return values[0] if values else None


def all_values(node: Node, key: str) -> list[str]:
    return list(node.fields.get(key.lower(), []))


def blocks(node: Node, name: str) -> Iterable[Node]:
    name = name.lower()
    for header, child in node.blocks:
        if header and header[0].lower() == name:
            yield child


def parse_int(value: str | None) -> int | None:
    if value is None:
        return None
    try:
        return int(value, 10)
    except ValueError:
        return None


@dataclass
class AnimationInfo:
    name: str
    kps: int
    frames: int
    blank: bool
    anitime: int


@dataclass
class CarInfo:
    path: Path
    model_name: str = ""
    ani_count: int = 0
    sfx_count: int = 0
    animations: list[AnimationInfo] = field(default_factory=list)
    anifx: list[int] | None = None
    error: str | None = None


def _read_exact(handle, count: int) -> bytes:
    data = handle.read(count)
    if len(data) != count:
        raise EOFError(f"truncated read: wanted {count}, got {len(data)}")
    return data


def _decode_name(data: bytes) -> str:
    return data.split(b"\0", 1)[0].decode("latin-1", errors="replace")


def read_car(path: Path) -> CarInfo:
    info = CarInfo(path=path)
    try:
        with path.open("rb") as handle:
            header = _read_exact(handle, 40)
            info.model_name = _decode_name(header[:32])
            info.ani_count = int.from_bytes(header[32:36], "little", signed=True)
            info.sfx_count = int.from_bytes(header[36:40], "little", signed=True)
            if not 0 <= info.ani_count <= MAX_MODEL_COUNT:
                info.error = f"AniCount={info.ani_count} outside 0..{MAX_MODEL_COUNT}"
                return info
            if not 0 <= info.sfx_count <= MAX_MODEL_COUNT:
                info.error = f"SfxCount={info.sfx_count} outside 0..{MAX_MODEL_COUNT}"
                return info

            model_header = _read_exact(handle, 12)
            vcount = int.from_bytes(model_header[0:4], "little", signed=True)
            fcount = int.from_bytes(model_header[4:8], "little", signed=True)
            texture_size = int.from_bytes(model_header[8:12], "little", signed=True)
            if vcount <= 0 or vcount > (1 << 20):
                info.error = f"VCount={vcount} outside 1..{1 << 20}"
                return info
            if fcount < 0 or fcount > (1 << 20):
                info.error = f"FCount={fcount} outside 0..{1 << 20}"
                return info
            if texture_size < 0:
                info.error = f"negative TextureSize={texture_size}"
                return info
            handle.seek(fcount * 64 + vcount * 16 + texture_size, 1)

            for _ in range(info.ani_count):
                anim_header = _read_exact(handle, 40)
                name = _decode_name(anim_header[:32])
                kps = int.from_bytes(anim_header[32:36], "little", signed=True)
                frames = int.from_bytes(anim_header[36:40], "little", signed=True)
                blank = name.startswith("BLANK") and (len(name) == 5 or name[5] == " ")
                if frames < 0 or frames > MAX_ANIMATION_FRAMES:
                    info.error = f"animation '{name}' FramesCount={frames} outside range"
                    return info
                if frames == 0:
                    if not blank:
                        info.error = f"animation '{name}' has 0 frames but is not BLANK"
                        return info
                    info.animations.append(AnimationInfo(name, kps, 0, True, 0))
                    continue
                if kps <= 0:
                    info.error = f"animation '{name}' has aniKPS={kps}"
                    return info
                anitime = (frames * 1000) // kps
                if anitime <= 0 or anitime > 2**31 - 1:
                    info.error = f"animation '{name}' has invalid duration {anitime}ms"
                    return info
                info.animations.append(AnimationInfo(name, kps, frames, False, anitime))
                handle.seek(vcount * frames * 6, 1)

            for index in range(info.sfx_count):
                sound_header = _read_exact(handle, 36)
                length = int.from_bytes(sound_header[32:36], "little", signed=True)
                if length < 0 or length > MAX_WAV_LENGTH:
                    info.error = f"sound[{index}] length={length} outside range"
                    return info
                handle.seek(length, 1)

            anifx_data = handle.read(256)
            if len(anifx_data) == 256:
                info.anifx = [
                    int.from_bytes(anifx_data[i * 4 : i * 4 + 4], "little", signed=True)
                    for i in range(64)
                ]
            return info
    except (OSError, EOFError) as exc:
        info.error = str(exc)
        return info


@dataclass
class Finding:
    category: str
    message: str
    contexts: set[str] = field(default_factory=set)


class Report:
    def __init__(self) -> None:
        self.errors: dict[tuple[str, str], Finding] = {}
        self.warnings: dict[tuple[str, str], Finding] = {}
        self.characters = 0
        self.weapons = 0
        self.contexts = 0
        self.models: dict[Path, CarInfo] = {}
        self.references = 0
        self.hunter_refs: list[tuple[str, str, str, str]] = []

    def _add(self, table, category: str, message: str, context: str) -> None:
        key = (category, message)
        finding = table.get(key)
        if finding is None:
            finding = Finding(category, message)
            table[key] = finding
        finding.contexts.add(context)

    def error(self, category: str, message: str, context: str) -> None:
        self._add(self.errors, category, message, context)

    def warning(self, category: str, message: str, context: str) -> None:
        self._add(self.warnings, category, message, context)

    def model(self, path: Path) -> CarInfo:
        info = self.models.get(path)
        if info is None:
            info = read_car(path)
            self.models[path] = info
            if info.error:
                self.error("car error", f"{path}: {info.error}", "file")
            if info.anifx is not None:
                bad = [index for index, fx in enumerate(info.anifx)
                       if fx != -1 and not 0 <= fx < info.sfx_count]
                if bad:
                    first, last = bad[0], bad[-1]
                    self.warning(
                        "sound reference",
                        f"{path.name}: {len(bad)} Anifx entries outside SfxCount={info.sfx_count} "
                        f"(first {first}={info.anifx[first]}, last {last}={info.anifx[last]})",
                        "file",
                    )
        return info


def contexts() -> list[tuple[str, set[str]]]:
    result: list[tuple[str, set[str]]] = [("base", set())]
    for area in range(1, 9):
        token = f"area{area}"
        result.append((token, {token}))
        for time in range(3):
            result.append((f"{token}/time{time}", {token, f"time{time}"}))
    for time in range(3):
        token = f"time{time}"
        result.append((token, {token}))
    for char in range(10):
        token = f"char{char}"
        result.append((token, {token}))
    return result


def resolve_path(game_dir: Path, root: str, value: str) -> Path:
    value = value.replace("\\", "/").lstrip("/")
    if value.lower().startswith("huntdat/"):
        return game_dir / value
    return game_dir / root / value


def check_reference(
    report: Report,
    context: str,
    definition: str,
    label: str,
    value_text: str | None,
    model: CarInfo | None,
    allow_sentinel: bool = False,
) -> None:
    if value_text is None:
        return
    value = parse_int(value_text)
    if value is None:
        report.error("bad value", f"{definition} {label}='{value_text}' is not an integer", context)
        return
    report.references += 1
    if allow_sentinel and value == -1:
        return
    if model is None:
        return
    if value < 0 or value >= model.ani_count:
        report.error(
            "invalid animation",
            f"{definition} {label}={value} but {model.path.name} AniCount={model.ani_count}",
            context,
        )
        return
    if value < len(model.animations):
        animation = model.animations[value]
        if animation.blank and animation.frames == 0:
            report.warning(
                "blank clip",
                f"{definition} {label}={value} -> BLANK/zero AniTime in {model.path.name}",
                context,
            )


SPECIES_FIELDS = (
    "runanim", "walkanim", "jumpanim", "swimanim", "flyanim", "diveanim",
    "glideanim", "takeoffanim", "landanim", "slideanim", "shakelandanim",
    "shakewateranim", "climbanim", "fireanim", "waterdieanim", "lookanim",
    "smellanim", "roaranim",
)


def audit_character(
    report: Report,
    game_dir: Path,
    context: str,
    definition: Node,
) -> None:
    name = first(definition, "name") or "(unnamed)"
    file_value = first(definition, "file")
    if not file_value:
        report.error("missing field", f"{name} has no file", context)
        return
    model_path = resolve_path(game_dir, "HUNTDAT", file_value)
    if not model_path.is_file():
        report.error("missing model", f"{model_path} <- {name} ({context})", context)
        return
    model = report.model(model_path)

    for key in SPECIES_FIELDS:
        for value in all_values(definition, key):
            check_reference(report, context, name, key, value, model)
    for value in all_values(definition, "reloadanim"):
        check_reference(report, context, name, "reloadAnim", value, model, allow_sentinel=True)

    for block in blocks(definition, "killtype"):
        for key in ("eatanim", "anim"):
            for value in all_values(block, key):
                check_reference(report, context, name, f"killtype.{key}", value, model)
        for key in ("hunteranim", "hunterswimanim"):
            for value in all_values(block, key):
                report.hunter_refs.append((context, name, f"killtype.{key}", value))

    for block in blocks(definition, "deathtype"):
        for key in ("dieanim", "sleepanim", "fallanim"):
            for value in all_values(block, key):
                check_reference(report, context, name, f"deathtype.{key}", value, model)

    for block in blocks(definition, "idlegroup"):
        for value in all_values(block, "anim"):
            check_reference(report, context, name, "idlegroup.anim", value, model)

    for block in blocks(definition, "tropinfo"):
        for value in all_values(block, "tropanim"):
            check_reference(report, context, name, "tropinfo.tropAnim", value, model)


WEAPON_FIELDS = ("getanim", "shtanim", "putanim", "rldanimfull", "rldanimpart", "rldanim")


def audit_weapon(report: Report, game_dir: Path, context: str, definition: Node) -> None:
    name = first(definition, "name") or "(unnamed weapon)"
    file_value = first(definition, "file")
    if not file_value:
        return
    model_path = resolve_path(game_dir, "HUNTDAT/WEAPONS", file_value)
    if not model_path.is_file():
        report.error("missing model", f"{model_path} <- {name} ({context})", context)
        return
    model = report.model(model_path)
    for key in WEAPON_FIELDS:
        for value in all_values(definition, key):
            check_reference(report, context, name, key, value, model)


def find_hunter(definition_list: list[Node]) -> Node | None:
    for definition in definition_list:
        if (first(definition, "name") or "").lower() == "hunter":
            return definition
    return None


def check_hunter_reference(
    report: Report,
    context: str,
    definition: str,
    label: str,
    value_text: str,
    hunter_models: list[CarInfo],
) -> None:
    value = parse_int(value_text)
    if value is None:
        report.error("bad value", f"{definition} {label}='{value_text}' is not an integer", context)
        return
    report.references += 1
    if not hunter_models:
        report.error(
            "hunter variant",
            f"{definition} {label}={value}: no hunter model could be read",
            context,
        )
        return
    valid = [model for model in hunter_models if 0 <= value < model.ani_count]
    max_count = max(model.ani_count for model in hunter_models)
    if not valid:
        report.error(
            "invalid hunter animation",
            f"{definition} {label}={value} invalid for every hunter variant (max AniCount={max_count})",
            context,
        )
        return
    if len(valid) < len(hunter_models):
        valid_paths = {model.path for model in valid}
        valid_names = ", ".join(sorted({model.path.name for model in valid}))
        invalid_names = ", ".join(
            sorted({model.path.name for model in hunter_models if model.path not in valid_paths})
        )
        report.warning(
            "hunter variant",
            f"{definition} {label}={value} valid for {valid_names}; invalid for {invalid_names}",
            context,
        )


def run_audit(game_dir: Path, res_path: Path) -> Report:
    report = Report()
    parsed = parse_script(res_path)
    characters_section = None
    weapons_section = None
    for header, child in parsed.blocks:
        if not header:
            continue
        section = header[0].lower()
        if section == "characters":
            characters_section = child
        elif section == "weapons":
            weapons_section = child

    if characters_section is None:
        report.error("script", f"{res_path} has no characters section", "file")
        return report

    character_defs = [
        child for header, child in characters_section.blocks
        if (not header or header[0].lower() != "overwrite") and first(child, "name")
    ]
    weapon_defs = []
    if weapons_section is not None:
        weapon_defs = [
            child for header, child in weapons_section.blocks
            if (not header or header[0].lower() != "overwrite") and first(child, "name")
        ]

    report.characters = len(character_defs)
    report.weapons = len(weapon_defs)

    hunter_paths: set[Path] = set()
    for _, tokens in contexts():
        effective_chars = [effective(node, tokens) for node in character_defs]
        hunter_def = find_hunter(effective_chars)
        if hunter_def is not None:
            hunter_file = first(hunter_def, "file")
            if hunter_file:
                hunter_paths.add(resolve_path(game_dir, "HUNTDAT", hunter_file))

    hunter_models: list[CarInfo] = []
    for hunter_path in sorted(hunter_paths, key=lambda path: str(path).lower()):
        if hunter_path.is_file():
            hunter_models.append(report.model(hunter_path))

    for context_name, tokens in contexts():
        report.contexts += 1
        effective_chars = [effective(node, tokens) for node in character_defs]
        for definition in effective_chars:
            audit_character(report, game_dir, context_name, definition)
        for definition in weapon_defs:
            audit_weapon(report, game_dir, context_name, effective(definition, tokens))

    for context_name, name, label, value in report.hunter_refs:
        check_hunter_reference(report, context_name, name, label, value, hunter_models)

    return report


def print_report(report: Report, game_dir: Path, res_path: Path, strict: bool) -> int:
    print("C2 animation reference audit")
    print(f"  game:       {game_dir}")
    print(f"  script:     {res_path}")
    print(f"  characters: {report.characters}; weapons: {report.weapons}")
    print(
        f"  contexts:   {report.contexts}; references checked: {report.references}; "
        f"models read: {len(report.models)}"
    )

    def emit(title: str, table: dict[tuple[str, str], Finding]) -> None:
        if not table:
            return
        print(f"\n{title} ({len(table)})")
        for finding in sorted(table.values(), key=lambda item: (item.category, item.message)):
            shown = sorted(finding.contexts)
            if len(shown) > 6:
                context_text = ", ".join(shown[:6]) + f", ... (+{len(shown) - 6})"
            else:
                context_text = ", ".join(shown)
            print(f"  [{finding.category}] {finding.message}   [{context_text}]")

    emit("ERRORS", report.errors)
    emit("WARNINGS", report.warnings)

    if report.errors:
        return 1
    if strict and report.warnings:
        return 1
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-dir", default=r"E:\Games\CarnivoresLegacy",
                        help="folder containing HUNTDAT")
    parser.add_argument("--res", default=None,
                        help="path to _RES.TXT (default: <game-dir>/HUNTDAT/_RES.TXT)")
    parser.add_argument("--strict", action="store_true", help="treat warnings as failures")
    args = parser.parse_args(argv)

    game_dir = Path(args.game_dir)
    res_path = Path(args.res) if args.res else game_dir / "HUNTDAT" / "_RES.TXT"
    if not game_dir.is_dir():
        print(f"error: game directory not found: {game_dir}", file=sys.stderr)
        return 2
    if not res_path.is_file():
        print(f"error: resource script not found: {res_path}", file=sys.stderr)
        return 2

    report = run_audit(game_dir, res_path)
    return print_report(report, game_dir, res_path, args.strict)


if __name__ == "__main__":
    raise SystemExit(main())
