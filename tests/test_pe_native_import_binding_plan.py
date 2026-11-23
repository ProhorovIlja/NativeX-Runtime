"""Synthetic-only regression tests for native PE32+ import binding planning."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.pe_native_import_binding_plan import ValidationError, build_plan


PE = {"kind": "pe32plus-metadata", "machine": "AMD64", "executable_tested": False,
      "import_symbols": [
          {"module": "KERNEL32.dll", "name": "GetCurrentProcessId", "ordinal": None,
           "lookup_rva": 0x1250, "iat_rva": 0x1350},
          {"module": "KERNEL32.dll", "name": None, "ordinal": 123,
           "lookup_rva": 0x1258, "iat_rva": 0x1358}],
      "delay_import_symbols": [{"module": "USER32.dll", "name": "MessageBoxW", "ordinal": None,
                                "lookup_rva": 0x12A0, "iat_rva": None}]}
CAPS = {"schema": "scarlett-native-import-bindings/v1", "bindings": [
    {"guest_module": "kernel32.DLL", "guest_name": "GetCurrentProcessId", "action": "host-export",
     "host_module": "kernel32.dll", "host_symbol": "GetCurrentProcessId"},
    {"guest_module": "KERNEL32.dll", "guest_ordinal": 123, "action": "native-adapter",
     "adapter_symbol": "NativeOrdinal123"},
    {"guest_module": "USER32.dll", "guest_name": "MessageBoxW", "action": "unsupported"}]}


class ImportBindingPlanTests(unittest.TestCase):
    def test_mixed_name_ordinal_delay_case_and_slots(self):
        result = build_plan(PE, CAPS)
        self.assertEqual(result["summary"], {"total": 3, "direct": 1, "adapters": 1,
                                             "unsupported": 1, "unresolved": 0,
                                             "immediate": 2, "delay_load": 1})
        self.assertFalse(result["all_resolved"])
        self.assertTrue(result["dry_run_only"])
        self.assertIs(result["game_executed"], False)
        self.assertEqual(result["bindings"][0]["iat_rva"], 0x1350)
        self.assertEqual(result["bindings"][1]["target"]["symbol"], "NativeOrdinal123")
        self.assertTrue(result["bindings"][2]["delay_load"])
        self.assertIsNone(result["bindings"][2]["iat_rva"])

    def test_unresolved_is_never_assumed_direct(self):
        plan = build_plan(PE, {"schema": CAPS["schema"], "bindings": []})
        self.assertEqual(plan["summary"]["unresolved"], 3)
        self.assertTrue(all(item["action"] == "unresolved" for item in plan["bindings"]))
        self.assertEqual(plan["summary"]["direct"], 0)

    def test_completed_explicit_manifest(self):
        caps = json.loads(json.dumps(CAPS))
        caps["bindings"][2].update(action="native-adapter", adapter_symbol="NativeMessageBox")
        complete = build_plan(PE, caps)
        self.assertTrue(complete["all_resolved"])
        self.assertEqual(complete["summary"]["adapters"], 2)

    def test_fail_closed_on_malformed_schema_and_duplicates(self):
        for invalid in (
            {"schema": "wrong", "bindings": []},
            {"schema": CAPS["schema"], "bindings": [CAPS["bindings"][0], CAPS["bindings"][0]]},
            {"schema": CAPS["schema"], "bindings": [dict(CAPS["bindings"][0], guest_name=None)]},
            {"schema": CAPS["schema"], "bindings": [dict(CAPS["bindings"][0], action="unknown")]},
            {"schema": CAPS["schema"], "bindings": [dict(CAPS["bindings"][0], host_symbol="")]},
            {"schema": CAPS["schema"], "bindings": [dict(CAPS["bindings"][0], guest_ordinal=True)]},
        ):
            with self.subTest(invalid=invalid), self.assertRaises(ValidationError):
                build_plan(PE, invalid)
        for pe in (dict(PE, machine="ARM64"), dict(PE, executable_tested=True),
                   dict(PE, import_symbols=[dict(PE["import_symbols"][0], iat_rva=3)]),
                   dict(PE, import_symbols=[dict(PE["import_symbols"][0], ordinal=2)])):
            with self.subTest(pe=pe), self.assertRaises(ValidationError):
                build_plan(pe, CAPS)

    def test_cli_is_read_only_deterministic_and_complete_is_opt_in(self):
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)
            (folder / "pe.json").write_text(json.dumps(PE), encoding="utf-8")
            (folder / "caps.json").write_text(json.dumps(CAPS), encoding="utf-8")
            exe = ROOT / "tools" / "pe_native_import_binding_plan.py"
            command = [sys.executable, str(exe), "--pe-json", str(folder / "pe.json"),
                       "--bindings", str(folder / "caps.json")]
            run1 = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(run1.returncode, 0, run1.stderr)
            run2 = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(run1.stdout, run2.stdout)
            output = folder / "plan.json"
            run3 = subprocess.run(command + ["--out", str(output), "--require-complete"],
                                  capture_output=True, text=True)
            self.assertEqual(run3.returncode, 3, run3.stderr)
            self.assertEqual(json.loads(output.read_text("utf-8"))["summary"]["total"], 3)
            # Invalid input never truncates a previously written result.
            (folder / "caps.json").write_text("{}", encoding="utf-8")
            failed = subprocess.run(command + ["--out", str(output)], capture_output=True, text=True)
            self.assertEqual(failed.returncode, 2)
            self.assertEqual(json.loads(output.read_text("utf-8"))["summary"]["total"], 3)

    def test_bounded_json_read(self):
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)
            (folder / "pe.json").write_bytes(b" " * (2 * 1024 * 1024 + 1))
            (folder / "caps.json").write_text(json.dumps(CAPS), encoding="utf-8")
            exe = ROOT / "tools" / "pe_native_import_binding_plan.py"
            run = subprocess.run([sys.executable, str(exe), "--pe-json", str(folder / "pe.json"),
                                  "--bindings", str(folder / "caps.json")],
                                 capture_output=True, text=True)
            self.assertEqual(run.returncode, 2)
            self.assertIn("limit", run.stderr)


if __name__ == "__main__":
    unittest.main()
