"""Import MedievalLife resident rigs as native UE skeletal layer assets.

The source contract is read at runtime from Art/MedievalLife/PeopleRigged/
people_rig_report.json so source regeneration does not require frozen hashes.
Each person gets one Skeleton shared by every imported layer. Idle imports the
full set of SkeletalMesh layers; Walk is animation-only and targets that same
Skeleton. The report is written after every successful person, so a failed
run can resume without discarding completed people.
"""
from __future__ import annotations

import hashlib
import json
import time
from pathlib import Path

import unreal as ue


ROOT = Path(__file__).resolve().parents[3]
ART = ROOT / "Art" / "MedievalLife" / "PeopleRigged"
SOURCE_REPORT = ART / "people_rig_report.json"
REPORT = ART / "UE_People_Rigged_Import_Report.json"
DEST = "/Game/ThreeHearths/Generated/MedievalLife/PeopleRigged"
EXPECTED_BONES = 17
EXPECTED = {
    "Idle": {"length_s": 2.0, "keys": 61, "fps": 30},
    "Walk": {"length_s": 1.0, "keys": 31, "fps": 30},
}
SOURCE_TO_UNREAL_MATRIX = [
    [100.0, 0.0, 0.0, 0.0],
    [0.0, -100.0, 0.0, 0.0],
    [0.0, 0.0, 100.0, 0.0],
    [0.0, 0.0, 0.0, 1.0],
]


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _source_contract() -> dict:
    if not SOURCE_REPORT.is_file():
        raise RuntimeError("Missing source people rig report: " + str(SOURCE_REPORT))
    source = json.loads(SOURCE_REPORT.read_text(encoding="utf-8"))
    if not str(source.get("status", "")).startswith(("source_", "passed_source_")):
        raise RuntimeError("Source people rig report is not complete")
    bones = source.get("bones", {})
    if len(bones) != EXPECTED_BONES or "root" not in bones:
        raise RuntimeError(f"Expected {EXPECTED_BONES} authored people bones, got {len(bones)}")
    people = source.get("people", [])
    if {row.get("id") for row in people} != {"gatekeeper", "royal_guard", "carter"}:
        raise RuntimeError("Source report must contain gatekeeper, royal_guard and carter")
    return source


def _people(source: dict) -> dict[str, dict]:
    result = {}
    for person in source["people"]:
        person_id = person["id"]
        layers = [item["layer"] for item in person.get("layers", [])]
        if len(layers) != len(set(layers)) or not layers:
            raise RuntimeError("Duplicate or empty layer set for " + person_id)
        clips = person.get("clips", {})
        if set(clips) != {"Idle", "Walk"}:
            raise RuntimeError("Expected Idle and Walk source clips for " + person_id)
        paths = {}
        for clip in ("Idle", "Walk"):
            source_path = ART / clips[clip]["fbx"]
            if not source_path.is_file():
                raise RuntimeError("Missing people FBX: " + str(source_path))
            paths[clip] = source_path
        result[person_id] = {"layers": layers, "clips": paths}
    return result


def _validate_sources() -> tuple[dict, dict, dict]:
    source = _source_contract()
    people = _people(source)
    hashes = {
        person_id: {clip: _sha256(path) for clip, path in row["clips"].items()}
        for person_id, row in people.items()
    }
    return source, people, hashes


def _switches() -> set[str]:
    _, switches, _ = ue.SystemLibrary.parse_command_line(ue.SystemLibrary.get_command_line())
    return set(switches)


