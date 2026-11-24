"""Read-only symbol dependency closure for explicit, independently built PE inventories.

Identity imports only; no API-set mapping, adapter selection, loading or execution.
An external module declaration alone is never export evidence.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.pe_native_import_binding_plan import build_plan, _read_json, ValidationError
from tools.pe_native_export_verify import _parse_host_inventory, _find_export
from tools.pe_native_package_preflight import build_preflight, _module

MAX_PACKAGE_SYMBOLS = 32768
PRESENT = {"present-direct", "present-forwarded"}


def build_symbol_closure(package):
    baseline = build_preflight(package)
    images = {_module(item["name"], "image"): item["pe"] for item in package["images"]}
    externals = {_module(x, "external") for x in package["external_modules"]}
    # The existing verifier validates EAT/name consistency for all supplied DLLs.
    providers = []
    empty = {}
    for name, pe in images.items():
        if not name.endswith(".dll"):
            continue
        if pe.get("export_dll_name") is None and pe.get("exports") == [] and pe.get("export_addresses") == []:
            empty[name] = {"name_map": {}, "ordinal_map": {}}
        else:
            providers.append({"module": name, "pe": pe})
    hosts = _parse_host_inventory({"schema": "scarlett-host-pe-export-inventories/v1", "modules": providers})
    hosts.update(empty)
    augmented = []
    checks = {}
    total = 0
    for owner, pe in sorted(images.items()):
        # Reuse established import identity/slot validation, but never generate
        # or apply binding declarations. Name and ordinal remain distinct.
        plan = build_plan(pe, {"schema": "scarlett-native-import-bindings/v1", "bindings": []})
        total += len(plan["bindings"])
        if total > MAX_PACKAGE_SYMBOLS:
            raise ValidationError("package symbol limit exceeded")
        direct = set(_module(x, "import") for x in pe["import_modules"])
        delayed = set(_module(x, "delay import") for x in pe["delay_import_modules"])
        declared = [direct, delayed]
        graph = [set(direct), set(delayed)]
        observed = [set(), set()]
        rows = []
        for binding in plan["bindings"]:
            phase = int(binding["delay_load"])
            module = _module(binding["guest_module"], "symbol import")
            if module not in declared[phase]:
                raise ValidationError("symbol module absent from corresponding import descriptor inventory")
            observed[phase].add(module)
            name, ordinal = binding["guest_name"], binding["guest_ordinal"]
            status, chain = _find_export(hosts, module, "name" if name is not None else "ordinal", name if name is not None else ordinal)
            if status == "missing-host-module" and chain[-1]["module"] in externals:
                status = "external-inventory-unavailable"
            # Only actually referenced forwarders contribute dependencies.
            # Retain reached providers even if the final symbol is missing.
            graph[phase].update(step["module"] for step in chain[1:] if step["module"] != owner)
            rows.append({"module": module, "name": name, "ordinal": ordinal,
                         "delay_load": bool(phase), "status": status, "chain": chain})
        for phase in (0, 1):
            for module in sorted(declared[phase] - observed[phase]):
                rows.append({"module": module, "name": None, "ordinal": None,
                             "delay_load": bool(phase), "status": "descriptor-without-symbol-evidence", "chain": []})
        total += len(rows) - len(plan["bindings"])
        if total > MAX_PACKAGE_SYMBOLS:
            raise ValidationError("package evidence limit exceeded")
        checks[owner] = rows
        augmented.append({"name": owner, "pe": {**pe, "import_modules": sorted(graph[0]), "delay_import_modules": sorted(graph[1])}})
    graph = build_preflight({**package, "images": augmented})
    startup, all_reached = set(graph["reachable_images"]), set(graph["all_reachable_images"])
    results = []
    for owner in sorted(all_reached):
        for row in checks[owner]:
            results.append({"image": owner, "required_at_startup": owner in startup and not row["delay_load"], **row})
    issues = [x for x in results if x["status"] not in PRESENT]
    startup_issues = [x for x in issues if x["required_at_startup"]]
    return {"kind": "nativex-pe-symbol-closure/v1", "dry_run_only": True,
            "abi_verified": False, "execution_verified": False,
            "entry": graph["entry"], "descriptor_only_all_names_resolved": baseline["all_names_resolved"],
            "forwarder_added_images": sorted(all_reached - set(baseline["all_reachable_images"])),
            "startup_symbols_present": graph["immediate_names_resolved"] and not startup_issues,
            "all_symbols_present": graph["all_names_resolved"] and not issues,
            "checked_count": len(results), "issue_count": len(issues), "startup_issue_count": len(startup_issues),
            "dependency_graph": graph, "checks": results}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", required=True)
    parser.add_argument("--require-startup", action="store_true")
    parser.add_argument("--require-all", action="store_true")
    args = parser.parse_args(argv)
    try:
        report = build_symbol_closure(_read_json(args.package))
        print(json.dumps(report, sort_keys=True, indent=2, ensure_ascii=True))
        return 3 if ((args.require_startup and not report["startup_symbols_present"]) or
                     (args.require_all and not report["all_symbols_present"])) else 0
    except (OSError, ValidationError) as exc:
        print(f"PE symbol closure: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
