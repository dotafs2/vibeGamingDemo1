"""Bounded native Level0 run, optionally using the existing paid Kimi gateway.

Never initializes a ledger, resets credit, copies keys into evidence, or touches
an unrelated process. All children are hidden and identified by an owned record.
"""
import argparse
import json
import math
import sqlite3
import os
from pathlib import Path
import subprocess
import sys
import time

PROJECT=Path(__file__).resolve().parents[1]
GATEWAY=PROJECT/'Plugins/ThreeHearths/Tools/kimi_gateway.py'
ENDPOINT=PROJECT/'Saved/ThreeHearths/Budget/gateway-endpoint.json'
RUNS=PROJECT/'Saved/ThreeHearths/AincradLevel0/Runs'
WORLD=PROJECT/'Saved/ThreeHearths/AincradLevel0/world.json'
PROJECT_FILE=PROJECT/'CropoutSampleProject.uproject'
EDITOR_CANDIDATES=(Path(r'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'),
                   Path(r'C:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'))
parser=argparse.ArgumentParser()
parser.add_argument('--seconds',type=int,default=120,help='Continuous native runtime, 30..1800 seconds; keep the world alive through resident thought cooldowns.')
parser.add_argument('--editor','-EditorPath',dest='editor',help='Path to UnrealEditor.exe; probes the installed C:/D: UE 5.8 paths when omitted.')
parser.add_argument('--api',action='store_true')
parser.add_argument('--confirm-local-rejection',help='Explicitly recover one proven unsent local HTTP 400 operation; verifies ledger absence before launch.')
parser.add_argument('--capture',action='store_true')
parser.add_argument('--life',action='store_true',help='Install/continue the versioned tool-commission life extension in the same world.')
parser.add_argument('--survival',action='store_true',help='Install/continue versioned finite rations and bodily needs in this same world; implies no autonomous food production.')
parser.add_argument('--foraging',action='store_true',help='Explicitly install the finite shared renewable berry source; requires life and survival.')
parser.add_argument('--max-decisions',type=int,default=3,help='Maximum fee dispatches for this bounded run, including directed life events.')
parser.add_argument('--budget-profile',choices=('city-validation','overnight'),default='city-validation')
parser.add_argument('--external-liability-cny',type=float,default=17.3692551,help='Known liability from another machine, included in the CNY 95 allocation check.')
parser.add_argument('--stop-at-utc',type=int,help='Absolute UTC epoch at which this runner must stop; leaves startup and settlement safety margins.')
parser.add_argument('--verify-walk',action='store_true',help='Local capsule/door/eye-height check, separate from paid resident behavior.')
parser.add_argument('--exercise-routes',action='store_true',help='Local manual route verification; never combined with API.')
opt=parser.parse_args()
# Operator pause applies only before a new run. Existing owned runs retain
# their deadline and normal save/settlement/cleanup path.
pause_new_runs = PROJECT/'Saved/ThreeHearths/AincradLevel0/PAUSE_NEW_RUNS'
if pause_new_runs.exists():
    print(json.dumps({'status':'paused','reason':'PAUSE_NEW_RUNS exists; no run or fee started'}),flush=True)
    raise SystemExit(75)
if not math.isfinite(opt.external_liability_cny) or opt.external_liability_cny<0: parser.error('--external-liability-cny must be finite and non-negative')
if opt.stop_at_utc is not None:
    if opt.stop_at_utc<=0: parser.error('--stop-at-utc must be a positive UTC epoch')
    remaining=opt.stop_at_utc-time.time()
    if remaining<60: parser.error('--stop-at-utc is expired or leaves less than the safe startup window')
    runtime_seconds=min(opt.seconds,int(remaining-30))
    if runtime_seconds<30: parser.error('--stop-at-utc leaves less than the minimum 30-second run after safety margins')
else:
    remaining=None
    runtime_seconds=opt.seconds
if not 30 <= opt.seconds <= 1800: parser.error('--seconds must be between 30 and 1800')
assert 0<=opt.max_decisions<=24
if opt.foraging and not (opt.life and opt.survival): parser.error('--foraging requires --life --survival')
assert not (opt.api and opt.exercise_routes),'Manual verification must stay separate from paid decisions.'
assert not (opt.api and opt.verify_walk),'Player collision verification is local-only.'
EDITOR=Path(opt.editor) if opt.editor else next((p for p in EDITOR_CANDIDATES if p.is_file()),None)
assert EDITOR is not None and EDITOR.is_file(),'UnrealEditor.exe was not found; pass --editor/-EditorPath.'
assert PROJECT_FILE.is_file(),'Level0 project file is missing.'
assert WORLD.is_file(),'Existing AincradLevel0 world.json is required; refusing to create a new world.'
try:
    world=json.loads(WORLD.read_text(encoding='utf-8-sig'))
