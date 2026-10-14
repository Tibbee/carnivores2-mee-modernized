import importlib.util
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("config_defaults", ROOT / "tools/c2_config_defaults.py")
config = importlib.util.module_from_spec(spec)
spec.loader.exec_module(config)


class ConfigDefaultsTests(unittest.TestCase):
    def test_production_readers_audio_and_package_coverage(self):
        config.check_coverage(ROOT)
        defaults = config.entries(config.engine_template(ROOT))
        self.assertNotIn("night_lighting", defaults)
        self.assertEqual(defaults["night_moon_glow_strength"], "100")
        self.assertEqual(defaults["night_moon_brightness"], "128")
        self.assertEqual(defaults["gpufeatures"], "127")
        self.assertEqual(len([k for k in defaults if k.startswith("env")]), 32)

    def test_custom_values_unknown_keys_comments_and_duplicates_survive(self):
        original = b"# My config\r\nnight_moon_brightness 100\r\nfov 80\r\nFOV 75\r\nfuture_key mine\r\n"
        result, added = config.complete_config(original, "fov 62\nnight_moon_brightness 128\nflashlight_range 6000\n")
        self.assertTrue(result.startswith(original))
        self.assertEqual(added, ["flashlight_range"])
        self.assertNotIn(b"fov 62", result)
        self.assertNotIn(b"night_moon_brightness 128", result)

    def test_idempotent(self):
        first, _ = config.complete_config(b"fov 75\n", config.engine_template())
        second, added = config.complete_config(first, config.engine_template())
        self.assertEqual(first, second)
        self.assertEqual(added, [])

    def test_bom_non_ascii_and_existing_line_endings_are_preserved(self):
        original = b"\xef\xbb\xbf# caf\xe9\r\nfov 75\ncustom thing\r\n"
        result, _ = config.complete_config(original, "fov 62\nnight_moon_brightness 128\n")
        self.assertTrue(result.startswith(original))
        self.assertTrue(result.endswith(b"night_moon_brightness 128\r\n"))

    def test_missing_final_newline_does_not_merge_keys(self):
        result, _ = config.complete_config(b"fov 80", "fov 62\nflashlight_key 70\n")
        self.assertTrue(result.startswith(b"fov 80\n"))
        self.assertIn(b"\nflashlight_key 70\n", result)

    def test_conditional_menu_choices_are_only_examples(self):
        result, added = config.complete_config(b"# Existing\n", config.engine_template())
        active = config.entries(result.decode("ascii"))
        self.assertFalse(active.keys() & config.OPTIONAL)
        self.assertEqual(set(k[1:] for k in added if k.startswith("#")), config.OPTIONAL)
        again, added = config.complete_config(result, config.engine_template())
        self.assertEqual(result, again)
        self.assertEqual(added, [])

    def test_existing_commented_custom_resolution_is_not_replaced(self):
        result, added = config.complete_config(b"#resolution 3440x1440\n", "fov 62\n#resolution 1920x1080\n")
        self.assertNotIn("#resolution", added)
        self.assertIn(b"#resolution 3440x1440", result)
        self.assertNotIn(b"1920x1080", result)

    def test_rejects_unsupported_configs_without_touching_them(self):
        for invalid in (b"\xff\xfeX\x00", b"\xfe\xff\x00X", b"fov\x0062", b"X" * (1024 * 1024 + 1)):
            with self.assertRaises(ValueError):
                config.complete_config(invalid, "fov 62\n")

    def test_cli_dry_run_backup_apply_and_second_apply(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "config with spaces.cfg"
            original = b"# Preserve\r\nnight_moon_brightness 200\r\n"
            path.write_bytes(original)
            command = [sys.executable, str(ROOT / "tools/c2_config_defaults.py"), str(path)]
            subprocess.run(command, check=True, capture_output=True)
            self.assertEqual(path.read_bytes(), original)
            subprocess.run(command + ["--apply"], check=True, capture_output=True)
            completed = path.read_bytes()
            self.assertTrue(completed.startswith(original))
            self.assertEqual(path.with_name(path.name + ".before-defaults.bak").read_bytes(), original)
            subprocess.run(command + ["--apply"], check=True, capture_output=True)
            self.assertEqual(path.read_bytes(), completed)

    def test_existing_backup_is_not_overwritten(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "config.cfg"
            original = b"fov 80\n"
            path.write_bytes(original)
            previous = path.with_name(path.name + ".before-defaults.bak")
            previous.write_bytes(b"Older backup")
            subprocess.run([sys.executable, str(ROOT / "tools/c2_config_defaults.py"), str(path), "--apply"],
                           check=True, capture_output=True)
            self.assertEqual(previous.read_bytes(), b"Older backup")
            self.assertEqual(path.with_name(path.name + ".before-defaults.1.bak").read_bytes(), original)

    def test_cli_creates_a_complete_first_run_template(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "config.cfg"
            command = [sys.executable, str(ROOT / "tools/c2_config_defaults.py"), str(path)]
            subprocess.run(command, check=True, capture_output=True)
            self.assertFalse(path.exists())
            subprocess.run(command + ["--apply"], check=True, capture_output=True)
            self.assertEqual(path.read_bytes(), config.engine_template().encode("utf-8"))

    def test_new_reader_key_fails_coverage_until_documented(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for relative in ("Hunt/Game/EngineInit.cpp", "Hunt/Core/Constants.h", "Hunt/Core/GameState.h",
                             "Hunt/Audio/AudioInspection.h", "Menu/Resources.cpp"):
                target = root / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / relative, target)
            source = root / "Menu/Resources.cpp"
            text = source.read_text(encoding="utf-8")
            text = text.replace('configscan::KeyEquals(field.key, "renderer")', 'configscan::KeyEquals(field.key, "new_config_setting")', 1)
            source.write_text(text, encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "new_config_setting"):
                config.check_coverage(root)


if __name__ == "__main__":
    unittest.main()
