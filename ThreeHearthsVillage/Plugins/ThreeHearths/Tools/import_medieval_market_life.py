"""Import MedievalLife MarketLifeKit FBX layers as native UE StaticMeshes.

The MarketLifeKit manifest is the source of truth. Each FBX is imported with
Interchange DO_NOT_COMBINE, so every declared object/layer remains a separate
StaticMesh under /Game/ThreeHearths/Generated/MedievalLife/MarketLifeKit. Source
hashes are recalculated on every run and failed reports retain completed rows.
"""
from __future__ import annotations

import hashlib
import json
import time
from pathlib import Path

import unreal as ue


ROOT = Path(__file__).resolve().parents[3]
ART = ROOT / "Art" / "MedievalLife" / "MarketLifeKit"
MANIFEST = ART / "manifest.json"
DEST = "/Game/ThreeHearths/Generated/MedievalLife/MarketLifeKit"
REPORT = ART / "UE_Import_Report.json"
INDEX = ROOT / "Content" / "ThreeHearths" / "Data" / "MedievalMarketLifeCatalog.json"
EXPECTED_IDS = {"bench_low", "work_table", "tool_rack", "linen_canopy", "clay_jar"}
MATRIX = [[100.0, 0.0, 0.0, 0.0], [0.0, -100.0, 0.0, 0.0],
          [0.0, 0.0, 100.0, 0.0], [0.0, 0.0, 0.0, 1.0]]


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _safe_source(relative: str) -> Path:
    source = (ART / relative).resolve()
    try:
        source.relative_to(ART.resolve())
    except ValueError as exc:
        raise RuntimeError("MarketLifeKit FBX escaped source directory: " + relative) from exc
    if source.suffix.lower() != ".fbx" or not source.is_file():
        raise RuntimeError("Missing MarketLifeKit FBX: " + str(source))
    return source


def _manifest() -> dict:
    if not MANIFEST.is_file():
        raise RuntimeError("Missing MarketLifeKit manifest: " + str(MANIFEST))
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if data.get("schema_version") != 1 or not isinstance(data.get("modules"), list):
        raise RuntimeError("MarketLifeKit manifest must have schema_version=1 and modules[]")
    modules = data["modules"]
    ids = {str(row.get("id", "")) for row in modules}
    if ids != EXPECTED_IDS or len(modules) != len(EXPECTED_IDS):
        raise RuntimeError("MarketLifeKit module IDs mismatch: " + repr(sorted(ids)))
    for row in modules:
        module_id = str(row.get("id"))
        layers = row.get("layers")
        if not isinstance(layers, list) or not layers:
            raise RuntimeError("MarketLifeKit layers missing for " + module_id)
        layer_ids = [str(item.get("layer", "")) for item in layers]
        if len(layer_ids) != len(set(layer_ids)) or any(not item.get("object_name") for item in layers):
            raise RuntimeError("MarketLifeKit layer/object_name contract invalid for " + module_id)
        bounds = row.get("bounds_m", {})
        if (not isinstance(bounds, dict) or len(bounds.get("min", [])) != 3 or
                len(bounds.get("max", [])) != 3 or
                any(float(a) >= float(b) for a, b in zip(bounds["min"], bounds["max"]))):
            raise RuntimeError("MarketLifeKit bounds_m invalid for " + module_id)
        source = _safe_source(str(row.get("fbx", "")))
        declared_hash = str(row.get("sha256", "")).strip().lower()
        if declared_hash and _sha256(source).lower() != declared_hash:
            raise RuntimeError("MarketLifeKit manifest sha256 is stale for " + module_id)
    return data


def _switches() -> set[str]:
    _, switches, _ = ue.SystemLibrary.parse_command_line(ue.SystemLibrary.get_command_line())
    return set(switches)


