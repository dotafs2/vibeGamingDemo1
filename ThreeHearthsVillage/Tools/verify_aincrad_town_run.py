"""Validate native persistent-world evidence and export a credential-free record.

Read-only toward world, API and ledger. Copies original UE PNG bytes without
retouching; never substitutes an overview for an NPC's submitted observation.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sqlite3

ROOT=Path(__file__).resolve().parents[1]
SAVED=ROOT/'Saved/ThreeHearths/AincradLevel0'
OUT=ROOT/'Docs/Validation/StartingTown_2026-09-08'
parser=argparse.ArgumentParser()
parser.add_argument('--baseline',type=Path,required=True)
parser.add_argument('--paid-world',type=Path,required=True)
parser.add_argument('--paid-run',required=True)
parser.add_argument('--restart-run',required=True)
parser.add_argument('--routes-run',required=True)
opt=parser.parse_args()

def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def saved_path(raw):
    # UE FPaths may retain its Engine/Binaries-relative prefix in metadata.
    marker='/ThreeHearthsVillage/'
    path=ROOT/raw.replace('\\','/').split(marker,1)[-1] if marker in raw.replace('\\','/') else Path(raw)
    path=path.resolve()
    assert path.is_relative_to(SAVED.resolve()),'Evidence must belong to this world save.'
    return path
def copy(path,dest):dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(path,dest)

before=read(opt.baseline);paid=read(opt.paid_world);after=read(SAVED/'world.json')
assert before['schema_version']==1 and paid['schema_version']==after['schema_version']==2
assert before['world_id']==paid['world_id']==after['world_id']
assert after['elapsed_seconds']>=paid['elapsed_seconds']>before['elapsed_seconds']
old={r['stable_id']:r for r in before['residents']}
new={r['stable_id']:r for r in after['residents']}
assert len(old)==len(new)==13 and old.keys()==new.keys()
for key,original in old.items():
    # None of these initial S0/S1 steps are allowed to spend Col or rewrite
    # identity, biography, remembered places, needs or health.
    for field,value in original.items():assert new[key][field]==value,(key,field)
assert sha(opt.baseline)==sha(SAVED/'world.json.pre-v2')
assert len(after['buildings'])==len(after['building_bindings'])==3

paid_run=read(SAVED/'Runs'/(opt.paid_run+'.json'))
restart=read(SAVED/'Runs'/(opt.restart_run+'.json'))
routes=read(SAVED/'Runs'/(opt.routes_run+'.json'))
assert paid_run['api'] and paid_run['ue_exit']==0 and paid_run['new_settled_calls']==3
assert restart['api'] and restart['ue_exit']==0 and restart['new_settled_calls']==0
assert routes['manual_route_verification'] and not routes['api'] and routes['ue_exit']==0
assert restart['budget_after']['reserved_cny']==0
assert paid_run['budget_after']['reserved_cny']==0
active=[r for r in paid['residents'] if r['runtime']['active']]
assert len(active)==3
ledger=ROOT/'Saved/ThreeHearths/Budget/kimi-city-validation-2026-09-07.sqlite3'
db=sqlite3.connect(ledger.as_uri()+'?mode=ro',uri=True)
db.row_factory=sqlite3.Row
rows=[]
for resident in active:
    rt=resident['runtime'];current=new[resident['stable_id']]['runtime']
    assert rt['last_result_source']=='kimi' and not rt['pending_operation']
    assert not current['pending_operation'] and current['phase']=='idle'
    assert current['last_think_utc']==rt['last_think_utc']
    assert current['memory']==rt['memory']
    request=read(saved_path(rt['last_request_evidence']))
    image_path=saved_path(request['image_file'])
    image_meta=read(image_path.with_suffix('.json'))
    assert request['resident_id']==image_meta['resident_id']==resident['stable_id']
    assert request['observation_id']==image_meta['observation_id']
    assert image_meta['target_id']==rt['building_id']
    assert hashlib.sha1(image_path.read_bytes()).hexdigest().upper()==image_meta['image_sha1']
    assert image_meta['horizontal_fov']==85 and image_meta['eye_cm'][2]==160
    record=db.execute('SELECT id,resident,state,charge,prompt_tokens,output_tokens,cached_tokens,created,finished,response FROM requests WHERE id=?',(rt['last_operation'],)).fetchone()
    assert record is not None and record['state']=='settled' and record['resident']==resident['stable_id']
    operation=dict(record);response=json.loads(operation.pop('response'))
    assert response['choices'][0]['message']['content']==rt['last_result_raw']
    folder=OUT/'Kimi'/resident['role']
    copy(image_path,folder/'submitted-fov.png')
    request['image_file']='submitted-fov.png'
    for filename,value in [('request.json',request),('observation.json',image_meta),('response.json',response),('ledger-receipt.json',operation)]:
        (folder/filename).write_text(json.dumps(value,ensure_ascii=False,indent=2),encoding='utf-8')
    events_path=saved_path(rt['last_request_evidence']).parent/'events.jsonl'
    events=[json.loads(line) for line in events_path.read_text(encoding='utf-8-sig').splitlines() if line.strip()]
    assert any(e['kind']=='manual_wall_sweep' and e['detail']=='side wall blocks resident-sized capsule' for e in events)
    interiors=[e for e in events if e['kind']=='observation' and e.get('source')=='local_verification' and 'view=interior' in e['detail']]
    assert interiors,'Every resident must actually enter before the interior capture.'
    interior_id=interiors[-1]['detail'].split()[0]
    interior_image=events_path.parent/(interior_id+'.png')
    interior_meta=read(interior_image.with_suffix('.json'))
    assert interior_meta['resident_id']==resident['stable_id'] and interior_meta['eye_cm'][2]==160
    copy(interior_image,OUT/'After'/(resident['role']+'-fov-interior.png'))
    copy(interior_image.with_suffix('.json'),OUT/'After'/(resident['role']+'-fov-interior.json'))
    copy(events_path,folder/'events.jsonl')
    history_path=events_path.parent/'history.jsonl'
    history=[json.loads(line) for line in history_path.read_text(encoding='utf-8-sig').splitlines() if line.strip()]
    assert len(history)>=len(events)
    assert any(e['kind']=='session_restore' and e.get('latest_decision_raw')==rt['last_result_raw'] for e in history)
    copy(history_path,folder/'history.jsonl')
    rows.append({'resident':resident['name'],'stable_id':resident['stable_id'],'building_id':rt['building_id'],
        'kimi_decision':json.loads(rt['last_result_raw']),'position_after_kimi':rt['position_cm'],
        'position_after_routes_and_restart':current['position_cm'],'unchanged_cooldown_utc':rt['last_think_utc'],
        'operation':operation,'observation_sha256':sha(image_path),'events':len(events)})
db.close()
# All requests were in flight together; the last began before the first ended.
assert max(r['operation']['created'] for r in rows)<min(r['operation']['finished'] for r in rows)
archives={}
archive=ROOT/'Saved/ThreeHearths/Archives/Medieval_20260908_81BDF793'
for name,expected in [('world.json','a9e60499c6a563affe267e36aee2cf1c45b8bf06a44a993b39090ba9a7abbd28'),('history.json','aaf51d9ebde27e72a5fa6cbc1e53dad1ec50c7dffd90e568ec083bea6d7b82ea')]:
    digest=sha(archive/name);assert digest==expected;archives[name]=digest
report={'world_id':after['world_id'],'residents_preserved':13,'original_fields_preserved':True,
    'elapsed_before_seconds':before['elapsed_seconds'],'elapsed_after_seconds':after['elapsed_seconds'],
    'migration_backup_byte_identical':True,'initial_binding_count':3,'concurrent_paid_requests':True,
    'paid_calls':paid_run['new_settled_calls'],'paid_cost_cny':paid_run['new_cost_cny'],
    'restart_new_calls':0,'pending_operations':0,'old_archive_unchanged':archives,
    'residents':rows,'budget_after':restart['budget_after'],'remote_unresolved_liability_cny':17.3692551,
    'scope':'Three active residents; manual route exercise is not an AI action. No trade, building installation or VR completion claimed.'}
for run in [paid_run,restart,routes]:
    (OUT/(run['run']+'.json')).write_text(json.dumps(run,ensure_ascii=False,indent=2),encoding='utf-8')
for name in ('starter-street','starter-inn','inn-interior'):
    copy(SAVED/'Views'/(name+'.png'),OUT/'After'/(name+'.png'))
copy(SAVED/'world.json',OUT/'world-checkpoint.json')
(OUT/'verification.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({key:report[key] for key in ('world_id','residents_preserved','paid_calls','paid_cost_cny','restart_new_calls','pending_operations')},ensure_ascii=False))
