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
from tools.pe_native_export_verify import verify_plan
from tools.pe_native_package_preflight import build_preflight
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
        # A *second*, source-built read-only stage checks that explicitly
        # named host PE exports really occur in provided PE inventories.
        # Here both "DLLs" are independent original synthetic fixtures;
        # no Windows system DLL or commercial title is loaded.
        host_path = Path(directory) / "synthetic export host.dll"
        fixture(host_path)
        host_report = json.loads(subprocess.run([executable, str(host_path)],
            capture_output=True, text=True, check=True).stdout)
        assert host_report["export_dll_name"] == "synthetic.dll"
        assert {e["ordinal"] for e in host_report["export_addresses"]} == {5,6,7}
        synthetic = json.loads(json.dumps(manifest))
        synthetic["bindings"][0].update(host_module="synthetic.dll", host_symbol="ScarlettAdd")
        synthetic_plan = build_plan(inventory, synthetic)
        sources = {"schema": "scarlett-host-pe-export-inventories/v1", "modules": [
            {"module": "synthetic.dll", "pe": host_report}]}
        check = verify_plan(synthetic_plan, sources)
        assert check["checks"][0]["status"] == "present-direct", check
        assert check["checks"][1]["status"] == "adapter-not-verified", check
        assert not check["abi_verified"] and not check["game_executed"]
        synthetic["bindings"][0]["host_symbol"] = "AliasForward"
        forward_plan = build_plan(inventory, synthetic)
        missing = verify_plan(forward_plan, sources)
        assert missing["checks"][0]["status"] == "missing-host-module", missing

        # Create a *synthetic* second host DLL with the forwarded symbol
        # KERNEL32.Sleep. Its internal export DLL name matches the assigned
        # module identity; the address is data in a crafted synthetic image.
        second_path = Path(directory) / "synthetic kernel32 host.dll"
        second = bytearray(host_path.read_bytes())
        second[0x3A0:0x3B4] = b"KERNEL32.dll\x00".ljust(0x14, b"\x00")
        second[0x3B4:0x3C4] = b"Sleep\x00".ljust(0x10, b"\x00")
        second_path.write_bytes(second)
        second_report = json.loads(subprocess.run([executable, str(second_path)],
            capture_output=True, text=True, check=True).stdout)
        assert second_report["export_dll_name"] == "KERNEL32.dll"
        sources["modules"].append({"module": "kernel32.dll", "pe": second_report})
        resolved = verify_plan(forward_plan, sources)
        assert resolved["checks"][0]["status"] == "present-forwarded", resolved
        assert [x["module"] for x in resolved["checks"][0]["chain"]] == [
            "synthetic.dll", "kernel32.dll"], resolved
        # Synthetic 2-image PE package startup dependency preflight.
        # A second copy of our fixture is changed to import `helper.dll`
        # instead of KERNEL32.dll; there is no mapping or guest execution.
        title_path = Path(directory) / "native synthetic title.exe"
        title_bytes = bytearray(path.read_bytes())
        title_bytes[0x240:0x240+13] = b"helper.dll\x00".ljust(13, b"\x00")
        title_path.write_bytes(title_bytes)
        title = json.loads(subprocess.run([executable, str(title_path)],
            capture_output=True, text=True, check=True).stdout)
        assert title["import_modules"] == ["helper.dll"], title
        package = {"schema": "scarlett-native-pe-package/v1",
                   "entry": "title.exe", "images": [
                       {"name": "title.exe", "pe": title},
                       {"name": "helper.dll", "pe": inventory}],
                   "external_modules": ["KERNEL32.dll"]}
        preflight = build_preflight(package)
        assert preflight["immediate_dependency_order"] == ["helper.dll", "title.exe"], preflight
        assert preflight["immediate_names_resolved"] and not preflight["all_names_resolved"]
        assert preflight["missing_delayed_modules"] == ["user32.dll"]
        package["images"].pop()
        incomplete = build_preflight(package)
        assert not incomplete["immediate_names_resolved"]
        assert incomplete["missing_immediate_modules"] == ["helper.dll"]
        # The original input PE and the synthetic host have not been patched.
        assert path.exists()  # Inventory and binding plan never alter input.
        assert path.read_bytes()[:2] == b"MZ"
    print("native Win64 PE import->binding->export+package graph: PASS (synthetic only)")


if __name__ == "__main__":
    main(sys.argv[1])