def _pipeline() -> object:
    pipeline = ue.InterchangeGenericAssetsPipeline()
    mesh = pipeline.get_editor_property("mesh_pipeline")
    mesh.set_editor_property("import_static_meshes", True)
    mesh.set_editor_property("import_skeletal_meshes", False)
    mesh.set_editor_property("collision", False)
    no_combine = getattr(ue.InterchangeCombineStaticMeshesBehavior, "DO_NOT_COMBINE", None)
    if no_combine is None:
        raise RuntimeError("UE 5.8 lacks DO_NOT_COMBINE static mesh behavior")
    mesh.set_editor_property("combine_static_meshes_behavior", no_combine)
    pipeline.get_editor_property("common_meshes_properties").set_editor_property("bake_meshes", True)
    pipeline.get_editor_property("animation_pipeline").set_editor_property("import_animations", False)
    pipeline.get_editor_property("material_pipeline").set_editor_property("import_materials", True)
    # This task imports FBX only.  The generic assets pipeline owns the FBX
    # translator; adding the GLTF override would broaden translator selection
    # unnecessarily and is not needed for these sources.
    return pipeline


def _disable_nanite(mesh: ue.StaticMesh) -> None:
    settings = mesh.get_editor_property("nanite_settings")
    if bool(settings.get_editor_property("enabled")):
        settings.set_editor_property("enabled", False)
        mesh.set_editor_property("nanite_settings", settings)


def _bounds(mesh: ue.StaticMesh) -> dict:
    value = mesh.get_bounds()
    origin, extent = value.origin, value.box_extent
    return {
        "origin_cm": [origin.x, origin.y, origin.z],
        "min_cm": [origin.x - extent.x, origin.y - extent.y, origin.z - extent.z],
        "max_cm": [origin.x + extent.x, origin.y + extent.y, origin.z + extent.z],
        "extent_cm": [extent.x, extent.y, extent.z],
    }


def _anchors_cm(module: dict) -> dict:
    return {
        name: [float(value[0]) * 100.0, -float(value[1]) * 100.0, float(value[2]) * 100.0]
        for name, value in module.get("anchors_m", {}).items()
    }


def _source_filename(mesh: ue.StaticMesh) -> str:
    try:
        data = mesh.get_editor_property("asset_import_data")
    except Exception as exc:
        raise RuntimeError("StaticMesh has no asset_import_data: " + mesh.get_path_name()) from exc
    for name in ("get_first_filename", "script_get_first_filename", "k2_get_first_filename"):
        method = getattr(data, name, None)
        if method:
            try:
                result = str(method())
                if result:
                    return result
            except Exception:
                pass
    raise RuntimeError("StaticMesh importData has no source filename: " + mesh.get_path_name())


def _same_source(actual: str, expected: Path) -> bool:
    try:
        return Path(actual).resolve().as_posix().casefold() == expected.resolve().as_posix().casefold()
    except OSError:
        return actual.replace("\\", "/").casefold() == expected.as_posix().casefold()


def _subsystem() -> object:
    subsystem = ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
    return subsystem if subsystem is not None else ue.get_default_object(ue.StaticMeshEditorSubsystem)


def _mesh_record(mesh: ue.StaticMesh, module: dict, layer: dict, source: Path,
                 checksum: str, destination: str) -> dict:
    if not mesh.get_path_name().startswith(destination + "/"):
        raise RuntimeError("MarketLifeKit mesh escaped module destination: " + mesh.get_path_name())
    if not _same_source(_source_filename(mesh), source):
        raise RuntimeError("MarketLifeKit importData source mismatch: " + mesh.get_path_name())
    _disable_nanite(mesh)
    slots = mesh.get_editor_property("static_materials")
    if not slots:
        raise RuntimeError("MarketLifeKit layer has no material slots: " + mesh.get_path_name())
    materials = []
    for slot in slots:
        material = slot.get_editor_property("material_interface")
        if material is None:
            raise RuntimeError("MarketLifeKit layer has empty material slot: " + mesh.get_path_name())
        materials.append(material.get_path_name())
    bounds = _bounds(mesh)
    if min(bounds["extent_cm"]) <= 0.0:
        raise RuntimeError("MarketLifeKit layer has degenerate bounds: " + mesh.get_path_name())
    subsystem = _subsystem()
    uv_channels = int(subsystem.get_num_uv_channels(mesh, 0))
    vertex_count = int(subsystem.get_number_verts(mesh, 0))
    if uv_channels < 1 or vertex_count <= 0:
        raise RuntimeError("MarketLifeKit layer lacks renderable UV/vertices: " + mesh.get_path_name())
    return {
        "id": f"{module['id']}/{layer['layer']}",
        "module_id": module["id"],
        "layer": layer["layer"],
        "object_name": layer["object_name"],
        "source": source.relative_to(ROOT).as_posix(),
        "source_sha256": checksum,
        "source_import_data": _source_filename(mesh),
        "mesh": mesh.get_path_name(),
        "ue_asset_folder": destination,
        "front_axis": "-Y",
        "up_axis": "+Z",
        "anchors_m": module.get("anchors_m", {}),
        "anchors_ue_cm": _anchors_cm(module),
        "bounds_authoring_m": module["bounds_m"],
        "bounds_ue_cm": bounds,
        "material_paths": materials,
        "material_slot_count": len(materials),
        "uv_channels": uv_channels,
        "vertex_count": vertex_count,
        "nanite": False,
        "reused": False,
    }


