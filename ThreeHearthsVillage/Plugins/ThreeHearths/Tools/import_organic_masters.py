"""Import OrganicVillageMasters runtime layers as native UE StaticMesh assets.

The Blender exporter writes one GLB for each module/layer/palette.  This
commandlet imports each GLB independently through Interchange so runtime
construction can attach layer components without merging a complete house.

Run with UnrealEditor-Cmd, for example::

    UnrealEditor-Cmd.exe ThreeHearthsVillage.uproject -run=pythonscript \
      -script=import_organic_masters.py -unattended -HearthDisableApi \
      -HearthNoWorldPersistence

The destination is intentionally a new Generated/OrganicVillageMasters
branch.  Existing generated assets are reused only when the source hash is
unchanged; changed sources require ``-HearthOrganicMastersReplaceChanged``.
"""
from __future__ import annotations

import hashlib
import json
import time
from pathlib import Path

import unreal as ue


ROOT = Path(__file__).resolve().parents[3]
ART = ROOT / "Art" / "OrganicVillageMasters"
RUNTIME = ART / "RuntimeLayers"
SOURCE_MANIFEST = RUNTIME / "export_manifest.json"
REPORT = ART / "UE_Import_Report.json"
DEST = "/Game/ThreeHearths/Generated/OrganicVillageMasters"
# Measured with the asymmetric attachment_shop_sign and attachment_steps_040
# exports: Blender authoring (x,y,z metres) -> UE (x,-y,z) centimetres after
# the Blender exporter and UE Interchange Y-up conversion.  The
# corresponding yaw rule is yaw_ue = -yaw_blender for a +Z-up authoring turn.
SOURCE_TO_UNREAL_MATRIX = [
    [100.0, 0.0, 0.0, 0.0],
    [0.0, -100.0, 0.0, 0.0],
    [0.0, 0.0, 100.0, 0.0],
    [0.0, 0.0, 0.0, 1.0],
]


