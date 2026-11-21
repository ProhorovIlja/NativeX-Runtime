"""Network-free official SDK release metadata tests."""
import json
import sys
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import sdk_radar

class TestSdkReleases(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows = sdk_radar.load(ROOT / "sources" / "sdk_releases.json")
        cls.fixtures = json.loads((ROOT / "tests" / "fixtures" /
            "sdk_release_metadata.json").read_text(encoding="utf-8"))["repositories"]

    def test_registered_official_sources(self):
        self.assertEqual(len(self.rows), 6)
        self.assertEqual(len({x["repository"].lower() for x in self.rows}), 6)

    def test_valid_releases(self):
        results = [sdk_radar.scan(r, self.fixtures) for r in self.rows]
        self.assertEqual(sum(r["status"] == "released" for r in results), 5)
        self.assertEqual(sum(r["status"] == "no-release" for r in results), 1)
        self.assertIn("not evidence", sdk_radar.report(self.rows, results, "today"))

    def test_missing_release_distinct_from_failure(self):
        row = next(r for r in self.rows if r["repository"] == "microsoft/DirectStorage")
        self.assertEqual(sdk_radar.scan(row, self.fixtures)["status"], "no-release")
        self.assertEqual(sdk_radar.scan(row, {})["status"], "unavailable")

    def test_malformed_metadata_unavailable(self):
        row = self.rows[0]
        invalid = dict(self.fixtures)
        invalid[row["repository"]] = {
            "tag_name": "v1", "published_at": "2026-11-22T00:00:00Z",
            "html_url": "https://example.com/unsafe"}
        result = sdk_radar.scan(row, invalid)
        self.assertEqual(result["status"], "unavailable")
        self.assertEqual(result["error"], "ValueError")

    def test_release_date_must_have_timezone(self):
        row = self.rows[0]
        invalid = dict(self.fixtures)
        invalid[row["repository"]] = dict(self.fixtures[row["repository"]],
            published_at="2026-11-22T00:00:00")
        self.assertEqual(sdk_radar.scan(row, invalid)["status"], "unavailable")

    def test_invalid_registry_names_rejected(self):
        row = self.rows[0]
        with tempfile.TemporaryDirectory() as directory:
            file = Path(directory) / "bad.json"
            payload = {"schema_version": 1, "sources": [dict(row, repository="../bad")]}
            file.write_text(json.dumps(payload))
            with self.assertRaisesRegex(ValueError, "invalid release repository"):
                sdk_radar.load(file)
            payload["sources"] = [row, row]
            file.write_text(json.dumps(payload))
            with self.assertRaisesRegex(ValueError, "duplicate SDK repository"):
                sdk_radar.load(file)

    def test_no_network_in_fixture_mode(self):
        with patch.object(sdk_radar, "fetch_latest", side_effect=AssertionError("network used")):
            for row in self.rows:
                self.assertNotEqual(
                    sdk_radar.scan(row, self.fixtures)["status"], "unavailable")

if __name__ == "__main__":
    unittest.main()