def _save_objects(objects: list[object]) -> None:
    subsystem = ue.get_editor_subsystem(ue.EditorAssetSubsystem)
    seen = set()
    for obj in objects:
        path = obj.get_path_name()
        if path in seen:
            continue
        seen.add(path)
        if not path.startswith(DEST + "/"):
            raise RuntimeError("MarketLifeKit import escaped destination: " + path)
        if not subsystem.save_loaded_asset(obj, False):
            raise RuntimeError("Cannot save MarketLifeKit asset: " + path)


def _layer_for_mesh(mesh: ue.StaticMesh, module: dict) -> str | None:
    leaf = mesh.get_path_name().rsplit("/", 1)[-1].split(".", 1)[0].casefold()
    for row in module["layers"]:
        layer = str(row["layer"])
        object_name = str(row["object_name"])
        if leaf == object_name.casefold() or leaf.endswith("__" + layer.casefold()):
            return layer
    return None


def _import_module(module: dict, previous: dict, replace_changed: bool) -> list[dict]:
    module_id = module["id"]
    source = _safe_source(module["fbx"])
    checksum = _sha256(source)
    layers = module["layers"]
    prior = {f"{module_id}/{row['layer']}": previous.get(f"{module_id}/{row['layer']}") for row in layers}
    cached = []
    for row in layers:
        old = prior[f"{module_id}/{row['layer']}"]
        mesh = ue.load_asset(old["mesh"]) if old and old.get("source_sha256") == checksum else None
        if not isinstance(mesh, ue.StaticMesh):
            cached = []
            break
        cached.append(_mesh_record(mesh, module, row, source, checksum, DEST + "/" + module_id))
    if len(cached) == len(layers):
        for record in cached:
            record["reused"] = True
        return cached
    destination = DEST + "/" + module_id
    replacing = bool(replace_changed and any(
        prior[f"{module_id}/{row['layer']}"] and
        prior[f"{module_id}/{row['layer']}"]["mesh"].startswith(destination + "/")
        for row in layers))
    if ue.EditorAssetLibrary.does_directory_exist(destination) and not replacing:
        raise RuntimeError("Existing MarketLifeKit destination requires prior report or replace switch: " + destination)
    pipeline = _pipeline()
    params = ue.ImportAssetParameters()
    params.set_editor_property("is_automated", True)
    params.set_editor_property("replace_existing", replacing)
    params.set_editor_property("override_pipelines", [ue.SoftObjectPath(pipeline.get_path_name())])
    imported = []
    params.on_assets_import_done.bind_callable(lambda objects: imported.extend(objects))
    manager = ue.InterchangeManager.get_interchange_manager_scripted()
    started = time.monotonic()
    if not manager.import_asset(destination, manager.create_source_data(str(source)), params):
        raise RuntimeError("Interchange rejected MarketLifeKit module: " + module_id)
    meshes = [obj for obj in imported if isinstance(obj, ue.StaticMesh)]
    if len(meshes) != len(layers):
        raise RuntimeError(f"Expected {len(layers)} MarketLifeKit StaticMeshes for {module_id}, got "
                           f"{[mesh.get_path_name() for mesh in meshes]}")
    by_layer = {}
    for mesh in meshes:
        layer = _layer_for_mesh(mesh, module)
        if layer is None:
            raise RuntimeError("Cannot classify MarketLifeKit imported mesh: " + mesh.get_path_name())
        if layer in by_layer:
            raise RuntimeError("Duplicate MarketLifeKit layer: " + module_id + "/" + layer)
        by_layer[layer] = mesh
    if set(by_layer) != {row["layer"] for row in layers}:
        raise RuntimeError("MarketLifeKit layer set mismatch for " + module_id)
    records = [_mesh_record(by_layer[row["layer"]], module, row, source, checksum, destination)
               for row in sorted(layers, key=lambda item: item["layer"])]
    for record in records:
        record["import_seconds"] = time.monotonic() - started
    _save_objects(imported)
    return records


