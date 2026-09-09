"""Capture V2 characters in a private world copy, with API disabled.

This records helper checks, not an artistic approval. Inspect the original PNGs.
Never alters formal identity, budget, cooldowns, or an unrelated process.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "Saved/ThreeHearths/AincradLevel0"
p = argparse.ArgumentParser()
p.add_argument("--name", default="characters_v2_" + time.strftime("%Y%m%d_%H%M%S", time.gmtime()))
p.add_argument("--seconds", type=int, default=38)
p.add_argument("--stop-at-utc", type=int, required=True)
p.add_argument("--workshop", action="store_true", help="also capture the installed smithy_cold_forge scene context")
p.add_argument("--trade-signs", action="store_true", help="also capture the three installed trade signs in scene context")
p.add_argument("--axe", action="store_true", help="also capture the existing axe appearance and condition metadata")
p.add_argument("--held-tool-look", action="store_true", help="exercise the real eye held-tool look only in this API-disabled world copy")
args = p.parse_args()
assert args.name and all(c.isalnum() or c == "_" for c in args.name), "Use letters, numbers and underscore in a unique copy name"
assert 20 <= args.seconds <= 120
assert time.time() + args.seconds + 50 < args.stop_at_utc, "Insufficient time before authorized stop"
formal = BASE / "world.json"
original = formal.read_bytes()
world = json.loads(original)
assert len(world["residents"]) == 13
copy = BASE / "VerificationWorlds" / (args.name + ".json")
output = BASE / "CharacterReview" / args.name
record = BASE / "TwoHourIteration" / (args.name + "_process.json")
assert not copy.exists() and not output.exists() and not record.exists(), "Do not overwrite previous evidence"
copy.parent.mkdir(parents=True, exist_ok=True)
copy.write_bytes(original)
log = BASE / "TwoHourIteration" / (args.name + ".log")
stdout_log = log.with_name(log.stem + "_stdout.log")
editor = next((x for x in (Path(r"D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"), Path(r"C:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe")) if x.is_file()), None)
assert editor, "UnrealEditor.exe unavailable"
command = [str(editor), str(ROOT / "CropoutSampleProject.uproject"), "/Game/ThreeHearths/Maps/L_AincradLevel0", "-game", "-windowed", "-ResX=1600", "-ResY=1000", "-RenderOffscreen", "-unattended", "-nosplash", "-nosound", "-nop4", "-ExecCmds=t.MaxFPS 30", "-AincradReviewSeconds=" + str(args.seconds), "-HearthDisableApi", "-AincradLife", "-AincradCharactersV2", "-AincradCharacterYaw=-90", "-AincradCharacterReview", "-AincradVerificationWorld=" + str(copy), "-AincradStopUtc=" + str(args.stop_at_utc), "-abslog=" + str(log)]
if args.workshop:
    command.append("-AincradWorkshopReview")
if args.trade_signs:
    command.append("-AincradTradeSignsReview")
if args.axe:
    command.append("-AincradAxeReview")
if args.held_tool_look:
    command.append("-AincradExerciseHeldToolLook")
meta = {"source": "local_verification", "world_id": world["world_id"], "formal_sha256_before": hashlib.sha256(original).hexdigest(), "verification_world": str(copy), "output": str(output), "api_enabled": False, "workshop_review_requested": args.workshop, "trade_sign_review_requested": args.trade_signs, "axe_review_requested": args.axe, "held_tool_look_exercise_requested": args.held_tool_look, "stop_at_utc": args.stop_at_utc, "started_utc_epoch": time.time(), "artistic_approval": False}
owned = None
try:
    with stdout_log.open("wb") as stream:
        owned = subprocess.Popen(command, stdout=stream, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
        meta["owned_pid"] = owned.pid
        record.write_text(json.dumps(meta, ensure_ascii=False, indent=2), encoding="utf-8")
        print("owned_ue_pid", owned.pid, flush=True)
        try:
            meta["exit_code"] = owned.wait(timeout=min(args.seconds + 80, max(1, args.stop_at_utc - time.time() - 10)))
        except subprocess.TimeoutExpired:
            meta["timed_out"] = True
            owned.terminate()
            try: owned.wait(timeout=5)
            except subprocess.TimeoutExpired: owned.kill(); owned.wait(timeout=5)
            meta["exit_code"] = owned.returncode
finally:
    if owned is not None and owned.poll() is None:
        owned.kill(); owned.wait(timeout=5)
    meta["finished_utc_epoch"] = time.time()
    meta["formal_sha256_after"] = hashlib.sha256(formal.read_bytes()).hexdigest()
    meta["formal_world_unchanged"] = meta["formal_sha256_before"] == meta["formal_sha256_after"]
    meta["owned_child_stopped"] = owned is None or owned.poll() is not None
    index = output / "index.json"
    if index.is_file():
        data = json.loads(index.read_text(encoding="utf-8-sig"))
        meta["helper_passed"] = data.get("passed", False)
        meta["capture_pipeline_passed"] = data.get("cpu_pose_and_capture_pipeline_passed", False)
        meta["automatic_animation_playback_validated"] = data.get("automatic_animation_playback_validated", False)
        if args.workshop:
            meta["workshop_review_status"] = data.get("workshop_review", {}).get("status", "missing")
        if args.trade_signs:
            trade_sign_review = data.get("trade_sign_review", {})
            meta["trade_sign_review_status"] = trade_sign_review.get("status", "missing")
            meta["trade_sign_captured_count"] = trade_sign_review.get("captured_count", 0)
            meta["trade_sign_all_three_captured"] = trade_sign_review.get("all_three_captured", False)
        if args.axe:
            axe_review = data.get("axe_review", {})
            meta["axe_review_status"] = axe_review.get("status", "missing")
            meta["axe_capture_completed"] = axe_review.get("render_capture_completed", False)
            meta["axe_parts_valid_for_condition"] = axe_review.get("parts_valid_for_condition", False)
        meta["png_count"] = len(list(output.glob("*.png")))
    else: meta["helper_passed"] = False; meta["capture_pipeline_passed"] = False
    record.write_text(json.dumps(meta, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps(meta, ensure_ascii=False), flush=True)
assert meta["formal_world_unchanged"] and meta["owned_child_stopped"]
assert meta.get("exit_code") == 0 and meta["capture_pipeline_passed"] and (not args.trade_signs or meta.get("trade_sign_all_three_captured", False)) and (not args.axe or meta.get("axe_capture_completed", False)), "Inspect log and original evidence; not approved"
