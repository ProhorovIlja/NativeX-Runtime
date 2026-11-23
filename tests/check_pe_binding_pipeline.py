"""End-to-end synthetic Windows x64 PE -> inventory -> native binding plan.

Invokes only our read-only PE metadata inspector. Never executes the PE file.
"""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.pe_native_import_binding_plan import build_plan
from check_pe_intake import fixture


def main(executable):
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "original synthetic PE32+ with spaces.exe"
        fixture(path)
        child = subprocess.run([executable, str(path)], capture_output=True, text=True, check=True)
        inventory = json.loads(child.stdout)
        assert inventory["executable_tested"] is False
        assert len(inventory["import_symbols"]) == 2
        assert len(inventory["delay_import_symbols"]) == 1
        manifest = {"schema": "scarlett-native-import-bindings/v1", "bindings": [
            {"guest_module": "kernel32.dll", "guest_name": "GetCurrentProcessId",
             "action": "host-export", "host_module": "KERNEL32.dll",
             "host_symbol": "GetCurrentProcessId"},
            {"guest_module": "KERNEL32.dll", "guest_ordinal": 123,
             "action": "native-adapter", "adapter_symbol": "SyntheticWin64Ordinal123"},
            {"guest_module": "user32.dll", "guest_name": "MessageBoxW",
             "action": "unsupported"}]}
        plan = build_plan(inventory, manifest)
        assert plan["summary"] == {"total": 3, "direct": 1, "adapters": 1,
                                   "unsupported": 1, "unresolved": 0,
                                   "immediate": 2, "delay_load": 1}, plan
        assert plan["bindings"][0]["iat_rva"] == 0x1250, plan
        assert plan["bindings"][1]["iat_rva"] == 0x1258, plan
        assert plan["bindings"][2]["iat_rva"] is None, plan
        assert plan["bindings"][2]["delay_load"] is True
        assert not plan["all_resolved"]
        # Unknown import must never be silently mapped to an OS export.
        manifest["bindings"].pop()
        other = build_plan(inventory, manifest)
        assert other["summary"]["unresolved"] == 1, other
        assert not other["all_resolved"]
        assert path.exists()  # Inventory and binding plan never alter input.
        assert path.read_bytes()[:2] == b"MZ"
    print("native Win64 PE import inventory->binding plan: PASS (synthetic only)")


if __name__ == "__main__":
    main(sys.argv[1])
