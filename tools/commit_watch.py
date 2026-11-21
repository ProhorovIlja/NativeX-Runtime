#!/usr/bin/env python3
"""Bounded GitHub commit-subject collection. Commit titles are not correctness proof."""
import argparse
import datetime
import json
import os
from pathlib import Path
import re
from urllib.parse import quote
from urllib.request import Request, urlopen

SHA = re.compile(r"[a-f0-9]{40}\Z")
REPO = re.compile(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+\Z")


def subject(value):
    if not isinstance(value, str):
        return ""
    first = value.splitlines()[0] if value else ""
    return "".join(c if c.isalnum() or c in " .,;:_+-/#" else " " for c in first)[:140]


def summarize(repo, baseline, commits):
    if not isinstance(repo, str) or not REPO.fullmatch(repo) or ".." in repo:
        raise ValueError("invalid repository")
    if not isinstance(baseline, str) or not SHA.fullmatch(baseline):
        raise ValueError("invalid baseline")
    if not isinstance(commits, list):
        raise ValueError("invalid commit list")
    validated = []
    for row in commits[:5]:
        if not isinstance(row, dict):
            continue
        sha = row.get("sha")
        if not isinstance(sha, str) or not SHA.fullmatch(sha):
            continue
        data = row.get("commit") if isinstance(row.get("commit"), dict) else {}
        cm = data.get("committer") if isinstance(data.get("committer"), dict) else {}
        validated.append({"sha": sha, "subject": subject(data.get("message")),
                          "when": str(cm.get("date") or "")[:25],
                          "url": "https://github.com/"+repo+"/commit/"+sha})
    head = validated[0]["sha"] if validated else None
    position = next((i for i, row in enumerate(validated) if row["sha"] == baseline), None)
    status = ("unavailable" if head is None else "unchanged" if head == baseline
              else "bounded-delta" if position is not None else "recent-only-not-exhaustive")
    return {"repository": repo, "head": head, "baseline": baseline,
            "status": status, "commits": [] if status == "unchanged"
            else validated[:position] if position is not None else validated}


def fetch_commits(repo, ref, limit):
    url = ("https://api.github.com/repos/" + repo + "/commits?sha="
           + quote(ref, safe="") + "&per_page=" + str(limit))
    headers = {"Accept": "application/vnd.github+json",
               "X-GitHub-Api-Version": "2022-11-28",
               "User-Agent": "Scarlett-Windows-Compat-commit-leads"}
    if os.environ.get("GITHUB_TOKEN"):
        headers["Authorization"] = "Bearer " + os.environ["GITHUB_TOKEN"]
    with urlopen(Request(url, headers=headers), timeout=12) as response:
        if response.geturl() != url:
            raise ValueError("unexpected redirect")
        data = response.read(1024 * 1024 + 1)
        if len(data) > 1024 * 1024:
            raise ValueError("commit metadata too large")
        result = json.loads(data)
        if not isinstance(result, list):
            raise ValueError("unexpected API response")
        return result


def render(records):
    lines = ["# Public upstream commit leads", "",
             "GitHub commit subjects are **author-supplied metadata**, not validated features.",
             "Only five recent commits per branch: missing baseline means the list is incomplete.", ""]
    for record in records:
        lines.append("## "+record["repository"])
        lines.append("Status: "+record["status"]+"; head: "+(record["head"] or "unknown")[:12])
        for item in record["commits"]:
            lines.append("- ["+item["sha"][:12]+"]("+item["url"]+"): "
                         +item["subject"]+" ("+item["when"]+")")
        lines.append("")
    lines.extend(["No game files or proprietary assets are downloaded.",
                  "Confirm exact source changes, tests and licenses before use.", ""])
    return "\n".join(lines)


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--registry", type=Path, default=Path("sources/upstreams.json"))
    p.add_argument("--offline-fixture", type=Path)
    p.add_argument("--report", type=Path)
    p.add_argument("--snapshot", type=Path)
    args = p.parse_args(argv)
    rows = json.loads(args.registry.read_text(encoding="utf-8"))["sources"]
    fixtures = (json.loads(args.offline_fixture.read_text(encoding="utf-8"))["repositories"]
                if args.offline_fixture else None)
    results = []
    for row in rows:
        try:
            commits = fixtures.get(row["repository"], []) if fixtures is not None \
                else fetch_commits(row["repository"], row["ref"], 5)
            results.append(summarize(row["repository"], row["baseline_sha"], commits))
        except (OSError, ValueError, KeyError, TypeError) as exc:
            results.append({"repository": row["repository"], "head": None,
                            "status": "unavailable", "commits": [],
                            "error": type(exc).__name__})
    out = render(results)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(out+"\n", encoding="utf-8")
    else:
        print(out)
    if args.snapshot:
        args.snapshot.parent.mkdir(parents=True, exist_ok=True)
        args.snapshot.write_text(json.dumps({
            "collected_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
            "records": results
        }, indent=2)+"\n", encoding="utf-8")
    return 0 if any(x["status"] != "unavailable" for x in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
