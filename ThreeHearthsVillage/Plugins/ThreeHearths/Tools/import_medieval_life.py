"""Import MedievalLife modules as independent native UE layer meshes.

The source GLBs contain one mesh node per semantic layer plus anchor empties.
Interchange is configured with ``DO_NOT_COMBINE`` so each module/layer becomes its
own StaticMesh; complete assemblies are never imported or merged.  Generated
low-poly meshes have Nanite disabled.  Royal banner layer materials receive a
masked, two-sided material-instance override while retaining their imported
PBR parameters.

Run headless with UnrealEditor-Cmd and the project's Hearth flags.  The script
writes ``Content/ThreeHearths/Data/MedievalLifeCatalog.json`` only after every
module/layer passes validation.  Horse and human modules remain explicitly
declared as unrigged static art in that index.
"""
from __future__ import annotations

import hashlib
import json
import time
from pathlib import Path

import unreal as ue


ROOT = Path(__file__).resolve().parents[3]
ART = ROOT / "Art" / "MedievalLife"
CATALOG = ART / "catalog.json"
DEST = "/Game/ThreeHearths/Generated/MedievalLife"
INDEX = ROOT / "Content" / "ThreeHearths" / "Data" / "MedievalLifeCatalog.json"
REPORT = ART / "UE_Import_Report.json"
LAYERS = {"structure", "finish", "attachments", "weathering"}
SOURCE_TO_UNREAL_MATRIX = [
    [100.0, 0.0, 0.0, 0.0],
    [0.0, -100.0, 0.0, 0.0],
    [0.0, 0.0, 100.0, 0.0],
    [0.0, 0.0, 0.0, 1.0],
]


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _load_catalog() -> dict:
    catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
    modules = catalog.get("modules")
    if not isinstance(modules, list) or len(modules) != 21:
        raise RuntimeError(f"Expected 21 MedievalLife modules, found {len(modules or [])}")
    ids = [str(row.get("id", "")) for row in modules]
    if len(set(ids)) != len(ids) or any(not value for value in ids):
        raise RuntimeError("MedievalLife module IDs must be unique and non-empty")
    for row in modules:
        layers = set(row.get("layers", []))
        if not layers or not layers <= LAYERS:
            raise RuntimeError(f"Invalid layers for {row.get('id')}: {sorted(layers)}")
        source = ART / row["path"]
        if not source.is_file():
            raise RuntimeError("Missing MedievalLife source: " + str(source))
        anchors = row.get("anchors", {})
        if not isinstance(anchors, dict) or any(
                not isinstance(value, list) or len(value) != 3
                for value in anchors.values()):
            raise RuntimeError("Invalid anchors for " + row["id"])
    return catalog


def _switches() -> set[str]:
    _, switches, _ = ue.SystemLibrary.parse_command_line(
        ue.SystemLibrary.get_command_line())
    return set(switches)


def _pipeline() -> tuple[object, object]:
    pipeline = ue.InterchangeGenericAssetsPipeline()
    mesh = pipeline.get_editor_property("mesh_pipeline")
    for key, value in (("import_static_meshes", True),
                       ("import_skeletal_meshes", False),
                       ("collision", False)):
        mesh.set_editor_property(key, value)
    # UE 5.8's C++ EInterchangeCombineStaticMeshesBehavior::DoNotCombine
    # is exposed by Unreal Python as DO_NOT_COMBINE.
    no_combine = getattr(ue.InterchangeCombineStaticMeshesBehavior, "DO_NOT_COMBINE", None)
    if no_combine is None:
        raise RuntimeError("UE Interchange has no DO_NOT_COMBINE static-mesh behavior")
    mesh.set_editor_property("combine_static_meshes_behavior", no_combine)
    pipeline.get_editor_property("common_meshes_properties").set_editor_property(
        "bake_meshes", True)
    pipeline.get_editor_property("animation_pipeline").set_editor_property(
        "import_animations", False)
    pipeline.get_editor_property("material_pipeline").set_editor_property(
        "import_materials", True)
    return pipeline, ue.InterchangeGLTFPipeline()


def _disable_nanite(mesh: ue.StaticMesh) -> None:
    settings = mesh.get_editor_property("nanite_settings")
    if bool(settings.get_editor_property("enabled")):
        settings.set_editor_property("enabled", False)
        mesh.set_editor_property("nanite_settings", settings)


def _set_banner_flags(material: object) -> None:
    """Set masked/two-sided overrides without replacing imported PBR inputs."""
    if isinstance(material, ue.MaterialInstanceConstant):
        overrides = material.get_editor_property("base_property_overrides")
        # Unreal Python reflects the bool UPROPERTY without the native ``b``
        # prefix (C++ bOverride_BlendMode -> Python override_blend_mode).
        overrides.set_editor_property("override_blend_mode", True)
        overrides.set_editor_property("blend_mode", ue.BlendMode.BLEND_MASKED)
        overrides.set_editor_property("override_two_sided", True)
        overrides.set_editor_property("two_sided", True)
        material.set_editor_property("base_property_overrides", overrides)
        return
    if isinstance(material, ue.Material):
        material.set_editor_property("blend_mode", ue.BlendMode.BLEND_MASKED)
        material.set_editor_property("two_sided", True)
        return
    raise RuntimeError("Banner slot is neither Material nor MaterialInstanceConstant: "
                       + material.get_path_name())


