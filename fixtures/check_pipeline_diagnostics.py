import json
import subprocess
import sys

raw = subprocess.check_output([sys.argv[1]], text=True)
a,b,c,d,e = map(json.loads,raw.splitlines())
assert 'private details' not in raw
assert a['schema']==1 and a['requests']==8 and a['unique_jobs']==6
assert a['suppressed_jobs']==1 and a['suppressed_requests']==1
assert a['completed_requests']==4 and a['usable_requests']==2
assert a['exception_requests']==1 and a['budget_requests']==1 and a['invalid_requests']==1
assert a['coalesced_requests']==1 and a['unknown_requests']==0
assert [i['status'] for i in a['items']]==['suppressed','completed','completed','completed','callback_exception','budget_exceeded','invalid','completed']
assert a['items'][1]['path']=='hit' and a['items'][2]['failure_remembered'] and a['items'][3]['coalesced']
assert a['items'][7]['path']=='cold' and a['items'][7]['write']=='stored'
for report,n in [(a,8),(b,2),(c,0),(d,4096),(e,0)]:
    assert len(report['items'])==n and report['omitted_items']==report['requests']-n
    assert sum(report[k] for k in ('invalid_requests','budget_requests','completed_requests','exception_requests','suppressed_requests','unknown_requests'))==report['requests']
    assert [i['index'] for i in report['items']]==list(range(n))
assert d['unknown_requests']==1 and d['items'][0]['status']=='unknown'
assert {k:v for k,v in a.items() if k not in ('items','omitted_items')}=={k:v for k,v in b.items() if k not in ('items','omitted_items')}
print('json_reports=5 real_requests=8 capped_items=4096 errors=0')
