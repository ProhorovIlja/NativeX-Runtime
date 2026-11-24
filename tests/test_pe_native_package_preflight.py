"""Synthetic-only PE package dependency preflight; no guest or host execution."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.pe_native_import_binding_plan import ValidationError
from tools.pe_native_package_preflight import build_preflight


def pe(imports=(), delayed=()):
    return {"kind": "pe32plus-metadata", "machine": "AMD64",
            "executable_tested": False, "import_modules": list(imports),
            "delay_import_modules": list(delayed)}


def package():
    return {"schema": "scarlett-native-pe-package/v1", "entry": "game.EXE",
            "images": [
                {"name": "GAME.exe", "pe": pe(["engine.dll", "Kernel32.dll"], ["user32.dll"])},
                {"name": "engine.dll", "pe": pe(["Kernel32.dll"])},
                {"name": "unused.dll", "pe": pe(["obsolete.dll"])},
            ], "external_modules": ["kernel32.dll"]}


class PackagePreflightTests(unittest.TestCase):
    def test_order_immediate_and_defer_delayed(self):
        result = build_preflight(package())
        self.assertEqual(result["immediate_dependency_order"], ["engine.dll", "game.exe"])
        self.assertEqual(result["reachable_images"], ["engine.dll", "game.exe"])
        self.assertEqual(result["unreached_images"], ["unused.dll"])
        self.assertEqual(result["declared_external_immediate"], ["kernel32.dll"])
        self.assertEqual(result["missing_delayed_modules"], ["user32.dll"])
        self.assertEqual(result["missing_immediate_modules"], [])
        self.assertTrue(result["immediate_names_resolved"])
        self.assertFalse(result["all_names_resolved"])
        self.assertFalse(result["provider_exports_verified"])
        self.assertFalse(result["abi_verified"])
        self.assertFalse(result["game_executed"])

    def test_all_explicit_names_still_not_abi_proof(self):
        config = package()
        config["external_modules"].append("USER32.dll")
        result = build_preflight(config)
        self.assertTrue(result["all_names_resolved"])
        self.assertFalse(result["provider_exports_verified"])

    def test_unknown_required_refused(self):
        config = package()
        config["external_modules"] = []
        result = build_preflight(config)
        self.assertEqual(result["missing_immediate_modules"], ["kernel32.dll"])
        self.assertEqual(result["immediate_dependency_order"], [])
        self.assertFalse(result["immediate_names_resolved"])

    def test_immediate_cycle_refused(self):
        config = package()
        config["images"][1]["pe"] = pe(["GAME.exe"])
        result = build_preflight(config)
        self.assertEqual(result["immediate_cycles"], [["game.exe", "engine.dll", "game.exe"]])
        self.assertEqual(result["immediate_dependency_order"], [])
        self.assertFalse(result["immediate_names_resolved"])

    def test_delayed_internal_not_boot_loaded(self):
        config = package()
        config["images"][0]["pe"] = pe([], ["engine.dll"])
        config["images"][1]["pe"] = pe(["GAME.exe"])
        result = build_preflight(config)
        self.assertEqual(result["immediate_dependency_order"], ["game.exe"])
        self.assertEqual(result["reachable_images"], ["game.exe"])
        self.assertEqual(result["unreached_images"], ["engine.dll", "unused.dll"])
        self.assertEqual(result["dependency_edges"], [
            {"from": "game.exe", "to": "engine.dll", "kind": "delayed-internal"}])

    def test_case_folding_and_duplicate_module_defense(self):
        config = package()
        config["images"].append({"name": "ENGINE.DLL", "pe": pe()})
        with self.assertRaises(ValidationError):
            build_preflight(config)
        config = package()
        config["external_modules"].append("KERNEL32.DLL")
        with self.assertRaises(ValidationError):
            build_preflight(config)
        config = package()
        config["external_modules"].append("ENGINE.dll")
        with self.assertRaises(ValidationError):
            build_preflight(config)

    def test_rejects_bad_inventory_and_paths(self):
        candidates = []
        config = package(); config["entry"]="../GAME.exe"; candidates.append(config)
        config = package(); config["images"][1]["name"]="C:\\engine.dll"; candidates.append(config)
        config = package(); config["external_modules"]=["/lib/xmem.dll"]; candidates.append(config)
        config = package(); config["images"][1]["pe"]["machine"]="ARM64"; candidates.append(config)
        config = package(); config["images"][0]["pe"]["executable_tested"] = True; candidates.append(config)
        config = package(); config["images"][1]["pe"]["import_modules"] = "kernel32.dll"; candidates.append(config)
        config = package(); config["images"][1]["pe"]["import_modules"] = ["kernel32.dll", None]; candidates.append(config)
        config = package(); config["entry"] = "missing.exe"; candidates.append(config)
        config = package(); config["schema"] = "unknown"; candidates.append(config)
        for candidate in candidates:
            with self.subTest(candidate=candidate.get("entry")):
                with self.assertRaises(ValidationError):
                    build_preflight(candidate)

    def test_cli_exit_codes_atomic_and_deterministic(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            config = root / "package.json"
            dest = root / "preflight.json"
            config.write_text(json.dumps(package()), encoding="utf-8")
            tool = ROOT / "tools" / "pe_native_package_preflight.py"
            cmd = [sys.executable, str(tool), "--package", str(config), "--out", str(dest)]
            good = subprocess.run(cmd + ["--require-immediate"], capture_output=True, text=True)
            self.assertEqual(good.returncode, 0, good.stderr)
            original = dest.read_bytes()
            another = subprocess.run(cmd + ["--require-all"], capture_output=True, text=True)
            self.assertEqual(another.returncode, 3)
            self.assertEqual(original, dest.read_bytes())
            config.write_text(json.dumps({"schema": "bad"}), encoding="utf-8")
            rejected = subprocess.run(cmd, capture_output=True, text=True)
            self.assertEqual(rejected.returncode, 2)
            self.assertEqual(original, dest.read_bytes())


if __name__ == "__main__":
    unittest.main()