def _pipeline(only_animations: bool, skeleton_path: str | None) -> ue.InterchangeGenericAssetsPipeline:
    pipeline = ue.InterchangeGenericAssetsPipeline()
    pipeline.set_editor_property("use_source_name_for_asset", False)
    pipeline.set_editor_property("scene_name_sub_folder", False)
    pipeline.set_editor_property("asset_type_sub_folders", False)
    mesh = pipeline.get_editor_property("mesh_pipeline")
    mesh.set_editor_property("import_static_meshes", False)
    mesh.set_editor_property("import_skeletal_meshes", not only_animations)
    behavior = getattr(ue.InterchangeCombineSkeletalMeshesBehavior, "DO_NOT_COMBINE", None)
    if behavior is None:
        raise RuntimeError("UE 5.8 lacks DO_NOT_COMBINE skeletal mesh behavior")
    mesh.set_editor_property("combine_skeletal_meshes_behavior", behavior)
    mesh.set_editor_property("create_physics_asset", False)
    common_mesh = pipeline.get_editor_property("common_meshes_properties")
    common_mesh.set_editor_property("bake_meshes", True)
    common = pipeline.get_editor_property("common_skeletal_meshes_and_animations_properties")
    common.set_editor_property("import_only_animations", only_animations)
    common.set_editor_property("try_auto_select_skeleton", False)
    if skeleton_path:
        skeleton = ue.load_asset(skeleton_path)
        if not isinstance(skeleton, ue.Skeleton):
            raise RuntimeError("Cannot load target people Skeleton: " + skeleton_path)
        common.set_editor_property("skeleton", skeleton)
    animation = pipeline.get_editor_property("animation_pipeline")
    animation.set_editor_property("import_animations", True)
    animation.set_editor_property("import_bone_tracks", True)
    animation.set_editor_property("custom_bone_animation_sample_rate", 30)
    animation.set_editor_property("import_custom_attribute", False)
    return pipeline


def _load_previous() -> dict:
    if not REPORT.is_file():
        return {}
    try:
        report = json.loads(REPORT.read_text(encoding="utf-8"))
        return {row.get("id"): row for row in report.get("people", [])
                if isinstance(row, dict) and row.get("id")}
    except (OSError, ValueError) as exc:
        raise RuntimeError("Cannot read existing people import report: " + str(exc)) from exc


def _asset(path: str, cls: type) -> object | None:
    if not path or not path.startswith(DEST + "/"):
        return None
    obj = ue.load_asset(path)
    return obj if isinstance(obj, cls) else None


def _existing_person(row: dict, expected_layers: list[str]) -> bool:
    if _asset(row.get("skeleton", ""), ue.Skeleton) is None:
        return False
    meshes = row.get("meshes", [])
    if len(meshes) != len(expected_layers) or {item.get("layer") for item in meshes} != set(expected_layers):
        return False
    if any(_asset(item.get("mesh", ""), ue.SkeletalMesh) is None for item in meshes):
        return False
    animations = row.get("animations")
    if not isinstance(animations, dict) or set(animations) != {"Idle", "Walk"}:
        return False
    return all(_asset(item.get("asset", ""), ue.AnimSequence) is not None
               for item in animations.values())


def _save_objects(objects: list[object]) -> None:
    subsystem = ue.get_editor_subsystem(ue.EditorAssetSubsystem)
    seen = set()
    for obj in objects:
        path = obj.get_path_name()
        if path in seen:
            continue
        seen.add(path)
        if not path.startswith(DEST + "/"):
            raise RuntimeError("Imported people asset escaped destination: " + path)
        if not subsystem.save_loaded_asset(obj, False):
            raise RuntimeError("Cannot save imported people asset: " + path)


def _layer_name(mesh: ue.SkeletalMesh, expected_layers: list[str]) -> str | None:
    leaf = mesh.get_path_name().rsplit("/", 1)[-1].split(".", 1)[0].lower()
    for layer in sorted(expected_layers, key=len, reverse=True):
        suffix = layer.lower()
        if leaf.endswith("__" + suffix) or leaf.endswith("_" + suffix):
            return layer
    return None


def _bounds(mesh: ue.SkeletalMesh) -> dict:
    box = mesh.get_bounds()
    origin, extent = box.origin, box.box_extent
    return {
        "min_cm": [origin.x - extent.x, origin.y - extent.y, origin.z - extent.z],
        "max_cm": [origin.x + extent.x, origin.y + extent.y, origin.z + extent.z],
        "extent_cm": [extent.x, extent.y, extent.z],
    }


