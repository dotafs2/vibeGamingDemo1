"""Bounded native Level0 run, optionally using the existing paid Kimi gateway.

Never initializes a ledger, resets credit, copies keys into evidence, or touches
an unrelated process. All children are hidden and identified by an owned record.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import time

PROJECT=Path(__file__).resolve().parents[1]
GATEWAY=PROJECT/'Plugins/ThreeHearths/Tools/kimi_gateway.py'
ENDPOINT=PROJECT/'Saved/ThreeHearths/Budget/gateway-endpoint.json'
RUNS=PROJECT/'Saved/ThreeHearths/AincradLevel0/Runs'
EDITOR=Path(r'C:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe')
parser=argparse.ArgumentParser()
parser.add_argument('--seconds',type=int,default=120)
parser.add_argument('--api',action='store_true')
parser.add_argument('--capture',action='store_true')
parser.add_argument('--life',action='store_true',help='Install/continue the versioned tool-commission life extension in the same world.')
parser.add_argument('--max-decisions',type=int,default=3,help='Maximum fee dispatches for this bounded run, including directed life events.')
parser.add_argument('--verify-walk',action='store_true',help='Local capsule/door/eye-height check, separate from paid resident behavior.')
parser.add_argument('--exercise-routes',action='store_true',help='Local manual route verification; never combined with API.')
opt=parser.parse_args()
assert 30<=opt.seconds<=300
assert 0<=opt.max_decisions<=24
assert not (opt.api and opt.exercise_routes),'Manual verification must stay separate from paid decisions.'
assert not (opt.api and opt.verify_walk),'Player collision verification is local-only.'
RUNS.mkdir(parents=True,exist_ok=True)
run='town-'+time.strftime('%Y%m%d-%H%M%S',time.gmtime())
meta_path=RUNS/(run+'.json')
meta={'run':run,'seconds':opt.seconds,'api':opt.api,'life':opt.life,'max_decisions':opt.max_decisions,'manual_route_verification':opt.exercise_routes,'ue_pid':None,'gateway_pid':None,'started_utc_epoch':time.time()}
flags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0
owned_ue=None;owned_gateway=None;logs=[]
def record():meta_path.write_text(json.dumps(meta,ensure_ascii=False,indent=2),encoding='utf-8')
def status():
    result=subprocess.run([sys.executable,str(GATEWAY),'status','--profile','city-validation'],capture_output=True,text=True,check=True,creationflags=flags)
    return json.loads(result.stdout)
def stop(p):
    if p is None:return
    if p.poll() is None:p.terminate()
    try:p.wait(timeout=8)
    except subprocess.TimeoutExpired:p.kill();p.wait(timeout=8)
try:
    if opt.api:
        assert not ENDPOINT.exists(),'An existing gateway owns the endpoint; do not start a duplicate.'
        before=status();meta['budget_before']=before
        assert not before['halted'],'Budget gateway is halted.'
        # Carry forward the documented unresolved liability from the other PC.
        prior=17.3692551
        assert before['liability_cny']+prior+opt.max_decisions*1.71776<=95,'Insufficient cumulative liability headroom.'
        settled=sum(int(v) for v in before.get('counts',{}).values())
        assert settled+opt.max_decisions<=600,'Insufficient cumulative call headroom.'
        stdout=(RUNS/(run+'-gateway.log')).open('w',encoding='utf-8');logs.append(stdout)
        stderr=(RUNS/(run+'-gateway-error.log')).open('w',encoding='utf-8');logs.append(stderr)
        owned_gateway=subprocess.Popen([sys.executable,'-u',str(GATEWAY),'serve','--profile','city-validation','--deadline-utc',str(int(time.time())+opt.seconds+120)],stdout=stdout,stderr=stderr,creationflags=flags)
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
    ue_log=RUNS/(run+'.log')
    args=[str(EDITOR),str(PROJECT/'CropoutSampleProject.uproject'),'/Game/ThreeHearths/Maps/L_AincradLevel0','-game','-windowed','-ResX=1600','-ResY=1000','-RenderOffscreen','-unattended','-nosplash','-nosound','-nop4','-ExecCmds=t.MaxFPS 30',f'-AincradReviewSeconds={opt.seconds}',f'-abslog={ue_log}']
    args.append('-AincradResidentApi' if opt.api else '-HearthDisableApi')
    args.append(f'-AincradDecisionLimit={opt.max_decisions}')
    args.append(f'-AincradDecisionWindow={max(12,opt.seconds-55)}')
    if opt.life:args.append('-AincradLife')
    if opt.verify_walk:args.append('-AincradVerifyWalk')
    if opt.capture:args.append('-AincradCapture')
    if opt.exercise_routes:args.append('-AincradExerciseRoutes')
    owned_ue=subprocess.Popen(args,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,creationflags=flags)
    meta['ue_pid']=owned_ue.pid;record()
    print(json.dumps({'run':run,'ue_pid':owned_ue.pid,'gateway_pid':meta['gateway_pid'],'api':opt.api}),flush=True)
    meta['ue_exit']=owned_ue.wait(timeout=opt.seconds+150)
    assert meta['ue_exit']==0,'Native UE run failed; inspect the recorded log.'
finally:
    stop(owned_ue);stop(owned_gateway)
    if owned_gateway and ENDPOINT.exists():
        try:
            if json.loads(ENDPOINT.read_text(encoding='utf-8-sig')).get('pid')==owned_gateway.pid:ENDPOINT.unlink()
        except (ValueError,OSError):pass
    for log in logs:log.close()
    if opt.api and 'budget_before' in meta:
        after=status();meta['budget_after']=after
        meta['new_cost_cny']=round(after['settled_cny']-meta['budget_before']['settled_cny'],8)
        meta['new_settled_calls']=after.get('counts',{}).get('settled',0)-meta['budget_before'].get('counts',{}).get('settled',0)
    meta['finished_utc_epoch']=time.time();record()
    print(json.dumps({'run':run,'exit':meta.get('ue_exit'),'new_settled_calls':meta.get('new_settled_calls',0),'new_cost_cny':meta.get('new_cost_cny',0),'owned_children_stopped':True}),flush=True)
