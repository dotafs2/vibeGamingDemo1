#!/usr/bin/env python3
import argparse,json,os,tempfile,math
from datetime import datetime,timezone
from pathlib import Path
EXPECTED_WORLD="F4390752-4A07-7DE7-FACD-32BAC6F72C54"
REQ_KEYS=("id","status","source","operation_id","need","target_id","category")
def _int(v,name,lo=0,hi=None):
    if isinstance(v,bool) or not isinstance(v,int) or v<lo or (hi is not None and v>hi): raise ValueError("invalid public resource "+name)
    return v

def _vec(v,name):
    if not isinstance(v,list) or len(v)!=3 or any(isinstance(x,bool) or not isinstance(x,(int,float)) or not math.isfinite(float(x)) for x in v): raise ValueError("invalid public resource "+name)
    return v

def _resource(src):
    if "foraging" not in src:
        return None
    r = src["foraging"]
    if not isinstance(r, dict) or r.get("source_id") != "starter_commons_berry_patch" or r.get("source") != "developer_ecosystem_bootstrap":
        raise ValueError("invalid public resource identity")
    _int(r.get("schema_version"), "schema", 1, 1)
    _int(r.get("capacity"), "capacity", 3, 3)
    _int(r.get("initial_stock"), "initial_stock", 3, 3)
    stock = _int(r.get("stock"), "stock", 0, 3)
    produced = _int(r.get("produced_total"), "produced_total", 0, 1000000000)
    harvested = _int(r.get("harvested_total"), "harvested_total", 0, 1000000000)
    if stock != 3 + produced - harvested:
        raise ValueError("public resource stock conservation failed")
    growth = r.get("growth_remainder_seconds")
    if isinstance(growth, bool) or not isinstance(growth, (int, float)) or not math.isfinite(growth) or not 0 <= growth < 1800:
        raise ValueError("invalid public resource growth remainder")
    if stock == 3 and growth != 0:
        raise ValueError("full public resource cannot bank growth")
    seq = _int(r.get("installed_at_life_seq"), "installed_at_life_seq", 0, src["life"]["seq"])
    return {"schema": 1, "source_id": r["source_id"], "source": r["source"], "stock": stock,
            "capacity": 3, "initial_stock": 3, "produced_total": produced, "harvested_total": harvested,
            "growth_remainder_seconds": growth, "installed_at_life_seq": seq,
            "work_position_cm": [0,465000,92], "visual_position_cm": [130,465000,0], "developer_bootstrap": True}

