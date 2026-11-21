"""Offline native input upstream regression snapshot; no GitHub API calls."""
import json
import sys
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"tools"))
import radar

class NativeInputWatch(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows=radar.load(ROOT/"sources"/"input_upstreams.json")
        cls.fixture=json.loads((ROOT/"tests"/"fixtures"/"input_public_metadata.json").read_text(
            encoding="utf-8"))["repositories"]

    def test_four_pinned_input_sources(self):
        self.assertEqual(len(self.rows),4)
        self.assertEqual(len({r["repository"] for r in self.rows}),4)
        self.assertTrue(all(r["initial_baseline_sha"]==r["baseline_sha"] for r in self.rows))

    def test_metadata_unchanged(self):
        results=[radar.scan(r,self.fixture) for r in self.rows]
        self.assertTrue(all(r["status"]=="unchanged" for r in results))
        self.assertIn("not a verified bug fix",radar.report(self.rows,results,"fixture"))

    def test_new_head_not_proof_of_support(self):
        row=self.rows[0]
        changed=dict(self.fixture)
        changed[row["repository"]]=dict(changed[row["repository"]],
                                        head_sha="f"*40)
        self.assertEqual(radar.scan(row,changed)["status"],"new-head-since-baseline")
        self.assertIn("not a verified bug fix",radar.report([row],[radar.scan(row,changed)],"fixture"))

if __name__=="__main__":
    unittest.main()
