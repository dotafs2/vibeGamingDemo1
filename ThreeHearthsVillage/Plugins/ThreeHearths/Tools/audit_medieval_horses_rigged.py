"""Cold-load and audit the imported MedievalLife horse rig assets.

Run this in a fresh headless UE 5.8 editor process after
``import_medieval_horses_rigged.py``.  It only loads and inspects assets; it
does not rebuild, reimport, mutate, or save them.  The JSON report records any
object-root bone that Interchange adds around the 18 authored bones.
"""
from __future__ import annotations

import hashlib
import json
import struct
from pathlib import Path

import unreal as ue


ROOT = Path(__file__).resolve().parents[3]
ART = ROOT / "Art" / "MedievalLife" / "Rigged"
MANIFEST = ROOT / "Content" / "ThreeHearths" / "Data" / "MedievalLifeHorseRigCatalog.json"
REPORT = ART / "UE_Rigged_Horse_Audit.json"
DEST = "/Game/ThreeHearths/Generated/MedievalLife/Rigged"
EXPECTED = {
    # AnimationLibrary.get_num_frames() reports sampled intervals (keys - 1).
    "Idle": {"length_s": 2.0, "frames": 60, "keys": 61, "fps": 30},
    "Walk": {"length_s": 1.2, "frames": 36, "keys": 37, "fps": 30},
}
EXPECTED_MATRIX = [
    [100.0, 0.0, 0.0, 0.0],
    [0.0, -100.0, 0.0, 0.0],
    [0.0, 0.0, 100.0, 0.0],
    [0.0, 0.0, 0.0, 1.0],
]


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _load(path: str, cls: type) -> object:
    if not path.startswith(DEST + "/"):
        raise RuntimeError("Manifest asset escaped rigged destination: " + path)
    obj = ue.load_asset(path)
    if not isinstance(obj, cls):
        raise RuntimeError(f"Expected {cls.__name__} at {path}, got {type(obj).__name__}")
    return obj


def _source_skeleton_contract() -> dict:
    """Read the authored joint names/parents from the source glTF skin."""
    source = ART / "horse_bay_idle.glb"
    blob = source.read_bytes()
    if blob[:4] != b"glTF":
        raise RuntimeError("Invalid source GLB header: " + str(source))
    offset = 12
    document = None
    while offset + 8 <= len(blob):
        size, kind = struct.unpack_from("<II", blob, offset)
        payload = blob[offset + 8:offset + 8 + size]
        if kind == 0x4E4F534A:  # JSON chunk
            document = json.loads(payload.decode("utf-8"))
            break
        offset += 8 + size
    if not isinstance(document, dict) or not document.get("skins"):
        raise RuntimeError("Source GLB has no skin: " + str(source))
    nodes = document.get("nodes", [])
    joints = document["skins"][0].get("joints", [])
    joint_set = set(joints)
    parent_by_node = {
        child: index
        for index, node in enumerate(nodes)
        for child in node.get("children", [])
    }
    names = {}
    parents = {}
    for index in joints:
        name = str(nodes[index].get("name", ""))
        if not name:
            raise RuntimeError("Source skin contains an unnamed authored joint")
        names[name] = index
        parent_index = parent_by_node.get(index)
        parents[name] = (str(nodes[parent_index].get("name", ""))
                         if parent_index in joint_set else None)
    if len(names) != 18 or any(not value for value in names):
        raise RuntimeError(f"Source GLB authored joint contract is not 18 named bones: {sorted(names)}")
    return {"names": sorted(names), "parents": parents}


def _name(value: object) -> str | None:
    text = str(value) if value is not None else ""
    return None if text in ("", "None", "NAME_None") else text


def _skeletal_parent_query(mesh: ue.SkeletalMesh):
    subsystem_class = getattr(ue, "SkeletalMeshEditorSubsystem", None)
    if subsystem_class is None:
        raise RuntimeError("UE 5.8 SkeletalMeshEditorSubsystem is unavailable")
    query = getattr(subsystem_class, "get_bone_parent", None)
    if query is None:
        subsystem = ue.get_editor_subsystem(subsystem_class)
        if subsystem is None:
            # Commandlets do not instantiate editor subsystems. This getter is
            # stateless and is exposed on the class default object in UE 5.8.
            subsystem = ue.get_default_object(subsystem_class)
        query = getattr(subsystem, "get_bone_parent", None)
    if query is None:
        query = getattr(mesh, "get_bone_parent", None)
    if query is None:
        raise RuntimeError("UE 5.8 get_bone_parent reflection is unavailable")
    def get_parent(bone: str) -> str | None:
        try:
            return _name(query(mesh, bone))
        except TypeError:
            # Some generated UE Python stubs expose ScriptMethod without the
            # UObject receiver even though the reflected function is static.
            return _name(query(bone))
    return get_parent


