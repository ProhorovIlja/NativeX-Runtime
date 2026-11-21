"""Offline GitHub-radar tests with synthetic metadata. No network."""
import sys
from pathlib import Path
import tempfile
import json
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import radar

class Radar(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.rows=radar.load(ROOT/'sources'/'upstreams.json')
    def test_registry(self):
        self.assertGreaterEqual(len(self.rows),10)
        self.assertEqual(len(self.rows),len({x['repository'].lower() for x in self.rows}))
    def test_fixture(self):
        row=self.rows[0]
        result=radar.scan(row,{row['repository']:{'head_sha':row['baseline_sha'],'license_metadata':'MIT'}})
        self.assertEqual(result['status'],'unchanged')
        self.assertIn('metadata',result['evidence'])
    def test_changed_is_not_compatibility_claim(self):
        row=self.rows[0]
        result=radar.scan(row,{row['repository']:{'head_sha':'f'*40 if row['baseline_sha']!='f'*40 else 'a'*40}})
        self.assertEqual(result['status'],'new-head-since-baseline')
        self.assertIn('not a verified',radar.report([row],[result],'today'))
    def test_missing_is_unknown(self):
        self.assertEqual(radar.scan(self.rows[0],{})['status'],'unavailable')
    def test_repo_input_validation(self):
        with tempfile.TemporaryDirectory() as temp:
            file=Path(temp)/'bad.json'
            file.write_text(json.dumps({'schema_version':1,'sources':[dict(self.rows[0],repository='https://bad.example')]}))
            with self.assertRaisesRegex(ValueError,'invalid repo'):radar.load(file)
            file.write_text(json.dumps({'schema_version':1,'sources':[dict(self.rows[0],ref='../main')]}))
            with self.assertRaisesRegex(ValueError,'invalid branch'):radar.load(file)
if __name__=='__main__':unittest.main()