def _write_index(manifest: dict, report: dict) -> None:
    INDEX.parent.mkdir(parents=True, exist_ok=True)
    INDEX.write_text(json.dumps({
        "schema_version": 1,
        "source_manifest": MANIFEST.relative_to(ROOT).as_posix(),
        "source_manifest_sha256": _sha256(MANIFEST),
        "destination_root": DEST,
        "source_to_unreal_matrix": MATRIX,
        "source_to_unreal_units": "metres_to_centimetres; reflected Y",
        "yaw_conversion": "yaw_ue = -yaw_blender",
        "assets": report["assets"],
        "modules": manifest["modules"],
        "limitations": ["MarketLifeKit contains static market props only; no runtime commerce behavior is implied."],
    }, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    report = {
        "schema_version": 1,
        "status": "running",
        "destination_root": DEST,
        "source_manifest": MANIFEST.relative_to(ROOT).as_posix(),
        "assets": [],
    }
    try:
        manifest = _manifest()
        replace_changed = "HearthMedievalMarketLifeReplaceChanged" in _switches()
        previous = {}
        if REPORT.is_file():
            old = json.loads(REPORT.read_text(encoding="utf-8"))
            previous = {row["id"]: row for row in old.get("assets", []) if row.get("id")}
        expected_ids = {f"{m['id']}/{layer['layer']}"
                        for m in manifest["modules"] for layer in m["layers"]}
        current_hashes = {
            module["id"]: _sha256(_safe_source(str(module["fbx"])))
            for module in manifest["modules"]
        }
        report.update(expected_modules=len(manifest["modules"]),
                     expected_layer_assets=len(expected_ids),
                     # Keep prior rows available for resume diagnostics. Each
                     # completed module below replaces only its own IDs.
                     assets=[previous[asset_id] for asset_id in sorted(expected_ids)
                             if asset_id in previous])
        for module in manifest["modules"]:
            module_ids = {f"{module['id']}/{layer['layer']}" for layer in module["layers"]}
            replacement = _import_module(module, previous, replace_changed)
            report["assets"] = [row for row in report["assets"] if row.get("id") not in module_ids]
            report["assets"].extend(replacement)
            REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        actual_ids = {row.get("id") for row in report["assets"]}
        if len(report["assets"]) != len(expected_ids) or actual_ids != expected_ids:
            raise RuntimeError("MarketLifeKit layer asset set mismatch")
        for row in report["assets"]:
            if row.get("source_sha256") != current_hashes.get(row.get("module_id")):
                raise RuntimeError("MarketLifeKit report source hash is stale: " + str(row.get("id")))
        report.update(status="passed", source_manifest_sha256=_sha256(MANIFEST),
                      source_to_unreal_matrix=MATRIX)
        _write_index(manifest, report)
    except Exception as exc:
        report.update(status="failed", error=str(exc))
        ue.log_error("[MedievalMarketLifeImport] " + str(exc))
        raise
    finally:
        REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
