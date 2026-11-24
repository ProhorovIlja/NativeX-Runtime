"""Read-only, deterministic native Windows import-binding *plan* for PE metadata.

Does not load, patch, execute or resolve code; no game files are bundled.
The guest PE inventory is produced by this project's `pe_intake` inspector.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import sys
import tempfile

SCHEMA = "scarlett-native-import-bindings/v1"
MAX_FILE_BYTES = 2 * 1024 * 1024
MAX_SYMBOLS = 32768


class ValidationError(ValueError):
    pass


def _obj(value, label):
    if not isinstance(value, dict):
        raise ValidationError(f"{label} must be an object")
    return value


def _ascii(value, label, max_len=256):
    if not isinstance(value, str) or not value or len(value) > max_len:
        raise ValidationError(f"{label} must be a nonempty bounded string")
    if any(ord(ch) < 33 or ord(ch) > 126 for ch in value):
        raise ValidationError(f"{label} must contain printable ASCII without spaces")
    return value


def _ordinal(value, label):
    if type(value) is not int or not 1 <= value <= 65535:
        raise ValidationError(f"{label} must be an ordinal from 1 to 65535")
    return value


def _key(obj, module_field, name_field, ordinal_field, where):
    module = _ascii(obj.get(module_field), where + "." + module_field).lower()
    name = obj.get(name_field)
    ordinal = obj.get(ordinal_field)
    if (name is None) == (ordinal is None):
        raise ValidationError(f"{where} needs exactly one name or ordinal")
    if name is not None:
        return module, "name", _ascii(name, where + "." + name_field)
    return module, "ordinal", _ordinal(ordinal, where + "." + ordinal_field)


def _slot(value, label):
    if value is None:
        return None
    if type(value) is not int or not 0 <= value <= 0xFFFFFFFF:
        raise ValidationError(f"{label} is not a 32-bit RVA")
    return value


def build_plan(pe_report, binding_manifest):
    pe = _obj(pe_report, "PE report")
    registry = _obj(binding_manifest, "binding manifest")
    if pe.get("kind") != "pe32plus-metadata" or pe.get("machine") != "AMD64":
        raise ValidationError("expected a read-only AMD64 PE32+ inventory")
    if pe.get("executable_tested") is not False:
        raise ValidationError("PE inventory must explicitly declare no execution")
    if registry.get("schema") != SCHEMA:
        raise ValidationError("unsupported bindings schema")
    declarations = registry.get("bindings")
    if not isinstance(declarations, list) or len(declarations) > MAX_SYMBOLS:
        raise ValidationError("bindings must be a bounded array")
    mapping = {}
    for index, declaration in enumerate(declarations):
        item = _obj(declaration, f"binding[{index}]")
        key = _key(item, "guest_module", "guest_name", "guest_ordinal", f"binding[{index}]")
        if key in mapping:
            raise ValidationError(f"duplicate binding for {key!r}")
        action = item.get("action")
        if action == "host-export":
            target = {"module": _ascii(item.get("host_module"), "host_module"),
                      "symbol": _ascii(item.get("host_symbol"), "host_symbol")}
            if item.get("adapter_symbol") is not None:
                raise ValidationError("host-export cannot name a native adapter")
        elif action == "native-adapter":
            target = {"symbol": _ascii(item.get("adapter_symbol"), "adapter_symbol")}
            if item.get("host_module") is not None or item.get("host_symbol") is not None:
                raise ValidationError("native-adapter cannot name a host export")
        elif action == "unsupported":
            target = None
            if any(item.get(field) is not None for field in ("host_module", "host_symbol", "adapter_symbol")):
                raise ValidationError("unsupported action cannot have a target")
        else:
            raise ValidationError(f"binding[{index}] has unknown action")
        mapping[key] = (action, target)

    entries = []
    counts = {"total": 0, "direct": 0, "adapters": 0, "unsupported": 0, "unresolved": 0,
              "immediate": 0, "delay_load": 0}
    for key_name, is_delayed in (("import_symbols", False), ("delay_import_symbols", True)):
        source = pe.get(key_name)
        if not isinstance(source, list) or len(source) > MAX_SYMBOLS or counts["total"] + len(source) > MAX_SYMBOLS:
            raise ValidationError(f"{key_name} must be a bounded array")
        for index, raw in enumerate(source):
            item = _obj(raw, f"{key_name}[{index}]")
            key = _key(item, "module", "name", "ordinal", f"{key_name}[{index}]")
            action, target = mapping.get(key, ("unresolved", None))
            record = {"guest_module": item["module"], "guest_name": item.get("name"),
                      "guest_ordinal": item.get("ordinal"),
                      "delay_load": is_delayed, "action": action, "target": target,
                      "lookup_rva": _slot(item.get("lookup_rva"), "lookup_rva"),
                      "iat_rva": _slot(item.get("iat_rva"), "iat_rva")}
            if record["iat_rva"] is not None and record["iat_rva"] & 7:
                raise ValidationError("unaligned 64-bit IAT slot")
            if record["lookup_rva"] is not None and record["lookup_rva"] & 7:
                raise ValidationError("unaligned 64-bit lookup slot")
            entries.append(record)
            counts["total"] += 1
            counts["delay_load" if is_delayed else "immediate"] += 1
            counts[{"host-export": "direct", "native-adapter": "adapters",
                    "unsupported": "unsupported", "unresolved": "unresolved"}[action]] += 1
    return {"kind": "native-pc-import-binding-plan/v1", "dry_run_only": True,
            "game_executed": False, "all_resolved": counts["unsupported"] == 0 and counts["unresolved"] == 0,
            "summary": counts, "bindings": entries}


def _read_json(path):
    file = Path(path)
    if file.stat().st_size > MAX_FILE_BYTES:
        raise ValidationError(f"{file.name} exceeds {MAX_FILE_BYTES} byte limit")
    with file.open("rb") as handle:
        data = handle.read(MAX_FILE_BYTES + 1)
    if len(data) > MAX_FILE_BYTES:
        raise ValidationError("JSON input grew beyond its read limit")
    try:
        return json.loads(data)
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ValidationError(f"invalid JSON input: {exc}") from exc


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pe-json", required=True, help="read-only JSON from pe_intake")
    parser.add_argument("--bindings", required=True, help="explicit native binding manifest JSON")
    parser.add_argument("--out", help="optional output JSON; written atomically on success")
    parser.add_argument("--require-complete", action="store_true",
                        help="exit 3 when an import is unsupported/unresolved")
    args = parser.parse_args(argv)
    try:
        result = build_plan(_read_json(args.pe_json), _read_json(args.bindings))
        encoded = json.dumps(result, indent=2, sort_keys=True, ensure_ascii=True) + "\n"
        if args.out:
            dest = Path(args.out)
            descriptor, temporary = tempfile.mkstemp(prefix=".native-import-", suffix=".tmp", dir=dest.parent)
            try:
                with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as handle:
                    handle.write(encoded)
                    handle.flush()
                    os.fsync(handle.fileno())
                os.replace(temporary, dest)
            finally:
                if os.path.exists(temporary):
                    os.unlink(temporary)
        else:
            sys.stdout.write(encoded)
        return 3 if args.require_complete and not result["all_resolved"] else 0
    except (OSError, ValidationError) as exc:
        print(f"native import plan: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
