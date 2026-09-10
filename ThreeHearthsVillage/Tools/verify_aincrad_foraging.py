"""API-disabled physical foraging check in an isolated existing-world copy."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

PROJECT = Path(__file__).resolve().parents[1]
WORLD = PROJECT / "Saved/ThreeHearths/AincradLevel0/world.json"
OUTPUT = PROJECT / "Docs/Validation/SAO_Overnight_2026-09-10/Foraging"
run_id = "foraging-copy-" + time.strftime("%Y%m%d-%H%M%S", time.gmtime())
copy_path = WORLD.parent / "VerificationWorlds" / (run_id + ".json")
log_path = copy_path.with_suffix(".log")
original = WORLD.read_bytes()
before = json.loads(original.decode("utf-8-sig"))
assert "foraging" not in before, "This first-install check requires a pre-foraging source"
assert all(not r["runtime"].get("pending_operation") and not r["runtime"].get("life_pending_option") for r in before["residents"])
copy_path.parent.mkdir(parents=True, exist_ok=True)
with copy_path.open("xb") as f:
    f.write(original)
args = [r"D:/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe", str(PROJECT / "CropoutSampleProject.uproject"),
        "/Game/ThreeHearths/Maps/L_AincradLevel0", "-game", "-windowed", "-ResX=1600", "-ResY=1000",
        "-RenderOffscreen", "-unattended", "-nosplash", "-nosound", "-nop4", "-HearthDisableApi",
        "-AincradLife", "-AincradSurvival", "-AincradForaging", "-AincradExerciseForaging",
        "-AincradDecisionLimit=0", "-AincradReviewSeconds=100", "-ExecCmds=t.MaxFPS 30",
        "-AincradVerificationWorld=" + str(copy_path), "-abslog=" + str(log_path)]
flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
process = subprocess.Popen(args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, creationflags=flags)
OUTPUT.mkdir(parents=True, exist_ok=True)
print(json.dumps({"run": run_id, "ue_pid": process.pid, "api": False}), flush=True)
try:
    code = process.wait(timeout=150)
finally:
    if process.poll() is None:
        process.terminate()
        try: process.wait(timeout=8)
        except subprocess.TimeoutExpired: process.kill(); process.wait(timeout=8)
after = json.loads(copy_path.read_text(encoding="utf-8-sig"))
events = after["life"]["events"][before["life"]["seq"]:]
harvests = [e for e in events if e.get("type") == "harvest_ration"]
formal_unchanged = hashlib.sha256(WORLD.read_bytes()).digest() == hashlib.sha256(original).digest()
source = after.get("foraging")
log = log_path.read_text(encoding="utf-8-sig", errors="replace")
failures = [line for line in log.splitlines() if any(marker in line for marker in
    ("FORAGING_VISUAL_FAILED", "FORAGING_INIT_FAILED", "FORAGING_TICK_FAILED", "LEVEL0_STORE_REFUSED", "LIFE_SAVE_FAILED", "TOWN_RESIDENT_INIT_FAILED"))]
passed = code == 0 and formal_unchanged and len(harvests) == 1 and source and source["stock"] == 2 and source["harvested_total"] == 1 and not failures
report = {"passed": bool(passed), "source": "manual local verification in independent copy; not Kimi autonomy",
          "run": run_id, "copy_path": str(copy_path), "log_path": str(log_path), "api": False,
          "ue_exit": code, "formal_world_byte_unchanged": formal_unchanged,
          "world_id": after.get("world_id"), "foraging": source, "harvests": harvests,
          "failures": failures, "positions": [{"id": r["stable_id"], "phase": r["runtime"].get("phase"),
          "position_cm": r["runtime"].get("position_cm"), "last_observation_path": r["runtime"].get("last_observation_path"),
          "life_last_outcome": r["runtime"].get("life_last_outcome")} for r in after["residents"] if r["runtime"].get("active")]}
(OUTPUT / (run_id + ".json")).write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
print(json.dumps({"passed": bool(passed), "formal_unchanged": formal_unchanged, "harvests": len(harvests), "report": str(OUTPUT / (run_id + ".json"))}), flush=True)
raise SystemExit(0 if passed else 1)
