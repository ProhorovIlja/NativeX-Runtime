"""Offline unreviewed-repository discovery tests."""
import sys
from pathlib import Path
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import discover

class Discovery(unittest.TestCase):
    def test_invalid_links(self):
        self.assertTrue(discover.valid('project/example'))
        self.assertFalse(discover.valid('https://bad.example/a'))
        self.assertFalse(discover.valid('x/../y'))
    def test_deduplicate(self):
        result=discover.normalize([{'full_name':'a/b','description':'safe','fork':True},
                                   {'full_name':'a/b','description':'duplicate'}],{'a/b'})
        self.assertEqual(len(result),1)
        self.assertEqual(result[0]['review'],'known')
        self.assertTrue(result[0]['fork'])
    def test_nontrusted_markdown_sanitized(self):
        result=discover.normalize([{'full_name':'fresh/project','description':'[evil](https://example.com)'}],set())
        output=discover.report(result,'today')
        self.assertIn('needs-review',output)
        self.assertNotIn('[evil]',output)
if __name__=='__main__':unittest.main()