def _mesh_record(mesh: ue.SkeletalMesh, skeleton_path: str, person_id: str,
                 expected_layers: list[str]) -> dict:
    if not mesh.get_path_name().startswith(DEST + "/"):
        raise RuntimeError("People SkeletalMesh escaped destination: " + mesh.get_path_name())
    skeleton = mesh.get_editor_property("skeleton")
    if skeleton is None or skeleton.get_path_name() != skeleton_path:
        raise RuntimeError("People layer uses wrong Skeleton: " + mesh.get_path_name())
    bounds = _bounds(mesh)
    if any(value <= 0.0 for value in bounds["extent_cm"]):
        raise RuntimeError("Degenerate people SkeletalMesh bounds: " + mesh.get_path_name())
    materials = []
    for slot in mesh.get_editor_property("materials"):
        material = slot.get_editor_property("material_interface")
        if material is None:
            raise RuntimeError("People SkeletalMesh has an empty material slot: " + mesh.get_path_name())
        materials.append(material.get_path_name())
    layer = _layer_name(mesh, expected_layers)
    if layer is None:
        raise RuntimeError("Cannot classify people layer: " + mesh.get_path_name())
    return {
        "id": person_id + "/" + layer,
        "layer": layer,
        "mesh": mesh.get_path_name(),
        "skeleton": skeleton_path,
        "materials": materials,
        "bounds_ue_cm": bounds,
    }


def _import_source(source: Path, destination: str, only_animations: bool,
                   skeleton_path: str | None, replace_existing: bool) -> list[object]:
    pipeline = _pipeline(only_animations, skeleton_path)
    params = ue.ImportAssetParameters()
    params.set_editor_property("is_automated", True)
    params.set_editor_property("replace_existing", replace_existing)
    params.set_editor_property("override_pipelines", [ue.SoftObjectPath(pipeline.get_path_name())])
    imported = []
    params.on_assets_import_done.bind_callable(lambda objects: imported.extend(objects))
    manager = ue.InterchangeManager.get_interchange_manager_scripted()
    if not manager.import_asset(destination, manager.create_source_data(str(source)), params):
        raise RuntimeError("Interchange rejected " + str(source))
    if any(isinstance(obj, ue.StaticMesh) for obj in imported):
        raise RuntimeError("People rig import produced a StaticMesh")
    if not imported:
        raise RuntimeError("People rig import produced no assets: " + str(source))
    _save_objects(imported)
    return imported


def _find_animation(objects: list[object], person_id: str, clip: str) -> ue.AnimSequence:
    animations = [obj for obj in objects if isinstance(obj, ue.AnimSequence)]
    if len(animations) != 1:
        raise RuntimeError(f"Expected one {person_id} {clip} AnimSequence by type, got "
                           f"{[obj.get_path_name() for obj in animations]}")
    return animations[0]


def _unwrap(value: object) -> object:
    return value[0] if isinstance(value, tuple) and len(value) == 1 else value


def _anim_record(anim: ue.AnimSequence, skeleton_path: str, clip: str) -> dict:
    skeleton = anim.get_editor_property("skeleton")
    if skeleton is None or skeleton.get_path_name() != skeleton_path:
        raise RuntimeError("People animation uses wrong Skeleton: " + anim.get_path_name())
    library = getattr(ue, "AnimationLibrary", None)
    if library is None:
        raise RuntimeError("UE 5.8 unreal.AnimationLibrary is unavailable")
    try:
        length = float(_unwrap(library.get_sequence_length(anim)))
        frames = int(_unwrap(library.get_num_frames(anim)))
        keys = int(_unwrap(library.get_num_keys(anim)))
        tracks = sorted({str(name) for name in _unwrap(library.get_animation_track_names(anim)) if str(name)})
        root_motion = bool(_unwrap(library.is_root_motion_enabled(anim)))
    except Exception as exc:
        raise RuntimeError("AnimationLibrary read failed for " + anim.get_path_name()) from exc
    expected = EXPECTED[clip]
    if root_motion or abs(length - expected["length_s"]) > 0.05:
        raise RuntimeError("People animation timing/root-motion contract failed: " + anim.get_path_name())
    if keys != expected["keys"] or frames != keys - 1 or abs(frames / length - expected["fps"]) > 0.1:
        raise RuntimeError(f"People {clip} frame contract failed: {(frames, keys, length)}")
    if not tracks:
        raise RuntimeError("People animation contains no bone tracks: " + anim.get_path_name())
    return {
        "clip": clip,
        "asset": anim.get_path_name(),
        "skeleton": skeleton_path,
        "length_s": length,
        "frames": frames,
        "keys": keys,
        "track_names": tracks,
        "root_motion": root_motion,
        "expected": expected,
    }