def export_state(src):
    if not isinstance(src,dict) or src.get("world_id")!=EXPECTED_WORLD: raise ValueError("wrong SAO world_id")
    if src.get("schema_version")!=2 or src.get("setting_id")!="sao_aincrad_floor_1": raise ValueError("unsupported SAO profile")
    rs,life=src.get("residents"),src.get("life")
    if not isinstance(rs,list) or len(rs)!=13 or not isinstance(life,dict): raise ValueError("expected 13 residents and life")
    ids=set(); accounts={a.get("resident_id"):a for a in life.get("accounts",[]) if isinstance(a,dict)}
    items=[{k:i.get(k) for k in ("id","kind","owner_id","custodian_id","edge","handle","source")} for i in life.get("items",[]) if isinstance(i,dict)]
    contracts=[{k:c.get(k) for k in ("id","part","item_id","owner_id","worker_id","price_col","reserved_col","status")} for c in life.get("contracts",[]) if isinstance(c,dict)]
    out=[]
    survival=src.get("survival") or {}
    survival_accounts={a.get("resident_id"):a for a in survival.get("accounts",[]) if isinstance(a,dict)}
    for r in rs:
        if not isinstance(r,dict) or not r.get("stable_id") or r["stable_id"] in ids: raise ValueError("invalid or duplicate identity")
        ids.add(r["stable_id"]); rt=r.get("runtime")
        if not isinstance(rt,dict): raise ValueError("runtime missing")
        pos=rt.get("position_cm")
        if not isinstance(pos,list) or len(pos)!=3 or any(isinstance(v,bool) or not isinstance(v,(int,float)) or not math.isfinite(v) for v in pos): raise ValueError("invalid position")
        ac=accounts.get(r["stable_id"],{})
        for k in ("coins_col","wood","iron","kindling","reserved_col"):
            v=r.get("coins_col",0) if k=="coins_col" else ac.get(k,0)
            if isinstance(v,bool) or not isinstance(v,int) or v<0: raise ValueError("invalid account")
        req=[{k:q.get(k) for k in REQ_KEYS if k in q} for q in (rt.get("capability_requests") or [])[:16] if isinstance(q,dict)]
        out.append({"stable_id":r["stable_id"],"name":r.get("name",""),"role":r.get("role",""),"home_id":r.get("home_id",""),"active":bool(rt.get("active",False)),"building_id":rt.get("building_id",""),"position_cm":pos,"phase":rt.get("phase",""),"action":rt.get("last_action",""),"result":rt.get("last_executed_result",""),"decision_status":rt.get("last_result",""),"self_goal":rt.get("self_goal",rt.get("last_goal","")),"self_need":rt.get("self_need",rt.get("last_need","")),"capability_requests":req,"account":{"coins_col":r.get("coins_col",0),"wood":ac.get("wood",0),"iron":ac.get("iron",0),"kindling":ac.get("kindling",0),"reserved_col":ac.get("reserved_col",0)}})
    for row in out:
        own=survival_accounts.get(row["stable_id"])
        resident=next(r for r in rs if r["stable_id"]==row["stable_id"])
        row["survival"]={"installed":own is not None,"hunger_satisfaction":resident.get("needs",{}).get("hunger"),
            "food":own.get("food") if own else None,"energy":own.get("energy") if own else None,
            "source":own.get("source") if own else None}
    buildings=[{k:b.get(k) for k in ("building_id","role","owner_id","position_cm")} for b in src.get("buildings",[]) if isinstance(b,dict)]
    return {"schema":1,"generated_at_utc":datetime.now(timezone.utc).isoformat(),"source_world_mtime":Path(SOURCE_PATH).stat().st_mtime if SOURCE_PATH else None,"world_id":src["world_id"],"setting_id":src["setting_id"],"project_codename":src.get("project_codename",""),"scene":"/Game/ThreeHearths/Maps/L_AincradLevel0","life":{"public_food_resource":_resource(src),"version":life.get("version"),"seq":life.get("seq"),"residents":out,"buildings":buildings,"items":items,"contracts":contracts}}
SOURCE_PATH=None
def main():
    global SOURCE_PATH
    ap=argparse.ArgumentParser(); ap.add_argument("--world",required=True); ap.add_argument("--output",required=True); a=ap.parse_args(); world=Path(a.world).resolve(); dst=Path(a.output).resolve(); SOURCE_PATH=world
    if dst==world or dst.parent==world.parent: raise ValueError("output must be outside formal world directory and not source")
    if dst.exists() and os.path.samefile(world,dst): raise ValueError("output aliases source world")
    out=export_state(json.loads(world.read_text(encoding="utf-8-sig"))); dst.parent.mkdir(parents=True,exist_ok=True); fd,tmp=tempfile.mkstemp(prefix=dst.name+".",suffix=".tmp",dir=str(dst.parent))
    try:
        with os.fdopen(fd,"w",encoding="utf-8",newline="") as f: json.dump(out,f,ensure_ascii=False,indent=2,allow_nan=False); f.write("\n"); f.flush(); os.fsync(f.fileno())
        os.replace(tmp,dst)
    finally:
        if os.path.exists(tmp): os.unlink(tmp)
    print(json.dumps({"output":str(dst),"world_id":out["world_id"],"residents":13,"seq":out["life"]["seq"]},ensure_ascii=False))
if __name__=="__main__": main()