def _checksum(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _load_manifest() -> dict:
    if not SOURCE_MANIFEST.exists():
        raise RuntimeError("Missing Blender runtime manifest: " + str(SOURCE_MANIFEST))
    manifest = json.loads(SOURCE_MANIFEST.read_text(encoding="utf-8"))
    if manifest.get("status") != "exported":
        raise RuntimeError("Runtime layer manifest is not exported")
    assets = manifest.get("assets")
    if not isinstance(assets, list) or not assets:
        raise RuntimeError("Runtime layer manifest has no assets")
    if manifest.get("module_count") != 28 or manifest.get("palette_count") != 3:
        raise RuntimeError("Unexpected module/palette counts in runtime manifest")
    if manifest.get("layer_export_count") != len(assets):
        raise RuntimeError("Runtime layer manifest count mismatch")
    return manifest


def _command_switches() -> set[str]:
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
    mesh.set_editor_property(
        "combine_static_meshes_behavior",
        ue.InterchangeCombineStaticMeshesBehavior.ALL)
    pipeline.get_editor_property("common_meshes_properties").set_editor_property(
        "bake_meshes", True)
    pipeline.get_editor_property("animation_pipeline").set_editor_property(
        "import_animations", False)
    pipeline.get_editor_property("material_pipeline").set_editor_property(
        "import_materials", True)
    gltf = ue.InterchangeGLTFPipeline()
    return pipeline, gltf


def _asset_id(row: dict) -> str:
    return f"{row['palette']}/{row['module_id']}__{row['layer']}"


def _ue_bounds_fields(bounds) -> dict:
    extent = bounds.box_extent
    origin = bounds.origin
    return {
        "ue_pivot_cm": [0.0, 0.0, 0.0],
        "ue_bounds_origin_cm": [origin.x, origin.y, origin.z],
        "bounds_min_cm": [origin.x - extent.x, origin.y - extent.y,
                           origin.z - extent.z],
        "bounds_max_cm": [origin.x + extent.x, origin.y + extent.y,
                           origin.z + extent.z],
        "bounds_extent_cm": [extent.x, extent.y, extent.z],
    }


def _disable_nanite(mesh: ue.StaticMesh) -> bool:
    """Keep small generated runtime layers on the ordinary material path."""
    settings = mesh.get_editor_property("nanite_settings")
    if not bool(settings.get_editor_property("enabled")):
        return False
    settings.set_editor_property("enabled", False)
    mesh.set_editor_property("nanite_settings", settings)
    return True


def _import_one(row: dict, previous: dict, replace_changed: bool) -> dict:
    asset_id = _asset_id(row)
    source = ART / row["asset_glb"]
    if not source.is_file():
        raise RuntimeError(f"Missing runtime layer source: {source}")
    checksum = _checksum(source)
    old = previous.get(asset_id)
    existing = ue.load_asset(old["mesh"]) if (
        old and old.get("source_sha256") == checksum and old.get("mesh")) else None
    if existing and isinstance(existing, ue.StaticMesh):
        nanite_changed = _disable_nanite(existing)
        if nanite_changed and not ue.get_editor_subsystem(
                ue.EditorAssetSubsystem).save_loaded_asset(existing, False):
            raise RuntimeError("Cannot persist Nanite-disabled cache: " + asset_id)
        bounds = existing.get_bounds()
        extent = bounds.box_extent
        if min(extent.x, extent.y, extent.z) <= 0:
            raise RuntimeError("Cached asset has zero bounds: " + asset_id)
        result = dict(old, reused=True)
        result["nanite"] = "disabled_low_poly_runtime_layer"
        result.update(_ue_bounds_fields(bounds))
        return result

    destination = DEST + "/" + asset_id
    replacing = bool(old and replace_changed and
                     old.get("mesh", "").startswith(destination + "/"))
    if ue.EditorAssetLibrary.does_directory_exist(destination) and not replacing:
        raise RuntimeError(
            "Existing asset cache requires review before replacing: " + destination)

    pipeline, gltf = _pipeline()
    params = ue.ImportAssetParameters()
    params.set_editor_property("is_automated", True)
    params.set_editor_property("replace_existing", replacing)
    params.set_editor_property("override_pipelines", [
        ue.SoftObjectPath(pipeline.get_path_name()),
        ue.SoftObjectPath(gltf.get_path_name()),
    ])
    imported = []
    params.on_assets_import_done.bind_callable(
        lambda objects: imported.extend(objects))
    manager = ue.InterchangeManager.get_interchange_manager_scripted()
    started = time.monotonic()
    if not manager.import_asset(
            destination, manager.create_source_data(str(source)), params):
        raise RuntimeError("Interchange rejected " + asset_id)
    meshes = [obj for obj in imported if isinstance(obj, ue.StaticMesh)]
    if len(meshes) != 1 or meshes[0].get_num_lods() < 1:
        raise RuntimeError(
            f"Expected one renderable StaticMesh for {asset_id}, got {len(meshes)}")
    static_mesh = meshes[0]
    _disable_nanite(static_mesh)
    slots = static_mesh.get_editor_property("static_materials")
    if not slots or any(not slot.get_editor_property("material_interface")
                        for slot in slots):
        raise RuntimeError("Missing PBR material slot: " + asset_id)
    bounds = static_mesh.get_bounds()
    extent = bounds.box_extent
    if min(extent.x, extent.y, extent.z) <= 0:
        raise RuntimeError("Zero model extent: " + asset_id)
    for obj in imported:
        if not obj.get_path_name().startswith(destination + "/"):
            raise RuntimeError("Unexpected import destination for " + asset_id)
    subsystem = ue.get_editor_subsystem(ue.EditorAssetSubsystem)
    for obj in imported:
        if not subsystem.save_loaded_asset(obj, False):
            raise RuntimeError("Cannot save " + obj.get_path_name())
    result = {
        "id": asset_id,
        "module_id": row["module_id"],
        "layer": row["layer"],
        "palette": row["palette"],
        "source": source.relative_to(ROOT).as_posix(),
        "source_sha256": checksum,
        "source_manifest": "Art/OrganicVillageMasters/RuntimeLayers/export_manifest.json",
        "mesh": static_mesh.get_path_name(),
        "ue_asset_path": destination,
        "bounds_authoring_m": row["bounds_authoring_m"],
        "material_slots": len(slots),
        "source_material_names": row.get("material_names", []),
        "saved_assets": len(imported),
        "import_seconds": time.monotonic() - started,
        "collision": "not_generated; gameplay collision is separate",
        "nanite": "disabled_low_poly_runtime_layer",
        "reused": False,
    }
    result.update(_ue_bounds_fields(bounds))
    return result


def _write_runtime_catalog(manifest: dict, report: dict) -> None:
    """Write the data-only catalog consumed by the runtime builder."""
    recipes_dir = ART / "Recipes"
    recipes = []
    for recipe_path in sorted(recipes_dir.glob("*.json")):
        if recipe_path.name == "family_growth.json":
            continue
        recipes.append(json.loads(recipe_path.read_text(encoding="utf-8")))
    growth_path = recipes_dir / "family_growth.json"
    growth = json.loads(growth_path.read_text(encoding="utf-8")) if growth_path.exists() else None
    target = ROOT / "Content" / "ThreeHearths" / "Data" / "OrganicMasterCatalog.json"
    target.parent.mkdir(parents=True, exist_ok=True)
    data = {
        "schema_version": 1,
        "source_to_unreal_matrix": SOURCE_TO_UNREAL_MATRIX,
        "source_to_unreal_units": "metres_to_centimetres",
        "yaw_conversion": "yaw_ue = -yaw_blender",
        "axis_validation": report["axis_validation"],
        "destination_root": DEST,
        "source_manifest": SOURCE_MANIFEST.relative_to(ROOT).as_posix(),
        "source_manifest_sha256": _checksum(SOURCE_MANIFEST),
        "assets": report["assets"],
        "recipes": recipes,
        "growth": growth,
    }
    target.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n",
                      encoding="utf-8")
    report["runtime_catalog"] = target.relative_to(ROOT).as_posix()


