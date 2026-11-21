#!/usr/bin/env python3
"""Read only official GitHub SDK *release metadata*; no assets or source binaries."""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import sys
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen

REPOSITORY = re.compile(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+\\Z")
TAG = re.compile(r"[A-Za-z0-9_.+/-]{1,128}\\Z")
CATEGORIES = {"upscaling-frame-generation", "upscaling-frame-generation-latency",
              "upscaling-frame-generation-latency-neural-interface",
              "linux-compat-d3d12-to-vulkan", "windows-asset-io",
              "linux-windows-compatibility"}

def load(path: Path) -> list[dict]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(payload, dict) or payload.get("schema_version") != 1:
        raise ValueError("invalid SDK registry schema")
    rows = payload.get("sources")
    if not isinstance(rows, list) or not rows:
        raise ValueError("SDK release registry is empty")
    seen: set[str] = set()
    for row in rows:
        if not isinstance(row, dict):
            raise ValueError("SDK registry row is not a mapping")
        name = row.get("repository")
        if not isinstance(name, str) or not REPOSITORY.fullmatch(name) or ".." in name:
            raise ValueError("invalid release repository")
        if name.casefold() in seen:
            raise ValueError("duplicate SDK repository")
        seen.add(name.casefold())
        if row.get("category") not in CATEGORIES:
            raise ValueError("invalid SDK category")
        if not isinstance(row.get("reference"), str) or not row["reference"]:
            raise ValueError("missing SDK reference")
    return rows

def fetch_latest(repo: str) -> dict:
    if not REPOSITORY.fullmatch(repo) or ".." in repo:
        raise ValueError("invalid repository")
    url = "https://api.github.com/repos/" + repo + "/releases/latest"
    headers = {"Accept": "application/vnd.github+json",
               "X-GitHub-Api-Version": "2022-11-28",
               "User-Agent": "Scarlett-Compat-official-sdk-release-radar"}
    token = os.environ.get("GITHUB_TOKEN")
    if token:
        headers["Authorization"] = "Bearer " + token
    with urlopen(Request(url, headers=headers), timeout=12) as response:
        if response.geturl() != url:
            raise ValueError("unexpected API redirect")
        raw = response.read(128 * 1024 + 1)
        if len(raw) > 128 * 1024:
            raise ValueError("oversized release metadata")
        payload = json.loads(raw)
        if not isinstance(payload, dict):
            raise ValueError("malformed latest release response")
        return payload

def scan(row: dict, fixtures: dict | None = None) -> dict:
    name = row["repository"]
    result = {"repository": name, "category": row["category"],
              "status": "unavailable", "tag": None, "published_at": None,
              "release_url": None, "error": None,
              "evidence": "public GitHub release metadata, not functionality or title compatibility"}
    try:
        if fixtures is not None:
            if name not in fixtures:
                result["error"] = "not-in-fixture"
                return result
            incoming = fixtures[name]
            if incoming is None:  # Deliberate 404 fixture.
                result["status"] = "no-release"
                return result
        else:
            try:
                incoming = fetch_latest(name)
            except HTTPError as ex:
                if ex.code == 404:
                    result["status"] = "no-release"
                else:
                    result["error"] = "HTTPError"
                return result
        if not isinstance(incoming, dict):
            raise ValueError("release response not a mapping")
        tag = incoming.get("tag_name")
        date = incoming.get("published_at")
        url = incoming.get("html_url")
        if not isinstance(tag, str) or not TAG.fullmatch(tag) or ".." in tag:
            raise ValueError("missing or invalid latest release tag")
        if not isinstance(date, str):
            raise ValueError("missing latest release date")
        timestamp = datetime.fromisoformat(date.replace("Z", "+00:00"))
        if timestamp.tzinfo is None:
            raise ValueError("release date has no timezone")
        # Restrict links to the same owner/repository and exact tag; do not use raw API URLs.
        if (not isinstance(url, str) or
                url != "https://github.com/" + name + "/releases/tag/" + tag):
            raise ValueError("untrusted release URL")
        result.update(status="released", tag=tag, published_at=timestamp.isoformat(),
                      release_url=url)
    except (URLError, ValueError, KeyError, TypeError, OSError, TimeoutError) as ex:
        result["error"] = type(ex).__name__
    return result

def report(rows: list[dict], results: list[dict], when: str) -> str:
    lines = ["# Official public PC SDK release metadata", "",
             "Collected UTC: " + when, "",
             "**Release tags are leads, not evidence of renderer, GPU, Linux, console, or GTA VI compatibility.**",
             "GitHub `releases/latest` selects the latest stable published GitHub release, which is not necessarily the newest commit or preview.",
             "A no-release result is normal for repositories distributing only sample source or packages.",
             "",
             "| Component | Release status | Tag | Published UTC |",
             "|---|---|---|---|"]
    for row, result in zip(rows, results):
        name = row["repository"]
        component = "[" + name + "](https://github.com/" + name + ")"
        tag = "`" + result["tag"] + "`" if result["tag"] else "—"
        date = (result["published_at"] or "—")[:19]
        lines.append(f"| {component} | {result['status']} | {tag} | {date} |")
    lines.extend(["", "No SDK package, game file, executable, model, proprietary runtime, or binary asset downloaded.",
                  "Provider SDK usage remains gated by license, device, renderer API, scene integration and measured behavior.", ""])
    return "\n".join(lines)

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--registry", type=Path,
                        default=Path("sources/sdk_releases.json"))
    parser.add_argument("--offline-fixture", type=Path)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--snapshot", type=Path)
    args = parser.parse_args(argv)
    rows = load(args.registry)
    fixtures = None
    if args.offline_fixture:
        fixtures_data = json.loads(args.offline_fixture.read_text(encoding="utf-8"))
        fixtures = fixtures_data["repositories"]
        if not isinstance(fixtures, dict):
            raise ValueError("fixture repositories must be a mapping")
    collected = datetime.now(timezone.utc).replace(microsecond=0).isoformat()
    results = [scan(row, fixtures) for row in rows]
    contents = report(rows, results, collected)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(contents, encoding="utf-8")
    else:
        print(contents)
    if args.snapshot:
        args.snapshot.parent.mkdir(parents=True, exist_ok=True)
        args.snapshot.write_text(json.dumps({"schema_version": 1,
                            "collected_at_utc": collected, "results": results},
                            indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"SDK releases: {len(rows)} checked; "
          f"{sum(r['status'] == 'unavailable' for r in results)} unavailable",
          file=sys.stderr)
    return 0 if any(r["status"] != "unavailable" for r in results) else 1

if __name__ == "__main__":
    raise SystemExit(main())
