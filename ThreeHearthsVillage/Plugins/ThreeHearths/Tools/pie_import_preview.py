"""Manual editor-only GLB preview. Importing this module does not start UE or PIE.

In a running village PIE: import pie_import_preview as preview; preview.start()
See Docs/PIE_Import_Preview.md for usage and the recorded UE acceptance results.
"""
import json
import math
from pathlib import Path
import struct
import subprocess
import uuid

from hearth_import_session import ImportSession

PROJECT_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_SOURCE = PROJECT_ROOT / "Art/HearthCottage/HearthCottage_SharedUV.glb"
COTTAGE_ASSET = "/Game/ThreeHearths/Generated/HearthCottage/SM_HearthCottage_SharedUV.SM_HearthCottage_SharedUV"
_current = None
_ticker = None
_inflight = {}  # Track only worker processes created by this module.


def is_usable(ue, obj):
    # A Python wrapper can survive EndPIE after its native UObject has gone.
    # In that case even passing it to IsValid raises during argument conversion.
    if obj is None:
        return False
    try:
        return ue.SystemLibrary.is_valid(obj)
    except (TypeError, RuntimeError):
        return False


def inspect_glb(source):
    """Read a bounded local fixture; Interchange still validates its mesh payload."""
    path = Path(source).resolve()
    if path.suffix.lower() != ".glb":
        raise ValueError("Preview requires a local .glb file")
    size = path.stat().st_size
    if not 20 <= size <= 8 * 1024 * 1024:
        raise ValueError("Preview GLB must be between 20 bytes and 8 MiB")
    with path.open("rb") as stream:
        magic, version, length, chunk_size, chunk_type = struct.unpack("<5I", stream.read(20))
        if magic != 0x46546C67 or version != 2 or length != size:
            raise ValueError("Invalid GLB v2 header or file length")
        if chunk_type != 0x4E4F534A or chunk_size > size - 20 or chunk_size % 4:
            raise ValueError("Invalid GLB JSON chunk")
        document = json.loads(stream.read(chunk_size))
    if not isinstance(document, dict) or document.get("asset", {}).get("version") != "2.0":
        raise ValueError("Expected a glTF 2.0 document")
    for item in document.get("buffers", []) + document.get("images", []):
        if "uri" in item:
            raise ValueError("Preview fixture must embed buffers/images in the GLB")
    meshes, materials = len(document.get("meshes", [])), len(document.get("materials", []))
    if not 1 <= meshes <= 64 or not 1 <= materials <= 32:
        raise ValueError("Preview requires 1-64 meshes and 1-32 materials")
    return {"source": str(path), "bytes": size, "source_meshes": meshes,
            "source_materials": materials}


