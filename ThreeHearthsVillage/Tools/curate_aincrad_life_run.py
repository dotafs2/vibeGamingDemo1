"""Curate one completed native Kimi run. Read-only toward live world and ledger.
Copies original submitted PNGs and decisions, never substitutes a staged view.
"""
import argparse, hashlib, json, shutil, sqlite3
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'Saved/ThreeHearths/AincradLevel0'
def read(path): return json.loads(path.read_text(encoding='utf-8-sig'))
def write(path,obj): path.write_text(json.dumps(obj,ensure_ascii=False,indent=2),encoding='utf-8')
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run',required=True)
    parser.add_argument('--output-root',type=Path,help='Evidence directory beneath Docs/Validation; defaults to the historical session.')
    parser.add_argument('--interruption-reason',help='Archive an intentionally interrupted run while retaining its nonzero native exit and actual duration.')
    args=parser.parse_args()
    assert args.run.startswith('town-') and all(c.isdigit() or c=='-' for c in args.run[5:]),'Invalid run id'
    meta=read(BASE/'Runs'/(args.run+'.json'))
    assert meta.get('api') and meta.get('finished_utc_epoch') and meta.get('budget_after'),'Run is not ended with a final budget snapshot'
    assert meta.get('ue_exit')==0 or args.interruption_reason,'Nonzero native exit requires an explicit interruption reason'
    assert not args.interruption_reason or meta.get('ue_exit')!=0,'Do not label a successful native exit interrupted'
    assert meta['budget_profile']=='overnight','Only the existing overnight ledger is supported'
    ledger=ROOT/'Saved/ThreeHearths/Budget/kimi-overnight-2026-09-06.sqlite3'
    conn=sqlite3.connect(ledger.as_uri()+'?mode=ro',uri=True);conn.row_factory=sqlite3.Row
    rows=conn.execute('SELECT * FROM requests WHERE created>=? AND created<=? ORDER BY created',(meta['started_utc_epoch'],meta['finished_utc_epoch'])).fetchall();conn.close()
    request_paths={x.stem[8:]:x for x in (BASE/'Observations'/meta['world_id']).rglob('request-*.json')}
    assert all(row['id'] in request_paths for row in rows),'Ledger window contains an unmatched request; manual reconciliation needed'
    assert sum(r['state']=='settled' for r in rows)==meta['new_settled_calls'],'Run and operation settlement count differ'
    assert abs(sum((r['charge'] or 0)/1e9 for r in rows if r['state']=='settled')-meta['new_cost_cny'])<.00000002,'Run and operation costs differ'
    validation=(ROOT/'Docs/Validation').resolve()
    output_root=args.output_root.resolve() if args.output_root else validation/'Two_Hour_Iteration_2026-09-08/PaidRuns'
    assert output_root.is_relative_to(validation),'Evidence output must remain beneath Docs/Validation'
    dest=output_root/args.run;dest.mkdir(parents=True,exist_ok=True)
    summaries=[]
    for i,row in enumerate(rows,1):
        request=request_paths[row['id']];j=read(request)
        assert j['world_id']==meta['world_id'] and j['resident_id']==row['resident']
        obs=request.parent/(j['observation_id']+'.json');png=obs.with_suffix('.png');ob=read(obs)
        assert hashlib.sha1(png.read_bytes()).hexdigest().upper()==ob['image_sha1'].upper()
        folder=dest/(f'{i:02d}-'+row['id']);folder.mkdir(exist_ok=True)
        # Relocate only the file reference; personal facts and image bytes are unchanged.
        j['image_file']='submitted-fov.png';write(folder/'request.json',j)
        shutil.copy2(png,folder/'submitted-fov.png');shutil.copy2(obs,folder/'observation.json')
        decision=json.loads(json.loads(row['response'])['choices'][0]['message']['content']) if row['state']=='settled' and row['response'] else None
        write(folder/'decision.json',decision)
        receipt={k:row[k] for k in ['id','resident','state','charge','prompt_tokens','output_tokens','cached_tokens','created','finished']}
        receipt['charge_cny']=(row['charge'] or 0)/1e9;write(folder/'receipt.json',receipt)
        summaries.append({'resident':j['personal']['name'],'operation':row['id'],'reason':j['personal'].get('decision_reason'),'action':(decision or {}).get('action'),'option_id':(decision or {}).get('option_id'),'need':(decision or {}).get('need'),'previous_executed_result':j['personal'].get('last_executed_result'),'image_verified':True,'evidence':folder.name})
    result={'run':meta,'decisions':summaries,'limits':'Choices are not proof of execution; use next request physical facts, native journals and world state.'}
    result['completion']={'kind':'controlled_interruption' if args.interruption_reason else 'normal_exit','native_exit_code':meta.get('ue_exit'),'actual_wall_seconds':meta['finished_utc_epoch']-meta['started_utc_epoch'],'reason':args.interruption_reason}
    write(dest/'run.json',result)
    print(json.dumps({'run':args.run,'calls':meta['new_settled_calls'],'cost_cny':meta['new_cost_cny'],'actions':[{k:s[k] for k in ['resident','reason','action','option_id']} for s in summaries]},ensure_ascii=False))
if __name__=='__main__': main()