except (OSError,ValueError) as exc:
    raise AssertionError('Existing AincradLevel0 world.json is unreadable.') from exc
assert isinstance(world,dict) and isinstance(world.get('residents'),list) and len(world['residents'])==13,'Existing AincradLevel0 world.json must contain the 13-resident checkpoint.'
confirmed_unsent_ledger = None
if opt.confirm_local_rejection:
    operation = opt.confirm_local_rejection
    assert len(operation)==36 and all(c in '0123456789ABCDEFabcdef-' for c in operation),'Invalid operation identifier'
    matching = [r for r in world['residents'] if r.get('runtime',{}).get('pending_operation')==operation]
    assert len(matching)==1,'Operation must match one pending resident'
    runtime = matching[0]['runtime']
    assert runtime.get('last_result')=='HTTP 400; pending operation retained' and runtime.get('last_result_source')=='api_uncertain'
    assert json.loads(runtime.get('last_result_raw',''))=={'error':'Invalid or unauthorized request options'}
    assert not runtime.get('life_pending_option'),'Do not replace an active life intent'
    confirmed_unsent_ledger=runtime['pending_budget_ledger_id']
    ledger_path=PROJECT/'Saved/ThreeHearths/Budget/kimi-overnight-2026-09-06.sqlite3'
    with sqlite3.connect(ledger_path.as_uri()+'?mode=ro',uri=True) as connection:
        assert connection.execute('SELECT ledger_id FROM meta WHERE id=1').fetchone()[0]==confirmed_unsent_ledger,'Ledger identity mismatch'
        assert connection.execute('SELECT COUNT(*) FROM requests WHERE id=?',(operation,)).fetchone()[0]==0,'Request was reserved or sent; do not clear or retry it'
RUNS.mkdir(parents=True,exist_ok=True)
run='town-'+time.strftime('%Y%m%d-%H%M%S',time.gmtime())
meta_path=RUNS/(run+'.json')
meta={'run':run,'world_id':world.get('world_id'),'seconds':runtime_seconds,'api':opt.api,'life':opt.life,'survival':opt.survival,'foraging':opt.foraging,'max_decisions':opt.max_decisions,'budget_profile':opt.budget_profile,'external_liability_cny':opt.external_liability_cny,'stop_at_utc':opt.stop_at_utc,'manual_route_verification':opt.exercise_routes,'ue_pid':None,'gateway_pid':None,'started_utc_epoch':time.time()}
flags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0
owned_ue=None;owned_gateway=None;logs=[]
def record():meta_path.write_text(json.dumps(meta,ensure_ascii=False,indent=2),encoding='utf-8')
def status():
    timeout=10
    if opt.stop_at_utc is not None: timeout=max(.1,min(timeout,opt.stop_at_utc-time.time()-0.5))
    result=subprocess.run([sys.executable,str(GATEWAY),'status','--profile',opt.budget_profile],capture_output=True,text=True,check=True,timeout=timeout,creationflags=flags)
    return json.loads(result.stdout)
def stop(p):
    if p is None:return
    if p.poll() is None:p.terminate()
    timeout=8
    if opt.stop_at_utc is not None: timeout=max(0,min(timeout,opt.stop_at_utc-time.time()-0.2))
    try:p.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        p.kill()
        try:p.wait(timeout=0)
        except subprocess.TimeoutExpired:pass
