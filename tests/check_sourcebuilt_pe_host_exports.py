"""Compile original Windows x64 host DLL fixtures and inspect their real PE EAT.

Optional: requires clang and lld-link. Does not load or execute Windows DLLs.
"""
from __future__ import annotations

import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.pe_native_import_binding_plan import build_plan
from tools.pe_native_export_verify import verify_plan


def checked(command):
    result = subprocess.run(command, text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError("\n".join(("command failed", str(command),
                                      result.stdout[-1000:], result.stderr[-1000:])))
    return result.stdout


def inspect(pe_intake, path):
    return json.loads(checked([pe_intake, str(path)]))


def main(exe):
    clang = shutil.which("clang") or shutil.which("clang.exe")
    linker = shutil.which("lld-link") or shutil.which("lld-link.exe")
    if not clang or not linker:
        print("SKIP: original Win64 cross-compiler fixture requires clang and lld-link")
        return 77
    fixtures = ROOT / "fixtures" / "native-pe-hosts"
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp)
        host_exports = {}
        for label in ("host_a", "host_b"):
            obj = out / f"{label}.obj"
            dll = out / f"{label}.dll"
            checked([clang, "-target", "x86_64-pc-windows-msvc", "-c",
                     str(fixtures / f"{label}.c"), "-o", str(obj)])
            checked([linker, "/dll", "/noentry", "/nodefaultlib", "/machine:x64",
                     "/def:" + str(fixtures / f"{label}.def"), "/out:" + str(dll), str(obj)])
            data = inspect(exe, dll)
            assert data["machine"] == "AMD64" and data["export_dll_name"] == f"{label}.dll", data
            assert data["executable_tested"] is False, data
            host_exports[label + ".dll"] = data
        a, b = host_exports["host_a.dll"], host_exports["host_b.dll"]
        a_by_name = {x["name"]: x for x in a["exports"]}
        assert a_by_name["NativeCall"] == {"name": "NativeCall", "ordinal": 5, "forwarder": None}
        assert a_by_name["ForwardNamed"]["forwarder"].lower() == "host_b.ping", a_by_name
        assert a_by_name["ForwardOrdinal"]["forwarder"].lower() == "host_b.#2", a_by_name
        assert b["exports"] == [{"name": "Ping", "ordinal": 1, "forwarder": None}], b
        assert {item["ordinal"] for item in b["export_addresses"]} == {1, 2}, b
        guest = {"kind": "pe32plus-metadata", "machine": "AMD64", "executable_tested": False,
                 "import_symbols": [{"module": "game.dll", "name": n, "ordinal": None,
                                     "lookup_rva": 0x1000 + 8 * i, "iat_rva": 0x2000 + 8 * i}
                                    for i, n in enumerate(("Direct", "Forward", "ByOrdinal"))],
                 "delay_import_symbols": []}
        bind = {"schema": "scarlett-native-import-bindings/v1", "bindings": [
            {"guest_module": "game.dll", "guest_name": guest_name,
             "action": "host-export", "host_module": "host_a.dll", "host_symbol": export}
            for guest_name, export in (("Direct", "NativeCall"),
                                       ("Forward", "ForwardNamed"),
                                       ("ByOrdinal", "ForwardOrdinal"))]}
        plan = build_plan(guest, bind)
        evidence = {"schema": "scarlett-host-pe-export-inventories/v1", "modules": [
            {"module": name, "pe": report} for name, report in sorted(host_exports.items())]}
        good = verify_plan(plan, evidence)
        assert good["all_host_exports_present"] and good["summary"]["present"] == 1, good
        assert good["summary"]["forwarded"] == 2 and not good["abi_verified"], good
        evidence["modules"] = evidence["modules"][:1]  # host_b missing
        missing = verify_plan(plan, evidence)
        assert missing["summary"]["missing"] == 2 and not missing["all_host_exports_present"], missing
    print("source-built original Windows x64 DLL EAT, named/ordinal forwarders: PASS (no DLL execution)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1]))
