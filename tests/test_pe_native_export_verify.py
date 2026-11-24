"""All host PE export evidence is synthetic, never a game or real OS DLL."""
from __future__ import annotations

import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.pe_native_import_binding_plan import build_plan, ValidationError
from tools.pe_native_export_verify import verify_plan


def synthetic_host(name, slots, names):
    """Construct explicitly labeled *synthetic* pe_intake output."""
    return {"kind": "pe32plus-metadata", "machine": "AMD64", "executable_tested": False,
            "export_dll_name": name,
            "export_addresses": [dict(ordinal=k, target_rva=rva, forwarder=fwd)
                                 for k, rva, fwd in slots],
            "exports": [dict(name=sym, ordinal=ordinal, forwarder=fwd)
                        for sym, ordinal, fwd in names]}


BASE_SLOTS = [(5, 0x1000, None), (6, 0x1100, "OTHER.Ping"),
              (7, 0x1110, "OTHER.#2"), (8, 0x1120, "BASE.SelfCycle"),
              (9, 0x1130, "OTHER.NoSuchName"), (10, 0x1140, "OTHER.#bad"),
              (11, 0x1150, "OTHER.Back")]
BASE_NAMES = [("NativeCall", 5, None), ("FwdCall", 6, "OTHER.Ping"),
              ("OrdinalForward", 7, "OTHER.#2"), ("SelfCycle", 8, "BASE.SelfCycle"),
              ("MissingForwarder", 9, "OTHER.NoSuchName"),
              ("InvalidForwarder", 10, "OTHER.#bad"),
              ("Cycle", 11, "OTHER.Back")]
OTHER_SLOTS = [(1, 0x2000, None), (2, 0x2100, None), (3, 0x2200, "BASE.Cycle")]
OTHER_NAMES = [("Ping", 1, None), ("Back", 3, "BASE.Cycle")]
HOSTS = {"schema": "scarlett-host-pe-export-inventories/v1", "modules": [
    {"module": "base.dll", "pe": synthetic_host("BASE.DLL", BASE_SLOTS, BASE_NAMES)},
    {"module": "other.dll", "pe": synthetic_host("other.dll", OTHER_SLOTS, OTHER_NAMES)}]}


def make_plan(host_symbol="NativeCall"):
    guest = {"kind": "pe32plus-metadata", "machine": "AMD64", "executable_tested": False,
             "import_symbols": [
                 {"module": "GAMECORE.dll", "name": "Startup", "ordinal": None,
                  "lookup_rva": 0x1000, "iat_rva": 0x1500},
                 {"module": "GAMECORE.dll", "name": None, "ordinal": 11,
                  "lookup_rva": 0x1008, "iat_rva": 0x1508},
             ],
             "delay_import_symbols": [
                 {"module": "USER.dll", "name": "Later", "ordinal": None,
                  "lookup_rva": 0x1200, "iat_rva": 0x1600}]}
    manifest = {"schema": "scarlett-native-import-bindings/v1", "bindings": [
        {"guest_module": "gamecore.dll", "guest_name": "Startup", "action": "host-export",
         "host_module": "base.dll", "host_symbol": host_symbol},
        {"guest_module": "GAMECORE.dll", "guest_ordinal": 11, "action": "native-adapter",
         "adapter_symbol": "NativeUnverifiedAdapter"},
        {"guest_module": "USER.dll", "guest_name": "Later", "action": "unsupported"}]}
    return build_plan(guest, manifest)


