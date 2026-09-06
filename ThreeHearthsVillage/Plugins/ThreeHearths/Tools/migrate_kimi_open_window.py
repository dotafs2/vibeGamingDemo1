"""One-time migration from the expired night window to capped local development."""
from dataclasses import asdict, replace
import json
import os
import sqlite3

from kimi_budget import DEADLINE, Ledger, NIGHT_POLICY, encoded, fingerprint
from kimi_gateway import LEDGER_PATH

OLD_DEADLINE = 1788658200


def main():
    ledger = Ledger(LEDGER_PATH)
    guard_path = ledger.guard
    guard = json.loads(guard_path.read_text(encoding="utf-8"))
    old_policy = replace(NIGHT_POLICY, deadline_utc=OLD_DEADLINE)
    old_hash = fingerprint(asdict(old_policy))
    assert DEADLINE == 0 and NIGHT_POLICY.deadline_utc == 0
    assert guard["policy_sha256"] == old_hash
    assert guard["policy"] == asdict(old_policy)

    with sqlite3.connect(LEDGER_PATH) as db:
        row = db.execute(
            "SELECT ledger_id,policy_sha256 FROM meta WHERE id=1"
        ).fetchone()
        assert row and row[0] == guard["ledger_id"] and row[1] == old_hash
        assert not db.execute(
            "SELECT 1 FROM requests WHERE state IN ('reserved','uncertain')"
        ).fetchone()
        db.execute(
            "UPDATE meta SET policy_sha256=? WHERE id=1",
            (ledger.policy_hash,),
        )

    updated = {
        "ledger_id": guard["ledger_id"],
        "policy": asdict(NIGHT_POLICY),
        "policy_sha256": ledger.policy_hash,
    }
    temporary = guard_path.with_suffix(".migration.tmp")
    with temporary.open("x", encoding="utf-8") as file:
        file.write(encoded(updated))
        file.flush()
        os.fsync(file.fileno())
    os.replace(temporary, guard_path)
    status = ledger.status()
    assert status["ledger_id"] == guard["ledger_id"]
    assert status["deadline_utc"] == 0 and status["reserved_cny"] == 0
    print(encoded(status))


if __name__ == "__main__":
    main()
