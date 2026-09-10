import copy
import json
import os
import tempfile
import unittest
from pathlib import Path

from audit_aincrad_survival import audit


class TestAudit(unittest.TestCase):
    def make_world(self):
        residents = [
            {
                "stable_id": f"r{i}",
                "runtime": {
                    "active": i < 3,
                    "last_goal": "legacy",
                    "self_goal": "self goal" if i == 0 else "",
                },
                "needs": {"hunger": 99.5},
            }
            for i in range(13)
        ]
        baseline = {
            "world_id": "w",
            "setting_id": "s",
            "life": {"seq": 0, "events": []},
            "residents": residents,
        }
        world = copy.deepcopy(baseline)
        world["survival"] = {
            "schema_version": 1,
            "accounts": [
                {
                    "resident_id": f"r{i}",
                    "food": 1.0 if i == 0 else 2,
                    "energy": 99.0,
                    "source": "developer_survival_bootstrap",
                }
                for i in range(3)
            ],
            "initial_conditions": [
                {
                    "resident_id": f"r{i}",
                    "source": "developer_survival_bootstrap",
                    "fact": "initial survival ration=2",
                }
                for i in range(3)
            ],
        }
        world["life"] = {
            "seq": 2,
            "events": [
                {
                    "seq": 1,
                    "type": "rest",
                    "actor_id": "r1",
                    "operation_id": "op-rest",
                },
                {
                    "seq": 2,
                    "type": "eat_ration",
                    "actor_id": "r0",
                    "operation_id": "op-eat",
                },
            ],
            "applied": [
                {
                    "operation_id": "op-rest",
                    "actor_id": "r1",
                    "option_id": "rest:r1",
                    "event_seq": 1,
                },
                {
                    "operation_id": "op-eat",
                    "actor_id": "r0",
                    "option_id": "eat_ration:r0",
                    "event_seq": 2,
                },
            ],
        }
        return baseline, world

    def put_files(self, root, baseline, world):
        source = root / "input"
        output = root / "output"
        source.mkdir(parents=True)
        output.mkdir(parents=True)
        (source / "baseline.json").write_text(json.dumps(baseline), encoding="utf-8")
        (source / "world.json").write_text(json.dumps(world), encoding="utf-8")
        return source / "baseline.json", source / "world.json", output / "report.json"

    def make_foraging_world(self):
        baseline, world = self.make_world()
        world["foraging"] = {
            "schema_version": 1,
            "source_id": "starter_commons_berry_patch",
            "source": "developer_ecosystem_bootstrap",
            "stock": 2,
            "capacity": 3,
            "initial_stock": 3,
            "produced_total": 0,
            "harvested_total": 1,
            "growth_remainder_seconds": 17.5,
            "installed_at_life_seq": 2,
        }
        world["survival"]["accounts"][0]["food"] = 2
        world["life"]["seq"] = 3
        world["life"]["events"].append({
            "seq": 3,
            "type": "harvest_ration",
            "actor_id": "r0",
            "operation_id": "op-harvest",
        })
        world["life"]["applied"].append({
            "operation_id": "op-harvest",
            "actor_id": "r0",
            "option_id": "harvest_ration:starter_commons_berry_patch",
            "event_seq": 3,
        })
        return baseline, world

    def test_valid_chain_numeric_goal_and_hardlink_guard(self):
        with tempfile.TemporaryDirectory() as directory:
            baseline, world, output = self.put_files(Path(directory), *self.make_world())
            result = audit(baseline, world, output)
            self.assertEqual(result["status"], "passed")
            self.assertEqual(result["successful_eat_count"], 1)
            self.assertEqual(result["successful_rest_count"], 1)
            self.assertEqual(result["residents"][0]["goal"], "self goal")
            self.assertEqual(result["residents"][1]["goal"], "legacy")
            with self.assertRaises(ValueError):
                audit(baseline, world, world)
            hardlink = output.parent / "hardlink.json"
            os.link(world, hardlink)
            with self.assertRaises(ValueError):
                audit(baseline, world, hardlink)

    def test_rejects_fabrication_duplicate_condition_and_missing_event(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            baseline_data, world_data = self.make_world()
            baseline, world, output = self.put_files(root, baseline_data, world_data)

            fabricated = copy.deepcopy(world_data)
            fabricated["survival"]["accounts"][0]["food"] = 2
            (root / "fabricated.json").write_text(json.dumps(fabricated), encoding="utf-8")
            self.assertEqual(
                audit(baseline, root / "fabricated.json", output)["status"],
                "unable_to_verify",
            )

            duplicate = copy.deepcopy(world_data)
            duplicate["survival"]["initial_conditions"][1] = copy.deepcopy(
                duplicate["survival"]["initial_conditions"][0]
            )
            (root / "duplicate.json").write_text(json.dumps(duplicate), encoding="utf-8")
            self.assertEqual(
                audit(baseline, root / "duplicate.json", output)["status"],
                "unable_to_verify",
            )

            missing = copy.deepcopy(world_data)
            missing["life"]["events"].pop(0)
            (root / "missing.json").write_text(json.dumps(missing), encoding="utf-8")
            self.assertEqual(
                audit(baseline, root / "missing.json", output)["status"],
                "unable_to_verify",
            )

    def test_valid_foraging_chain_and_public_report(self):
        with tempfile.TemporaryDirectory() as directory:
            baseline, world, output = self.put_files(
                Path(directory), *self.make_foraging_world()
            )
            result = audit(baseline, world, output)
            self.assertEqual(result["status"], "passed")
            self.assertEqual(result["successful_harvest_count"], 1)
            self.assertEqual(result["foraging_source_count"], 1)
            self.assertEqual(result["public_foraging_source"]["stock"], 2)
            r0 = next(row for row in result["residents"] if row["stable_id"] == "r0")
            self.assertEqual(r0["completed_actions"], ["eat_ration", "harvest_ration"])

    def test_rejects_foraging_fabrication_stock_applied_and_replay(self):
        cases = {}
        baseline, valid = self.make_foraging_world()

        cases["fabricated private food"] = copy.deepcopy(valid)
        cases["fabricated private food"]["survival"]["accounts"][1]["food"] = 1

        cases["changed source stock"] = copy.deepcopy(valid)
        cases["changed source stock"]["foraging"]["stock"] = 3

        cases["bad source"] = copy.deepcopy(valid)
        cases["bad source"]["foraging"]["source"] = "self_report"

        cases["missing applied"] = copy.deepcopy(valid)
        cases["missing applied"]["life"]["applied"].pop()

        cases["replayed operation"] = copy.deepcopy(valid)
        cases["replayed operation"]["life"]["seq"] = 4
        replay = copy.deepcopy(cases["replayed operation"]["life"]["events"][-1])
        replay["seq"] = 4
        cases["replayed operation"]["life"]["events"].append(replay)

        cases["harvest without schema"] = copy.deepcopy(valid)
        del cases["harvest without schema"]["foraging"]

        cases["harvest not after install"] = copy.deepcopy(valid)
        cases["harvest not after install"]["foraging"]["installed_at_life_seq"] = 3

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for index, (name, candidate) in enumerate(cases.items()):
                case_root = root / str(index)
                baseline_path, world_path, output = self.put_files(
                    case_root, baseline, candidate
                )
                with self.subTest(name=name):
                    self.assertEqual(
                        audit(baseline_path, world_path, output)["status"],
                        "unable_to_verify",
                    )


if __name__ == "__main__":
    unittest.main()
