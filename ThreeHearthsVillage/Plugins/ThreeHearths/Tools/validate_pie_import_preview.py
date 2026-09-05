"""Run explicitly inside an offline village PIE; restarts PIE during validation."""
import json
from pathlib import Path
import struct
import time
import unreal
import pie_import_preview as preview

_runner = None


class Validation:
    def __init__(self):
        self.output = preview.PROJECT_ROOT / 'Saved/ThreeHearths/import-preview/acceptance.json'
        self.rows = []
        self.steps = self.run_steps()
        self.waiting = None
        self.handle = unreal.register_slate_post_tick_callback(self.tick)

    def write(self, status, error=None):
        self.output.write_text(json.dumps({'status':status, 'error':error,
            'engine':unreal.SystemLibrary.get_engine_version(), 'cases':self.rows},
            ensure_ascii=False, indent=2), encoding='utf-8')

    def record(self, name, session, expected, elapsed_before=None):
        assert session.state == expected, (name, session.report)
        backend = session.backend
        if expected in ('fallback', 'stopped'):
            assert not preview.is_usable(unreal, backend.preview_actor), name
            if preview.is_usable(unreal, backend.original):
                assert backend.original.get_editor_property('visible') == backend.was_visible, name
        row = dict(session.report, name=name, worker_count=len(preview._inflight))
        if elapsed_before is not None:
            after = json.loads(backend.village.get_snapshot())
            row['simulation_advanced_seconds'] = after['elapsed'] - elapsed_before
            assert row['simulation_advanced_seconds'] > 0, name
            assert after['api_requests'] == 0, name
        self.rows.append(row)
        self.write('running')

    def settled(self, session):
        return session.state != 'loading' and not preview._inflight

    def run_steps(self):
        # Create only test fixtures in Saved; do not change the shared UV source.
        folder = self.output.parent
        folder.mkdir(parents=True, exist_ok=True)
        truncated = folder / 'truncated.glb'
        truncated.write_bytes(b'glTF')
        empty = folder / 'empty-mesh.glb'
        payload = json.dumps({'asset':{'version':'2.0'},
            'meshes':[{'primitives':[]}], 'materials':[{}]}).encode()
        payload += b' ' * (-len(payload) % 4)
        empty.write_bytes(struct.pack('<5I',0x46546C67,2,20+len(payload),len(payload),0x4E4F534A)+payload)

        session = preview.start()
        before = json.loads(session.backend.village.get_snapshot())['elapsed']
        yield lambda: self.settled(session), 120
        self.record('success', session, 'ready', before)
        preview.stop()
        self.record('restore_after_success', session, 'stopped')

        for name, source in [('missing',folder/'missing.glb'),('truncated',truncated),('empty_geometry',empty)]:
            session = preview.start(source=source)
            yield lambda: self.settled(session), 120
            self.record(name, session, 'fallback')

        session = preview.start(timeout_seconds=0.15)
        yield lambda: self.settled(session), 20
        self.record('timeout_and_worker_cleanup', session, 'fallback')

        session = preview.start()
        worker = session.backend.worker
        rejected = False
        try:
            preview.start()
        except RuntimeError:
            rejected = True
        assert rejected and preview._current is session, 'Duplicate request changed the active preview'
        preview.stop()
        yield lambda: not preview._inflight, 20
        assert worker.poll() is not None
        self.record('duplicate_rejected_and_cancel_cleanup', session, 'stopped')

        session = preview.start()
        worker = session.backend.worker
        session.backend.village.restart_village()
        yield lambda: self.settled(session), 20
        assert worker.poll() is not None
        self.record('village_restart', session, 'stopped')

        session = preview.start()
        worker = session.backend.worker
        levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        levels.editor_request_end_play()
        yield lambda: not levels.is_in_play_in_editor() and not preview._inflight, 20
        assert worker.poll() is not None
        self.record('pie_exit', session, 'stopped')
        rejected = False
        try:
            preview.start()
        except RuntimeError:
            rejected = True
        assert rejected, 'Preview was allowed in the editor world'
        self.rows.append({'name':'reject_editor_world','state':'passed'})

        levels.editor_request_begin_play()
        yield lambda: levels.is_in_play_in_editor() and unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world() is not None, 30
        session = preview.start()
        before = json.loads(session.backend.village.get_snapshot())['elapsed']
        yield lambda: self.settled(session), 120
        self.record('success_after_new_pie', session, 'ready', before)
        # Leave the final successful preview for visual inspection.

    def tick(self, _delta):
        try:
            if self.waiting:
                condition, deadline = self.waiting
                if not condition():
                    if time.monotonic() > deadline:
                        raise TimeoutError('Validation step timed out')
                    return
                self.waiting = None
            condition, timeout = next(self.steps)
            self.waiting = (condition, time.monotonic()+timeout)
        except StopIteration:
            self.write('passed')
            unreal.unregister_slate_post_tick_callback(self.handle)
            unreal.log('[HearthImportValidation] passed: '+str(self.output))
        except Exception as exc:
            preview.stop()
            self.write('failed',str(exc))
            unreal.unregister_slate_post_tick_callback(self.handle)
            unreal.log_error('[HearthImportValidation] '+str(exc))


def run():
    global _runner
    if _runner is not None:
        raise RuntimeError('Use a fresh module/session for a new validation run')
    _runner = Validation()
    return _runner
