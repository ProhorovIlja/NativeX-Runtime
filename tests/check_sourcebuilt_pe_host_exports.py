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
from tools.pe_native_package_preflight import build_preflight
from tools.pe_native_symbol_closure import build_symbol_closure


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
                     "/def:" + str(fixtures / f"{label}.def"), "/out:" + str(dll),
                     "/implib:" + str(out / f"{label}.lib"), str(obj)])
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
        # Link an independent bridge DLL and two actual PE executables against
        # the generated import libraries. No hand-authored import inventories.
        def compile_object(source, label, extra=()):
            obj = out / (label + ".obj")
            checked([clang, "-target", "x86_64-pc-windows-msvc", "-c", *extra,
                     str(fixtures / source), "-o", str(obj)])
            return obj

        bridge = out / "host_bridge.dll"
        checked([linker, "/dll", "/noentry", "/nodefaultlib", "/machine:x64",
                 "/out:" + str(bridge), "/implib:" + str(out / "host_bridge.lib"),
                 str(compile_object("host_bridge.c", "bridge")), str(out / "host_b.lib")])
        host_exports["host_bridge.dll"] = inspect(exe, bridge)
        assert host_exports["host_bridge.dll"]["import_modules"] == ["host_b.dll"]
        assert host_exports["host_bridge.dll"]["exports"][0]["name"] == "BridgeCall"

        def identities(report):
            return {(x["module"].lower(), x["name"], x["ordinal"])
                    for x in report["import_symbols"]}

        def verify_actual(report, providers):
            # Identity bindings only for the independently linked test DLLs.
            # No implicit production mappings, live addresses, or execution.
            declarations = [{"guest_module": m, "guest_name": name,
                             "guest_ordinal": ordinal, "action": "host-export",
                             "host_module": m,
                             "host_symbol": name if name is not None else f"#{ordinal}"}
                            for m, name, ordinal in sorted(identities(report),
                                                           key=lambda x: (x[0], str(x[1]), str(x[2])))]
            plan = build_plan(report, {"schema": "scarlett-native-import-bindings/v1",
                                       "bindings": declarations})
            assert plan["all_resolved"] and all(x["iat_rva"] is not None for x in plan["bindings"])
            return verify_plan(plan, {"schema": "scarlett-host-pe-export-inventories/v1",
                                      "modules": [{"module": n, "pe": r} for n, r in providers.items()]})

        def package_for(label, client, providers):
            return {"schema": "scarlett-native-pe-package/v1", "entry": label,
                    "images": [{"name": label, "pe": client}] +
                              [{"name": n, "pe": r} for n, r in providers.items()],
                    "external_modules": []}

        for forward_only in (False, True):
            label = "forward_only" if forward_only else "client"
            client = out / (label + ".exe")
            obj = compile_object("client.c", label, ("-DFORWARD_ONLY",) if forward_only else ())
            libs = [out / "host_a.lib"] if forward_only else [out / (n + ".lib") for n in
                                                             ("host_a", "host_b", "host_bridge")]
            checked([linker, "/entry:entry", "/subsystem:console", "/nodefaultlib", "/machine:x64",
                     "/out:" + str(client), str(obj), *map(str, libs)])
            before = client.read_bytes()
            report = inspect(exe, client)
            expected = {("host_a.dll", "ForwardNamed", None), ("host_a.dll", "ForwardOrdinal", None)}
            if not forward_only:
                expected |= {("host_a.dll", "NativeCall", None), ("host_b.dll", None, 2),
                             ("host_bridge.dll", "BridgeCall", None)}
            assert identities(report) == expected, report["import_symbols"]
            assert not report["delay_import_symbols"] and report["executable_tested"] is False
            result = verify_actual(report, host_exports)
            assert result["all_host_exports_present"] and result["summary"]["forwarded"] == 2, result
            assert result["summary"]["present"] == (0 if forward_only else 3), result
            assert not result["abi_verified"] and not result["game_executed"]
            pkg = build_preflight(package_for(client.name, report, host_exports))
            assert pkg["all_names_resolved"] and not pkg["provider_exports_verified"], pkg
            if not forward_only:
                order = pkg["immediate_dependency_order"]
                assert order.index("host_b.dll") < order.index("host_bridge.dll") < order.index(client.name)
                bridge_result = verify_actual(host_exports["host_bridge.dll"], host_exports)
                assert bridge_result["summary"]["present"] == 1 and bridge_result["all_host_exports_present"]
            absent = {n: r for n, r in host_exports.items() if n != "host_b.dll"}
            missing = verify_actual(report, absent)
            assert missing["summary"]["missing"] == (2 if forward_only else 3), missing
            missing_pkg = build_preflight(package_for(client.name, report, absent))
            # A module-only graph cannot see export forwarders. Confirm that
            # the symbol-level check catches precisely this gap.
            assert missing_pkg["all_names_resolved"] is forward_only, missing_pkg
            full = build_symbol_closure(package_for(client.name, report, host_exports))
            assert full["all_symbols_present"] and not full["execution_verified"], full
            if forward_only:
                assert full["forwarder_added_images"] == ["host_b.dll"], full
            incomplete = build_symbol_closure(package_for(client.name, report, absent))
            assert not incomplete["startup_symbols_present"], incomplete
            assert incomplete["descriptor_only_all_names_resolved"] is forward_only, incomplete
            assert client.read_bytes() == before
    print("source-built Win64 EXE/DLL import, ordinal, forwarded export and dependency checks: PASS (no execution)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1]))