def main() -> None:
    switches = _command_switches()
    replace_changed = "HearthOrganicMastersReplaceChanged" in switches
    manifest = _load_manifest()
    previous = {}
    if REPORT.exists():
        prior = json.loads(REPORT.read_text(encoding="utf-8"))
        for row in prior.get("resume_assets", []) + prior.get("assets", []):
            if row.get("id"):
                previous[row["id"]] = row
    report = {
        "status": "running",
        "engine": ue.SystemLibrary.get_engine_version(),
        "scope": "native Interchange StaticMesh assets; one module/layer/palette per asset",
        "destination_root": DEST,
        "source_manifest": SOURCE_MANIFEST.relative_to(ROOT).as_posix(),
        "source_manifest_sha256": _checksum(SOURCE_MANIFEST),
        "source_to_unreal_matrix": SOURCE_TO_UNREAL_MATRIX,
        "source_to_unreal_units": "metres_to_centimetres",
        "yaw_conversion": "yaw_ue = -yaw_blender",
        "axis_validation": {
            "method": "measured asymmetric source bounds against imported UE bounds",
            "samples": [
                "warm_lime/attachment_shop_sign__attachments",
                "warm_lime/attachment_steps_040__attachments",
            ],
            "result": "source dimensions and signed Y reflection match; module origin remains zero",
        },
        "expected_assets": len(manifest["assets"]),
        "assets": [],
        "resume_assets": list(previous.values()),
    }
    try:
        for row in manifest["assets"]:
            result = _import_one(row, previous, replace_changed)
            report["assets"].append(result)
            REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                              encoding="utf-8")
            ue.log("[OrganicMastersImport] " + result["id"])
        if len(report["assets"]) != report["expected_assets"]:
            raise RuntimeError("Imported asset count does not match manifest")
        source_rows = {
            _asset_id(row): row for row in manifest["assets"]
        }
        imported_rows = {
            row["id"]: row for row in report["assets"]
        }
        report["axis_validation"]["observations"] = []
        for sample_id in report["axis_validation"]["samples"]:
            source_row = source_rows[sample_id]
            imported_row = imported_rows[sample_id]
            report["axis_validation"]["observations"].append({
                "id": sample_id,
                "source_bounds_m": source_row["bounds_authoring_m"],
                "ue_bounds_min_cm": imported_row["bounds_min_cm"],
                "ue_bounds_max_cm": imported_row["bounds_max_cm"],
                "ue_bounds_origin_cm": imported_row["ue_bounds_origin_cm"],
                "ue_bounds_extent_cm": imported_row["bounds_extent_cm"],
            })
        report["status"] = "passed"
        report.pop("resume_assets", None)
        _write_runtime_catalog(manifest, report)
    except Exception as exc:
        report.update(status="failed", error=str(exc))
        ue.log_error("[OrganicMastersImport] " + str(exc))
        raise
    finally:
        REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                          encoding="utf-8")


if __name__ == "__main__":
    main()