def _bounds(mesh: ue.SkeletalMesh) -> dict:
    box = mesh.get_bounds()
    origin, extent = box.origin, box.box_extent
    return {
        "min_cm": [origin.x - extent.x, origin.y - extent.y, origin.z - extent.z],
        "max_cm": [origin.x + extent.x, origin.y + extent.y, origin.z + extent.z],
        "extent_cm": [extent.x, extent.y, extent.z],
    }


def _mesh_audit(mesh: ue.SkeletalMesh, skeleton_path: str) -> dict:
    skeleton = mesh.get_editor_property("skeleton")
    if skeleton is None or skeleton.get_path_name() != skeleton_path:
        raise RuntimeError("SkeletalMesh is bound to the wrong Skeleton: " + mesh.get_path_name())
    materials = []
    for slot in mesh.get_editor_property("materials"):
        material = slot.get_editor_property("material_interface")
        if material is None:
            raise RuntimeError("SkeletalMesh has an empty material slot: " + mesh.get_path_name())
        materials.append(material.get_path_name())
    bounds = _bounds(mesh)
    component = ue.SkeletalMeshComponent()
    component.set_skeletal_mesh_asset(mesh)
    actual_bones = [str(component.get_bone_name(i)) for i in range(component.get_num_bones())]
    if any(value <= 0.0 for value in bounds["extent_cm"]):
        raise RuntimeError("SkeletalMesh has degenerate bounds: " + mesh.get_path_name())
    return {
        "mesh": mesh.get_path_name(),
        "class": "SkeletalMesh",
        "skeleton": skeleton_path,
        "material_paths": materials,
        "material_slot_count": len(materials),
        "bounds_ue_cm": bounds,
        "reference_bone_names": actual_bones,
    }


def _unwrap(value: object) -> object:
    """Unwrap the one-element tuples returned by UE Python out parameters."""
    if isinstance(value, tuple) and len(value) == 1:
        return value[0]
    return value


def _transform_signature(transform: object) -> tuple[float, ...]:
    """Return numeric FTransform components, avoiding UObject repr addresses."""
    def components(value: object, names: tuple[str, ...]) -> tuple[float, ...]:
        if value is None:
            raise RuntimeError("Animation pose returned no transform component")
        return tuple(round(float(getattr(value, name)), 6) for name in names)
    return (components(getattr(transform, "translation", None), ("x", "y", "z")) +
            components(getattr(transform, "rotation", None), ("x", "y", "z", "w")) +
            components(getattr(transform, "scale3d", None), ("x", "y", "z")))


def _source_bounds_cm() -> list[float]:
    audit_path = ART / "fbx_roundtrip_audit.json"
    source_audit = json.loads(audit_path.read_text(encoding="utf-8"))
    if source_audit.get("status") != "passed_cold_fbx_skins_actions_and_scale":
        raise RuntimeError("Source FBX scale audit is not passed: " + str(audit_path))
    row = next((item for item in source_audit.get("files", [])
                if item.get("file") == "horse_bay_idle.fbx"), None)
    if row is None or len(row.get("size_m", [])) != 3:
        raise RuntimeError("Source FBX idle bounds are missing")
    return [float(value) * 100.0 for value in row["size_m"]]