def _configure_mesh(mesh: ue.StaticMesh, banner: bool) -> dict:
    _disable_nanite(mesh)
    slots = mesh.get_editor_property("static_materials")
    if not slots:
        raise RuntimeError("Layer has no material slots: " + mesh.get_path_name())
    names = []
    banner_count = 0
    for slot in slots:
        material = slot.get_editor_property("material_interface")
        if material is None:
            raise RuntimeError("Missing material slot: " + mesh.get_path_name())
        names.append(material.get_path_name())
        if banner:
            _set_banner_flags(material)
            banner_count += 1
    return {
        "material_slots": len(slots),
        "material_paths": names,
        "banner_masked_two_sided_slots": banner_count,
        "nanite": "disabled_low_poly_runtime_layer",
    }


def _save_mesh_materials(mesh: ue.StaticMesh) -> None:
    subsystem = ue.get_editor_subsystem(ue.EditorAssetSubsystem)
    objects = [mesh]
    objects.extend(slot.get_editor_property("material_interface")
                   for slot in mesh.get_editor_property("static_materials")
                   if slot.get_editor_property("material_interface"))
    for obj in objects:
        if not subsystem.save_loaded_asset(obj, False):
            raise RuntimeError("Cannot save " + obj.get_path_name())


def _mesh_layer(mesh: ue.StaticMesh, module_id: str, layers: set[str]) -> str | None:
    leaf = mesh.get_path_name().rsplit("/", 1)[-1].split(".", 1)[0]
    for layer in sorted(layers):
        if leaf == f"{module_id}__{layer}" or leaf.endswith(f"__{layer}"):
            return layer
    return None


def _bounds(mesh: ue.StaticMesh) -> dict:
    value = mesh.get_bounds()
    origin, extent = value.origin, value.box_extent
    return {
        "origin_cm": [origin.x, origin.y, origin.z],
        "min_cm": [origin.x - extent.x, origin.y - extent.y, origin.z - extent.z],
        "max_cm": [origin.x + extent.x, origin.y + extent.y, origin.z + extent.z],
        "extent_cm": [extent.x, extent.y, extent.z],
    }


def _anchors_cm(row: dict) -> dict:
    return {key: [value[0] * 100.0, -value[1] * 100.0, value[2] * 100.0]
            for key, value in row.get("anchors", {}).items()}


def _import_module(row: dict, previous: dict, replace_changed: bool) -> list[dict]:
    module_id = row["id"]
    source = ART / row["path"]
    checksum = _sha256(source)
    layers = set(row["layers"])
    prior = {f"{module_id}/{layer}": previous.get(f"{module_id}/{layer}")
             for layer in layers}
    cached = []
    for layer in sorted(layers):
        old = prior[f"{module_id}/{layer}"]
        mesh = ue.load_asset(old["mesh"]) if (
            old and old.get("source_sha256") == checksum and old.get("mesh")) else None
        if not isinstance(mesh, ue.StaticMesh):
            cached = []
            break
        cached.append((layer, mesh))
    if len(cached) == len(layers):
        result = []
        for layer, mesh in cached:
            flags = _configure_mesh(mesh, module_id == "royal_banner")
            _save_mesh_materials(mesh)
            result.append(dict(prior[f"{module_id}/{layer}"], reused=True,
                               **flags, bounds_ue=_bounds(mesh)))
        return result

    destination = DEST + "/" + module_id
    replacing = bool(replace_changed and any(
        prior[f"{module_id}/{layer}"] and
        prior[f"{module_id}/{layer}"].get("mesh", "").startswith(destination + "/")
        for layer in layers))
    if ue.EditorAssetLibrary.does_directory_exist(destination) and not replacing:
        raise RuntimeError("Existing MedievalLife cache requires review: " + destination)
    pipeline, gltf = _pipeline()
    params = ue.ImportAssetParameters()
    params.set_editor_property("is_automated", True)
    params.set_editor_property("replace_existing", replacing)
    params.set_editor_property("override_pipelines", [
        ue.SoftObjectPath(pipeline.get_path_name()),
        ue.SoftObjectPath(gltf.get_path_name()),
    ])
    imported = []
    params.on_assets_import_done.bind_callable(lambda objects: imported.extend(objects))
    manager = ue.InterchangeManager.get_interchange_manager_scripted()
    started = time.monotonic()
    if not manager.import_asset(destination, manager.create_source_data(str(source)), params):
        raise RuntimeError("Interchange rejected " + module_id)
    meshes = [obj for obj in imported if isinstance(obj, ue.StaticMesh)]
    by_layer = {}
    # Interchange deliberately gives a single imported mesh the source filename
    # (e.g. watch_bench), instead of the mesh-node name. The source catalog and
    # verified GLB contain exactly one semantic layer in this case.
    if len(meshes) == 1 and len(layers) == 1:
        by_layer[next(iter(layers))] = meshes[0]
    for mesh in meshes:
        layer = _mesh_layer(mesh, module_id, layers)
        if layer is not None:
            if layer in by_layer and by_layer[layer] != mesh:
                raise RuntimeError(f"Duplicate imported layer {module_id}/{layer}")
            by_layer[layer] = mesh
    if set(by_layer) != layers:
        raise RuntimeError(f"Imported layer mismatch for {module_id}: expected {sorted(layers)}, got {sorted(by_layer)}")
    subsystem = ue.get_editor_subsystem(ue.EditorAssetSubsystem)
    result = []
    for layer in sorted(layers):
        mesh = by_layer[layer]
        flags = _configure_mesh(mesh, module_id == "royal_banner")
        bounds = _bounds(mesh)
        if min(bounds["extent_cm"]) <= 0:
            raise RuntimeError("Degenerate bounds for " + module_id + "/" + layer)
        result.append({
            "id": f"{module_id}/{layer}",
            "module_id": module_id,
            "layer": layer,
            "source": source.relative_to(ROOT).as_posix(),
            "source_sha256": checksum,
            "mesh": mesh.get_path_name(),
            "ue_asset_folder": destination,
            "anchors_m": row.get("anchors", {}),
            "anchors_cm": _anchors_cm(row),
            "front_axis": "-Y",
            "up_axis": "+Z",
            "bounds_authoring_m": row["bounds_m"],
            "bounds_ue": bounds,
            "module_notes": row.get("notes", ""),
            "import_seconds": time.monotonic() - started,
            "reused": False,
            **flags,
        })
    for obj in imported:
        if not obj.get_path_name().startswith(destination + "/"):
            raise RuntimeError("Unexpected MedievalLife import destination")
        if not subsystem.save_loaded_asset(obj, False):
            raise RuntimeError("Cannot save " + obj.get_path_name())
    # Material overrides are part of the imported static-material assets and
    # are saved above together with the layer meshes.
    return result


