"""Import the authored MedievalLife horses as native UE skeletal assets.

The FBX clips are imported through UE 5.8 Interchange.  The idle source creates
one shared Skeleton and three independent SkeletalMesh layer assets per coat;
the walk source is animation-only and is explicitly bound to that Skeleton.
This keeps the static MedievalLife layer catalog untouched and makes retries
safe: unchanged source files reuse the recorded assets, while changed files
require ``-HearthMedievalHorseReplaceChanged`` before replacement.

Run from UnrealEditor-Cmd with the same Hearth command-line switches used by
the other MedievalLife importer.  This script does not alter the FBX/GLB/Blend
source files or launch an editor itself.
"""
from __future__ import annotations

import hashlib
import json
import time
from pathlib import Path

import unreal as ue


ROOT = Path(__file__).resolve().parents[3]
ART = ROOT / "Art" / "MedievalLife" / "Rigged"
DEST = "/Game/ThreeHearths/Generated/MedievalLife/Rigged"
MANIFEST = ROOT / "Content" / "ThreeHearths" / "Data" / "MedievalLifeHorseRigCatalog.json"
REPORT = ART / "UE_Rigged_Horse_Import_Report.json"
SOURCE_REPORT = ART / "rig_report.json"
HORSES = {
    "bay": {"id": "horse_bay", "idle": ART / "horse_bay_idle.fbx", "walk": ART / "horse_bay_walk.fbx"},
    "grey": {"id": "horse_grey", "idle": ART / "horse_grey_idle.fbx", "walk": ART / "horse_grey_walk.fbx"},
}
LAYERS = ("attachments", "finish", "structure")
EXPECTED = {
    "Idle": {"length_s": 2.0, "frames": 61, "fps": 30},
    "Walk": {"length_s": 1.2, "frames": 37, "fps": 30},
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
        raise RuntimeError("Missing authored rig report: " + str(SOURCE_REPORT))
    source = json.loads(SOURCE_REPORT.read_text(encoding="utf-8"))
    if source.get("status") not in (
            "passed_source_binding_and_authored_kinematics",
            "passed_deformed_hoof_contacts_and_loops"):
        raise RuntimeError("Source rig report is not a passed authored report")
    if int(source.get("bones_per_horse", 0)) != 18:
        raise RuntimeError("Expected exactly 18 authored horse bones")
    walk = source.get("walk", {})
    if walk and (int(walk.get("fps", 0)) != 30 or
                 abs(float(walk.get("cycle_seconds", 0.0)) - 1.2) > 0.001):
        raise RuntimeError("Source walk contract is not 1.2 seconds at 30 fps")
    return source


def _validate_sources() -> dict:
    source_contract = _source_contract()
    hashes = {}
    for coat, row in HORSES.items():
        hashes[coat] = {}
        for clip in ("idle", "walk"):
            source = row[clip]
            if not source.is_file():
                raise RuntimeError("Missing rigged horse FBX: " + str(source))
            hashes[coat][clip] = _sha256(source)
    return {"contract": source_contract, "hashes": hashes}


def _switches() -> set[str]:
    _, switches, _ = ue.SystemLibrary.parse_command_line(
        ue.SystemLibrary.get_command_line())
    return set(switches)


def _pipeline(only_animations: bool, skeleton_path: str | None) -> ue.InterchangeGenericAssetsPipeline:
    """Configure properties named by UE 5.8 Interchange public headers."""
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

    common = pipeline.get_editor_property(
        "common_skeletal_meshes_and_animations_properties")
    common.set_editor_property("import_only_animations", only_animations)
    common.set_editor_property("try_auto_select_skeleton", False)
    if skeleton_path:
        skeleton = ue.load_asset(skeleton_path)
        if not isinstance(skeleton, ue.Skeleton):
            raise RuntimeError("Requested shared Skeleton is missing: " + skeleton_path)
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
        return {row.get("id"): row for row in report.get("horses", [])
                if isinstance(row, dict) and row.get("id")}
    except (OSError, ValueError) as exc:
        raise RuntimeError("Cannot read existing horse report: " + str(exc)) from exc


def _asset(path: str, cls: type) -> object | None:
    if not path or not path.startswith(DEST + "/"):
        return None
    obj = ue.load_asset(path)
    return obj if isinstance(obj, cls) else None


def _existing_horse(row: dict) -> bool:
    skeleton = _asset(row.get("skeleton", ""), ue.Skeleton)
    if skeleton is None or len(row.get("meshes", [])) != len(LAYERS):
        return False
    if {item.get("layer") for item in row.get("meshes", [])} != set(LAYERS):
        return False
    if any(_asset(item.get("mesh", ""), ue.SkeletalMesh) is None
           for item in row.get("meshes", [])):
        return False
    animations = row.get("animations")
    if not isinstance(animations, dict) or set(animations) != {"Idle", "Walk"}:
        return False
    for item in animations.values():
        if _asset(item.get("asset", ""), ue.AnimSequence) is None:
            return False
    return True


def _save_objects(objects: list[object]) -> None:
    subsystem = ue.get_editor_subsystem(ue.EditorAssetSubsystem)
    seen = set()
    for obj in objects:
        path = obj.get_path_name()
        if path in seen:
            continue
        seen.add(path)
        if not path.startswith(DEST + "/"):
            raise RuntimeError("Imported asset escaped rigged horse destination: " + path)
        if not subsystem.save_loaded_asset(obj, False):
            raise RuntimeError("Cannot save imported asset: " + path)


def _layer_name(mesh: ue.SkeletalMesh) -> str | None:
    leaf = mesh.get_path_name().rsplit("/", 1)[-1].split(".", 1)[0].lower()
    for layer in LAYERS:
        if leaf.endswith("__" + layer) or leaf.endswith("_" + layer):
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


def _mesh_record(mesh: ue.SkeletalMesh, skeleton_path: str, horse_id: str) -> dict:
    if not mesh.get_path_name().startswith(DEST + "/"):
        raise RuntimeError("Skeletal mesh escaped destination: " + mesh.get_path_name())
    skeleton = mesh.get_editor_property("skeleton")
    if skeleton is None or skeleton.get_path_name() != skeleton_path:
        raise RuntimeError("Mesh does not use the coat Skeleton: " + mesh.get_path_name())
    bounds = _bounds(mesh)
    if any(value <= 0.0 for value in bounds["extent_cm"]):
        raise RuntimeError("Degenerate SkeletalMesh bounds: " + mesh.get_path_name())
    materials = []
    for slot in mesh.get_editor_property("materials"):
        material = slot.get_editor_property("material_interface")
        if material is None:
            raise RuntimeError("SkeletalMesh has an empty material slot: " + mesh.get_path_name())
        materials.append(material.get_path_name())
    layer = _layer_name(mesh)
    if layer is None:
        raise RuntimeError("Cannot classify horse layer: " + mesh.get_path_name())
    return {
        "id": horse_id + "/" + layer,
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
    params.set_editor_property("override_pipelines", [
        ue.SoftObjectPath(pipeline.get_path_name()),
    ])
    imported = []
    params.on_assets_import_done.bind_callable(lambda objects: imported.extend(objects))
    manager = ue.InterchangeManager.get_interchange_manager_scripted()
    if not manager.import_asset(destination,
                                manager.create_source_data(str(source)), params):
        raise RuntimeError("Interchange rejected " + str(source))
    if any(isinstance(obj, ue.StaticMesh) for obj in imported):
        raise RuntimeError("Rigged horse import produced a StaticMesh")
    if not imported:
        raise RuntimeError("Rigged horse import produced no assets: " + str(source))
    _save_objects(imported)
    return imported


def _find_animation(objects: list[object], clip: str, horse_id: str) -> ue.AnimSequence:
    animations = [obj for obj in objects if isinstance(obj, ue.AnimSequence)]
    # Blender's single-action FBX stack is named Scene. Its source file is
    # already unambiguously the requested clip, so do not infer it from names.
    if len(animations) == 1:
        return animations[0]
    wanted = [obj for obj in animations if clip.lower() in obj.get_name().lower()]
    if len(wanted) != 1:
        raise RuntimeError(f"Expected one {horse_id} {clip} AnimSequence, got "
                           f"{[obj.get_path_name() for obj in animations]}")
    return wanted[0]


def _anim_record(anim: ue.AnimSequence, skeleton_path: str, clip: str) -> dict:
    skeleton = anim.get_editor_property("skeleton")
    if skeleton is None or skeleton.get_path_name() != skeleton_path:
        raise RuntimeError("Animation uses the wrong Skeleton: " + anim.get_path_name())
    library = getattr(ue, "AnimationLibrary", None)
    if library is None:
        raise RuntimeError("UE 5.8 unreal.AnimationLibrary is unavailable")
    def unwrap(value: object) -> object:
        return value[0] if isinstance(value, tuple) and len(value) == 1 else value
    try:
        length = float(unwrap(library.get_sequence_length(anim)))
        frames = int(unwrap(library.get_num_frames(anim)))
        keys = int(unwrap(library.get_num_keys(anim)))
        root_motion = bool(unwrap(library.is_root_motion_enabled(anim)))
    except Exception as exc:
        raise RuntimeError("AnimationLibrary data-model read failed for " + anim.get_path_name()) from exc
    if root_motion:
        raise RuntimeError("Imported horse animation unexpectedly enables root motion: " +
                           anim.get_path_name())
    expected = EXPECTED[clip]
    if abs(length - expected["length_s"]) > 0.05:
        raise RuntimeError(f"{clip} length {length:.4f}s != {expected['length_s']:.4f}s")
    if keys != int(expected["frames"]) or frames != keys - 1:
        raise RuntimeError(f"{clip} sampled frames/keys {(frames, keys)} do not match "
                           f"expected {(int(expected['frames']) - 1, int(expected['frames']))}")
    if abs((frames / length) - expected["fps"]) > 0.1:
        raise RuntimeError(f"{clip} computed sample rate {(frames / length):.4f} != {expected['fps']}")
    return {
        "clip": clip,
        "asset": anim.get_path_name(),
        "skeleton": skeleton_path,
        "length_s": length,
        "frames": frames,
        "keys": keys,
        "sampled_keys": keys,
        "expected": expected,
        "root_motion": root_motion,
        "root_motion_expected": False,
    }


def _import_horse(coat: str, source_info: dict, previous: dict,
                  replace_changed: bool) -> dict:
    row = HORSES[coat]
    horse_id = row["id"]
    old = previous.get(horse_id)
    hashes = source_info["hashes"][coat]
    if old and old.get("source_sha256") == hashes and _existing_horse(old):
        return dict(old, reused=True)
    destination = DEST + "/" + horse_id
    if old is None and ue.EditorAssetLibrary.does_directory_exist(destination):
        raise RuntimeError("Existing horse destination requires a prior report: " + destination)
    old_hashes = old.get("source_sha256", {}) if old else {}
    if old and old_hashes != hashes and not replace_changed:
        raise RuntimeError("Source changed for " + horse_id +
                           "; rerun with -HearthMedievalHorseReplaceChanged")
    started = time.monotonic()
    skeleton_path = old.get("skeleton") if old and _asset(old.get("skeleton", ""), ue.Skeleton) else None
    mesh_records = list(old.get("meshes", [])) if old else []
    animations = dict(old.get("animations", {})) if old else {}

    if not mesh_records or old_hashes.get("idle") != hashes["idle"]:
        imported = _import_source(row["idle"], destination, False, skeleton_path,
                                  bool(old and replace_changed))
        meshes = [obj for obj in imported if isinstance(obj, ue.SkeletalMesh)]
        if len(meshes) != len(LAYERS):
            raise RuntimeError("Expected three independent horse layer SkeletalMeshes for " + horse_id +
                               "; got " + str([m.get_path_name() for m in meshes]))
        skeletons = [obj for obj in imported if isinstance(obj, ue.Skeleton)]
        if skeleton_path is None:
            if skeletons:
                skeleton_path = skeletons[0].get_path_name()
            else:
                skeleton_path = meshes[0].get_editor_property("skeleton").get_path_name()
        mesh_records = sorted((_mesh_record(mesh, skeleton_path, horse_id)
                               for mesh in meshes), key=lambda item: item["layer"])
        if {item["layer"] for item in mesh_records} != set(LAYERS):
            raise RuntimeError("Horse layer set mismatch for " + horse_id)
        animations["Idle"] = _anim_record(_find_animation(imported, "Idle", horse_id),
                                           skeleton_path, "Idle")

    if old_hashes.get("walk") != hashes["walk"] or "Walk" not in animations:
        imported = _import_source(row["walk"], destination + "/Walk", True, skeleton_path,
                                  bool(old and replace_changed))
        animations["Walk"] = _anim_record(_find_animation(imported, "Walk", horse_id),
                                           skeleton_path, "Walk")
    return {
        "id": horse_id,
        "source_sha256": hashes,
        "source_files": {clip: row[clip].relative_to(ROOT).as_posix()
                         for clip in ("idle", "walk")},
        "skeleton": skeleton_path,
        "meshes": mesh_records,
        "animations": animations,
        "source_bones_expected": 18,
        "authoring_units": "metres",
        "ue_units": "centimetres",
        "source_to_unreal_matrix": SOURCE_TO_UNREAL_MATRIX,
        "import_seconds": time.monotonic() - started,
        "reused": False,
    }


def _write_manifest(source_info: dict, horses: list[dict]) -> None:
    MANIFEST.parent.mkdir(parents=True, exist_ok=True)
    MANIFEST.write_text(json.dumps({
        "schema_version": 1,
        "status": "passed",
        "destination_root": DEST,
        "source_report": SOURCE_REPORT.relative_to(ROOT).as_posix(),
        "source_to_unreal_matrix": SOURCE_TO_UNREAL_MATRIX,
        "source_to_unreal_units": "metres_to_centimetres; reflected Y",
        "mesh_policy": "three independent SkeletalMesh layer assets per coat",
        "skeleton_policy": "one shared Skeleton per coat; walk is animation-only",
        "horses": horses,
        "limitations": [
            "horse rigs are static authored horse meshes with imported Idle/Walk clips; no gameplay integration is implied",
            "walk root motion is disabled; actor locomotion remains a runtime concern",
        ],
    }, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    source_info = _validate_sources()
    previous = _load_previous()
    replace_changed = "HearthMedievalHorseReplaceChanged" in _switches()
    report = {
        "schema_version": 1,
        "status": "running",
        "destination_root": DEST,
        "expected_horses": 2,
        "expected_skeletal_mesh_layers": 6,
        "horses": list(previous.values()),
    }
    try:
        for coat in HORSES:
            result = _import_horse(coat, source_info, previous, replace_changed)
            report["horses"] = [row for row in report["horses"]
                               if row.get("id") != result["id"]] + [result]
            REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                              encoding="utf-8")
        if len(report["horses"]) != len(HORSES):
            raise RuntimeError("Horse count mismatch")
        report["status"] = "passed"
        _write_manifest(source_info, report["horses"])
    except Exception as exc:
        report.update(status="failed", error=str(exc))
        ue.log_error("[MedievalLifeRiggedHorseImport] " + str(exc))
        raise
    finally:
        REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                          encoding="utf-8")


if __name__ == "__main__":
    main()
