#!/usr/bin/env python3
"""Bounded read-only watcher for the SAO snapshot exporter."""
import argparse,datetime,os,subprocess,sys,time,math
from pathlib import Path

def utc(s):
    parsed=datetime.datetime.fromisoformat(s.replace("Z","+00:00"))
    if parsed.tzinfo is None: raise ValueError("deadline needs an explicit UTC offset")
    return parsed.astimezone(datetime.timezone.utc)
def safe_paths(world,output):
    w=Path(world).resolve(); o=Path(output).resolve()
    if o==w or o.parent==w.parent: raise ValueError("output must be outside formal world directory")
    if o.exists() and os.path.samefile(w,o): raise ValueError("output aliases source world")
    return w,o
def run_once(world,output):
    exporter=Path(__file__).with_name("export_sao_life_snapshot.py")
    flags=subprocess.CREATE_NO_WINDOW if os.name=="nt" else 0
    return subprocess.run([sys.executable,str(exporter),"--world",str(world),"--output",str(output)],capture_output=True,text=True,timeout=10,creationflags=flags)
def main():
    ap=argparse.ArgumentParser(); ap.add_argument("--world",required=True); ap.add_argument("--output",required=True); ap.add_argument("--stop-at-utc",required=True); ap.add_argument("--interval",type=float,default=5.0); a=ap.parse_args()
    if not math.isfinite(a.interval) or a.interval<5: ap.error("--interval must be >= 5 seconds")
    deadline=utc(a.stop_at_utc); world,output=safe_paths(a.world,a.output)
    if not world.is_file(): raise ValueError("world input does not exist")
    last_mtime=None; last_log=0.0
    while (deadline-datetime.datetime.now(datetime.timezone.utc)).total_seconds()>12:
        try: mtime=world.stat().st_mtime
        except OSError as e: print(f"watch: source unavailable; retrying ({e})",file=sys.stderr); time.sleep(max(0,min(5,(deadline-datetime.datetime.now(datetime.timezone.utc)).total_seconds()))); continue
        if last_mtime is None or mtime!=last_mtime:
            try:
                p=run_once(world,output)
                if p.returncode: raise RuntimeError((p.stderr or p.stdout or "export failed").strip()[:200])
                last_mtime=mtime
                if time.monotonic()-last_log>=60: print(f"watch: snapshot refreshed from mtime {mtime}",flush=True); last_log=time.monotonic()
            except Exception as e:
                now=time.monotonic()
                if now-last_log>=5: print(f"watch: export failed; retaining prior snapshot ({e})",file=sys.stderr,flush=True); last_log=now
        remaining=(deadline-datetime.datetime.now(datetime.timezone.utc)).total_seconds()
        if remaining<=0: break
        time.sleep(min(a.interval,remaining))
    print("watch: deadline reached; stopped without further writes",flush=True)
if __name__=="__main__":
    try: main()
    except KeyboardInterrupt: print("watch: interrupted safely",file=sys.stderr)
    except (ValueError, OSError) as e: print(f"watch: {e}",file=sys.stderr); raise SystemExit(2)
