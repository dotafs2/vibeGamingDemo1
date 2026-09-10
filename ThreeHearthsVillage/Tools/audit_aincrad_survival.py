from __future__ import annotations

import argparse
import json
import math
import os
from datetime import datetime, timezone
from pathlib import Path

BOOTSTRAP_SOURCE = "developer_survival_bootstrap"
INITIAL_FACT = "initial survival ration=2"
FORAGING_SOURCE_ID = "starter_commons_berry_patch"
FORAGING_SOURCE = "developer_ecosystem_bootstrap"
FORAGING_CAPACITY = 3
FORAGING_GROWTH_SECONDS = 1800
PROJECT_ROOT = Path(__file__).resolve().parents[1]
VALIDATION_ROOT = (PROJECT_ROOT / "Docs" / "Validation").resolve()


def _failure(reason):
    return {"status": "unable_to_verify", "reason": reason}


def _number(value, low, high, integer=False):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return None
    if not math.isfinite(value) or value < low or value > high:
        return None
    if integer and value != math.floor(value):
        return None
    return int(value) if integer else value


def _active(residents):
    result = set()
    for resident in residents:
        if not isinstance(resident, dict) or not isinstance(resident.get("stable_id"), str):
            return None
        if resident.get("runtime", {}).get("active") is True:
            result.add(resident["stable_id"])
    return result


def _check_output_collision(baseline, world, output):
    if output in (baseline, world) or output.parent in (baseline.parent, world.parent):
        raise ValueError("output must not be the source file or source directory")
    if output.exists() and (os.path.samefile(output, baseline) or os.path.samefile(output, world)):
        raise ValueError("output must not be a hardlink to a source file")


def _validate_foraging(world, baseline, life_seq, base_seq):
    if "foraging" in baseline:
        return None, "baseline must precede the one-time foraging installation"
    if "foraging" not in world:
        return None, None
    source = world.get("foraging")
    if not isinstance(source, dict) or _number(source.get("schema_version"), 1, 1, True) is None:
        return None, "foraging schema invalid"
    if source.get("source_id") != FORAGING_SOURCE_ID or source.get("source") != FORAGING_SOURCE:
        return None, "foraging identity or source invalid"
    stock = _number(source.get("stock"), 0, FORAGING_CAPACITY, True)
    capacity = _number(source.get("capacity"), FORAGING_CAPACITY, FORAGING_CAPACITY, True)
    initial_stock = _number(source.get("initial_stock"), FORAGING_CAPACITY, FORAGING_CAPACITY, True)
    produced = _number(source.get("produced_total"), 0, 1_000_000_000, True)
    harvested = _number(source.get("harvested_total"), 0, 1_000_000_000, True)
    remainder = _number(source.get("growth_remainder_seconds"), 0, FORAGING_GROWTH_SECONDS)
    installed_seq = _number(source.get("installed_at_life_seq"), base_seq, life_seq, True)
    if None in (stock, capacity, initial_stock, produced, harvested, remainder, installed_seq):
        return None, "foraging numeric fields invalid"
    if remainder >= FORAGING_GROWTH_SECONDS:
        return None, "foraging growth remainder must be below interval"
    if stock != initial_stock + produced - harvested:
        return None, "foraging stock history mismatch"
    if stock == capacity and remainder != 0:
        return None, "full foraging source must have zero growth remainder"
    return source, None


def _validate_events(world, baseline, active, foraging):
    life = world.get("life")
    if not isinstance(life, dict) or not isinstance(life.get("events"), list):
        return None, "life history missing"
    life_seq = _number(life.get("seq"), 0, 4096, True)
    base_seq = _number(baseline.get("life", {}).get("seq", 0), 0, 4096, True)
    if life_seq is None or base_seq is None or base_seq > life_seq:
        return None, "invalid life sequence"
    events = life["events"]
    baseline_events = baseline.get("life", {}).get("events")
    if not isinstance(baseline_events, list) or len(baseline_events) != base_seq:
        return None, "baseline life event history is malformed"
    seqs = []
    for event in events:
        if not isinstance(event, dict):
            return None, "malformed life event"
        seq = _number(event.get("seq"), 1, 4096, True)
        if seq is None:
            return None, "invalid life event sequence"
        seqs.append(seq)
    if seqs != list(range(1, life_seq + 1)):
        return None, "life event history is truncated or non-contiguous"
    if events[:base_seq] != baseline_events:
        return None, "life event history before survival installation changed"
    applied = life.get("applied")
    if not isinstance(applied, list):
        return None, "life applied history missing"
    applied_by_operation = {}
    for record in applied:
        if not isinstance(record, dict):
            return None, "malformed applied record"
        operation = record.get("operation_id")
        event_seq = _number(record.get("event_seq"), 1, 4096, True)
        if not isinstance(operation, str) or not operation or event_seq is None:
            return None, "invalid applied record"
        if operation in applied_by_operation:
            return None, "duplicate applied operation"
        applied_by_operation[operation] = record
    valid = []
    seen_operations = set()
    for event in events:
        event_type = event.get("type")
        if event_type == "harvest_ration" and foraging is None:
            return None, "harvest event exists without foraging schema"
        if (event_type == "harvest_ration"
                and event["seq"] <= foraging["installed_at_life_seq"]):
            return None, "harvest event predates foraging installation"
        if event["seq"] <= base_seq or event_type not in ("eat_ration", "rest", "harvest_ration"):
            continue
        actor = event.get("actor_id")
        operation = event.get("operation_id")
        record = applied_by_operation.get(operation)
        expected_option = ("harvest_ration:" + FORAGING_SOURCE_ID
                           if event_type == "harvest_ration" else event_type + ":" + str(actor))
        if (actor not in active or not isinstance(operation, str) or operation in seen_operations
                or record is None or record.get("actor_id") != actor
                or record.get("option_id") != expected_option or record.get("event_seq") != event["seq"]):
            return None, "survival event lacks a unique matching applied transaction"
        seen_operations.add(operation)
        valid.append(event)
    return valid, None


