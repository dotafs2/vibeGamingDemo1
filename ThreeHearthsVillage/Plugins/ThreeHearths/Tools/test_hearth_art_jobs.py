import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import hearth_art_jobs as jobs


class HearthArtJobsTests(unittest.TestCase):
    def setUp(self):
        # Keep test artifacts inside the writable project sandbox; some managed
        # Windows runners deny Python's default TEMP ACL even for the owner.
        self.temp = tempfile.TemporaryDirectory(dir=Path.cwd())
        root = Path(self.temp.name)
        self.input = root / "board.json"
        self.catalog = root / "catalog.json"
        self.output = root / "out"
        requests = [
            {"id": "asset-courtyard", "category": "asset", "status": "proposed", "summary": "tile\n kiln courtyard prop", "resolution": "2m\tgrid", "requester_ids": ["npc-01"]},
            {"id": "narrative-01", "category": "narrative", "status": "resolved_existing", "summary": "story", "resolution": "existing", "requester_ids": ["npc-02"]},
            {"id": "action-01", "category": "existing_action", "status": "needs_details", "summary": "action", "resolution": "details", "requester_ids": ["npc-03"]},
            {"id": "mechanic-01", "category": "mechanic", "status": "deferred", "summary": "pathing", "resolution": "runtime", "requester_ids": ["npc-04"]},
            {"id": "clarification-01", "category": "clarification", "status": "proposed", "summary": "question", "resolution": "answer", "requester_ids": ["npc-05"]},
        ]
        requests.extend({"id": f"request-{index:02d}", "category": "narrative", "status": "deferred", "summary": "bounded", "resolution": "later", "requester_ids": ["npc-01"]} for index in range(5, 64))
        self.input.write_text(json.dumps({"schema_version": 1, "world_id": "world-01", "requests": requests}), encoding="utf-8")
        self.catalog.write_text(json.dumps({"components": [
            {"component_asset_id": "workshop_tile_kiln", "source_glb": "modules/workshop_tile_kiln.glb"},
            {"component_asset_id": "floor_timber_2m", "source_glb": "modules/floor_timber_2m.glb"},
        ], "units": "metres", "authoring_axes": "+Z up / -Y front", "glb_to_authoring_coordinates": "(x,-z,y)", "schema_version": 2}), encoding="utf-8")

    def tearDown(self):
        self.temp.cleanup()

    def test_inclusion_and_exclusion(self):
        manifest = jobs.export(self.input, self.output, self.catalog)
        self.assertEqual(["art-8-world-01-15-asset-courtyard"], [item["job_id"] for item in manifest["jobs"]])
        self.assertEqual("workshop_tile_kiln", manifest["jobs"][0]["reuse_first"][0]["asset_id"])
        self.assertEqual("tile kiln courtyard prop", manifest["jobs"][0]["brief"]["objective"])
        self.assertTrue((self.output / jobs.OUTPUT_MD).exists())

    def test_full_runtime_categories_statuses_and_64_bound(self):
        board = jobs.validate_board(json.loads(self.input.read_text(encoding="utf-8")))
        self.assertEqual(64, len(board["requests"]))
        self.assertEqual({"narrative", "existing_action", "asset", "mechanic", "clarification"}, {item["category"] for item in board["requests"][:5]})
        self.assertEqual({"proposed", "resolved_existing", "needs_details", "deferred"}, {item["status"] for item in board["requests"][:5]})

    def test_deterministic_regeneration(self):
        jobs.export(self.input, self.output, self.catalog)
        first = [(path.name, path.read_bytes()) for path in sorted(self.output.iterdir())]
        jobs.export(self.input, self.output, self.catalog)
        self.assertEqual(first, [(path.name, path.read_bytes()) for path in sorted(self.output.iterdir())])

    def test_unsafe_text_and_duplicate_id_rejected(self):
        bad = json.loads(self.input.read_text(encoding="utf-8"))
        bad["requests"][0]["summary"] = "bad\x00text"
        with self.assertRaises(jobs.InputError):
            jobs.validate_board(bad)
        bad["requests"][0]["summary"] = "safe"
        bad["requests"][1]["id"] = bad["requests"][0]["id"]
        with self.assertRaises(jobs.InputError):
            jobs.validate_board(bad)

    def test_invalid_schema_and_catalog_reference_rejected(self):
        with self.assertRaises(jobs.InputError):
            jobs.validate_board({"schema_version": 2, "world_id": "w", "requests": []})
        unsafe_catalog = {"schema_version": 2, "components": [{"component_asset_id": "x", "source_glb": "../../secret.glb"}], "units": "metres", "authoring_axes": "+Z up", "glb_to_authoring_coordinates": "conversion"}
        with self.assertRaises(jobs.InputError):
            jobs._catalog_entries(unsafe_catalog, self.catalog)

    def test_asset_context_is_preserved_and_design_questions_stay_unresolved(self):
        context = {
            "purpose": "木桶 / barrel",
            "target_id": "home-plot-01",
            "resident_contexts": [{
                "resident_id": "_npc_target",
                "name": "",
                "personality": "quiet",
                "inner_story": "",
                "design_goal": "keep the work area legible",
            }],
            "target_position_cm": [120.0, -40, 0],
            "observation_id": "observation-木桶",
        }
        board = json.loads(self.input.read_text(encoding="utf-8"))
        board["requests"] = [{
            "id": "asset-unicode", "category": "asset", "status": "proposed",
            "summary": "木桶 / barrel", "resolution": "model review", "requester_ids": ["_npc_target"],
            "asset_context": context,
        }]
        self.input.write_text(json.dumps(board, ensure_ascii=False), encoding="utf-8")

        manifest = jobs.export(self.input, self.output, self.catalog)
        job = manifest["jobs"][0]
        self.assertEqual(context, job["brief"]["asset_context"])
        self.assertEqual(context, job["provenance"]["asset_context"])
        self.assertEqual("unknown", job["brief"]["design_questions"]["dimensions_cm"])
        self.assertEqual(context["target_position_cm"], job["brief"]["design_questions"]["placement"]["target_snapshot_cm"])
        self.assertEqual("pending definition", job["brief"]["design_questions"]["required_capabilities"])
        self.assertEqual("pending", job["brief"]["return_contract"]["npc_review"])
        markdown = (self.output / jobs.OUTPUT_MD).read_text(encoding="utf-8")
        self.assertIn("NPC and target context:", markdown)
        self.assertIn("Target: `home\\-plot\\-01`", markdown)
        self.assertIn("Return contract:", markdown)

    def test_world_scoped_job_ids_are_unambiguous(self):
        board = json.loads(self.input.read_text(encoding="utf-8"))
        board["requests"] = [board["requests"][0]]
        self.input.write_text(json.dumps(board), encoding="utf-8")
        first = jobs.export(self.input, self.output / "home_a", self.catalog)
        board["world_id"] = "home-01"
        self.input.write_text(json.dumps(board), encoding="utf-8")
        second = jobs.export(self.input, self.output / "home_b", self.catalog)
        self.assertNotEqual(first["jobs"][0]["job_id"], second["jobs"][0]["job_id"])
        self.assertIn("home-01", second["jobs"][0]["job_id"])

    def test_unicode_no_match_has_no_false_catalog_candidates(self):
        board = json.loads(self.input.read_text(encoding="utf-8"))
        board["requests"] = [{
            "id": "asset-plant", "category": "asset", "status": "proposed",
            "summary": "植物 комнаты غرفة", "resolution": "model", "requester_ids": ["_npc"]
        }]
        self.input.write_text(json.dumps(board, ensure_ascii=False), encoding="utf-8")
        manifest = jobs.export(self.input, self.output, self.catalog)
        self.assertEqual([], manifest["jobs"][0]["reuse_first"])
        self.assertEqual("needs_catalog_review", manifest["jobs"][0]["reuse_status"])

    def test_malformed_context_fails_before_output_writes(self):
        board = json.loads(self.input.read_text(encoding="utf-8"))
        board["requests"] = [{
            "id": "asset-bad", "category": "asset", "status": "proposed",
            "summary": "barrel", "resolution": "model", "requester_ids": ["_npc"],
            "asset_context": {
                "purpose": "barrel", "target_id": "plot-home-01", "resident_contexts": [],
                "target_position_cm": [0, 0, 0], "observation_id": "obs",
            },
        }]
        self.input.write_text(json.dumps(board), encoding="utf-8")
        self.output.mkdir(parents=True, exist_ok=True)
        sentinel = self.output / jobs.OUTPUT_JSON
        sentinel.write_text("existing", encoding="utf-8")
        with self.assertRaises(jobs.InputError):
            jobs.export(self.input, self.output, self.catalog)
        self.assertEqual("existing", sentinel.read_text(encoding="utf-8"))

    def test_context_requires_asset_proposed_and_consistent_residents(self):
        context = {
            "purpose": "context",
            "target_id": "plot-home-01",
            "resident_contexts": [{
                "resident_id": "_npc", "name": "", "personality": "",
                "inner_story": "", "design_goal": "",
            }],
            "target_position_cm": [0, 0, 0],
            "observation_id": "",
        }
        board = json.loads(self.input.read_text(encoding="utf-8"))
        board["requests"] = [{
            "id": "narrative-context", "category": "narrative", "status": "proposed",
            "summary": "context", "resolution": "review", "requester_ids": ["_npc"],
            "asset_context": context,
        }]
        with self.assertRaises(jobs.InputError):
            jobs.validate_board(board)

        context["resident_contexts"].append(dict(context["resident_contexts"][0]))
        board["requests"][0]["category"] = "asset"
        with self.assertRaises(jobs.InputError):
            jobs.validate_board(board)

        context["resident_contexts"] = [dict(context["resident_contexts"][0])]
        context["target_position_cm"] = [True, 0, 0]
        with self.assertRaises(jobs.InputError):
            jobs.validate_board(board)

        context["target_position_cm"] = None
        context["purpose"] = "CONTEXT"
        self.assertEqual("CONTEXT", jobs.validate_board(board)["requests"][0]["asset_context"]["purpose"])
        board["requests"][0]["requester_ids"].append("_unrepresented_resident")
        with self.assertRaises(jobs.InputError):
            jobs.validate_board(board)


if __name__ == "__main__":
    unittest.main()
