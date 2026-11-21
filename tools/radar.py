#!/usr/bin/env python3
"""Read-only GitHub revision watcher; no source checkout, binaries, or game content."""
from __future__ import annotations
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import sys
from urllib.error import HTTPError, URLError
from urllib.parse import quote
from urllib.request import Request, urlopen

REPO=re.compile(r"[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+\Z")
REF=re.compile(r"[A-Za-z0-9_./-]+\Z")
SHA=re.compile(r"[0-9a-f]{40}\Z")

def load(path: Path) -> list[dict]:
    record=json.loads(path.read_text(encoding='utf-8'))
    if not isinstance(record,dict) or record.get('schema_version')!=1:
        raise ValueError('unsupported registry format')
    rows=record.get('sources')
    if not isinstance(rows,list) or not rows: raise ValueError('empty registry')
    seen=set()
    for row in rows:
        name,ref,sha=row.get('repository'),row.get('ref'),row.get('baseline_sha')
        if not isinstance(name,str) or not REPO.fullmatch(name) or '..' in name: raise ValueError('invalid repo')
        if not isinstance(ref,str) or not REF.fullmatch(ref) or '..' in ref or ref.startswith('/'): raise ValueError('invalid branch')
        if not isinstance(sha,str) or not SHA.fullmatch(sha): raise ValueError('invalid SHA')
        if name.lower() in seen: raise ValueError('duplicate repo')
        seen.add(name.lower())
    return rows

def github(url: str) -> dict:
    if not url.startswith('https://api.github.com/repos/'): raise ValueError('not GitHub')
    headers={'Accept':'application/vnd.github+json','User-Agent':'Scarlett-Windows-Compat-radar',
             'X-GitHub-Api-Version':'2022-11-28'}
    token=os.environ.get('GITHUB_TOKEN')
    if token: headers['Authorization']='Bearer '+token
    with urlopen(Request(url,headers=headers),timeout=12) as response:
        if response.geturl()!=url: raise ValueError('unexpected redirect')
        payload=response.read(1024*1024+1)
        if len(payload)>1024*1024:raise ValueError('too much metadata')
        obj=json.loads(payload)
        if not isinstance(obj,dict):raise ValueError('malformed metadata')
        return obj

def scan(row:dict,fixture:dict|None=None)->dict:
    name,ref=row['repository'],row['ref']
    result={'repository':name,'ref':ref,'baseline_sha':row['baseline_sha'],'head_sha':None,
            'status':'unavailable','license_metadata':None,'pushed_at':None,'error':None,
            'evidence':'GitHub metadata only — not a compatibility test'}
    if fixture is not None:
        data=fixture.get(name)
        if not isinstance(data,dict):
            result['error']='not-in-fixture';return result
        head=data.get('head_sha')
        license_id=data.get('license_metadata')
        pushed=data.get('pushed_at')
    else:
        try:
            root='https://api.github.com/repos/'+name
            metadata=github(root)
            ref_data=github(root+'/git/ref/heads/'+quote(ref,safe='/'))
            head=ref_data['object']['sha']
            license_info=metadata.get('license')
            license_id=license_info.get('spdx_id') if isinstance(license_info,dict) else None
            pushed=metadata.get('pushed_at')
        except (ValueError,KeyError,TypeError,HTTPError,URLError,OSError,TimeoutError) as e:
            result['error']=type(e).__name__;return result
    result['head_sha']=head if isinstance(head,str) and SHA.fullmatch(head) else None
    result['license_metadata']=license_id if isinstance(license_id,str) else None
    result['pushed_at']=pushed if isinstance(pushed,str) else None
    if result['head_sha']:
        result['status']='unchanged' if result['head_sha']==row['baseline_sha'] else 'new-head-since-baseline'
    return result

def report(rows:list[dict],results:list[dict],when:str)->str:
    lines=['# Public upstream metadata radar','',f'Collected {when}','',
           'A different commit SHA is **not** a verified bug fix, playable title or working port.','',
           '| Repo | Status | HEAD | License metadata |','|---|---|---|---|']
    for row,r in zip(rows,results):
        name=row['repository']
        lines.append(f'| [{name}](https://github.com/{name}/tree/{row["ref"]}) | {r["status"]} | {(r["head_sha"] or "unknown")[:12]} | {r["license_metadata"] or "unverified"} |')
    lines.extend(['','No game files, copyrighted shaders, binaries or other proprietary content were fetched.',''])
    return '\n'.join(lines)

def main(argv=None)->int:
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--registry',type=Path,default=Path('sources/upstreams.json'))
    ap.add_argument('--offline-fixture',type=Path)
    ap.add_argument('--report',type=Path)
    ap.add_argument('--snapshot',type=Path)
    args=ap.parse_args(argv)
    rows=load(args.registry)
    fixture=json.loads(args.offline_fixture.read_text(encoding='utf-8'))['repositories'] if args.offline_fixture else None
    stamp=datetime.now(timezone.utc).replace(microsecond=0).isoformat()
    result=[scan(x,fixture) for x in rows]
    markdown=report(rows,result,stamp)
    if args.report:
        args.report.parent.mkdir(parents=True,exist_ok=True)
        args.report.write_text(markdown,encoding='utf-8')
    else:print(markdown)
    if args.snapshot:
        args.snapshot.parent.mkdir(parents=True,exist_ok=True)
        args.snapshot.write_text(json.dumps({'collected_at_utc':stamp,'results':result},indent=2)+'\n',encoding='utf-8')
    print(f'{len(rows)} inspected; {sum(r["status"]=="unavailable" for r in result)} unavailable',file=sys.stderr)
    return 0 if any(x['status']!='unavailable' for x in result) else 1
if __name__=='__main__':raise SystemExit(main())
