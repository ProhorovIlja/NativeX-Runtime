#!/usr/bin/env python3
"""Public GitHub repository discovery; unreviewed leads only, no cloning."""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import sys
from urllib.parse import quote
from urllib.request import Request, urlopen

REPO=re.compile(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+\Z')

def valid(name):
    return isinstance(name,str) and bool(REPO.fullmatch(name)) and '..' not in name

def normalize(entries:list[dict],known:set[str])->list[dict]:
    result,seen=[],set()
    for raw in entries:
        if not isinstance(raw,dict):continue
        name=raw.get('full_name')
        if not valid(name) or name.lower() in seen:continue
        seen.add(name.lower())
        result.append({'repository':name,'url':'https://github.com/'+name,'fork':bool(raw.get('fork')),
                       'archived':bool(raw.get('archived')),'description':str(raw.get('description') or '')[:160],
                       'pushed_at':str(raw.get('pushed_at') or '')[:40],
                       'review':'known' if name.lower() in known else 'needs-review',
                       'evidence':'GitHub search metadata only'})
    return result

def clean_cell(s:str)->str:
    return ''.join(c if c.isalnum() or c in ' .,;_-/' else ' ' for c in s).replace('\n',' ')

def report(rows:list[dict],stamp:str)->str:
    lines=['# GitHub candidate discovery','',f'Collected {stamp}','',
           'Unreviewed suggestions only; no game execution, safe binaries or permission to import code is implied.','',
           '| Source | Review | Fork | Description |','|---|---|---|---|']
    for x in rows:
        lines.append(f'| [{x["repository"]}]({x["url"]}) | {x["review"]} | {"yes" if x["fork"] else "no"} | {clean_cell(x["description"])} |')
    lines.extend(['','Review exact commit, evidence, license and reproducibility before promotion.',''])
    return '\n'.join(lines)

def search(q:str,limit:int)->list[dict]:
    url='https://api.github.com/search/repositories?q='+quote(q,safe='')+f'&sort=updated&order=desc&per_page={limit}'
    headers={'Accept':'application/vnd.github+json','User-Agent':'Scarlett-Windows-Compat-discovery'}
    token=os.environ.get('GITHUB_TOKEN')
    if token:headers['Authorization']='Bearer '+token
    with urlopen(Request(url,headers=headers),timeout=15) as response:
        if response.geturl()!=url:raise ValueError('unexpected redirect')
        payload=response.read(2*1024*1024+1)
        if len(payload)>2*1024*1024:raise ValueError('too much search metadata')
        obj=json.loads(payload)
        if not isinstance(obj,dict) or not isinstance(obj.get('items'),list):raise ValueError('malformed GitHub result')
        return obj['items']

def main(argv=None)->int:
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--queries',type=Path,default=Path('sources/discovery_queries.json'))
    p.add_argument('--registry',type=Path,default=Path('sources/upstreams.json'))
    p.add_argument('--offline-fixture',type=Path)
    p.add_argument('--report',type=Path)
    p.add_argument('--snapshot',type=Path)
    p.add_argument('--limit',type=int,default=10)
    args=p.parse_args(argv)
    if not 1<=args.limit<=30:p.error('limit must be 1..30')
    queries=json.loads(args.queries.read_text(encoding='utf-8'))['queries']
    if not isinstance(queries,list) or not 1<=len(queries)<=12:p.error('invalid queries')
    known={row['repository'].lower() for row in json.loads(args.registry.read_text(encoding='utf-8'))['sources']}
    fixture=json.loads(args.offline_fixture.read_text(encoding='utf-8'))['queries'] if args.offline_fixture else None
    entries,errors=[],[]
    for q in queries:
        if not isinstance(q,str) or not 3<=len(q)<=120:p.error('invalid query')
        try:entries.extend((fixture.get(q,[]) if fixture is not None else search(q,args.limit))[:args.limit])
        except (ValueError,KeyError,TypeError,OSError,TimeoutError) as e:
            errors.append({'query':q,'error':type(e).__name__})
    rows=normalize(entries,known)
    stamp=datetime.now(timezone.utc).replace(microsecond=0).isoformat()
    output=report(rows,stamp)
    if args.report:
        args.report.parent.mkdir(parents=True,exist_ok=True)
        args.report.write_text(output,encoding='utf-8')
    else:print(output)
    if args.snapshot:
        args.snapshot.parent.mkdir(parents=True,exist_ok=True)
        args.snapshot.write_text(json.dumps({'collected_at_utc':stamp,'results':rows,'errors':errors},indent=2)+'\n',encoding='utf-8')
    print(f'candidates={len(rows)} errors={len(errors)}',file=sys.stderr)
    return 1 if errors and not rows else 0
if __name__=='__main__':raise SystemExit(main())
