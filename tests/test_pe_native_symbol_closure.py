"""Independent synthetic PE inventories; no execution or proprietary material."""
import copy
import json
import subprocess
import tempfile
import sys
import unittest
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.pe_native_symbol_closure import build_symbol_closure
from tools.pe_native_import_binding_plan import ValidationError


def pe(module=None, imports=(), delayed=(), exports=()):
    def syms(items):
        return [{"module": m, "name": s if isinstance(s,str) else None,
                 "ordinal": s if isinstance(s,int) else None} for m,s in items]
    return {"kind":"pe32plus-metadata", "machine":"AMD64", "executable_tested":False,
            "import_modules": sorted({m for m,s in imports}),
            "delay_import_modules": sorted({m for m,s in delayed}),
            "import_symbols":syms(imports), "delay_import_symbols":syms(delayed),
            "export_dll_name":module if exports else None,
            "exports":[{"name":n,"ordinal":i,"forwarder":f} for i,n,f in exports if n is not None],
            "export_addresses":[{"ordinal":i,"target_rva":4096+i*16,"forwarder":f} for i,n,f in exports]}


def package(delayed=False):
    request = [("A.dll","Forward")]
    return {"schema":"scarlett-native-pe-package/v1","entry":"client.exe","external_modules":[],
            "images":[{"name":"client.exe","pe":pe(imports=[] if delayed else request,delayed=request if delayed else [])},
                      {"name":"a.dll","pe":pe("a.dll",exports=[(1,"Forward","b.#2")])},
                      {"name":"b.dll","pe":pe("b.dll",imports=[("c.dll","Ping")],exports=[(2,None,None)])},
                      {"name":"c.dll","pe":pe("c.dll",exports=[(1,"Ping",None)])}]}


class ClosureTests(unittest.TestCase):
    def test_forwarded_provider_imports_are_checked(self):
        r=build_symbol_closure(package())
        self.assertTrue(r["all_symbols_present"])
        self.assertEqual(r["forwarder_added_images"],["b.dll","c.dll"])
        self.assertEqual(r["checked_count"],2)
        self.assertFalse(r["abi_verified"])
    def test_missing_forwarded_provider(self):
        p=package();p["images"].pop(2)
        r=build_symbol_closure(p)
        self.assertTrue(r["descriptor_only_all_names_resolved"])
        self.assertFalse(r["startup_symbols_present"])
        self.assertEqual(r["checks"][0]["status"],"missing-host-module")
    def test_forwarded_provider_has_missing_import(self):
        p=package();p["images"].pop()
        r=build_symbol_closure(p)
        self.assertFalse(r["all_symbols_present"])
        self.assertEqual(r["issue_count"],1)
    def test_delay_propagates(self):
        p=package(True);p["images"].pop()
        r=build_symbol_closure(p)
        self.assertTrue(r["startup_symbols_present"])
        self.assertFalse(r["all_symbols_present"])
        self.assertEqual(r["startup_issue_count"],0)
    def test_external_declaration_is_not_evidence(self):
        p=package();p["images"].pop(2);p["external_modules"]=["b.dll"]
        r=build_symbol_closure(p)
        self.assertEqual(r["checks"][0]["status"],"external-inventory-unavailable")
        self.assertFalse(r["all_symbols_present"])
    def test_missing_export(self):
        p=package();p["images"][3]["pe"]=pe("c.dll",exports=[(1,"Other",None)])
        self.assertFalse(build_symbol_closure(p)["all_symbols_present"])
    def test_cycle(self):
        p=package();p["images"][2]["pe"]=pe("b.dll",exports=[(2,None,"a.Forward")])
        r=build_symbol_closure(p)
        self.assertIn("forwarder-cycle",{x["status"] for x in r["checks"]})
        self.assertFalse(r["all_symbols_present"])
    def test_inconsistent_descriptor(self):
        p=package();p["images"][0]["pe"]["import_modules"]=[]
        with self.assertRaises(ValidationError):build_symbol_closure(p)
    def test_empty_descriptor(self):
        p=package();p["images"][0]["pe"]["import_symbols"]=[]
        self.assertFalse(build_symbol_closure(p)["all_symbols_present"])
    def test_deterministic_and_unchanged(self):
        p=package();before=copy.deepcopy(p);r=build_symbol_closure(p)
        self.assertEqual(p,before)
        p["images"].reverse();self.assertEqual(r,build_symbol_closure(p))
    def test_forwarder_promotes_optional_provider(self):
        p=package();p["images"][0]["pe"]=pe(imports=[("a.dll","Forward")],delayed=[("b.dll",2)])
        p["images"].pop()
        r=build_symbol_closure(p)
        self.assertFalse(r["startup_symbols_present"])
        self.assertEqual(r["startup_issue_count"],1)
        self.assertIn("b.dll",r["dependency_graph"]["reachable_images"])
    def test_cli_exit_codes(self):
        p=package(True);p["images"].pop()
        with tempfile.TemporaryDirectory() as tmp:
            f=Path(tmp)/"package.json";f.write_text(json.dumps(p),encoding="utf-8")
            tool=Path(__file__).resolve().parents[1]/"tools"/"pe_native_symbol_closure.py"
            cmd=[sys.executable,str(tool),"--package",str(f)]
            good=subprocess.run(cmd+["--require-startup"],capture_output=True,text=True)
            self.assertEqual(good.returncode,0,good.stderr)
            bad=subprocess.run(cmd+["--require-all"],capture_output=True,text=True)
            self.assertEqual(bad.returncode,3,bad.stderr)
            self.assertEqual(good.stdout,bad.stdout)
            f.write_text("{}",encoding="utf-8")
            self.assertEqual(subprocess.run(cmd,capture_output=True).returncode,2)
    def test_unreferenced_export_not_dependency(self):
        p=package();p["images"][1]["pe"]=pe("a.dll",exports=[(1,"Forward","b.#2"),(2,"Unused","absent.Missing")])
        self.assertTrue(build_symbol_closure(p)["all_symbols_present"])

if __name__=="__main__":unittest.main()
