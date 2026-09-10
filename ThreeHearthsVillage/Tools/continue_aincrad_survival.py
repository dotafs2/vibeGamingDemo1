#!/usr/bin/env python3
"""Continue serial native runs with one deadline and verified checkpoints."""
import argparse
import json
import os
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
REPO = PROJECT.parent
TOOLS = PROJECT / "Tools"
WORLD = PROJECT / "Saved/ThreeHearths/AincradLevel0/world.json"
BASELINE = WORLD.with_name("world.json.pre-survival-v1")
VALIDATION = PROJECT / "Docs/Validation/SAO_Overnight_2026-09-10"
RUNS = WORLD.parent / "Runs"
PUBLIC_RUN_KEYS = {"run", "ue_pid", "gateway_pid", "api", "exit", "new_settled_calls", "new_cost_cny", "owned_children_stopped"}
FLAGS = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0


def command(arguments):
    return [sys.executable, "-X", "utf8", "-B", *map(str, arguments)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stop-at-utc", required=True, type=int)
    parser.add_argument("--seconds", type=int, default=1800)
    parser.add_argument("--max-decisions", type=int, default=24)
    parser.add_argument("--api", action="store_true")
    parser.add_argument("--foraging", action="store_true")
    args = parser.parse_args()
    if not 30 <= args.seconds <= 1800:
        parser.error("--seconds must be 30..1800")
    if not 0 <= args.max_decisions <= 24:
        parser.error("--max-decisions must be 0..24")
    if args.stop_at_utc <= time.time():
        parser.error("--stop-at-utc is expired")
    if not WORLD.is_file() or not BASELINE.is_file():
        parser.error("existing world and pre-survival baseline are required")

    summary = {"status": "running", "stop_at_utc": args.stop_at_utc,
               "api": args.api, "controller_pid": os.getpid(), "runs": []}
    VALIDATION.mkdir(parents=True, exist_ok=True)

    def checkpoint(stage, **fields):
        summary.update(fields)
        summary["stage"] = stage
        summary["updated_at_utc"] = datetime.now(timezone.utc).isoformat()
        temporary = VALIDATION / "loop-summary.tmp"
        temporary.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        os.replace(temporary, VALIDATION / "loop-summary.json")

    def verify(arguments):
        remaining = args.stop_at_utc - time.time()
        if remaining < 1:
            return False
        try:
            result = subprocess.run(command(arguments), cwd=REPO, capture_output=True,
                                    timeout=min(20, remaining), creationflags=FLAGS)
            return result.returncode == 0
        except (OSError, subprocess.TimeoutExpired):
            return False

    reason = "deadline"
    checkpoint("ready")
    while args.stop_at_utc - time.time() >= 90:
        duration = min(args.seconds, int(args.stop_at_utc - time.time() - 30))
        arguments = [TOOLS / "run_aincrad_residents.py", "--seconds", duration,
                     "--life", "--survival", "--budget-profile", "overnight",
                     "--max-decisions", args.max_decisions, "--stop-at-utc", args.stop_at_utc]
        if args.api:
            arguments.append("--api")
        if args.foraging:
            arguments.append("--foraging")
        run_id = None
        checkpoint("native_run", active_run=None)
        # The existing runner owns its UE/gateway children and enforces this deadline.
        # Never kill the runner or retry a failed paid run from this controller.
        with subprocess.Popen(command(arguments), cwd=REPO, stdout=subprocess.PIPE,
                              stderr=subprocess.DEVNULL, text=True, encoding="utf-8",
                              errors="replace", creationflags=FLAGS) as child:
            for line in child.stdout:
                try:
                    event = json.loads(line)
                except ValueError:
                    continue
                if not isinstance(event, dict):
                    continue
                public = {key: value for key, value in event.items() if key in PUBLIC_RUN_KEYS}
                if public:
                    print(json.dumps(public, ensure_ascii=False), flush=True)
                if isinstance(event.get("run"), str):
                    run_id = event["run"]
                    checkpoint("native_run", active_run=run_id)
            exit_code = child.wait()
        if exit_code != 0:
            reason = "pause_requested" if exit_code == 75 else f"runner_nonzero:{exit_code}"
            break
        if not run_id or Path(run_id).name != run_id or not run_id.startswith("town-"):
            reason = "runner_output_missing_run_id"
            break
        try:
            metadata = json.loads((RUNS / (run_id + ".json")).read_text(encoding="utf-8-sig"))
        except (OSError, ValueError):
            reason = "run_metadata_invalid"
            break
        record = {"run": run_id, "new_cost_cny": metadata.get("new_cost_cny"),
                  "new_settled_calls": metadata.get("new_settled_calls"), "verified": False}
        summary["runs"].append(record)
        checkpoint("verifying")
        if metadata.get("ue_exit") != 0 or not metadata.get("finished_utc_epoch"):
            reason = "run_metadata_not_successful"
            break
        if args.api and not verify([TOOLS / "curate_aincrad_life_run.py", "--run", run_id,
                                    "--output-root", VALIDATION / "PaidRuns"]):
            reason = "curator_failed_or_deadline"
            break
        if not verify([TOOLS / "audit_aincrad_survival.py", "--baseline", BASELINE,
                       "--world", WORLD, "--output", VALIDATION / "Survival" / (run_id + ".json")]):
            reason = "audit_failed_or_deadline"
            break
        record["verified"] = True
        checkpoint("verified", active_run=None)
        print(json.dumps({"run": run_id, "checkpoint_verified": True}), flush=True)
    status = "completed" if reason == "deadline" else "stopped"
    checkpoint("finished", status=status, stop_reason=reason, active_run=None)
    print(json.dumps({"status": status, "stop_reason": reason,
                      "runs": [record["run"] for record in summary["runs"]]}), flush=True)
    return 0 if reason == "deadline" else 1


if __name__ == "__main__":
    raise SystemExit(main())