def _write_index(catalog: dict, report: dict) -> None:
    modules = {row["id"]: row for row in catalog["modules"]}
    limitations = list(catalog.get("limitations", []))
    limitations.extend([
        "horse_bay and horse_grey are static unrigged art; no skeleton or animation is included",
        "royal_guard_body and carter_body are static unrigged art; no rig or animation is included",
    ])
    INDEX.parent.mkdir(parents=True, exist_ok=True)
    INDEX.write_text(json.dumps({
        "schema_version": 1,
        "source_catalog": "Art/MedievalLife/catalog.json",
        "source_catalog_sha256": _sha256(CATALOG),
        "destination_root": DEST,
        "source_to_unreal_matrix": SOURCE_TO_UNREAL_MATRIX,
        "source_to_unreal_units": "metres_to_centimetres; reflected Y",
        "yaw_conversion": "yaw_ue = -yaw_blender",
        "assets": report["assets"],
        "assemblies": catalog.get("assemblies", []),
        "modules": [{
            "id": row["id"], "path": row["path"], "layers": row["layers"],
            "anchors_m": row.get("anchors", {}), "anchors_cm": _anchors_cm(row),
            "notes": row.get("notes", ""),
        } for row in catalog["modules"]],
        "limitations": limitations,
    }, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    report["runtime_catalog"] = INDEX.relative_to(ROOT).as_posix()


def main() -> None:
    catalog = _load_catalog()
    switches = _switches()
    replace_changed = "HearthMedievalLifeReplaceChanged" in switches
    previous = {}
    if REPORT.exists():
        prior = json.loads(REPORT.read_text(encoding="utf-8"))
        for row in prior.get("assets", []) + prior.get("resume_assets", []):
            if row.get("id"):
                previous[row["id"]] = row
    report = {
        "status": "running",
        "engine": ue.SystemLibrary.get_engine_version(),
        "scope": "native Interchange StaticMesh layers; no assemblies merged",
        "destination_root": DEST,
        "source_catalog": CATALOG.relative_to(ROOT).as_posix(),
        "expected_modules": len(catalog["modules"]),
        "expected_layer_assets": sum(len(row["layers"]) for row in catalog["modules"]),
        "assets": [],
        "resume_assets": list(previous.values()),
    }
    try:
        for row in catalog["modules"]:
            report["assets"].extend(_import_module(row, previous, replace_changed))
            REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                              encoding="utf-8")
        if len(report["assets"]) != report["expected_layer_assets"]:
            raise RuntimeError("MedievalLife layer asset count mismatch")
        report["status"] = "passed"
        report.pop("resume_assets", None)
        _write_index(catalog, report)
    except Exception as exc:
        report.update(status="failed", error=str(exc))
        ue.log_error("[MedievalLifeImport] " + str(exc))
        raise
    finally:
        REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                          encoding="utf-8")


if __name__ == "__main__":
    main()