try:
    if opt.api:
        assert not ENDPOINT.exists(),'An existing gateway owns the endpoint; do not start a duplicate.'
        if opt.stop_at_utc is not None: assert time.time()+20<opt.stop_at_utc,'Stop deadline is too close to safely start the gateway.'
        before=status();meta['budget_before']=before
        assert not before['halted'],'Budget gateway is halted.'
        assert before['liability_cny']+opt.external_liability_cny+opt.max_decisions*1.71776<=95,'Insufficient cumulative liability headroom.'
        if opt.budget_profile=='city-validation':
            calls=sum(int(v) for v in before.get('counts',{}).values())
            assert calls+opt.max_decisions<=600,'Insufficient cumulative call headroom.'
        stdout=(RUNS/(run+'-gateway.log')).open('w',encoding='utf-8');logs.append(stdout)
        stderr=(RUNS/(run+'-gateway-error.log')).open('w',encoding='utf-8');logs.append(stderr)
        gateway_deadline=int(time.time()+runtime_seconds+120)
        if opt.stop_at_utc is not None: gateway_deadline=min(gateway_deadline,opt.stop_at_utc-20)
        owned_gateway=subprocess.Popen([sys.executable,'-u',str(GATEWAY),'serve','--profile',opt.budget_profile,'--deadline-utc',str(gateway_deadline)],stdout=stdout,stderr=stderr,creationflags=flags)
        meta['gateway_pid']=owned_gateway.pid;record()
        ready=False
        for _ in range(80):
            if owned_gateway.poll() is not None:raise RuntimeError('Gateway startup failed; see private diagnostic.')
            if ENDPOINT.exists():
                try:ready=json.loads(ENDPOINT.read_text(encoding='utf-8-sig')).get('pid')==owned_gateway.pid
                except (ValueError,OSError):pass
            if ready:break
            time.sleep(.25)
        assert ready,'Gateway did not publish its owned descriptor.'
    if opt.stop_at_utc is not None: assert time.time()+60<opt.stop_at_utc,'Stop deadline is too close to safely start UE and clean up.'
    ue_log=RUNS/(run+'.log')
    args=[str(EDITOR),str(PROJECT_FILE),'/Game/ThreeHearths/Maps/L_AincradLevel0','-game','-windowed','-ResX=1600','-ResY=1000','-RenderOffscreen','-unattended','-nosplash','-nosound','-nop4','-ExecCmds=t.MaxFPS 30',f'-AincradReviewSeconds={runtime_seconds}',f'-abslog={ue_log}']
    if opt.stop_at_utc is not None: args.append(f'-AincradStopUtc={opt.stop_at_utc}')
    args.append('-AincradResidentApi' if opt.api else '-HearthDisableApi')
    args.append(f'-AincradDecisionLimit={opt.max_decisions}')
    args.append(f'-AincradDecisionWindow={max(12,runtime_seconds-55)}')
    if opt.life:args.append('-AincradLife')
    if opt.survival:args.append('-AincradSurvival')
    if opt.foraging:args.append('-AincradForaging')
    if confirmed_unsent_ledger:
        args.extend(['-AincradConfirmedUnsentOperation='+opt.confirm_local_rejection,'-AincradConfirmedUnsentLedgerId='+confirmed_unsent_ledger])
        meta['confirmed_local_rejection']={'operation_id':opt.confirm_local_rejection,'ledger_id':confirmed_unsent_ledger,'ledger_matching_rows':0,'replayed':False}
    if opt.verify_walk:args.append('-AincradVerifyWalk')
    if opt.capture:args.append('-AincradCapture')
    if opt.exercise_routes:args.append('-AincradExerciseRoutes')
    owned_ue=subprocess.Popen(args,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,creationflags=flags)
    meta['ue_pid']=owned_ue.pid;record()
    print(json.dumps({'run':run,'ue_pid':owned_ue.pid,'gateway_pid':meta['gateway_pid'],'api':opt.api}),flush=True)
    wait_timeout=runtime_seconds+150
    if opt.stop_at_utc is not None:
        wait_timeout=min(wait_timeout,max(0,opt.stop_at_utc-time.time()-20))
        if wait_timeout<=0: raise subprocess.TimeoutExpired(args,0)
    meta['ue_exit']=owned_ue.wait(timeout=wait_timeout)
    assert meta['ue_exit']==0,'Native UE run failed; inspect the recorded log.'
finally:
    stop(owned_ue);stop(owned_gateway)
    if owned_gateway and ENDPOINT.exists():
        try:
            if json.loads(ENDPOINT.read_text(encoding='utf-8-sig')).get('pid')==owned_gateway.pid:ENDPOINT.unlink()
        except (ValueError,OSError):pass
    for log in logs:log.close()
    if opt.api and 'budget_before' in meta:
        try:
            after=status();meta['budget_after']=after
            meta['new_cost_cny']=round(after['settled_cny']-meta['budget_before']['settled_cny'],8)
            meta['new_settled_calls']=after.get('counts',{}).get('settled',0)-meta['budget_before'].get('counts',{}).get('settled',0)
        except (subprocess.TimeoutExpired,subprocess.CalledProcessError,OSError):
            meta['budget_after_error']='status timeout or unavailable during deadline cleanup'
    meta['finished_utc_epoch']=time.time();record()
    print(json.dumps({'run':run,'exit':meta.get('ue_exit'),'new_settled_calls':meta.get('new_settled_calls',0),'new_cost_cny':meta.get('new_cost_cny',0),'owned_children_stopped':True}),flush=True)
