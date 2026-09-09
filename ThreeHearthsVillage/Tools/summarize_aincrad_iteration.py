#!/usr/bin/env python3
"""Print a compact, read-only summary of the current Aincrad iteration."""

from __future__ import annotations

import argparse
import json
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1] / "Saved" / "ThreeHearths" / "AincradLevel0"
RESULT_LIMIT = 160


def load(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8-sig") as stream:
        value = json.load(stream)
    return value if isinstance(value, dict) else {}


def crop(value: Any, limit: int = RESULT_LIMIT) -> Any:
    if value is None:
        return None
    text = str(value).replace("\r", " ").replace("\n", " ").strip()
    return text if len(text) <= limit else text[: limit - 1].rstrip() + "…"


def epoch(value: Any) -> float | None:
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def utc_epoch(value: str | None) -> float | None:
    if not value:
        return None
    try:
        return datetime.fromisoformat(value.replace("Z", "+00:00")).timestamp()
    except ValueError:
        return None


def run_files() -> list[Path]:
    return sorted((ROOT / "Runs").glob("*.json"))


def summarize() -> dict[str, Any]:
    world = load(ROOT / "world.json")
    session = load(ROOT / "TwoHourIteration" / "session.json")
    world_id = world.get("world_id")
    start = utc_epoch(session.get("start_utc"))

    residents = []
    for resident in world.get("residents", []):
        runtime = resident.get("runtime") or {}
        if not runtime.get("active"):
            continue
        residents.append(
            {
                "name": resident.get("name"),
                "phase": runtime.get("phase"),
                "last_action": runtime.get("last_action"),
                "last_executed_result": crop(runtime.get("last_executed_result")),
                "last_think_utc": runtime.get("last_think_utc"),
                "next_ordinary_due_epoch": (epoch(runtime.get("last_think_utc")) + 1800) if epoch(runtime.get("last_think_utc")) is not None else None,
            }
        )

    contracts = []
    for contract in (world.get("life") or {}).get("contracts", []):
        contracts.append(
            {
                "id": contract.get("id"),
                "part": contract.get("part"),
                "status": contract.get("status"),
                "price": contract.get("price_col", contract.get("price")),
            }
        )

    runs = []
    for path in run_files():
        try:
            run = load(path)
        except (OSError, json.JSONDecodeError):
            continue
        if run.get("world_id") != world_id:
            continue
        started = epoch(run.get("started_utc_epoch"))
        finished = epoch(run.get("finished_utc_epoch"))
        runs.append((started if started is not None else float("-inf"), path.name, run, finished))
    runs.sort(key=lambda item: (item[0], item[1]))
    ended = [item for item in runs if item[3] is not None and (start is None or item[0] >= start)]
    latest = runs[-1][2] if runs else {}
    latest_name = latest.get("run") if latest else None
    budget_runs = [item for item in runs if item[3] is not None and item[2].get("budget_after")]
    budget_item = budget_runs[-1] if budget_runs else None
    latest_budget = budget_item[2].get("budget_after") if budget_item else {}
    return {
        "world_id": world_id,
        "elapsed_seconds": world.get("elapsed_seconds"),
        "session_stop_at_utc": session.get("stop_at_utc"),
        "due_note": "ordinary 1800-second minimum for current active residents; eligible events/follow-up may occur sooner",
        "active_residents": residents,
        "life": {
            "seq": (world.get("life") or {}).get("seq"),
            "contracts": contracts,
        },
        "session_runs": {
            "ended_same_world": len(ended),
            "settled_calls": sum(int(item[2].get("new_settled_calls") or 0) for item in ended),
            "new_fees_cny": round(sum(float(item[2].get("new_cost_cny") or 0) for item in ended), 8),
        },
        "latest_run": {
            "run": latest_name,
            "finished": latest.get("finished_utc_epoch") if latest else None,
            "ue_exit": latest.get("ue_exit") if latest else None,
        },
        "latest_budget_snapshot": {
            "run": budget_item[2].get("run") if budget_item else None,
            "as_of_utc_epoch": budget_item[3] if budget_item else None,
            "settled": latest_budget.get("settled_cny"),
            "liability": latest_budget.get("liability_cny"),
            "counts": latest_budget.get("counts"),
        },
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pretty", action="store_true", help="indent JSON for human reading")
    args = parser.parse_args()
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    print(json.dumps(summarize(), ensure_ascii=False, indent=2 if args.pretty else None, separators=None if args.pretty else (",", ":")))


if __name__ == "__main__":
    main()