def _import_person(person_id: str, spec: dict, hashes: dict, previous: dict,
                   replace_changed: bool) -> dict:
    old = previous.get(person_id)
    if old and old.get("source_sha256") == hashes and _existing_person(old, spec["layers"]):
        return dict(old, reused=True)
    destination = DEST + "/" + person_id
    if old is None and ue.EditorAssetLibrary.does_directory_exist(destination):
        raise RuntimeError("Existing people destination requires a prior report: " + destination)
    old_hashes = old.get("source_sha256", {}) if old else {}
    if old and old_hashes != hashes and not replace_changed:
        raise RuntimeError("People source changed for " + person_id +
                           "; rerun with -HearthMedievalPeopleReplaceChanged")
    started = time.monotonic()
    skeleton_path = old.get("skeleton") if old and _asset(old.get("skeleton", ""), ue.Skeleton) else None
    mesh_records = list(old.get("meshes", [])) if old else []
    animations = dict(old.get("animations", {})) if old else {}
    if not mesh_records or old_hashes.get("Idle") != hashes["Idle"]:
        imported = _import_source(spec["clips"]["Idle"], destination, False, skeleton_path,
                                  bool(old and replace_changed))
        meshes = [obj for obj in imported if isinstance(obj, ue.SkeletalMesh)]
        if len(meshes) != len(spec["layers"]):
            raise RuntimeError(f"Expected {len(spec['layers'])} {person_id} SkeletalMesh layers, got "
                               f"{[mesh.get_path_name() for mesh in meshes]}")
        skeletons = [obj for obj in imported if isinstance(obj, ue.Skeleton)]
        if skeleton_path is None:
            skeleton_path = (skeletons[0].get_path_name() if skeletons else
                             meshes[0].get_editor_property("skeleton").get_path_name())
        mesh_records = sorted((_mesh_record(mesh, skeleton_path, person_id, spec["layers"])
                               for mesh in meshes), key=lambda item: item["layer"])
        if {item["layer"] for item in mesh_records} != set(spec["layers"]):
            raise RuntimeError("People layer set mismatch for " + person_id)
        animations["Idle"] = _anim_record(_find_animation(imported, person_id, "Idle"),
                                           skeleton_path, "Idle")
    if old_hashes.get("Walk") != hashes["Walk"] or "Walk" not in animations:
        imported = _import_source(spec["clips"]["Walk"], destination + "/Walk", True,
                                  skeleton_path, bool(old and replace_changed))
        animations["Walk"] = _anim_record(_find_animation(imported, person_id, "Walk"),
                                           skeleton_path, "Walk")
    return {
        "id": person_id,
        "source_sha256": hashes,
        "source_files": {clip: spec["clips"][clip].relative_to(ROOT).as_posix()
                         for clip in ("Idle", "Walk")},
        "skeleton": skeleton_path,
        "meshes": mesh_records,
        "animations": animations,
        "source_bones_expected": EXPECTED_BONES,
        "authoring_units": "metres",
        "ue_units": "centimetres",
        "source_to_unreal_matrix": SOURCE_TO_UNREAL_MATRIX,
        "import_seconds": time.monotonic() - started,
        "reused": False,
    }


def main() -> None:
    source, people, hashes = _validate_sources()
    previous = _load_previous()
    replace_changed = "HearthMedievalPeopleReplaceChanged" in _switches()
    report = {
        "schema_version": 1,
        "status": "running",
        "destination_root": DEST,
        "expected_people": len(people),
        "expected_skeletal_mesh_layers": sum(len(item["layers"]) for item in people.values()),
        "people": list(previous.values()),
    }
    try:
        for person_id, spec in people.items():
            result = _import_person(person_id, spec, hashes[person_id], previous, replace_changed)
            report["people"] = [row for row in report["people"] if row.get("id") != person_id] + [result]
            REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        if len(report["people"]) != len(people):
            raise RuntimeError("People count mismatch")
        report.update(status="passed", source_report=SOURCE_REPORT.relative_to(ROOT).as_posix(),
                      source_to_unreal_matrix=SOURCE_TO_UNREAL_MATRIX,
                      source_to_unreal_units="metres_to_centimetres; reflected Y")
    except Exception as exc:
        report.update(status="failed", error=str(exc))
        ue.log_error("[MedievalLifePeopleRiggedImport] " + str(exc))
        raise
    finally:
        REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
