#!/usr/bin/env python3
"""Record a completed, already-curated run without changing the world or ledger."""
from __future__ import annotations
import argparse
import hashlib
import json
import re
from datetime import datetime, timezone
from pathlib import Path
from summarize_aincrad_iteration import summarize

PROJECT = Path(__file__).resolve().parents[1]
BASE = PROJECT / "Saved/ThreeHearths/AincradLevel0"
PRIVATE = BASE / "TwoHourIteration"
DOCS = PROJECT / "Docs/Validation/Two_Hour_Iteration_2026-09-08"

def read(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))

def write(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", required=True)
    parser.add_argument("--note", required=True, help="Factual result and next task, not a completion claim")
    parser.add_argument("--engine-owner", default="root; no active owned UE process")
    args = parser.parse_args()
    if not re.fullmatch(r"town-\d{8}-\d{6}", args.run):
        parser.error("invalid run identifier")
    world_bytes = (BASE / "world.json").read_bytes()
    world = json.loads(world_bytes.decode("utf-8-sig"))
    run = read(BASE / "Runs" / (args.run + ".json"))
    assert run.get("finished_utc_epoch") and run.get("ue_exit") == 0, "run is not successfully complete"
    assert run.get("world_id") == world["world_id"], "world mismatch"
    assert (DOCS / "PaidRuns" / args.run / "run.json").is_file(), "curate paid evidence first"
    summary = summarize()
    assert summary["latest_run"]["run"] == args.run, "cannot rewind checkpoint to an older run"
    session_path = PRIVATE / "session.json"
    session = read(session_path)
    due = [r["next_ordinary_due_epoch"] for r in summary["active_residents"] if r.get("next_ordinary_due_epoch")]
    utc = lambda epoch: datetime.fromtimestamp(epoch, timezone.utc).isoformat().replace("+00:00", "Z")
    due_text = utc(min(due)) + ".." + utc(max(due)) if due else "no active ordinary deadline"
    session.update(latest_run=args.run, latest_run_status="completed; owned runner children stopped",
                   current_owned_run=None, current_engine_owner=args.engine_owner,
                   next_ordinary_decisions_utc=due_text)
    verification_path = DOCS / "verification.json"
    verification = read(verification_path)
    verification["latest_completed_run"] = args.run
    verification["two_hour_settled_calls"] = summary["session_runs"]["settled_calls"]
    verification["two_hour_new_cost_cny"] = summary["session_runs"]["new_fees_cny"]
    verification.setdefault("continued_runs", {})[args.run] = {
        "seconds": run["seconds"], "calls": run["new_settled_calls"], "cost_cny": run["new_cost_cny"],
        "evidence": "PaidRuns/" + args.run, "life_seq": world["life"]["seq"], "note": args.note}
    checkpoint = {"run": args.run, "world_sha256": hashlib.sha256(world_bytes).hexdigest(),
                  "summary": summary, "note": args.note}
    readme_path = DOCS / "README.md"
    text = readme_path.read_text(encoding="utf-8")
    start = text.index("## 最新状态")
    end = text.index("## 当前批次", start)
    updated = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC")
    status = (f"## 最新状态（{updated}）\n\n"
              f"继续至既定北京时间09:00截止；最新正式run `{args.run}`，本夜累计"
              f"{summary['session_runs']['settled_calls']}次真实请求 / {summary['session_runs']['new_fees_cny']:.7f}元。"
              f"{args.note} 下一自然思考窗口 `{due_text}`；真实新事件按既有规则触发。"
              "以下批次按历史顺序保留，旧段落中的下一次时间不是当前调度。\n\n")
    new_text = text[:start] + status + text[end:]
    assert (BASE / "world.json").read_bytes() == world_bytes, "world changed during checkpoint preparation"
    write(session_path, session)
    write(verification_path, verification)
    write(PRIVATE / (args.run + "-checkpoint.json"), checkpoint)
    readme_path.write_text(new_text, encoding="utf-8")
    print(json.dumps({"run": args.run, "checkpoint_updated": True,
                      "world_unchanged": (BASE / "world.json").read_bytes() == world_bytes,
                      "next_ordinary": due_text, "calls": summary["session_runs"]["settled_calls"],
                      "new_fees_cny": summary["session_runs"]["new_fees_cny"]}, ensure_ascii=False))

if __name__ == "__main__":
    main()
