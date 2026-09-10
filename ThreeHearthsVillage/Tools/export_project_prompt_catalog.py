"""Export project-owned prompt literals and NPC identity fields; never credentials."""
import json
import re
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
OUT = PROJECT / "Docs/Prompts"
OUT.mkdir(parents=True, exist_ok=True)
MODULE = PROJECT / "Plugins/ThreeHearths/Source/ThreeHearths"
files = sorted((MODULE / "Private").glob("HearthAincrad*.cpp")) + sorted((MODULE / "Public").glob("HearthAincrad*.h"))
files = [p for p in files if "Tests" not in p.name]
assert len(files) <= 400, "Review scope before expanding catalog scan."
cue = re.compile(r"你是|你要|请根据|必须|Return only|system.?prompt|world.?rules|Use only|You are|SAO|Kimi", re.I)
literal = re.compile(r'TEXT\("((?:\\.|[^"\\])*)"\)')
entries = []
source_files = []
for path in files:
    assert path.stat().st_size <= 1024 * 1024, f"Source exceeds bounded scan: {path.name}"
    text = path.read_text(encoding="utf-8-sig")
    found = []
    for match in literal.finditer(text):
        try:
            value = json.loads('"' + match.group(1) + '"')
        except ValueError:
            continue
        if len(value) < 60 or not cue.search(value):
            continue
        found.append({"line": text.count("\n", 0, match.start()) + 1, "text": value})
    if found:
        relative = path.relative_to(PROJECT).as_posix()
        source_files.append(relative)
        entries.extend({"source": relative, **row} for row in found)
catalog = {
    "scope": "Bounded project C++ prompt/instruction literal candidates; excludes platform instructions and credentials.",
    "limitations": "Runtime concatenation, structured personal data and images remain in source and request evidence; not a complete chat transcript export.",
    "current_resident_runtime": "Plugins/ThreeHearths/Source/ThreeHearths/Private/HearthAincradResidentRuntime.cpp",
    "source_files": source_files,
    "entries": entries,
}
catalog['scope'] = 'Current Aincrad C++ prompt/instruction candidates only; legacy village and API test prompts excluded.'
(OUT / "runtime_prompt_catalog.json").write_text(json.dumps(catalog, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
state_path = PROJECT / "Saved/ThreeHearths/AincradLevel0/world.json"
if state_path.exists():
    assert state_path.stat().st_size <= 1024 * 1024
    state = json.loads(state_path.read_text(encoding="utf-8-sig"))
    keys = ("stable_id", "name", "personality", "story", "role", "faction", "home_id")
    profiles = {"world_id": state["world_id"], "setting_id": state["setting_id"],
                "source": "Selected stable identity and personal story fields from the existing SAO world; not a replacement save.",
                "residents": [{k: row[k] for k in keys} for row in state["residents"]]}
    (OUT / "resident_profiles.json").write_text(json.dumps(profiles, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print(json.dumps({"prompt_candidates": len(entries), "source_files": len(source_files), "profiles": (OUT / "resident_profiles.json").exists()}))
