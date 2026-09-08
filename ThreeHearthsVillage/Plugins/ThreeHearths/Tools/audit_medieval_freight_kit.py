"""Read-only cold audit for the native MedievalLife FreightKit import.

The manifest remains the source of truth.  This audit reloads every declared
StaticMesh, checks its importData/source hash, renderability, material slots,
UVs and Nanite policy, then compares each module's union bounds with the
manifest's metre-space bounds after the project's measured Unreal conversion.
It writes a failed report with the rows reached before an exception; it never
turns an incomplete import into a passing result.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path

import unreal as ue


ROOT = Path(__file__).resolve().parents[3]
ART = ROOT / "Art" / "MedievalLife" / "FreightKit"
MANIFEST = ART / "manifest.json"
IMPORT_REPORT = ART / "UE_Import_Report.json"
REPORT = ART / "UE_ColdAudit.json"
DEST = "/Game/ThreeHearths/Generated/MedievalLife/FreightKit"
EXPECTED_IDS = {"cargo_planks", "cargo_beams", "cart_traces", "freight_depot_rack"}
MATRIX = [[100.0, 0.0, 0.0, 0.0], [0.0, -100.0, 0.0, 0.0],
          [0.0, 0.0, 100.0, 0.0], [0.0, 0.0, 0.0, 1.0]]


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _source(relative: str) -> Path:
    value = (ART / relative).resolve()
    try:
        value.relative_to(ART.resolve())
    except ValueError as exc:
        raise RuntimeError("FreightKit FBX escaped source directory: " + relative) from exc
    if value.suffix.lower() != ".fbx" or not value.is_file():
        raise RuntimeError("Missing FreightKit FBX: " + str(value))
    return value


def _manifest() -> dict:
    if not MANIFEST.is_file():
        raise RuntimeError("Missing FreightKit manifest: " + str(MANIFEST))
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if data.get("schema_version") != 1 or not isinstance(data.get("modules"), list):
        raise RuntimeError("FreightKit manifest must have schema_version=1 and modules[]")
    ids = {str(row.get("id", "")) for row in data["modules"]}
    if ids != EXPECTED_IDS or len(data["modules"]) != len(EXPECTED_IDS):
        raise RuntimeError("FreightKit module IDs mismatch: " + repr(sorted(ids)))
    for module in data["modules"]:
        module_id = str(module.get("id", ""))
        layers = module.get("layers")
        if not isinstance(layers, list) or not layers:
            raise RuntimeError("FreightKit layers missing for " + module_id)
        names = [str(item.get("layer", "")) for item in layers]
        if any(not item.get("object_name") for item in layers) or len(names) != len(set(names)):
            raise RuntimeError("FreightKit layer/object_name contract invalid for " + module_id)
        bounds = module.get("bounds_m", {})
        if (not isinstance(bounds, dict) or len(bounds.get("min", [])) != 3 or
                len(bounds.get("max", [])) != 3 or
                any(float(a) >= float(b) for a, b in zip(bounds["min"], bounds["max"]))):
            raise RuntimeError("FreightKit bounds_m invalid for " + module_id)
        source = _source(str(module.get("fbx", "")))
        declared_hash = str(module.get("sha256", "")).strip().lower()
        if declared_hash and _sha256(source).lower() != declared_hash:
            raise RuntimeError("FreightKit manifest sha256 is stale for " + module_id)
    return data


def _source_filename(mesh: ue.StaticMesh) -> str:
    data = mesh.get_editor_property("asset_import_data")
    for name in ("get_first_filename", "script_get_first_filename", "k2_get_first_filename"):
        method = getattr(data, name, None)
        if method:
            try:
                value = str(method())
                if value:
                    return value
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


def _bounds(mesh: ue.StaticMesh) -> tuple[list[float], list[float]]:
    value = mesh.get_bounds()
    lo = [value.origin.x - value.box_extent.x, value.origin.y - value.box_extent.y,
          value.origin.z - value.box_extent.z]
    hi = [value.origin.x + value.box_extent.x, value.origin.y + value.box_extent.y,
          value.origin.z + value.box_extent.z]
    if any(a >= b for a, b in zip(lo, hi)):
        raise RuntimeError("Degenerate FreightKit bounds: " + mesh.get_path_name())
    return lo, hi


def _expected_bounds(module: dict) -> tuple[list[float], list[float]]:
    bounds = module["bounds_m"]
    # Authored Blender space is metres, +Z up and front -Y.  The project
    # conversion is measured X*100, Y*-100, Z*100.
    return ([float(bounds["min"][0]) * 100.0, -float(bounds["max"][1]) * 100.0,
             float(bounds["min"][2]) * 100.0],
            [float(bounds["max"][0]) * 100.0, -float(bounds["min"][1]) * 100.0,
             float(bounds["max"][2]) * 100.0])


def _asset_row(row: dict, module: dict, layer: dict, checksum: str, subsystem: object) -> dict:
    asset_id = f"{module['id']}/{layer['layer']}"
    if row.get("id") != asset_id:
        raise RuntimeError("Import report layer ID mismatch: " + repr(row.get("id")))
    mesh_path = str(row.get("mesh", ""))
    if not mesh_path.startswith(DEST + "/"):
        raise RuntimeError("FreightKit mesh escaped destination: " + mesh_path)
    mesh = ue.load_asset(mesh_path)
    if not isinstance(mesh, ue.StaticMesh):
        raise RuntimeError("FreightKit asset is not a StaticMesh: " + mesh_path)
    source = _source(str(module["fbx"]))
    import_data = _source_filename(mesh)
    if not _same_source(import_data, source):
        raise RuntimeError("FreightKit importData source mismatch: " + mesh_path)
    if row.get("source_sha256") != checksum:
        raise RuntimeError("FreightKit source hash mismatch: " + asset_id)
    settings = mesh.get_editor_property("nanite_settings")
    if bool(settings.get_editor_property("enabled")):
        raise RuntimeError("FreightKit Nanite must be disabled: " + mesh_path)
    slots = mesh.get_editor_property("static_materials")
    if not slots:
        raise RuntimeError("FreightKit layer has no material slots: " + mesh_path)
    material_paths = []
    for slot in slots:
        material = slot.get_editor_property("material_interface")
        if material is None:
            raise RuntimeError("FreightKit layer has an empty material slot: " + mesh_path)
        material_paths.append(material.get_path_name())
    uv_channels = int(subsystem.get_num_uv_channels(mesh, 0))
    vertex_count = int(subsystem.get_number_verts(mesh, 0))
    if uv_channels < 1 or vertex_count <= 0:
        raise RuntimeError("FreightKit layer lacks UVs/vertices: " + mesh_path)
    lo, hi = _bounds(mesh)
    return {
        "id": asset_id,
        "module_id": module["id"],
        "layer": layer["layer"],
        "mesh": mesh_path,
        "source": source.relative_to(ROOT).as_posix(),
        "source_sha256": checksum,
        "source_import_data": import_data,
        "bounds_ue_cm": {"min_cm": lo, "max_cm": hi},
        "material_paths": material_paths,
        "material_slot_count": len(slots),
        "uv_channels": uv_channels,
        "vertex_count": vertex_count,
        "nanite": False,
        "cold_loaded": True,
    }


def main() -> None:
    report = {
        "schema_version": 1,
        "status": "running",
        "audit": "cold native reload; no assets modified",
        "destination_root": DEST,
        "assets": [],
        "modules": [],
    }
    try:
        manifest = _manifest()
        if not IMPORT_REPORT.is_file():
            raise RuntimeError("Missing importer report: " + str(IMPORT_REPORT))
        imported = json.loads(IMPORT_REPORT.read_text(encoding="utf-8"))
        if imported.get("status") != "passed":
            raise RuntimeError("Importer report is not passed: " + repr(imported.get("status")))
        expected = {f"{module['id']}/{layer['layer']}"
                    for module in manifest["modules"] for layer in module["layers"]}
        rows = {row.get("id"): row for row in imported.get("assets", [])}
        if set(rows) != expected:
            raise RuntimeError("Importer layer set mismatch")
        report["source_manifest"] = MANIFEST.relative_to(ROOT).as_posix()
        report["source_manifest_sha256"] = _sha256(MANIFEST)
        report["source_to_unreal_matrix"] = MATRIX
        subsystem = _subsystem()
        module_rows = []
        for module in manifest["modules"]:
            source = _source(str(module["fbx"]))
            checksum = _sha256(source)
            actual = []
            for layer in module["layers"]:
                record = _asset_row(rows[f"{module['id']}/{layer['layer']}"], module,
                                    layer, checksum, subsystem)
                report["assets"].append(record)
                actual.append((record["bounds_ue_cm"]["min_cm"], record["bounds_ue_cm"]["max_cm"]))
            actual_lo = [min(bounds[0][axis] for bounds in actual) for axis in range(3)]
            actual_hi = [max(bounds[1][axis] for bounds in actual) for axis in range(3)]
            expected_lo, expected_hi = _expected_bounds(module)
            error = max(abs(a - b) for a, b in zip(actual_lo + actual_hi,
                                                   expected_lo + expected_hi))
            if error > 0.5:
                raise RuntimeError(f"FreightKit bounds mismatch {module['id']}: {error:.4f} cm")
            module_rows.append({"id": module["id"], "bounds_axis_error_cm": error,
                                "bounds_ue_cm": {"min_cm": actual_lo, "max_cm": actual_hi},
                                "expected_bounds_ue_cm": {"min_cm": expected_lo,
                                                           "max_cm": expected_hi}})
        report["modules"] = module_rows
        report["status"] = "passed"
        report["layer_count"] = len(report["assets"])
        report["module_count"] = len(module_rows)
        report["limitations"] = ["Static FreightKit geometry only; no runtime cart behavior tested.",
                                  "No animation or rig verification is applicable."]
    except Exception as exc:
        report.update(status="failed", error=str(exc))
        ue.log_error("[MedievalFreightKitAudit] " + str(exc))
        raise
    finally:
        REPORT.parent.mkdir(parents=True, exist_ok=True)
        REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        ue.log("[MedievalFreightKitAudit] " + report["status"])


if __name__ == "__main__":
    main()
