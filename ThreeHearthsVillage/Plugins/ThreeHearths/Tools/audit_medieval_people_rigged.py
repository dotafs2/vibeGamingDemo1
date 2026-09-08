"""Read-only UE 5.8 cold audit for MedievalLife resident rigs."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal as ue


ROOT = Path(__file__).resolve().parents[3]
ART = ROOT / "Art" / "MedievalLife" / "PeopleRigged"
SOURCE_REPORT = ART / "people_rig_report.json"
IMPORT_REPORT = ART / "UE_People_Rigged_Import_Report.json"
AUDIT_REPORT = ART / "UE_People_Rigged_Cold_Audit.json"
DEST = "/Game/ThreeHearths/Generated/MedievalLife/PeopleRigged"
EXPECTED_BONES = 17
EXPECTED = {
    "Idle": {"length_s": 2.0, "frames": 60, "keys": 61, "fps": 30},
    "Walk": {"length_s": 1.0, "frames": 30, "keys": 31, "fps": 30},
}
EXPECTED_MATRIX = [
    [100.0, 0.0, 0.0, 0.0],
    [0.0, -100.0, 0.0, 0.0],
    [0.0, 0.0, 100.0, 0.0],
    [0.0, 0.0, 0.0, 1.0],
]


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _source_contract() -> tuple[dict, dict, dict]:
    source = json.loads(SOURCE_REPORT.read_text(encoding="utf-8"))
    if not str(source.get("status", "")).startswith(("source_", "passed_source_")):
        raise RuntimeError("Source people rig report is not complete")
    expected_names = set(source.get("bones", {}))
    if len(expected_names) != EXPECTED_BONES:
        raise RuntimeError("Expected exactly 17 authored people bones")
    people = {}
    for row in source.get("people", []):
        person_id = row["id"]
        people[person_id] = {
            "layers": [item["layer"] for item in row.get("layers", [])],
            "source_files": {clip: ART / row["clips"][clip]["fbx"] for clip in ("Idle", "Walk")},
        }
    if set(people) != {"gatekeeper", "royal_guard", "carter"}:
        raise RuntimeError("Source report must contain all three people")
    hashes = {
        person_id: {clip: _sha256(path) for clip, path in spec["source_files"].items()}
        for person_id, spec in people.items()
    }
    return source, people, {"names": expected_names, "hashes": hashes}


def _load(path: str, cls: type) -> object:
    if not path.startswith(DEST + "/"):
        raise RuntimeError("People asset escaped destination: " + path)
    obj = ue.load_asset(path)
    if not isinstance(obj, cls):
        raise RuntimeError(f"Expected {cls.__name__} at {path}, got {type(obj).__name__}")
    return obj


def _bounds(mesh: ue.SkeletalMesh) -> dict:
    box = mesh.get_bounds()
    origin, extent = box.origin, box.box_extent
    return {
        "min_cm": [origin.x - extent.x, origin.y - extent.y, origin.z - extent.z],
        "max_cm": [origin.x + extent.x, origin.y + extent.y, origin.z + extent.z],
        "extent_cm": [extent.x, extent.y, extent.z],
    }


def _mesh_bones(mesh: ue.SkeletalMesh) -> list[str]:
    component = ue.SkeletalMeshComponent()
    component.set_skeletal_mesh_asset(mesh)
    getter_count = getattr(component, "get_num_bones", None)
    getter_name = getattr(component, "get_bone_name", None)
    if getter_count is None or getter_name is None:
        raise RuntimeError("UE SkeletalMesh bone component API is unavailable")
    count = int(getter_count())
    names = [str(getter_name(index)) for index in range(count)]
    if any(not name or name in {"None", "NAME_None"} for name in names):
        raise RuntimeError("People SkeletalMesh contains unnamed reference bone")
    return names


def _mesh_audit(mesh: ue.SkeletalMesh, skeleton_path: str, expected_names: set[str]) -> dict:
    skeleton = mesh.get_editor_property("skeleton")
    if skeleton is None or skeleton.get_path_name() != skeleton_path:
        raise RuntimeError("People layer is bound to wrong Skeleton: " + mesh.get_path_name())
    materials = []
    for slot in mesh.get_editor_property("materials"):
        material = slot.get_editor_property("material_interface")
        if material is None:
            raise RuntimeError("People layer has empty material slot: " + mesh.get_path_name())
        materials.append(material.get_path_name())
    bounds = _bounds(mesh)
    if any(value <= 0.0 for value in bounds["extent_cm"]):
        raise RuntimeError("People layer has degenerate bounds: " + mesh.get_path_name())
    bone_names = _mesh_bones(mesh)
    expected_cf = {name.casefold() for name in expected_names}
    actual_cf = {name.casefold() for name in bone_names}
    missing = sorted(expected_cf - actual_cf)
    extra = sorted(actual_cf - expected_cf)
    if missing or len(extra) > 1 or len(bone_names) not in (EXPECTED_BONES, EXPECTED_BONES + 1):
        raise RuntimeError(f"People reference bones mismatch at {mesh.get_path_name()}: "
                           f"missing={missing}, extra={extra}, count={len(bone_names)}")
    return {
        "mesh": mesh.get_path_name(),
        "class": "SkeletalMesh",
        "skeleton": skeleton_path,
        "reference_bones": bone_names,
        "authored_bone_count": EXPECTED_BONES,
        "object_root_bones": extra,
        "material_paths": materials,
        "material_slot_count": len(materials),
        "bounds_ue_cm": bounds,
    }


def _unwrap(value: object) -> object:
    return value[0] if isinstance(value, tuple) and len(value) == 1 else value


def _transform_signature(transform: object) -> tuple[float, ...]:
    def components(value: object, names: tuple[str, ...]) -> tuple[float, ...]:
        if value is None:
            raise RuntimeError("People pose returned no transform component")
        return tuple(round(float(getattr(value, name)), 6) for name in names)
    return (components(getattr(transform, "translation", None), ("x", "y", "z")) +
            components(getattr(transform, "rotation", None), ("x", "y", "z", "w")) +
            components(getattr(transform, "scale3d", None), ("x", "y", "z")))


def _animation_audit(anim: ue.AnimSequence, skeleton_path: str, clip: str,
                     expected_names: set[str]) -> dict:
    skeleton = anim.get_editor_property("skeleton")
    if skeleton is None or skeleton.get_path_name() != skeleton_path:
        raise RuntimeError("People animation is bound to wrong Skeleton: " + anim.get_path_name())
    library = getattr(ue, "AnimationLibrary", None)
    if library is None:
        raise RuntimeError("UE 5.8 unreal.AnimationLibrary is unavailable")
    try:
        length = float(_unwrap(library.get_sequence_length(anim)))
        frames = int(_unwrap(library.get_num_frames(anim)))
        keys = int(_unwrap(library.get_num_keys(anim)))
        track_names = sorted({str(name) for name in _unwrap(library.get_animation_track_names(anim)) if str(name)})
        root_motion = bool(_unwrap(library.is_root_motion_enabled(anim)))
    except Exception as exc:
        raise RuntimeError("AnimationLibrary read failed for " + anim.get_path_name()) from exc
    expected = EXPECTED[clip]
    if root_motion:
        raise RuntimeError("People animation enables root motion: " + anim.get_path_name())
    if abs(length - expected["length_s"]) > 0.05 or frames != expected["frames"] or keys != expected["keys"]:
        raise RuntimeError(f"People {clip} timing mismatch: {(length, frames, keys)}")
    if abs(frames / length - expected["fps"]) > 0.1:
        raise RuntimeError("People animation sample rate mismatch: " + anim.get_path_name())
    expected_cf = {name.casefold() for name in expected_names}
    tracks_cf = {name.casefold() for name in track_names}
    missing = sorted(expected_cf - tracks_cf)
    extra = sorted(tracks_cf - expected_cf)
    if missing or len(extra) > 1:
        raise RuntimeError(f"People animation tracks mismatch at {anim.get_path_name()}: "
                           f"missing={missing}, extra={extra}")
    sample_frames = sorted({0, frames // 4, frames // 2, (frames * 3) // 4, frames})
    changed_tracks = []
    for track in track_names:
        poses = []
        for frame in sample_frames:
            pose = _unwrap(library.get_bone_pose_for_frame(anim, track, frame, False))
            poses.append(_transform_signature(pose))
        if len(set(poses)) > 1:
            changed_tracks.append(track)
    if not changed_tracks:
        raise RuntimeError("People animation has no sampled pose variation: " + anim.get_path_name())
    return {
        "asset": anim.get_path_name(),
        "class": "AnimSequence",
        "clip": clip,
        "skeleton": skeleton_path,
        "length_s": length,
        "frames": frames,
        "keys": keys,
        "track_names": track_names,
        "changed_tracks": changed_tracks,
        "root_motion": root_motion,
        "expected": expected,
    }


def _audit_person(row: dict, spec: dict, expected_names: set[str], current_hashes: dict) -> dict:
    if row.get("source_sha256") != current_hashes:
        raise RuntimeError("People source hash is stale: " + row["id"])
    if set(row.get("animations", {})) != {"Idle", "Walk"}:
        raise RuntimeError("People report must contain Idle and Walk: " + row["id"])
    if {item.get("layer") for item in row.get("meshes", [])} != set(spec["layers"]):
        raise RuntimeError("People layer set is incomplete: " + row["id"])
    skeleton = _load(row["skeleton"], ue.Skeleton)
    meshes = [_load(item["mesh"], ue.SkeletalMesh) for item in row["meshes"]]
    mesh_rows = [_mesh_audit(mesh, row["skeleton"], expected_names) for mesh in meshes]
    animations = {
        clip: _animation_audit(_load(item["asset"], ue.AnimSequence), row["skeleton"], clip, expected_names)
        for clip, item in row["animations"].items()
    }
    return {
        "id": row["id"],
        "skeleton": skeleton.get_path_name(),
        "meshes": mesh_rows,
        "animations": animations,
        "source_hash_matches": True,
        "source_sha256": current_hashes,
    }


def main() -> None:
    source, people, contract = _source_contract()
    if not IMPORT_REPORT.is_file():
        raise RuntimeError("Missing people import report: " + str(IMPORT_REPORT))
    manifest = json.loads(IMPORT_REPORT.read_text(encoding="utf-8"))
    if manifest.get("status") != "passed":
        raise RuntimeError("People import report is not passed")
    rows = manifest.get("people", [])
    if len(rows) != len(people) or {row.get("id") for row in rows} != set(people):
        raise RuntimeError("People import report does not contain exactly three people")
    if manifest.get("source_to_unreal_matrix") != EXPECTED_MATRIX:
        raise RuntimeError("People source_to_unreal_matrix does not match expected authored mapping")
    audited = []
    report = {"schema_version": 1, "status": "running", "destination_root": DEST,
              "people": [], "source_to_unreal_matrix": EXPECTED_MATRIX,
              "source_to_unreal_matrix_basis": "expected authored X,+Z / Y,-Z, metres_to_centimetres"}
    try:
        for row in rows:
            audited.append(_audit_person(row, people[row["id"]], contract["names"],
                                         contract["hashes"][row["id"]]))
        if len({row["skeleton"] for row in audited}) != 3:
            raise RuntimeError("Each person must have an independent Skeleton")
        report.update(status="passed", people=audited, source_report=SOURCE_REPORT.relative_to(ROOT).as_posix(),
                      cold_load=True, static_meshes_rejected=True)
    except Exception as exc:
        report.update(status="failed", error=str(exc), people=audited)
        ue.log_error("[MedievalLifePeopleRiggedAudit] " + str(exc))
        raise
    finally:
        AUDIT_REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