def audit(baseline, world, output):
    baseline_path = Path(baseline).resolve(strict=True)
    world_path = Path(world).resolve(strict=True)
    output_path = Path(output).resolve()
    _check_output_collision(baseline_path, world_path, output_path)
    baseline_data = json.loads(baseline_path.read_text(encoding="utf-8-sig"))
    world_data = json.loads(world_path.read_text(encoding="utf-8-sig"))
    if not isinstance(baseline_data, dict) or not isinstance(world_data, dict):
        return _failure("world document must be an object")
    if "survival" in baseline_data:
        return _failure("baseline must precede the one-time survival installation")
    if (baseline_data.get("world_id") != world_data.get("world_id")
            or baseline_data.get("setting_id") != world_data.get("setting_id")):
        return _failure("world identity or setting changed")
    baseline_residents = baseline_data.get("residents")
    world_residents = world_data.get("residents")
    if not isinstance(baseline_residents, list) or not isinstance(world_residents, list):
        return _failure("resident list missing")
    if len(baseline_residents) != 13 or len(world_residents) != 13:
        return _failure("resident count invalid")
    if any(not isinstance(r, dict) or not isinstance(r.get("stable_id"), str) or not r["stable_id"]
           or not isinstance(r.get("runtime"), dict) or not isinstance(r.get("needs"), dict)
           for r in baseline_residents + world_residents):
        return _failure("resident identity, runtime or needs malformed")
    baseline_ids = [r.get("stable_id") for r in baseline_residents]
    world_ids = [r.get("stable_id") for r in world_residents]
    if len(set(baseline_ids)) != 13 or len(set(world_ids)) != 13 or set(baseline_ids) != set(world_ids):
        return _failure("stable IDs changed or are duplicated")
    baseline_active = _active(baseline_residents)
    world_active = _active(world_residents)
    if baseline_active is None or world_active is None or baseline_active != world_active:
        return _failure("active set changed or is malformed")
    survival = world_data.get("survival")
    if not isinstance(survival, dict) or survival.get("schema_version") != 1:
        return _failure("survival schema missing")
    accounts = survival.get("accounts")
    conditions = survival.get("initial_conditions")
    if not isinstance(accounts, list) or not isinstance(conditions, list):
        return _failure("survival arrays missing")
    if len(accounts) != len(world_active) or len(conditions) != len(world_active):
        return _failure("survival array count does not match active residents")
    account_by_id = {}
    for account in accounts:
        resident_id = account.get("resident_id") if isinstance(account, dict) else None
        if resident_id in account_by_id:
            return _failure("account IDs are duplicated")
        food = _number(account.get("food"), 0, 2, True) if isinstance(account, dict) else None
        energy = _number(account.get("energy"), 0, 100, True) if isinstance(account, dict) else None
        if not isinstance(account, dict) or resident_id not in world_active or account.get("source") != BOOTSTRAP_SOURCE:
            return _failure("account identity or source invalid")
        if food is None or energy is None:
            return _failure("account food or energy is invalid")
        account_by_id[resident_id] = account
    condition_ids = set()
    for condition in conditions:
        if not isinstance(condition, dict):
            return _failure("initial condition malformed")
        resident_id = condition.get("resident_id")
        if resident_id in condition_ids or resident_id not in world_active:
            return _failure("initial condition IDs are malformed or duplicated")
        if condition.get("source") != BOOTSTRAP_SOURCE or condition.get("fact") != INITIAL_FACT:
            return _failure("initial condition source or fact invalid")
        condition_ids.add(resident_id)
    if condition_ids != world_active:
        return _failure("initial condition IDs do not match active residents")
    for resident in world_residents:
        if _number(resident.get("needs", {}).get("hunger"), 0, 100) is None:
            return _failure("hunger satisfaction is invalid")
    life_seq = _number(world_data.get("life", {}).get("seq"), 0, 4096, True)
    base_seq = _number(baseline_data.get("life", {}).get("seq", 0), 0, 4096, True)
    if life_seq is None or base_seq is None:
        return _failure("invalid life sequence")
    foraging, foraging_error = _validate_foraging(world_data, baseline_data, life_seq, base_seq)
    if foraging_error:
        return _failure(foraging_error)
    valid_events, event_error = _validate_events(world_data, baseline_data, world_active, foraging)
    if event_error:
        return _failure(event_error)
    eat_events = [event for event in valid_events if event["type"] == "eat_ration"]
    rest_events = [event for event in valid_events if event["type"] == "rest"]
    harvest_events = [event for event in valid_events if event["type"] == "harvest_ration"]
    eat_by_actor = {resident_id: 0 for resident_id in world_active}
    harvest_by_actor = {resident_id: 0 for resident_id in world_active}
    for event in eat_events:
        eat_by_actor[event["actor_id"]] += 1
    for event in harvest_events:
        harvest_by_actor[event["actor_id"]] += 1
    if foraging is None and harvest_events:
        return _failure("harvest event exists without foraging schema")
    if foraging is not None and foraging["harvested_total"] != len(harvest_events):
        return _failure("foraging harvested_total does not match transaction history")
    if sum(account["food"] for account in accounts) != len(world_active) * 2 + len(harvest_events) - len(eat_events):
        return _failure("ration conservation mismatch")
    for resident_id, account in account_by_id.items():
        if account["food"] != 2 + harvest_by_actor[resident_id] - eat_by_actor[resident_id]:
            return _failure("per-resident ration history mismatch")
    if foraging is not None:
        expected_total = len(world_active) * 2 + foraging["initial_stock"] + foraging["produced_total"] - len(eat_events)
        if sum(account["food"] for account in accounts) + foraging["stock"] != expected_total:
            return _failure("combined private food and public stock conservation mismatch")
    residents_by_id = {resident["stable_id"]: resident for resident in world_residents}
    rows = []
    for resident_id in sorted(world_active):
        resident = residents_by_id[resident_id]
        actions = [event["type"] for event in valid_events if event["actor_id"] == resident_id]
        runtime = resident.get("runtime", {})
        self_goal = runtime.get("self_goal")
        legacy_goal = runtime.get("last_goal")
        goal = (self_goal if isinstance(self_goal, str) and self_goal
                else legacy_goal if isinstance(legacy_goal, str) else "")
        rows.append({"stable_id": resident_id, "food": account_by_id[resident_id]["food"],
                     "energy": account_by_id[resident_id]["energy"],
                     "hunger_satisfaction": resident["needs"]["hunger"],
                     "completed_actions": actions,
                     "pending_operation": runtime.get("life_pending_operation", ""),
                     "goal": goal})
    return {"status": "passed", "world_id": world_data["world_id"],
            "generated_at_utc": datetime.now(timezone.utc).isoformat(),
            "baseline_life_seq": baseline_data["life"]["seq"],
            "observed_life_seq": world_data["life"]["seq"],
            "active_residents": sorted(world_active), "initial_rations": len(world_active) * 2,
            "successful_eat_count": len(eat_events), "successful_rest_count": len(rest_events),
            "successful_harvest_count": len(harvest_events),
            "foraging_source_count": 1 if foraging is not None else 0,
            "foraging_present": foraging is not None,
            "public_foraging_source": ({
                "source_id": foraging["source_id"], "source": foraging["source"],
                "stock": foraging["stock"], "capacity": foraging["capacity"],
                "initial_stock": foraging["initial_stock"],
                "produced_total": foraging["produced_total"],
                "harvested_total": foraging["harvested_total"],
                "growth_remainder_seconds": foraging["growth_remainder_seconds"],
                "installed_at_life_seq": foraging["installed_at_life_seq"],
            } if foraging is not None else None),
            "residents": rows}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--world", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    output = Path(args.output).resolve()
    try:
        if not output.is_relative_to(VALIDATION_ROOT):
            raise ValueError("output must be inside ThreeHearthsVillage/Docs/Validation")
        result = audit(args.baseline, args.world, output)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + chr(10), encoding="utf-8")
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(json.dumps(_failure(str(error)), ensure_ascii=False))
        return 2
    print(json.dumps(result, ensure_ascii=False))
    return 0 if result.get("status") == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
