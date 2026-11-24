"""Offline check of explicit native host PE exports for a Windows PE binding plan.

Works on read-only JSON inventories from pe_intake. Never loads DLLs,
resolves live pointers, rewrites executable imports or executes guest code.
Existence in a supplied inventory is not proof of the host ABI or runtime.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import sys
import tempfile

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.pe_native_import_binding_plan import _read_json, _ascii, _ordinal, ValidationError

SCHEMA = "scarlett-host-pe-export-inventories/v1"
MAX_MODULES = 64
MAX_EAT = 8192
MAX_NAMES = 4096
MAX_IMPORTS = 32768
MAX_FORWARD_DEPTH = 16
_MODULE_NAME = re.compile(r"[A-Za-z0-9_-][A-Za-z0-9_.-]*\.dll\Z", re.I)


def _module(value, label):
    name = _ascii(value, label)
    if not _MODULE_NAME.fullmatch(name) or ".." in name or name.startswith("."):
        raise ValidationError(f"{label} must be a plain DLL basename")
    return name.casefold()


def _uint(value, label, upper=0xFFFFFFFF):
    if type(value) is not int or not 0 <= value <= upper:
        raise ValidationError(f"{label} must be a bounded integer")
    return value


def _forwarder(text):
    if not isinstance(text, str) or not 3 <= len(text) <= 256 or "." not in text:
        return None
    module, symbol = text.rsplit(".", 1)
    if not module.lower().endswith(".dll"):
        module += ".dll"
    try:
        module = _module(module, "forwarded module")
        if symbol.startswith("#"):
            if not re.fullmatch(r"#[0-9]+", symbol):
                return None
            return module, "ordinal", _ordinal(int(symbol[1:]), "forwarded ordinal")
        return module, "name", _ascii(symbol, "forwarded symbol")
    except ValidationError:
        return None


def _parse_host_inventory(host_inventory):
    if not isinstance(host_inventory, dict) or host_inventory.get("schema") != SCHEMA:
        raise ValidationError("unsupported host inventory schema")
    modules = host_inventory.get("modules")
    if not isinstance(modules, list) or len(modules) > MAX_MODULES:
        raise ValidationError("host modules must be a bounded list")
    results = {}
    for mod_idx, record in enumerate(modules):
        if not isinstance(record, dict):
            raise ValidationError("module must be an object")
        name = _module(record.get("module"), f"modules[{mod_idx}].module")
        if name in results:
            raise ValidationError("duplicate host module identity")
        pe = record.get("pe")
        if (not isinstance(pe, dict) or pe.get("kind") != "pe32plus-metadata"
                or pe.get("machine") != "AMD64" or pe.get("executable_tested") is not False):
            raise ValidationError("host must be a read-only AMD64 PE32+ inventory")
        if _module(pe.get("export_dll_name"), "PE export DLL name") != name:
            raise ValidationError("host DLL export name differs from declared module")
        addresses = pe.get("export_addresses")
        names = pe.get("exports")
        if (not isinstance(addresses, list) or len(addresses) > MAX_EAT
                or not isinstance(names, list) or len(names) > MAX_NAMES):
            raise ValidationError("missing or unbounded PE export inventory")
        ordinal_map = {}
        for n, address in enumerate(addresses):
            if not isinstance(address, dict):
                raise ValidationError("EAT entry must be an object")
            ordinal = _uint(address.get("ordinal"), "export ordinal")
            target_rva = _uint(address.get("target_rva"), "export target RVA")
            if target_rva == 0 or ordinal in ordinal_map:
                raise ValidationError("empty or duplicate PE EAT entry")
            forward = address.get("forwarder")
            if forward is not None and not isinstance(forward, str):
                raise ValidationError("invalid export forwarder type")
            ordinal_map[ordinal] = {"rva": target_rva, "forwarder": forward}
        name_map = {}
        for n, named in enumerate(names):
            if not isinstance(named, dict):
                raise ValidationError("export name must be an object")
            symbol = _ascii(named.get("name"), "export name")
            ordinal = _uint(named.get("ordinal"), "named export ordinal")
            forward = named.get("forwarder")
            if (symbol in name_map or ordinal not in ordinal_map
                    or forward != ordinal_map[ordinal]["forwarder"]):
                raise ValidationError("named export does not match an EAT entry")
            name_map[symbol] = ordinal
        results[name] = {"name_map": name_map, "ordinal_map": ordinal_map}
    return results


def _find_export(modules, module, selector, symbol):
    visited = set()
    chain = []
    for _ in range(MAX_FORWARD_DEPTH + 1):
        key = (module, selector, symbol)
        if key in visited:
            return "forwarder-cycle", chain
        visited.add(key)
        chain.append({"module": module, "name": symbol if selector == "name" else None,
                      "ordinal": symbol if selector == "ordinal" else None})
        inventory = modules.get(module)
        if inventory is None:
            return "missing-host-module", chain
        if selector == "name":
            ordinal = inventory["name_map"].get(symbol)
            if ordinal is None:
                return "missing-host-export", chain
        else:
            ordinal = symbol
        target = inventory["ordinal_map"].get(ordinal)
        if target is None:
            return "missing-host-eat-entry", chain
        if target["forwarder"] is None:
            return ("present-direct" if len(chain) == 1 else "present-forwarded"), chain
        nxt = _forwarder(target["forwarder"])
        if nxt is None:
            return "malformed-host-forwarder", chain
        module, selector, symbol = nxt
    return "forwarder-depth-limit", chain


def verify_plan(plan, host_inventory):
    if (not isinstance(plan, dict) or plan.get("kind") != "native-pc-import-binding-plan/v1"
            or plan.get("dry_run_only") is not True or plan.get("game_executed") is not False):
        raise ValidationError("expected an unexecuted native import binding plan")
    bindings = plan.get("bindings")
    if not isinstance(bindings, list) or len(bindings) > MAX_IMPORTS:
        raise ValidationError("plan bindings must be a bounded list")
    hosts = _parse_host_inventory(host_inventory)
    summary = {"total": 0, "host_export": 0, "present": 0, "forwarded": 0,
               "missing": 0, "declared_adapters": 0, "unsupported": 0, "unresolved": 0}
    checked = []
    for index, binding in enumerate(bindings):
        if not isinstance(binding, dict):
            raise ValidationError("binding must be an object")
        action = binding.get("action")
        target = binding.get("target")
        guest = {"guest_module": _ascii(binding.get("guest_module"), "guest module"),
                 "guest_name": binding.get("guest_name"),
                 "guest_ordinal": binding.get("guest_ordinal"),
                 "delay_load": binding.get("delay_load")}
        if type(guest["delay_load"]) is not bool:
            raise ValidationError("delay_load must be boolean")
        if (guest["guest_name"] is None) == (guest["guest_ordinal"] is None):
            raise ValidationError("guest binding must identify exactly one symbol")
        if guest["guest_name"] is not None:
            _ascii(guest["guest_name"], "guest symbol")
        else:
            _ordinal(guest["guest_ordinal"], "guest ordinal")
        if action == "host-export":
            if not isinstance(target, dict):
                raise ValidationError("host binding target must be an object")
            module = _module(target.get("module"), "host module")
            symbol = _ascii(target.get("symbol"), "host symbol")
            if symbol.startswith("#"):
                if not re.fullmatch(r"#[0-9]+", symbol):
                    raise ValidationError("invalid host ordinal spelling")
                selector, value = "ordinal", _ordinal(int(symbol[1:]), "host ordinal")
            else:
                selector, value = "name", symbol
            status, chain = _find_export(hosts, module, selector, value)
            summary["host_export"] += 1
            if status == "present-direct":
                summary["present"] += 1
            elif status == "present-forwarded":
                summary["forwarded"] += 1
            else:
                summary["missing"] += 1
        elif action in ("native-adapter", "unsupported", "unresolved"):
            status, chain = {"native-adapter": "adapter-not-verified",
                             "unsupported": "unsupported", "unresolved": "unresolved"}[action], []
            summary[{"native-adapter": "declared_adapters",
                     "unsupported": "unsupported", "unresolved": "unresolved"}[action]] += 1
        else:
            raise ValidationError("unknown binding action")
        checked.append({**guest, "status": status, "chain": chain})
        summary["total"] += 1
    return {"kind": "native-pc-host-export-inventory-check/v1", "dry_run_only": True,
            "game_executed": False, "abi_verified": False,
            "all_host_exports_present": summary["host_export"] > 0 and summary["missing"] == 0,
            "summary": summary, "checks": checked}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plan", required=True, help="JSON produced by pe_native_import_binding_plan")
    parser.add_argument("--host-inventories", required=True, help="explicit PE export inventory mapping")
    parser.add_argument("--out", help="optional atomic JSON report")
    parser.add_argument("--require-present", action="store_true", help="exit 3 if any host export missing")
    args = parser.parse_args(argv)
    try:
        report = verify_plan(_read_json(args.plan), _read_json(args.host_inventories))
        encoded = json.dumps(report, sort_keys=True, indent=2, ensure_ascii=True) + "\n"
        if args.out:
            destination = Path(args.out)
            descriptor, temporary = tempfile.mkstemp(prefix=".host-exports-", suffix=".tmp", dir=destination.parent)
            try:
                with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as f:
                    f.write(encoded)
                    f.flush()
                    os.fsync(f.fileno())
                os.replace(temporary, destination)
            finally:
                if os.path.exists(temporary):
                    os.unlink(temporary)
        else:
            sys.stdout.write(encoded)
        return 3 if args.require_present and not report["all_host_exports_present"] else 0
    except (OSError, ValidationError) as error:
        print(f"host export inventory check: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
