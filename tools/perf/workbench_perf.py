#!/usr/bin/env python3
"""Opt-in, isolated unattended renderer measurements; offline analysis by default.

Never deploys, changes originals, injects desktop input, or kills processes.
Licensed fixture copies and generated measurements belong in ignored build/.
"""
from __future__ import annotations

import argparse
import csv
import ctypes
import hashlib
import json
import math
import platform
from pathlib import Path
import re
import shutil
import statistics
import struct
import subprocess
import sys
import time

REPO = Path(__file__).resolve().parents[2]
REQUIRED = ("frame", "cpu_ms", "gpu_ms", "draw_calls", "triangles",
            "previous_frame_interval_ms", "previous_swap_ms", "PreCashGroundModel_cpu_ms")
COUNTERS = ("draw_calls", "triangles", "terrain_emitted_tiles", "terrain_vertices",
            "precache_points", "precache_water_cells", "precache_fogon")
NUMBER = re.compile(r"[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?\Z")
SHA = re.compile(r"[a-f0-9]{64}\Z")


def sha256(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest() if hasattr(hashlib, "file_digest") else _hash_stream(stream)


def _hash_stream(stream) -> str:
    digest = hashlib.sha256()
    for block in iter(lambda: stream.read(1024 * 1024), b""):
        digest.update(block)
    return digest.hexdigest()


def save_json(path: Path, data) -> None:
    path.write_text(json.dumps(data, indent=2, allow_nan=False) + "\n", encoding="utf-8")


def stats(values) -> dict:
    ordered = sorted(values)
    if not ordered:
        return {"n": 0, "unavailable": True}
    n = len(ordered)
    return {"n": n, "mean": statistics.fmean(ordered), "median": statistics.median(ordered),
            "p95": ordered[math.ceil(.95 * n) - 1], "p99": ordered[math.ceil(.99 * n) - 1],
            "min": ordered[0], "max": ordered[-1]}


def read_capture(path: Path, expected_frames: int = 120) -> dict:
    with path.open(newline="", encoding="utf-8-sig") as stream:
        reader = csv.reader(stream)
        columns = next(reader, [])
        if len(set(columns)) != len(columns) or not all(re.fullmatch(r"[A-Za-z_]\w*", c, re.ASCII) for c in columns):
            raise ValueError(f"{path}: invalid/duplicate columns")
        missing = set(REQUIRED) - set(columns)
        if missing:
            raise ValueError(f"{path}: missing columns {sorted(missing)}")
        data = {c: [] for c in columns}
        rows = 0
        for fields in reader:
            if len(fields) != len(columns):
                raise ValueError(f"{path}: wrong field count at frame {rows}")
            for column, raw in zip(columns, fields):
                if not NUMBER.fullmatch(raw) or not math.isfinite(value := float(raw)):
                    raise ValueError(f"{path}: nonnumeric/nonfinite {column} at frame {rows}")
                if column == "frame" and value != rows:
                    raise ValueError(f"{path}: nonsequential frame {rows}")
                if column == "gpu_ms" or column.endswith("_gpu_ms"):
                    if value != -1:
                        raise ValueError(f"{path}: GPU sentinel contract changed")
                    continue
                if column in ("previous_frame_interval_ms", "previous_swap_ms") and value == -1:
                    continue
                if value < 0:
                    raise ValueError(f"{path}: negative {column}")
                if (column in COUNTERS or column.endswith("_calls")) and not value.is_integer():
                    raise ValueError(f"{path}: fractional counter {column}")
                data[column].append(value)
            rows += 1
    if rows != expected_frames:
        raise ValueError(f"{path}: expected {expected_frames} frames, got {rows}")
    return {column: stats(values) for column, values in data.items() if column != "frame"}


def inside(path: Path, parent: Path) -> bool:
    return path == parent or parent in path.parents


def local_file(root: Path, relative: str) -> Path:
    path = (root / relative).resolve()
    if not inside(path, root.resolve()) or not path.is_file():
        raise ValueError(f"Missing or escaping evidence path: {relative}")
    return path


def analyze(session: Path) -> dict:
    root = session.parent.resolve()
    manifest = json.loads(session.read_text(encoding="utf-8"))
    if manifest.get("schema_version") != 1 or manifest.get("kind") != "unattended-workbench-renderer":
        raise ValueError("Unsupported session schema/kind")
    settings = manifest["settings"]
    if (settings["resolution"], settings["view_option"], settings["radius"], settings["frames"]) != ("2560x1440", 255, 230, 120):
        raise ValueError("Session is not the agreed max-view/resolution protocol")
    if not isinstance(settings["warmup_ms"], int) or not 1000 <= settings["warmup_ms"] <= 30000:
        raise ValueError("Insufficient/invalid warm-up")
    provenance = manifest["provenance"]
    for key in ("exe_sha256", "config_sha256", "profile_sha256"):
        if not SHA.fullmatch(provenance[key]):
            raise ValueError(f"Missing/invalid provenance {key}")
    if not provenance.get("assets") or not all(SHA.fullmatch(h) for h in provenance["assets"].values()):
        raise ValueError("Missing/invalid asset provenance")
    if not manifest.get("originals_unchanged") or manifest.get("status") != "complete":
        raise ValueError("Session incomplete or original-file preservation unverified")
    captures = manifest["captures"]
    expected = manifest["repeats"] * len(manifest["targets"]) * 3
    if not isinstance(manifest["repeats"], int) or not 3 <= manifest["repeats"] <= 5 or len(captures) != expected:
        raise ValueError("Need 3-5 complete repeated triples")
    targets = manifest["targets"]
    if not targets or len(set(targets)) != len(targets) or any(t not in ("ui", "test") for t in targets):
        raise ValueError("Invalid capture targets")
    groups = {target: [] for target in targets}
    evidence_paths = set()
    for offset in range(0, len(captures), 3):
        triple = captures[offset:offset + 3]
        target = targets[(offset // 3) % len(targets)]
        repeat = offset // (3 * len(targets)) + 1
        parsed = []
        for entry, phase, state in zip(triple, ("before", "target", "after"), ("closed", target, "closed")):
            if (entry["target"], entry["repeat"], entry["phase"], entry["state"]) != (target, repeat, phase, state):
                raise ValueError("Nonalternating or mislabeled capture order")
            path = local_file(root, entry["csv"])
            log = local_file(root, entry["log"])
            if path in evidence_paths or sha256(path) != entry["csv_sha256"] or sha256(log) != entry["log_sha256"]:
                raise ValueError("Duplicate or changed capture evidence")
            evidence_paths.add(path)
            log_text = log.read_text(encoding="utf-8", errors="replace")
            if f"Unattended workbench mode: {state}\n" not in log_text or "Unattended capture complete; framebuffer saved." not in log_text:
                raise ValueError("Runtime mode/completion not confirmed by log")
            parsed.append(read_capture(path))
        before, measured, after = parsed
        metrics = {}
        for key in ("cpu_ms", "previous_frame_interval_ms", "previous_swap_ms", "PreCashGroundModel_cpu_ms"):
            if min(part[key]["n"] for part in parsed) < 119:
                raise ValueError(f"Insufficient valid samples for {key}")
            baseline = (before[key]["median"] + after[key]["median"]) / 2
            metrics[key] = {"baseline_median": baseline, "target_median": measured[key]["median"],
                            "delta_ms": measured[key]["median"] - baseline,
                            "closed_drift_ms": after[key]["median"] - before[key]["median"]}
        workload = {key: [part[key] for part in parsed] for key in COUNTERS if all(key in part for part in parsed)}
        groups[target].append({"repeat": repeat, "metrics": metrics, "workload": workload,
                               "captures": parsed})
    aggregate = {}
    for target, triples in groups.items():
        aggregate[target] = {key: {"delta_ms": stats(t["metrics"][key]["delta_ms"] for t in triples),
                                  "closed_drift_ms": stats(t["metrics"][key]["closed_drift_ms"] for t in triples)}
                             for key in triples[0]["metrics"]}
    return {"schema_version": 1, "kind": manifest["kind"], "settings": settings,
            "provenance": provenance, "aggregate": aggregate, "triples": groups,
            "precache_reference_ms": 1.9,
            "limitations": ["Unattended renderer only: hidden, no audio; not foreground gameplay FPS or input latency.",
                            "UI means the default full inspector; Test means its separate status strip. No tuning edits.",
                            "CSV GPU fields are unavailable (-1), not GPU timing.",
                            "Previous interval/swap are lagged throughput/presentation values, not display latency.",
                            "Actors/wind/time remain live; inspect closed drift and workload before attribution.",
                            "Engine counters omit ImGui draw work. No automatic performance PASS threshold."]}


def set_config(source: bytes) -> bytes:
    # Latin-1 roundtrip preserves arbitrary existing bytes/comments in the COPY.
    text = source.decode("latin-1")
    for key, value in {"glperf_logging": "1", "fps_limit": "0", "resolution": "2560x1440",
                       "display_mode": "0"}.items():
        pattern = re.compile(rf"(?mi)^(\s*{key}[ \t]+)[^\s#;]+")
        text, count = pattern.subn(lambda m: m[1] + value, text)
        if not count:
            text += ("" if text.endswith("\n") else "\r\n") + f"{key} {value}\r\n"
    return text.encode("latin-1")


def profile_copy(source: bytes) -> bytes:
    # Verified C2 TTrophyRoom + seven option ints; mutate ONLY isolated copy.
    if len(source) != 1660:
        raise ValueError("Expected current C2 trophy00.sav layout (1660 bytes)")
    result = bytearray(source)
    struct.pack_into("<i", result, 1540, 255)
    return bytes(result)


def inventory(root: Path) -> dict:
    return {p.relative_to(root).as_posix(): sha256(p) for p in sorted(root.rglob("*")) if p.is_file()}


def runtime_assets(runtime: Path) -> dict:
    assets = inventory(runtime / "HUNTDAT")
    for folder in ("shaders", "MULTIPLAYER"):
        assets.update({folder + "/" + key: value for key, value in inventory(runtime / folder).items()})
    return assets


def assert_no_game() -> None:
    if sys.platform != "win32":
        raise ValueError("Live capture requires Windows")
    names = subprocess.check_output(["tasklist", "/FO", "CSV", "/NH"], text=True, errors="replace")
    prohibited = {"v_gl.exe", "v_gl.ren", "v_soft.ren", "carnivores1_gl.exe", "carnivores1_soft.exe",
                  "carnivores2menu.exe", "startlegacy.exe"}
    if any(row and row[0].lower() in prohibited for row in csv.reader(names.splitlines())):
        raise ValueError("Game/menu already running; close it yourself before preparing/running")


def system_info() -> dict:
    command = "[pscustomobject]@{cpu=(Get-CimInstance Win32_Processor | Select-Object -First 1 -ExpandProperty Name); gpu=@(Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion); os=(Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version,BuildNumber)} | ConvertTo-Json -Depth 4"
    result = json.loads(subprocess.check_output(["powershell", "-NoProfile", "-Command", command], text=True, encoding="utf-8", errors="replace"))
    result.update({"platform": platform.platform(), "python": sys.version})
    return result


def prepare(game: Path, exe: Path, output: Path, targets: list[str], repeats: int, warmup: int) -> Path:
    game, exe, output = game.resolve(), exe.resolve(), output.resolve()
    if inside(output, game) or inside(game, output) or inside(exe, output) or output.exists():
        raise ValueError("Output must be new and separate from original game/build executable")
    if not inside(output, REPO / "build"):
        raise ValueError("Use an ignored session directory under this repository's build/")
    assert_no_game()
    cache = (REPO / "build/ogl-release/CMakeCache.txt").read_text(encoding="utf-8")
    if not re.search(r"^GL_PERF_HOOKS:BOOL=ON\s*$", cache, re.M):
        raise ValueError("ogl-release must have GL_PERF_HOOKS=ON")
    if exe != (REPO / "build/ogl-release/bin/Carnivores1_GL.exe").resolve():
        raise ValueError("Use the current ogl-release binary, not Shipping or an arbitrary executable")
    pe = exe.read_bytes()
    offset = struct.unpack_from("<I", pe, 0x3c)[0]
    if pe[offset:offset + 4] != b"PE\0\0" or struct.unpack_from("<H", pe, offset + 4)[0] != 0x14c:
        raise ValueError("Expected x86 PE")
    # Freshness is necessary but not a reproducibility claim; runtime logs must
    # additionally confirm the mode to reject an older executable.
    build_inputs = [REPO / "CMakeLists.txt", REPO / "CMakePresets.json"]
    for folder in ("Hunt", "deps"):
        build_inputs.extend(p for p in (REPO / folder).rglob("*") if p.suffix in (".h", ".hpp", ".c", ".cpp"))
    if any(p.stat().st_mtime_ns > exe.stat().st_mtime_ns for p in build_inputs):
        raise ValueError("Executable predates source/build inputs; rebuild ogl-release first")
    # Require a clean source anchor; commit separately before collecting evidence.
    dirty = subprocess.check_output(["git", "-C", str(REPO), "status", "--porcelain", "--untracked-files=all",
                                     "--", "Hunt", "shaders", "deps", "CMakeLists.txt", "CMakePresets.json"], text=True)
    if dirty.strip():
        raise ValueError("Engine/build inputs are dirty; checkpoint them before collecting evidence")
    header = (REPO / "Hunt/Core/UnattendedCapture.h").read_text(encoding="utf-8")
    if "-capture-workbench=" not in header:
        raise ValueError("Source lacks workbench capture support")
    baseline = inventory(game)
    config = set_config((game / "config.cfg").read_bytes())
    profile = profile_copy((game / "trophy00.sav").read_bytes())
    # Full isolated asset copy; no writable junctions/hardlinks to originals.
    runtime = output / "runtime"
    runtime.mkdir(parents=True)
    shutil.copytree(game / "HUNTDAT", runtime / "HUNTDAT")
    # CharacterLoader loads the shipped hitbox/avatar CAR even in offline hunts.
    if (game / "MULTIPLAYER").is_dir():
        shutil.copytree(game / "MULTIPLAYER", runtime / "MULTIPLAYER")
    shutil.copytree(exe.parent / "shaders", runtime / "shaders")
    shutil.copy2(exe, runtime / "Carnivores1_GL.exe")
    inputs = output / "inputs"
    inputs.mkdir()
    (inputs / "config.cfg").write_bytes(config)
    (inputs / "trophy00.sav").write_bytes(profile)
    if (game / "trophy00.sab").exists():
        shutil.copy2(game / "trophy00.sab", inputs / "trophy00.sab")
    asset_hashes = runtime_assets(runtime)
    save_json(output / "originals.json", baseline)
    manifest = {"schema_version": 1, "kind": "unattended-workbench-renderer", "status": "prepared",
                "original_game": str(game), "originals_unchanged": False, "targets": targets, "repeats": repeats,
                "settings": {"resolution": "2560x1440", "view_option": 255, "radius": 230, "frames": 120,
                             "warmup_ms": warmup, "scenario": "area4_spawn", "audio": False, "hidden": True},
                "provenance": {"exe_sha256": sha256(runtime / "Carnivores1_GL.exe"),
                               "config_sha256": sha256(inputs / "config.cfg"), "profile_sha256": sha256(inputs / "trophy00.sav"),
                               "source_head": subprocess.check_output(["git", "-C", str(REPO), "rev-parse", "HEAD"], text=True).strip(),
                               "assets": asset_hashes, "system": system_info(),
                               "cmake_cache_sha256": sha256(REPO / "build/ogl-release/CMakeCache.txt")}, "captures": []}
    save_json(output / "session.json", manifest)
    return output / "session.json"


def hidden_windows(pid: int) -> None:
    user = ctypes.WinDLL("user32", use_last_error=True)
    callback_type = ctypes.WINFUNCTYPE(ctypes.c_int, ctypes.c_void_p, ctypes.c_void_p)
    user.GetForegroundWindow.restype = ctypes.c_void_p
    user.GetWindowThreadProcessId.argtypes = (ctypes.c_void_p, ctypes.POINTER(ctypes.c_ulong))
    user.IsWindowVisible.argtypes = (ctypes.c_void_p,)
    found_visible = []
    @callback_type
    def visit(hwnd, _):
        owner = ctypes.c_ulong()
        user.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid and (user.IsWindowVisible(hwnd) or user.GetForegroundWindow() == hwnd):
            found_visible.append(hwnd)
        return 1
    user.EnumWindows(visit, 0)
    if found_visible:
        raise ValueError("Unattended process became visible/took foreground; no automatic termination")


def validate_bitmap(path: Path) -> None:
    with path.open("rb") as stream:
        header = stream.read(54)
    if (len(header) != 54 or header[:2] != b"BM" or struct.unpack_from("<ii", header, 18) != (2560, 1440)
            or path.stat().st_size != 54 + 2560 * 1440 * 4):
        raise ValueError("Missing/invalid final 2560x1440 framebuffer evidence")


def run(session: Path) -> None:
    assert_no_game()
    session = session.resolve()
    root, manifest = session.parent, json.loads(session.read_text(encoding="utf-8"))
    if (manifest.get("schema_version") != 1 or manifest.get("kind") != "unattended-workbench-renderer"
            or manifest.get("status") != "prepared"):
        raise ValueError("Only a newly prepared session can run (no overwrite/resume)")
    original_game = Path(manifest["original_game"]).resolve()
    if not inside(root, REPO / "build") or inside(root, original_game) or inside(original_game, root):
        raise ValueError("Session must stay in ignored build/, separate from the original game")
    for folder in ("runtime", "inputs"):
        if (root / folder).resolve() != root / folder:
            raise ValueError("Prepared runtime/inputs must not be writable directory links")
    runtime, inputs = root / "runtime", root / "inputs"
    exe = runtime / "Carnivores1_GL.exe"
    provenance = manifest["provenance"]
    if sha256(exe) != provenance["exe_sha256"] or sha256(inputs / "config.cfg") != provenance["config_sha256"] or sha256(inputs / "trophy00.sav") != provenance["profile_sha256"]:
        raise ValueError("Prepared executable/settings changed")
    if runtime_assets(runtime) != provenance["assets"]:
        raise ValueError("Prepared assets changed")
    process = None
    manifest["status"] = "running"
    save_json(session, manifest)
    try:
        for repeat in range(1, manifest["repeats"] + 1):
            for target in manifest["targets"]:
                for phase, state in zip(("before", "target", "after"), ("closed", target, "closed")):
                    assert_no_game()
                    for name in ("config.cfg", "trophy00.sav", "trophy00.sab"):
                        if (inputs / name).exists():
                            shutil.copy2(inputs / name, runtime / name)
                        elif (runtime / name).exists():
                            (runtime / name).unlink()
                    run_dir = root / f"r{repeat}-{target}-{phase}"
                    run_dir.mkdir()
                    bitmap = run_dir / "frame.bmp"
                    before = {p.name: sha256(p) for p in runtime.glob("glperf-capture-*.csv")}
                    args = [str(exe), r"prj=huntdat\areas\area4", "reg=0", "din=0", "wep=0", "dtm=0", "-observ", "-camfixed",
                            "x=528.5", "y=768.5", "-windowed", "-res=2560x1440", "-capture-background",
                            f"-capture-warmup-ms={manifest['settings']['warmup_ms']}", f"-capture-workbench={state}",
                            f"-capture-output={bitmap}"]
                    print(f"r{repeat} {target}/{phase}: {state}", flush=True)
                    process = subprocess.Popen(args, cwd=runtime)
                    save_json(run_dir / "process.json", {"pid": process.pid, "args": args})
                    deadline = time.monotonic() + manifest["settings"]["warmup_ms"] / 1000 + 90
                    while process.poll() is None:
                        hidden_windows(process.pid)
                        if time.monotonic() > deadline:
                            raise ValueError(f"Capture timeout; owned PID {process.pid} NOT killed; close it yourself")
                        time.sleep(.1)
                    # Retain diagnostics even when legacy DoHalt exits with code zero.
                    for name in ("render.log", "carnivor.log"):
                        if (runtime / name).exists():
                            shutil.copy2(runtime / name, run_dir / name)
                    log_text = (run_dir / "render.log").read_text(encoding="utf-8", errors="replace")
                    if "ABNORMAL_HALT:" in log_text:
                        raise ValueError("Capture halted: " + log_text.split("ABNORMAL_HALT:", 1)[1].strip())
                    if process.returncode:
                        raise ValueError(f"Capture process exited {process.returncode}")
                    changed = [p for p in runtime.glob("glperf-capture-*.csv") if before.get(p.name) != sha256(p)]
                    if len(changed) != 1:
                        raise ValueError("Expected exactly one new complete capture")
                    read_capture(changed[0])
                    validate_bitmap(bitmap)
                    csv_path, log_path = run_dir / "capture.csv", run_dir / "render.log"
                    shutil.copy2(changed[0], csv_path)
                    shutil.copy2(runtime / "render.log", log_path)
                    text = log_path.read_text(encoding="utf-8", errors="replace")
                    if f"Unattended workbench mode: {state}\n" not in text or "Unattended capture complete; framebuffer saved." not in text:
                        raise ValueError("Capture mode/completion not confirmed; stale build or failed UI")
                    if (runtime / "carnivor.log").exists():
                        shutil.copy2(runtime / "carnivor.log", run_dir / "carnivor.log")
                    manifest["captures"].append({"repeat": repeat, "target": target, "phase": phase, "state": state,
                                                 "csv": csv_path.relative_to(root).as_posix(), "csv_sha256": sha256(csv_path),
                                                 "log": log_path.relative_to(root).as_posix(), "log_sha256": sha256(log_path)})
                    save_json(session, manifest)
        manifest["status"] = "complete"
    except Exception as error:
        manifest["status"] = "failed"
        manifest["failure"] = str(error)
        if process and process.poll() is None:
            manifest["still_running_pid"] = process.pid
        raise
    finally:
        manifest["originals_unchanged"] = inventory(Path(manifest["original_game"])) == json.loads((root / "originals.json").read_text(encoding="utf-8"))
        save_json(session, manifest)
    save_json(root / "report.json", analyze(session))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    prep = commands.add_parser("prepare", help="Copy private fixtures into a new ignored session; DOES NOT launch")
    prep.add_argument("--game-dir", type=Path, required=True)
    prep.add_argument("--output", type=Path, required=True)
    prep.add_argument("--exe", type=Path, default=REPO / "build/ogl-release/bin/Carnivores1_GL.exe")
    prep.add_argument("--include-test", action="store_true")
    prep.add_argument("--repeats", type=int, choices=range(3, 6), default=3)
    prep.add_argument("--warmup-ms", type=int, default=3000)
    execute = commands.add_parser("run", help="EXPLICIT OPT-IN: launches hidden captures, never kills/deploys")
    execute.add_argument("session", type=Path)
    report = commands.add_parser("analyze", help="Offline, strict evidence validation and report")
    report.add_argument("session", type=Path)
    args = parser.parse_args()
    try:
        if args.command == "prepare":
            if not 1000 <= args.warmup_ms <= 30000:
                raise ValueError("Warm-up must be 1000-30000 ms")
            print(prepare(args.game_dir, args.exe, args.output, ["ui", "test"] if args.include_test else ["ui"], args.repeats, args.warmup_ms))
        elif args.command == "run":
            run(args.session)
        else:
            print(json.dumps(analyze(args.session), indent=2, allow_nan=False))
    except (ValueError, OSError, KeyError, TypeError, OverflowError, struct.error, json.JSONDecodeError, subprocess.CalledProcessError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
