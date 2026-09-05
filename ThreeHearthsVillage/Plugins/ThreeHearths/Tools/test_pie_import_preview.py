"""Offline lifecycle tests. These do not substitute for Unreal PIE validation."""
import json
from pathlib import Path
import struct
import tempfile
from types import SimpleNamespace
import unittest

from hearth_import_session import ImportSession
from pie_import_preview import DEFAULT_SOURCE, inspect_glb, is_usable


class FakeBackend:
    def __init__(self):
        self.current = True
        self.callback = None
        self.visible = "original"
        self.started = True
        self.reports = []
        self.applied = []

    def is_current(self):
        return self.current

    def begin(self, callback):
        self.callback = callback
        return self.started

    def apply(self, objects):
        # Fail after a partial change, so the test requires rollback.
        self.visible = "partially_applied"
        if objects != ["valid_mesh"]:
            raise ValueError("Imported materials are incomplete")
        self.applied.append(objects)
        self.visible = "imported"
        return {"material_slots": 19}

    def restore(self):
        self.visible = "original"

    def write_report(self, report):
        self.reports.append(report)


class LifecycleTests(unittest.TestCase):
    def setUp(self):
        self.now = 100.0
        self.backend = FakeBackend()
        self.session = ImportSession(self.backend, 10, lambda: self.now).begin()

    def test_keeps_original_until_success_then_restores_on_stop(self):
        self.now += 0.1
        self.session.tick()
        self.assertEqual(self.backend.visible, "original")
        self.backend.callback(["valid_mesh"])
        self.assertEqual(self.backend.visible, "original")
        self.now += 0.1
        self.session.tick()
        self.assertEqual(self.backend.visible, "imported")
        self.assertEqual(self.session.report["ticks_while_loading"], 2)
        self.assertAlmostEqual(self.session.report["import_wall_seconds"], 0.2)
        self.session.stop()
        self.assertEqual(self.backend.visible, "original")
        self.assertEqual(self.session.report["outcome"], "ready")

    def test_failed_import_rolls_back_partial_changes(self):
        self.backend.callback([])
        self.session.tick()
        self.assertEqual(self.session.state, "fallback")
        self.assertEqual(self.backend.visible, "original")

    def test_engine_rejects_start(self):
        self.backend.started = False
        session = ImportSession(self.backend).begin()
        self.assertEqual(session.state, "fallback")
        self.assertEqual(self.backend.visible, "original")

    def test_timeout_uses_real_time_and_ignores_late_completion(self):
        self.now += 11
        self.session.tick()
        self.backend.callback(["valid_mesh"])
        self.session.tick()
        self.assertEqual(self.session.state, "fallback")
        self.assertEqual(self.backend.applied, [])

    def test_expired_result_cannot_beat_deadline(self):
        self.now += 9
        self.backend.callback(["valid_mesh"])
        self.now += 2
        self.session.tick()
        self.assertEqual(self.backend.applied, [])
        self.assertEqual(self.session.state, "fallback")

    def test_stop_then_old_callback_cannot_modify_new_preview(self):
        callback = self.backend.callback
        self.session.stop()
        new_session = ImportSession(self.backend, 10, lambda: self.now).begin()
        callback(["valid_mesh"])
        self.assertFalse(self.session.tick())
        new_session.tick()
        self.assertEqual(self.backend.applied, [])
        self.assertEqual(new_session.state, "loading")

    def test_pie_exit_or_village_restart_invalidates_completed_result(self):
        self.backend.callback(["valid_mesh"])
        self.backend.current = False
        self.assertFalse(self.session.tick())
        self.assertEqual(self.backend.applied, [])
        self.assertEqual(self.backend.visible, "original")

    def test_rejects_invalid_deadlines(self):
        for timeout in (0, -1, float("nan"), float("inf")):
            with self.subTest(timeout=timeout), self.assertRaises(ValueError):
                ImportSession(self.backend, timeout)

    def test_cleanup_failure_is_reported_instead_of_claiming_fallback(self):
        def failed_restore():
            raise RuntimeError("Actor restoration failed")
        self.backend.restore = failed_restore
        self.backend.callback([])
        self.session.tick()
        self.assertEqual(self.session.state, "error")
        self.session.stop()
        self.assertEqual(self.session.report["cleanup_error"], "Actor restoration failed")


class FixtureTests(unittest.TestCase):
    def test_destroyed_unreal_wrapper_is_treated_as_gone(self):
        def native_is_valid(obj):
            if obj == 'destroyed_world':
                raise TypeError('World: Internal Error - ObjectInstance is null!')
            return obj == 'living_world'
        ue = SimpleNamespace(SystemLibrary=SimpleNamespace(is_valid=native_is_valid))
        self.assertFalse(is_usable(ue, None))
        self.assertFalse(is_usable(ue, 'destroyed_world'))
        self.assertTrue(is_usable(ue, 'living_world'))

    def test_checked_in_shared_uv_cottage(self):
        result = inspect_glb(DEFAULT_SOURCE)
        self.assertEqual(result["source_meshes"], 45)
        self.assertEqual(result["source_materials"], 19)

    def test_rejects_missing_truncated_and_external_fixtures(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "fixture.glb"
            with self.assertRaises(FileNotFoundError):
                inspect_glb(path)
            path.write_bytes(b"glTF")
            with self.assertRaises(ValueError):
                inspect_glb(path)
            doc = {"asset": {"version": "2.0"}, "meshes": [{}],
                   "materials": [{}], "buffers": [{"uri": "external.bin"}]}
            payload = json.dumps(doc).encode()
            payload += b" " * (-len(payload) % 4)
            header = struct.pack("<5I", 0x46546C67, 2, 20 + len(payload), len(payload), 0x4E4F534A)
            path.write_bytes(header + payload)
            with self.assertRaisesRegex(ValueError, "embed"):
                inspect_glb(path)
            path.write_bytes(header[:-4] + b"FAIL" + payload)
            with self.assertRaisesRegex(ValueError, "JSON chunk"):
                inspect_glb(path)


if __name__ == "__main__":
    unittest.main()