class HostExportInventoryTests(unittest.TestCase):
    def test_present_direct_with_adapter_unverified(self):
        report = verify_plan(make_plan(), HOSTS)
        self.assertEqual(report["checks"][0]["status"], "present-direct")
        self.assertEqual(report["checks"][1]["status"], "adapter-not-verified")
        self.assertEqual(report["checks"][2]["status"], "unsupported")
        self.assertEqual(report["summary"], {"total": 3, "host_export": 1, "present": 1,
            "forwarded": 0, "missing": 0, "declared_adapters": 1, "unsupported": 1,
            "unresolved": 0})
        self.assertTrue(report["all_host_exports_present"])
        self.assertFalse(report["abi_verified"])
        self.assertFalse(report["game_executed"])

    def test_forwarded_name_and_ordinal_only(self):
        for target, expected_ordinal in (("FwdCall", None), ("OrdinalForward", 2)):
            with self.subTest(target=target):
                report = verify_plan(make_plan(target), HOSTS)
                self.assertEqual(report["checks"][0]["status"], "present-forwarded")
                chain = report["checks"][0]["chain"]
                self.assertEqual(len(chain), 2)
                self.assertEqual(chain[1]["module"], "other.dll")
                self.assertEqual(chain[1]["ordinal"], expected_ordinal)
                self.assertEqual(report["summary"]["forwarded"], 1)

    def test_missing_symbol_module_and_malformed_forwarder(self):
        expected = (("MissingForwarder", "missing-host-export"),
                    ("InvalidForwarder", "malformed-host-forwarder"),
                    ("TotallyMissing", "missing-host-export"))
        for name, status in expected:
            with self.subTest(name=name):
                report = verify_plan(make_plan(name), HOSTS)
                self.assertEqual(report["checks"][0]["status"], status)
                self.assertFalse(report["all_host_exports_present"])
        hosts = copy.deepcopy(HOSTS)
        hosts["modules"].pop()
        self.assertEqual(verify_plan(make_plan("FwdCall"), hosts)["checks"][0]["status"],
                         "missing-host-module")

    def test_cycles_and_determinism(self):
        for name in ("SelfCycle", "Cycle"):
            report = verify_plan(make_plan(name), HOSTS)
            self.assertEqual(report["checks"][0]["status"], "forwarder-cycle")
            self.assertEqual(report, verify_plan(make_plan(name), HOSTS))
            self.assertLessEqual(len(report["checks"][0]["chain"]), 16)

    def test_explicit_host_ordinal_not_named(self):
        result = verify_plan(make_plan("#7"), HOSTS)
        self.assertEqual(result["checks"][0]["status"], "present-forwarded")
        result = verify_plan(make_plan("#12"), HOSTS)
        self.assertEqual(result["checks"][0]["status"], "missing-host-eat-entry")

    def test_malformed_host_inventories_fail_closed(self):
        corruptions = []
        d = copy.deepcopy(HOSTS); d["modules"][0]["pe"]["export_dll_name"]="unrelated.dll";corruptions.append(d)
        d = copy.deepcopy(HOSTS); d["modules"].append(d["modules"][0]);corruptions.append(d)
        d = copy.deepcopy(HOSTS); d["modules"][0]["pe"]["export_addresses"][0]["target_rva"]=0;corruptions.append(d)
        d = copy.deepcopy(HOSTS); d["modules"][0]["pe"]["export_addresses"].append(d["modules"][0]["pe"]["export_addresses"][0]);corruptions.append(d)
        d = copy.deepcopy(HOSTS); d["modules"][0]["pe"]["exports"][0]["forwarder"]="OTHER.Ping";corruptions.append(d)
        d = copy.deepcopy(HOSTS); d["modules"][0]["pe"]["exports"][1]["name"]="NativeCall";corruptions.append(d)
        d = copy.deepcopy(HOSTS); d["modules"][0]["pe"]["machine"]="ARM64";corruptions.append(d)
        d = copy.deepcopy(HOSTS); d["modules"][0]["pe"]["executable_tested"]=True;corruptions.append(d)
        d = copy.deepcopy(HOSTS); d["modules"][0]["module"]="../base.dll";corruptions.append(d)
        for bad in corruptions:
            with self.subTest(bad=bad["modules"][0].get("module")):
                with self.assertRaises(ValidationError):
                    verify_plan(make_plan(), bad)

    def test_wrong_plan_schema_is_rejected(self):
        bad = make_plan()
        bad["dry_run_only"] = False
        with self.assertRaises(ValidationError):
            verify_plan(bad, HOSTS)
        bad = make_plan()
        bad["bindings"][0]["action"]="exec-system32"
        with self.assertRaises(ValidationError):
            verify_plan(bad, HOSTS)

    def test_cli_exit_codes_and_atomic_report(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            plan_path = root / "plan.json"
            host_path = root / "hosts.json"
            output_path = root / "report.json"
            plan_path.write_text(json.dumps(make_plan("NativeCall")), encoding="utf-8")
            host_path.write_text(json.dumps(HOSTS), encoding="utf-8")
            tool = ROOT / "tools" / "pe_native_export_verify.py"
            command = [sys.executable, str(tool), "--plan", str(plan_path),
                       "--host-inventories", str(host_path), "--out", str(output_path),
                       "--require-present"]
            good = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(good.returncode, 0, good.stderr)
            original = output_path.read_bytes()
            plan_path.write_text(json.dumps(make_plan("MissingForwarder")), encoding="utf-8")
            missing = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(missing.returncode, 3, missing.stderr)
            self.assertFalse(json.loads(output_path.read_text())["all_host_exports_present"])
            host_path.write_text("{}", encoding="utf-8")
            rejected = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(rejected.returncode, 2)
            self.assertEqual(output_path.read_bytes(), json.dumps(json.loads(output_path.read_text()),
                                    indent=2, sort_keys=True, ensure_ascii=True).encode()+b"\n")
            self.assertNotEqual(output_path.read_bytes(), original)

    def test_empty_host_bindings_not_a_success(self):
        plan = make_plan()
        plan["bindings"] = plan["bindings"][1:]
        report = verify_plan(plan, HOSTS)
        self.assertEqual(report["summary"]["host_export"], 0)
        self.assertFalse(report["all_host_exports_present"])


if __name__ == "__main__":
    unittest.main()