def _animation_audit(anim: ue.AnimSequence, skeleton_path: str, clip: str) -> dict:
    """Audit current UE 5.8 animation data, including actual pose variation."""
    expected = EXPECTED[clip]
    skeleton = anim.get_editor_property("skeleton")
    if skeleton is None or skeleton.get_path_name() != skeleton_path:
        raise RuntimeError("Animation is bound to the wrong Skeleton: " + anim.get_path_name())
    library = getattr(ue, "AnimationLibrary", None)
    if library is None:
        raise RuntimeError("UE 5.8 unreal.AnimationLibrary is unavailable")
    try:
        length = float(_unwrap(library.get_sequence_length(anim)))
        frames = int(_unwrap(library.get_num_frames(anim)))
        keys = int(_unwrap(library.get_num_keys(anim)))
        tracks = list(_unwrap(library.get_animation_track_names(anim)))
        root_motion = bool(_unwrap(library.is_root_motion_enabled(anim)))
    except Exception as exc:
        raise RuntimeError("AnimationLibrary data-model audit failed for " + anim.get_path_name()) from exc
    track_names = sorted({str(name) for name in tracks if str(name)})
    if not track_names or frames <= 0 or keys <= 0:
        raise RuntimeError("Animation has no nonempty sampled bone tracks: " + anim.get_path_name())
    if root_motion:
        raise RuntimeError("Root motion must be disabled: " + anim.get_path_name())
    if abs(length - expected["length_s"]) > 0.05:
        raise RuntimeError(f"{clip} length {length:.4f}s != {expected['length_s']:.4f}s")
    if frames != expected["frames"] or keys != expected["keys"]:
        raise RuntimeError(f"{clip} sampled frames/keys {(frames, keys)} != "
                           f"{(expected['frames'], expected['keys'])}")
    fps = frames / length if length > 0.0 else 0.0
    if abs(fps - expected["fps"]) > 0.1:
        raise RuntimeError(f"{clip} computed sample rate {fps:.4f} != {expected['fps']}")
    sample_frames = sorted({0, frames // 4, frames // 2, (frames * 3) // 4, frames})
    changed_tracks = []
    for track in track_names:
        poses = []
        try:
            for frame in sample_frames:
                pose = _unwrap(library.get_bone_pose_for_frame(anim, track, frame, False))
                poses.append(_transform_signature(pose))
        except Exception as exc:
            raise RuntimeError(f"Cannot sample animation track {track} in {anim.get_path_name()}") from exc
        if len(set(poses)) > 1:
            changed_tracks.append(track)
    if not changed_tracks:
        raise RuntimeError("Animation bone tracks are present but have no sampled variation: " +
                           anim.get_path_name())
    return {
        "asset": anim.get_path_name(),
        "class": "AnimSequence",
        "clip": clip,
        "skeleton": skeleton_path,
        "length_s": length,
        "frames": frames,
        "keys": keys,
        "computed_fps": fps,
        "track_names": track_names,
        "changed_tracks": changed_tracks,
        "root_motion": root_motion,
        "expected": expected,
    }


def _audit_horse(row: dict, current_hashes: dict, source_contract: dict,
                 source_bounds_cm: list[float]) -> dict:
    if row.get("source_sha256") != current_hashes:
        raise RuntimeError("Manifest source hash is stale; reimport changed horse source first: " + row["id"])
    skeleton = _load(row["skeleton"], ue.Skeleton)
    if set(row.get("animations", {})) != {"Idle", "Walk"}:
        raise RuntimeError("Horse must have exactly Idle and Walk animation records: " + row["id"])
    if {item.get("layer") for item in row.get("meshes", [])} != {"attachments", "finish", "structure"}:
        raise RuntimeError("Horse layer set is incomplete: " + row["id"])
    mesh_objects = [_load(item["mesh"], ue.SkeletalMesh) for item in row["meshes"]]
    mesh_rows = [_mesh_audit(mesh, row["skeleton"]) for mesh in mesh_objects]
    parent_query = _skeletal_parent_query(mesh_objects[0])
    authored_names = set(source_contract["names"])
    authored_folded = {name.casefold() for name in authored_names}
    for mesh_row in mesh_rows:
        actual_folded = {name.casefold() for name in mesh_row["reference_bone_names"]}
        if not authored_folded.issubset(actual_folded) or len(actual_folded - authored_folded) > 1:
            raise RuntimeError("Actual reference bones do not match the authored rig: " + mesh_row["mesh"])
    parent_rows = {}
    actual_parents = {}
    for name in sorted(authored_names):
        actual = parent_query(name)
        expected_parent = source_contract["parents"].get(name)
        if actual != expected_parent and not (expected_parent is None and actual):
            raise RuntimeError(f"Bone parent mismatch for {row['id']}:{name}: "
                               f"expected {expected_parent!r}, got {actual!r}")
        actual_parents[name] = actual
        parent_rows[name] = {"expected": expected_parent, "actual": actual}
    extra = sorted({actual for actual in actual_parents.values() if actual and actual not in authored_names})
    if len(extra) > 1:
        raise RuntimeError("More than one imported object-root bone: " + repr(extra))
    animations = {
        clip: _animation_audit(_load(item["asset"], ue.AnimSequence), row["skeleton"], clip)
        for clip, item in row["animations"].items()
    }
    # Unreal FName identity is case-insensitive. AnimationLibrary returns
    # lowercase FL/FR/HL/HR while reference meshes retain their display case.
    for clip, item in animations.items():
        folded_tracks = {name.casefold() for name in item["track_names"]}
        if not authored_folded.issubset(folded_tracks):
            raise RuntimeError(clip + " animation omits authored bones: " + repr(sorted(authored_folded - folded_tracks)))
    tracks = set().union(*({name.casefold() for name in item["track_names"]} for item in animations.values()))
    unknown_tracks = sorted(tracks - authored_folded)
    if len(unknown_tracks) > 1:
        raise RuntimeError("Animation contains unexpected extra bones: " + repr(unknown_tracks))
    union_min = [min(item["bounds_ue_cm"]["min_cm"][axis] for item in mesh_rows) for axis in range(3)]
    union_max = [max(item["bounds_ue_cm"]["max_cm"][axis] for item in mesh_rows) for axis in range(3)]
    union_size = [union_max[axis] - union_min[axis] for axis in range(3)]
    for axis, (actual, expected_size) in enumerate(zip(union_size, source_bounds_cm)):
        if abs(actual - expected_size) > max(2.0, expected_size * 0.03):
            raise RuntimeError(f"{row['id']} union bound axis {axis} {actual:.3f}cm "
                               f"!= source {expected_size:.3f}cm")
    return {
        "id": row["id"],
        "skeleton": row["skeleton"],
        "authored_bone_count": len(source_contract["names"]),
        "skeleton_bone_names": sorted(authored_names | set(extra)),
        "extra_object_root_bones": extra,
        "bone_parents": parent_rows,
        "meshes": mesh_rows,
        "union_bounds_ue_cm": {"min_cm": union_min, "max_cm": union_max,
                                "size_cm": union_size},
        "source_bounds_cm": source_bounds_cm,
        "animations": animations,
        "source_hash_matches": True,
        "source_sha256": current_hashes,
    }


def main() -> None:
    if not MANIFEST.is_file():
        raise RuntimeError("Missing horse rig manifest: " + str(MANIFEST))
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if manifest.get("status") != "passed":
        raise RuntimeError("Horse rig manifest is not passed")
    horses = manifest.get("horses", [])
    if len(horses) != 2:
        raise RuntimeError("Expected two horse coats in manifest")
    if manifest.get("source_to_unreal_matrix") != EXPECTED_MATRIX:
        raise RuntimeError("Manifest source_to_unreal_matrix does not match the expected authored X,+Z / Y,-Z, cm mapping")
    source_contract = _source_skeleton_contract()
    source_bounds_cm = _source_bounds_cm()
    current_hashes = {}
    for row in horses:
        current_hashes[row["id"]] = {
            clip: _sha256(ROOT / row["source_files"][clip])
            for clip in ("idle", "walk")
        }
    audited = [_audit_horse(row, current_hashes[row["id"]], source_contract,
                            source_bounds_cm) for row in horses]
    if len({row["skeleton"] for row in audited}) != 2:
        raise RuntimeError("Bay and grey must have distinct coat Skeleton assets")
    result = {
        "schema_version": 1,
        "status": "passed",
        "destination_root": DEST,
        "source_to_unreal_matrix": EXPECTED_MATRIX,
        "source_to_unreal_matrix_basis": "expected authored X,+Z / Y,-Z, metres_to_centimetres; audit enforces source bounds",
        "horses": audited,
        "static_meshes_rejected": True,
        "cold_load": True,
    }
    REPORT.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n",
                      encoding="utf-8")
    ue.log("[MedievalLifeRiggedHorseAudit] passed: " + json.dumps({
        "horses": len(audited),
        "skeletal_mesh_layers": sum(len(row["meshes"]) for row in audited),
        "animations": sum(len(row["animations"]) for row in audited),
        "authored_bones": [row["authored_bone_count"] for row in audited],
    }))


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        REPORT.write_text(json.dumps({"status": "failed", "error": str(exc)}, indent=2), encoding="utf-8")
        raise
