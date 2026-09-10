"""Read-only comparison of saved survival facts with native pre-Tick startup evidence."""
import argparse
import json
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
VALIDATION = (PROJECT / "Docs/Validation").resolve()


def projection(world):
    return {"world_id": world["world_id"], "life_seq": world["life"]["seq"],
            "survival": world["survival"], "foraging": world.get("foraging"),
            "bodies": [{"resident_id": r["stable_id"], "hunger": r["needs"]["hunger"],
                        "coins_col": r["coins_col"]} for r in world["residents"] if r["runtime"].get("active")]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=["record", "compare"])
    parser.add_argument("--world")
    parser.add_argument("--before")
    parser.add_argument("--log")
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    output = Path(args.output).resolve()
    if not output.is_relative_to(VALIDATION):
        parser.error("output must be under project Docs/Validation")
    inputs = [Path(v).resolve() for v in [args.world, args.before, args.log] if v]
    if output in inputs or any(output.exists() and output.samefile(v) for v in inputs):
        parser.error("output cannot alias input")
    if args.mode == "record":
        if not args.world: parser.error("record needs --world")
        value = projection(json.loads(Path(args.world).read_text(encoding="utf-8-sig")))
    else:
        if not args.before or not args.log: parser.error("compare needs --before --log")
        before = json.loads(Path(args.before).read_text(encoding="utf-8-sig"))
        lines = Path(args.log).read_text(encoding="utf-8-sig", errors="replace").splitlines()
        candidates = [line.split("SURVIVAL_RESUME_STATE ", 1)[1] for line in lines if "SURVIVAL_RESUME_STATE " in line]
        if len(candidates) != 1: parser.error("expected one native pre-Tick startup state")
        after = json.loads(candidates[0])
        fields = ("world_id", "life_seq", "survival", "foraging", "bodies")
        checks = {key: key in before and key in after and before[key] == after[key] for key in fields}
        value = {"passed": all(checks.values()), "checks": checks, "before": before, "native_pre_tick": after,
                 "scope": "exact saved body/resource facts across real cold startup; no offline time grant or initial ration refill"}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"output": str(output), "mode": args.mode, "passed": value.get("passed")}, ensure_ascii=False))
    return 0 if value.get("passed", True) else 1


if __name__ == "__main__":
    raise SystemExit(main())
