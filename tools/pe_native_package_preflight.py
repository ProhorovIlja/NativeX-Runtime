"""Pure static Windows x64 multi-image dependency preflight.

Orders explicitly provided PE images by immediate imports, identifies missing
external providers, transitive delay imports and internal dependency cycles. This tool
never maps, loads, invokes, relinks, downloads or executes any PE code.
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
from tools.pe_native_import_binding_plan import _read_json, _ascii, ValidationError

SCHEMA = "scarlett-native-pe-package/v1"
MAX_IMAGES = 64
MAX_PROVIDERS = 128
MAX_IMPORT_MODULES = 4096
_SUFFIX = re.compile(r"[A-Za-z0-9_-][A-Za-z0-9_.-]*\.(?:dll|exe)\Z", re.I)


def _module(value, label):
    name = _ascii(value, label)
    if not _SUFFIX.fullmatch(name) or ".." in name or name.startswith("."):
        raise ValidationError(f"{label} must be a plain .dll/.exe filename")
    return name.casefold()


def _list_names(value, label):
    if not isinstance(value, list) or len(value) > MAX_IMPORT_MODULES:
        raise ValidationError(f"{label} must be a bounded list")
    return sorted({_module(x, label) for x in value})


def build_preflight(package):
    if not isinstance(package, dict) or package.get("schema") != SCHEMA:
        raise ValidationError("unsupported PE package schema")
    entry = _module(package.get("entry"), "entry")
    images = package.get("images")
    externals = package.get("external_modules")
    if not isinstance(images, list) or not 1 <= len(images) <= MAX_IMAGES:
        raise ValidationError("package images must be a nonempty bounded list")
    if not isinstance(externals, list) or len(externals) > MAX_PROVIDERS:
        raise ValidationError("external_modules must be a bounded list")
    declared_external = set()
    for value in externals:
        name = _module(value, "external module")
        if name in declared_external:
            raise ValidationError("duplicate external provider declaration")
        declared_external.add(name)
    mapped = {}
    for item in images:
        if not isinstance(item, dict):
            raise ValidationError("package image must be an object")
        name = _module(item.get("name"), "package image")
        if name in mapped or name in declared_external:
            raise ValidationError("package module identity is duplicated or shadowed")
        report = item.get("pe")
        if (not isinstance(report, dict) or report.get("kind") != "pe32plus-metadata"
                or report.get("machine") != "AMD64" or report.get("executable_tested") is not False):
            raise ValidationError("package image must have a read-only AMD64 PE inventory")
        immediate = _list_names(report.get("import_modules"), "import_modules")
        delayed = _list_names(report.get("delay_import_modules"), "delay_import_modules")
        mapped[name] = {"immediate": immediate, "delayed": delayed}
    if entry not in mapped:
        raise ValidationError("package entry image is not supplied")

    # First classify the startup-only graph; crossing a delay edge changes
    # the entire downstream dependency closure into optional-on-demand work.
    def closure(seeds, include_delayed):
        seen = set()
        pending = list(seeds)
        while pending:
            name = pending.pop()
            if name in seen:
                continue
            seen.add(name)
            deps = mapped[name]["immediate"]
            if include_delayed:
                deps = deps + mapped[name]["delayed"]
            pending.extend(dep for dep in deps if dep in mapped and dep not in seen)
        return seen

    reachable = closure([entry], False)
    all_reachable = closure([entry], True)
    required_missing = set()
    delayed_missing = set()
    required_external = set()
    delayed_external = set()
    edges = []
    for name in sorted(all_reachable):
        for kind in ("immediate", "delayed"):
            required = name in reachable and kind == "immediate"
            for dep in mapped[name][kind]:
                if dep in mapped:
                    edges.append({"from": name, "to": dep, "kind": kind + "-internal"})
                elif dep in declared_external:
                    (required_external if required else delayed_external).add(dep)
                else:
                    (required_missing if required else delayed_missing).add(dep)

    # Report cycles of *immediate* imports separately for startup and for
    # optional modules. Delay-only cycles are not initialization cycles.
    # This is a conservative graph policy, not a Windows loader simulation.
    def immediate_order(nodes, seeds):
        active = set()
        completed = set()
        order = []
        cycles = []
        def place(name, chain):
            if name in completed:
                return
            if name in active:
                start = chain.index(name)
                candidate = chain[start:] + [name]
                if candidate not in cycles:
                    cycles.append(candidate)
                return
            active.add(name)
            for dep in mapped[name]["immediate"]:
                if dep in nodes:
                    place(dep, chain + [name])
            active.remove(name)
            completed.add(name)
            order.append(name)
        for name in seeds:
            place(name, [])
        return order, sorted(cycles)

    order, cycles = immediate_order(reachable, [entry])
    optional = all_reachable - reachable
    _, optional_cycles = immediate_order(optional, sorted(optional))
    startup_names_resolved = not required_missing and not cycles
    return {"kind": "native-pc-pe-package-preflight/v1", "dry_run_only": True,
            "game_executed": False, "abi_verified": False,
            "entry": entry,
            "reachable_images": sorted(reachable),
            "unreached_images": sorted(set(mapped) - reachable),
            "all_reachable_images": sorted(all_reachable),
            "delayed_only_images": sorted(optional),
            "unreferenced_images": sorted(set(mapped) - all_reachable),
            "immediate_dependency_order": order if startup_names_resolved else [],
            "dependency_edges": sorted(edges, key=lambda x: (x["from"], x["to"], x["kind"])),
            "missing_immediate_modules": sorted(required_missing),
            "missing_delayed_modules": sorted(delayed_missing),
            "declared_external_immediate": sorted(required_external),
            "declared_external_delayed": sorted(delayed_external),
            "immediate_cycles": cycles,
            "delayed_reachable_immediate_cycles": optional_cycles,
            "immediate_names_resolved": startup_names_resolved,
            "all_names_resolved": startup_names_resolved and not delayed_missing and not optional_cycles,
            "provider_exports_verified": False}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package", required=True, help="explicit synthetic/authorized PE package inventory")
    parser.add_argument("--out", help="optional JSON output written atomically")
    parser.add_argument("--require-immediate", action="store_true", help="exit 3 when immediate dependencies unresolved")
    parser.add_argument("--require-all", action="store_true", help="exit 3 when any import dependency is unresolved")
    args = parser.parse_args(argv)
    try:
        report = build_preflight(_read_json(args.package))
        encoded = json.dumps(report, indent=2, sort_keys=True, ensure_ascii=True) + "\n"
        if args.out:
            destination = Path(args.out)
            fd, temporary = tempfile.mkstemp(prefix=".pe-preflight-", suffix=".tmp", dir=destination.parent)
            try:
                with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as stream:
                    stream.write(encoded)
                    stream.flush()
                    os.fsync(stream.fileno())
                os.replace(temporary, destination)
            finally:
                if os.path.exists(temporary):
                    os.unlink(temporary)
        else:
            sys.stdout.write(encoded)
        incomplete = (args.require_immediate and not report["immediate_names_resolved"]
                      or args.require_all and not report["all_names_resolved"])
        return 3 if incomplete else 0
    except (OSError, ValidationError) as error:
        print(f"PE package preflight: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