class UnrealPreview:
    def __init__(self, ue, source):
        self.ue = ue
        self.source = Path(source).resolve()
        self.run_id = uuid.uuid4().hex
        self.preview_actor = None
        self.worker = None
        self.worker_log = None
        self.metadata = {}
        self.levels = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
        self.editor = ue.get_editor_subsystem(ue.UnrealEditorSubsystem)
        if not self.levels.is_in_play_in_editor():
            raise RuntimeError("Start village PIE before requesting a preview")
        self.world = self.editor.get_game_world()
        if not is_usable(ue, self.world):
            raise RuntimeError("A local PIE world is required")
        villages = ue.GameplayStatics.get_all_actors_of_class(self.world, ue.HearthVillage)
        if len(villages) != 1:
            raise RuntimeError("Expected one Three Hearths village in PIE")
        self.village = villages[0]
        snapshot = json.loads(self.village.get_snapshot())
        if snapshot["backend"] != "local_personality_policy" or snapshot["pending_request_count"]:
            raise RuntimeError("Run this validation with an isolated, disabled API config")
        self.village_run = snapshot["run"]
        candidates = []
        for actor in ue.GameplayStatics.get_all_actors_of_class(self.world, ue.StaticMeshActor):
            component = actor.static_mesh_component
            mesh = component.get_editor_property("static_mesh")
            if mesh and mesh.get_path_name() == COTTAGE_ASSET:
                candidates.append(component)
        if len(candidates) != 1:
            raise RuntimeError("Expected the saved Hearth Cottage once in the default map")
        self.original = candidates[0]
        self.was_visible = self.original.get_editor_property("visible")
        self.transform = self.original.get_world_transform()
        self.destination = "/Game/Developers/ThreeHearthsImportPreview/" + self.run_id
        self.report_path = PROJECT_ROOT / "Saved/ThreeHearths/import-preview" / (self.run_id + ".json")
        self.job_dir = self.report_path.with_suffix('')

    def is_current(self):
        ue = self.ue
        return (is_usable(ue, self.world)
                and self.editor.get_game_world() == self.world
                and is_usable(ue, self.village)
                and is_usable(ue, self.original)
                and json.loads(self.village.get_snapshot())["run"] == self.village_run)

    def begin(self, on_complete):
        ue = self.ue
        self.metadata = inspect_glb(self.source)
        worker_exe = Path(ue.Paths.engine_dir()).resolve() / 'Binaries/Win64/UnrealEditor-Cmd.exe'
        if not worker_exe.is_file():
            raise RuntimeError('This preview requires the Windows Unreal Editor commandlet')
        self.job_dir.mkdir(parents=True, exist_ok=False)
        request_path = self.job_dir / 'request.json'
        request_path.write_text(json.dumps({'source':str(self.source), 'destination':self.destination}), encoding='utf-8')
        args = [str(worker_exe), str(PROJECT_ROOT / 'CropoutSampleProject.uproject'),
                '-run=pythonscript', '-script=' + str(Path(__file__).with_name('hearth_import_worker.py')),
                '-HearthImportRequest=' + str(request_path), '-unattended', '-nullrhi',
                '-nosound', '-NoSplash', '-NoShaderCompile', '-NoAssetRegistryCache',
                '-abslog=' + str(self.job_dir / 'worker.log'),
                '-ini:Engine:[/Script/PythonScriptPlugin.PythonScriptPluginSettings]:bRemoteExecution=False']
        self.worker_log = (self.job_dir / 'stdout.log').open('wb')
        try:
            self.worker = subprocess.Popen(args, stdin=subprocess.DEVNULL, stdout=self.worker_log,
                stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
        except Exception:
            self.worker_log.close()
            raise
        self.done = on_complete
        self.metadata.update(worker_pid=self.worker.pid, import_mode='isolated_editor_worker')
        _inflight[self.run_id] = self
        return True

    def poll_worker(self):
        if self.worker is None or self.worker.poll() is None:
            return
        self.worker_log.close()
        _inflight.pop(self.run_id, None)
        if not self.is_current() or _current is None or _current.backend is not self or _current.state != 'loading':
            return
        result_path = self.job_dir / 'result.json'
        if self.worker.returncode != 0 or not result_path.exists():
            _current.fallback(f'Import worker exited with code {self.worker.returncode}; see {self.job_dir / "worker.log"}')
            return
        result = json.loads(result_path.read_text(encoding='utf-8'))
        self.metadata['worker_import_seconds'] = result.get('import_seconds')
        if not result.get('success'):
            _current.fallback(result.get('error', 'Import worker failed'))
            return
        registry = self.ue.AssetRegistryHelpers.get_asset_registry()
        registry.scan_paths_synchronous([self.destination], True)
        objects = [self.ue.load_asset(path) for path in result['assets']]
        self.done([obj for obj in objects if obj is not None])

    def apply(self, objects):
        ue = self.ue
        meshes = [obj for obj in objects if isinstance(obj, ue.StaticMesh)]
        if len(meshes) != 1:
            raise RuntimeError(f"Expected one combined mesh, received {len(meshes)}")
        mesh = meshes[0]
        slots = mesh.get_editor_property("static_materials")
        if len(slots) != self.metadata["source_materials"] or any(
                not slot.get_editor_property("material_interface") for slot in slots):
            raise RuntimeError("Imported material slots are missing or incomplete")
        extent = mesh.get_bounds().box_extent
        original_extent = self.original.get_editor_property("static_mesh").get_bounds().box_extent
        for axis in ("x", "y", "z"):
            value, expected = getattr(extent, axis), getattr(original_extent, axis)
            if not math.isfinite(value) or value <= 0 or abs(value - expected) > max(2.0, expected * 0.05):
                raise RuntimeError("Imported bounds do not match the cottage fixture (units/orientation)")
        self.preview_actor = ue.HearthImportPreviewLibrary.spawn_preview(self.original, mesh)
        if not is_usable(ue, self.preview_actor):
            raise RuntimeError("Could not spawn the PIE preview actor")
        # The saved cottage keeps its collision. Only its PIE visual is hidden.
        self.original.set_visibility(False)
        return {"mesh": mesh.get_path_name(), "material_slots": len(slots),
                "bounds_extent_cm": [extent.x, extent.y, extent.z],
                "collision": "original cottage collision retained"}

    def restore(self):
        if self.worker is not None and self.worker.poll() is None:
            self.worker.terminate()
        if is_usable(self.ue, self.original):
            self.original.set_visibility(self.was_visible)
        if is_usable(self.ue, self.preview_actor):
            self.preview_actor.destroy_actor()
        self.preview_actor = None

    def write_report(self, report):
        report.update(self.metadata)
        report.update(engine_version=self.ue.SystemLibrary.get_engine_version(),
                      destination=self.destination, pie_run=self.village_run)
        try:
            self.report_path.parent.mkdir(parents=True, exist_ok=True)
            self.report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
            self.ue.log(f"[HearthImportPreview] {report['state']}: {self.report_path}")
        except OSError as exc:
            self.ue.log_warning(f"[HearthImportPreview] Cannot write report: {exc}")


def stop():
    """Restore the PIE cottage, leaving unrelated editor imports alone."""
    global _ticker
    if _current is not None:
        _current.stop()
    if _ticker is not None and not _inflight:
        import unreal
        unreal.unregister_slate_post_tick_callback(_ticker)
        _ticker = None


def start(source=None, timeout_seconds=120.0):
    """Preview the local cottage in an existing PIE run; no network calls."""
    global _current, _ticker
    import unreal
    if _inflight:
        raise RuntimeError("Previous preview import is still finishing; wait before retrying")
    stop()
    backend = UnrealPreview(unreal, source or DEFAULT_SOURCE)
    _current = ImportSession(backend, timeout_seconds).begin()

    def tick(_delta):
        for backend in list(_inflight.values()):
            try:
                backend.poll_worker()
            except Exception as exc:
                if _current is not None and _current.backend is backend:
                    _current.fallback(str(exc))
        if not _current.tick():
            stop()

    _ticker = unreal.register_slate_post_tick_callback(tick)
    return _current
