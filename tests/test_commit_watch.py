"""Offline, deterministic commit-subject evidence tests."""
import sys
from pathlib import Path
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"tools"))
import commit_watch as m

P="f"*40
A="a"*40
B="b"*40
def event(sha,msg="GPU: test wave64"):
    return {"sha":sha,"commit":{"message":msg,"committer":{"date":"2026-11-21T21:00:00Z"}}}

class WatchTests(unittest.TestCase):
    def test_subject_sanitization(self):
        self.assertNotIn("](",m.subject("safe](https://evil.invalid) | x\nsecond"))
    def test_bounded_delta(self):
        result=m.summarize("x/y",P,[event(A),event(B),event(P)])
        self.assertEqual(result["status"],"bounded-delta")
        self.assertEqual([x["sha"] for x in result["commits"]],[A,B])
        self.assertIn("not validated",m.render([result]))
    def test_recent_only_is_not_exhaustive(self):
        result=m.summarize("x/y",P,[event(A),event(B)])
        self.assertEqual(result["status"],"recent-only-not-exhaustive")
        self.assertIn("incomplete",m.render([result]))
    def test_no_delta(self):
        result=m.summarize("x/y",P,[event(P)])
        self.assertEqual(result["status"],"unchanged")
        self.assertFalse(result["commits"])
    def test_reject_non_github_name(self):
        with self.assertRaisesRegex(ValueError,"invalid repository"):
            m.summarize("https://bad.example",P,[])
    def test_unknown_is_not_success(self):
        result=m.summarize("x/y",P,[])
        self.assertEqual(result["status"],"unavailable")
        self.assertEqual(result["commits"],[])
if __name__=="__main__":unittest.main()
