"""Asset-free tests: strict real Perl CLI and workbench report/fixture contracts.
No game launch, OS input, fixtures from the deployment, or network access.
"""
import copy
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import struct

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("workbench_perf", ROOT / "tools/perf/workbench_perf.py")
perf = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(perf)
PERL = shutil.which("perl")
HEADER = list(perf.REQUIRED) + ["RenderGround_cpu_ms", "RenderGround_gpu_ms", "terrain_emitted_tiles", "precache_points"]


def capture(cpu=10, frames=120):
    values = {"frame": 0, "cpu_ms": cpu, "gpu_ms": -1, "draw_calls": 100,
              "triangles": 2000, "previous_frame_interval_ms": 12, "previous_swap_ms": 1,
              "PreCashGroundModel_cpu_ms": 1.9, "RenderGround_cpu_ms": 4,
              "RenderGround_gpu_ms": -1, "terrain_emitted_tiles": 400, "precache_points": 500}
    rows = [",".join(HEADER)]
    for frame in range(frames):
        values["frame"] = frame
        rows.append(",".join(str(values[key]) for key in HEADER))
    return "\n".join(rows) + "\n"


class TempCase(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="c2-perf-tests-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def csv(self, text):
        path = self.root / "capture.csv"
        path.write_bytes(text.encode("utf-8"))
        return path


@unittest.skipUnless(PERL, "Perl unavailable: helper CLI NOT verified")
class PerlHelper(TempCase):
    def run_perl(self, *args):
        return subprocess.run([PERL, str(ROOT / "tools/perf/glperf_stats.pl"), *map(str, args)],
                              text=True, capture_output=True)

    def test_help_and_missing_arguments(self):
        self.assertEqual(self.run_perl("--help").returncode, 0)
        self.assertEqual(self.run_perl().returncode, 2)

    def test_compiles_and_reports_current_scopes_and_unavailable_gpu(self):
        syntax = subprocess.run([PERL, "-c", str(ROOT / "tools/perf/glperf_stats.pl")], capture_output=True)
        self.assertEqual(syntax.returncode, 0, syntax.stderr)
        result = self.run_perl(self.csv(capture()))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertRegex(result.stdout, r"cpu_ms\s+n=120 mean=10.000 med=10.000")
        self.assertRegex(result.stdout, r"gpu_ms\s+n=0 \(unavailable\)")
        for key in ("previous_frame_interval_ms", "previous_swap_ms", "PreCashGroundModel_cpu_ms", "terrain_emitted_tiles"):
            self.assertIn(key, result.stdout)
        self.assertRegex(result.stdout, r"RenderGround_gpu_ms\s+n=0 \(unavailable\)")

    def test_known_even_median_and_nearest_rank_percentiles(self):
        text = "cpu_ms,gpu_ms,triangles,draw_calls\n" + "".join(f"{i},-1,20,2\n" for i in range(1, 21))
        result = self.run_perl(self.csv(text))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("n=20 mean=10.500 med=10.500 p95=19.000 p99=20.000", result.stdout)

    def test_bom_crlf_scientific_and_valid_gpu_samples(self):
        result = self.run_perl(self.csv("\ufeffcpu_ms,gpu_ms,triangles,draw_calls\r\n1e1,2,3,4\r\n"))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertRegex(result.stdout, r"gpu_ms\s+n=1 mean=2.000")

    def test_previous_frame_sentinels_are_not_timings(self):
        text = "cpu_ms,gpu_ms,triangles,draw_calls,previous_frame_interval_ms,previous_swap_ms\n1,-1,3,4,-1,-1\n"
        result = self.run_perl(self.csv(text))
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertRegex(result.stdout, r"previous_frame_interval_ms\s+n=0")

    def test_rejects_bad_numeric_rows_headers_and_negative_values(self):
        for text in ("", "cpu_ms,gpu_ms,triangles,draw_calls\n", "cpu_ms,cpu_ms,gpu_ms,triangles,draw_calls\n1,2,-1,3,4\n",
                     "cpu_ms,gpu_ms,triangles,draw_calls\n1,-1,3\n", "cpu_ms,gpu_ms,triangles,draw_calls\n1,-1,3,4,5\n",
                     "cpu_ms,gpu_ms,triangles,draw_calls\n1,,3,4\n", "gpu_ms,triangles,draw_calls\n-1,3,4\n"):
            with self.subTest(text=text):
                self.assertNotEqual(self.run_perl(self.csv(text)).returncode, 0)
        for value in ("nan", "inf", "1e999", "garbage", "1 trailing", "-2"):
            with self.subTest(value=value):
                self.assertNotEqual(self.run_perl(self.csv(f"cpu_ms,gpu_ms,triangles,draw_calls\n{value},-1,3,4\n")).returncode, 0)

    def test_missing_file_fails_without_skipping_other_input(self):
        result = self.run_perl(self.root / "missing.csv", self.csv(capture()))
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("n=120 mean=10.000", result.stdout)


class CaptureReader(TempCase):
    def test_known_statistics(self):
        self.assertEqual(perf.stats([1, 2, 3, 4])["median"], 2.5)
        self.assertEqual(perf.stats(range(1, 21))["p95"], 19)
        self.assertEqual(perf.stats([])["n"], 0)

    def test_current_capture_and_sentinels(self):
        result = perf.read_capture(self.csv("\ufeff" + capture().replace("\n", "\r\n")))
        self.assertEqual(result["cpu_ms"]["median"], 10)
        self.assertEqual(result["gpu_ms"], {"n": 0, "unavailable": True})
        self.assertEqual(result["PreCashGroundModel_cpu_ms"]["median"], 1.9)

    def test_rejects_partial_nonsequential_and_missing_columns(self):
        for text in (capture(frames=119), capture(frames=121), capture().replace("0,10", "5,10", 1),
                     capture().replace("cpu_ms", "broken", 1), capture().replace("frame,", "frame,frame,", 1)):
            with self.subTest(text=text[:100]):
                with self.assertRaises(ValueError):
                    perf.read_capture(self.csv(text))

    def test_rejects_invalid_values_and_gpu_contract_changes(self):
        for value in ("nan", "inf", "1e999", "-2", "", "oops"):
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    perf.read_capture(self.csv(capture().replace("0,10,-1", f"0,{value},-1", 1)))
        for text in (capture().replace("10,-1,100", "10,2,100", 1),
                     capture().replace("10,-1,100", "10,-1,100.5", 1)):
            with self.assertRaises(ValueError):
                perf.read_capture(self.csv(text))

    def test_lagged_interval_first_sentinel_excluded(self):
        result = perf.read_capture(self.csv(capture().replace("2000,12,1", "2000,-1,-1", 1)))
        self.assertEqual(result["previous_frame_interval_ms"]["n"], 119)
        self.assertEqual(result["previous_swap_ms"]["n"], 119)


class SessionReport(TempCase):
    def session(self, targets=("ui",)):
        manifest = {"schema_version": 1, "kind": "unattended-workbench-renderer", "status": "complete",
                    "originals_unchanged": True, "repeats": 3, "targets": list(targets),
                    "settings": {"resolution": "2560x1440", "view_option": 255, "radius": 230, "frames": 120, "warmup_ms": 3000},
                    "provenance": {"exe_sha256": "a" * 64, "config_sha256": "b" * 64,
                                   "profile_sha256": "c" * 64, "assets": {"area4.map": "d" * 64}}, "captures": []}
        for repeat in range(1, 4):
            for target in targets:
                for phase, state, cpu in zip(("before", "target", "after"), ("closed", target, "closed"), (10, 11 if target == "ui" else 10.2, 10.4)):
                    name = f"{repeat}-{target}-{phase}"
                    csv_path, log_path = self.root / f"{name}.csv", self.root / f"{name}.log"
                    csv_path.write_text(capture(cpu), encoding="utf-8")
                    log_path.write_text(f"Unattended workbench mode: {state}\nUnattended capture complete; framebuffer saved.\n", encoding="utf-8")
                    manifest["captures"].append({"repeat": repeat, "target": target, "phase": phase, "state": state,
                                                 "csv": csv_path.name, "csv_sha256": perf.sha256(csv_path),
                                                 "log": log_path.name, "log_sha256": perf.sha256(log_path)})
        path = self.root / "session.json"
        perf.save_json(path, manifest)
        return path, manifest

    def test_bracketed_delta_drift_and_test_strip_separation(self):
        path, _ = self.session(("ui", "test"))
        result = perf.analyze(path)
        self.assertAlmostEqual(result["aggregate"]["ui"]["cpu_ms"]["delta_ms"]["median"], .8)
        self.assertAlmostEqual(result["aggregate"]["ui"]["cpu_ms"]["closed_drift_ms"]["median"], .4)
        self.assertAlmostEqual(result["aggregate"]["test"]["cpu_ms"]["delta_ms"]["median"], 0)
        self.assertIn("terrain_emitted_tiles", result["triples"]["ui"][0]["workload"])
        self.assertNotIn("performance_pass", result)

    def test_rejects_incomplete_unpreserved_mixed_settings_and_bad_provenance(self):
        path, original = self.session()
        for field, value in (("status", "prepared"), ("originals_unchanged", False), ("repeats", 1), ("targets", ["ui", "ui"])):
            manifest = copy.deepcopy(original)
            manifest[field] = value
            perf.save_json(path, manifest)
            with self.subTest(field=field), self.assertRaises(ValueError):
                perf.analyze(path)
        for key, value in (("resolution", "1920x1080"), ("view_option", 128), ("radius", 72), ("frames", 119), ("warmup_ms", 0)):
            manifest = copy.deepcopy(original)
            manifest["settings"][key] = value
            perf.save_json(path, manifest)
            with self.subTest(key=key), self.assertRaises(ValueError):
                perf.analyze(path)
        manifest = copy.deepcopy(original)
        manifest["provenance"]["exe_sha256"] = "unknown"
        perf.save_json(path, manifest)
        with self.assertRaises(ValueError):
            perf.analyze(path)

    def test_rejects_wrong_mode_order_and_reused_or_changed_evidence(self):
        path, original = self.session()
        for key, value in (("state", "test"), ("phase", "before"), ("repeat", 2), ("csv", "../outside.csv")):
            manifest = copy.deepcopy(original)
            manifest["captures"][1][key] = value
            perf.save_json(path, manifest)
            with self.subTest(key=key), self.assertRaises(ValueError):
                perf.analyze(path)
        perf.save_json(path, original)
        csv_path = self.root / original["captures"][0]["csv"]
        csv_path.write_text(capture(12), encoding="utf-8")
        with self.assertRaises(ValueError):
            perf.analyze(path)

    def test_mode_requires_runtime_confirmation_not_a_manifest_label(self):
        path, manifest = self.session()
        entry = manifest["captures"][1]
        log = self.root / entry["log"]
        log.write_text("Unattended workbench mode: closed\nUnattended capture complete; framebuffer saved.\n", encoding="utf-8")
        entry["log_sha256"] = perf.sha256(log)
        perf.save_json(path, manifest)
        with self.assertRaises(ValueError):
            perf.analyze(path)

    def test_cli_analyze_runs_without_game_or_computer_use(self):
        path, _ = self.session()
        result = subprocess.run([sys.executable, str(ROOT / "tools/perf/workbench_perf.py"), "analyze", str(path)], text=True, capture_output=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(json.loads(result.stdout)["kind"], "unattended-workbench-renderer")


class IsolatedInputs(TempCase):
    def test_config_copy_preserves_custom_bytes_comments_and_defaults(self):
        source = b"# owner\xff\r\nglperf_logging 0 # keep\r\nfps_limit 2\r\nflashlight_range 11000\r\nflashlight_intensity 200\r\n"
        result = perf.set_config(source)
        self.assertIn(b"glperf_logging 1 # keep", result)
        self.assertIn(b"# owner\xff", result)
        self.assertIn(b"flashlight_range 11000", result)
        self.assertIn(b"flashlight_intensity 200", result)
        self.assertIn(b"resolution 2560x1440", result)
        self.assertEqual(perf.set_config(result), result)
        self.assertIn(b"glperf_logging 0", source)

    def test_only_cloned_profile_view_bytes_change(self):
        source = bytes([42]) * 1660
        result = perf.profile_copy(source)
        self.assertEqual(result[:1540], source[:1540])
        self.assertEqual(result[1544:], source[1544:])
        self.assertEqual(result[1540:1544], b"\xff\0\0\0")
        for bad in (b"", bytes(1659), bytes(1661)):
            with self.assertRaises(ValueError):
                perf.profile_copy(bad)

    def test_bitmap_contract_without_real_framebuffer(self):
        path = self.root / "frame.bmp"
        path.write_bytes(b"not a bitmap")
        with self.assertRaises(ValueError):
            perf.validate_bitmap(path)

    def test_safe_paths_reject_escape_and_missing(self):
        (self.root / "evidence.csv").write_text("ok")
        self.assertEqual(perf.local_file(self.root, "evidence.csv"), self.root / "evidence.csv")
        for relative in ("../missing.csv", "missing.csv"):
            with self.assertRaises(ValueError):
                perf.local_file(self.root, relative)


class RunnerContracts(TempCase):
    def prepare_fake(self):
        repo, game = self.root / "repo", self.root / "game"
        binary = repo / "build/ogl-release/bin/Carnivores1_GL.exe"
        binary.parent.mkdir(parents=True)
        (binary.parent / "shaders").mkdir()
        (binary.parent / "shaders/sky.frag").write_text("synthetic shader")
        (repo / "Hunt/Core").mkdir(parents=True)
        (repo / "Hunt/Core/UnattendedCapture.h").write_text("-capture-workbench=")
        (repo / "deps").mkdir()
        for name in ("CMakeLists.txt", "CMakePresets.json"):
            (repo / name).write_text("synthetic")
        (repo / "build/ogl-release/CMakeCache.txt").write_text("GL_PERF_HOOKS:BOOL=ON\n")
        image = bytearray(128)
        struct.pack_into("<I", image, 0x3c, 64)
        image[64:68] = b"PE\0\0"
        struct.pack_into("<H", image, 68, 0x14c)
        binary.write_bytes(image)
        (game / "HUNTDAT").mkdir(parents=True)
        (game / "HUNTDAT/fixture.map").write_bytes(b"synthetic private fixture")
        (game / "MULTIPLAYER/AVATARS").mkdir(parents=True)
        (game / "MULTIPLAYER/AVATARS/Hitbox.car").write_bytes(b"synthetic offline-required avatar")
        (game / "config.cfg").write_bytes(b"glperf_logging 0\nflashlight_range 11000\n")
        (game / "trophy00.sav").write_bytes(bytes(1660))
        (game / "trophy00.sab").write_bytes(b"owner aux profile")
        original = perf.inventory(game)
        output = repo / "build/workbench-test"
        with mock.patch.object(perf, "REPO", repo), mock.patch.object(perf, "assert_no_game"), \
                mock.patch.object(perf, "system_info", return_value={"test_only": True}), \
                mock.patch.object(perf.subprocess, "check_output", side_effect=["", "a" * 40]):
            session = perf.prepare(game, binary, output, ["ui"], 3, 3000)
        self.assertEqual(perf.inventory(game), original)
        return session, game, original

    def test_prepare_clones_without_writing_originals_or_launching(self):
        with mock.patch.object(perf.subprocess, "Popen") as launch:
            session, game, original = self.prepare_fake()
            launch.assert_not_called()
        self.assertEqual(perf.inventory(game), original)
        self.assertIn(b"glperf_logging 1", (session.parent / "inputs/config.cfg").read_bytes())
        self.assertIn(b"glperf_logging 0", (game / "config.cfg").read_bytes())
        self.assertFalse((session.parent / "runtime/opengl32.dll").exists())
        avatar = "MULTIPLAYER/AVATARS/Hitbox.car"
        self.assertEqual((session.parent / "runtime" / avatar).read_bytes(), (game / avatar).read_bytes())
        manifest = json.loads(session.read_text())
        self.assertEqual(manifest["provenance"]["assets"][avatar], perf.sha256(game / avatar))

    def test_mocked_runner_collects_bracketed_evidence_and_preserves_originals(self):
        session, game, original = self.prepare_fake()
        count = 0
        class Process:
            pid = 987654
            returncode = 0
            def poll(self):
                return 0
        def launch(args, cwd):
            nonlocal count
            count += 1
            state = next(a.split("=", 1)[1] for a in args if a.startswith("-capture-workbench="))
            self.assertIn("-capture-background", args)
            self.assertIn("-camfixed", args)
            cwd = Path(cwd)
            (cwd / f"glperf-capture-{count}.csv").write_text(capture(11 if state == "ui" else 10))
            (cwd / "render.log").write_text(f"Unattended workbench mode: {state}\nUnattended capture complete; framebuffer saved.\n")
            return Process()
        with mock.patch.object(perf, "REPO", session.parents[2]), mock.patch.object(perf, "assert_no_game"), mock.patch.object(perf, "validate_bitmap"), \
                mock.patch.object(perf.subprocess, "Popen", side_effect=launch):
            perf.run(session)
        self.assertEqual(count, 9)
        self.assertEqual(perf.inventory(game), original)
        report = json.loads((session.parent / "report.json").read_text())
        self.assertEqual(report["aggregate"]["ui"]["cpu_ms"]["delta_ms"]["median"], 1)
        with mock.patch.object(perf, "assert_no_game"), self.assertRaises(ValueError):
            perf.run(session)  # Never silently overwrite/resume a completed run.

    def test_mocked_zero_exit_halt_retains_diagnostics_and_fails(self):
        session, game, original = self.prepare_fake()
        class Process:
            pid = 987654
            returncode = 0
            def poll(self):
                return 0
        def launch(args, cwd):
            (Path(cwd) / "render.log").write_text("ABNORMAL_HALT: Missing offline avatar.\n")
            return Process()
        with mock.patch.object(perf, "REPO", session.parents[2]), mock.patch.object(perf, "assert_no_game"), \
                mock.patch.object(perf.subprocess, "Popen", side_effect=launch):
            with self.assertRaisesRegex(ValueError, "Missing offline avatar"):
                perf.run(session)
        self.assertIn("ABNORMAL_HALT:", (session.parent / "r1-ui-before/render.log").read_text())
        self.assertEqual(json.loads(session.read_text())["status"], "failed")
        self.assertEqual(perf.inventory(game), original)

    def test_mocked_timeout_records_pid_without_killing(self):
        session, game, original = self.prepare_fake()
        class Process:
            pid = 987654
            def poll(self):
                return None
            def kill(self):
                raise AssertionError("Must not kill")
            def terminate(self):
                raise AssertionError("Must not terminate")
        with mock.patch.object(perf, "REPO", session.parents[2]), mock.patch.object(perf, "assert_no_game"), mock.patch.object(perf, "hidden_windows"), \
                mock.patch.object(perf.subprocess, "Popen", return_value=Process()), \
                mock.patch.object(perf.time, "monotonic", side_effect=[0, 1000]):
            with self.assertRaisesRegex(ValueError, "NOT killed"):
                perf.run(session)
        manifest = json.loads(session.read_text())
        self.assertEqual(manifest["status"], "failed")
        self.assertEqual(manifest["still_running_pid"], 987654)
        self.assertTrue(manifest["originals_unchanged"])
        self.assertEqual(perf.inventory(game), original)


if __name__ == "__main__":
    unittest.main(verbosity=2)
