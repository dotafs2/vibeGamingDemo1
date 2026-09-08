"""Validate the OrganicVillageMasters catalog and round-trip every listed GLB.

Run from Blender's Python (the script imports ``bpy``).  The validator is
deliberately contract-driven: catalog.json supplies the asset paths and the
expected hierarchy/connector metadata, while this script checks the claims
against a fresh GLB import without changing any source assets.
"""
from __future__ import annotations

import argparse
import json
import math
import re
import sys
from pathlib import Path

import bpy
from mathutils import Vector


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CATALOG = ROOT / "catalog.json"
DEFAULT_REPORT = ROOT / "validation.json"


def finite(value):
    if isinstance(value, (list, tuple)):
        return all(finite(x) for x in value)
    if not isinstance(value, (str, bytes, int, float)):
        try:
            return all(finite(x) for x in value)
        except TypeError:
            pass
    return isinstance(value, (int, float)) and math.isfinite(float(value))


def bounds(objects):
    points = [obj.matrix_world @ Vector(corner) for obj in objects for corner in obj.bound_box]
    if not points:
        return None
    lo = [min(point[i] for point in points) for i in range(3)]
    hi = [max(point[i] for point in points) for i in range(3)]
    return {"min": lo, "max": hi, "size": [hi[i] - lo[i] for i in range(3)]}


def object_stats(objects):
    meshes = [obj for obj in objects if obj.type == "MESH"]
    for mesh in meshes:
        mesh.data.calc_loop_triangles()
    return {
        "objects": len(objects),
        "mesh_objects": len(meshes),
        "triangles": sum(len(mesh.data.loop_triangles) for mesh in meshes),
        "materials": sum(len(mesh.data.materials) for mesh in meshes),
        "uv_mesh_objects": sum(bool(mesh.data.uv_layers) for mesh in meshes),
        "bounds_m": bounds(meshes),
    }


def pbr_stats():
    """Return material-node evidence from the just-imported GLB."""
    materials = list(bpy.data.materials)
    missing_principled = []
    nonfinite_pbr = []
    for material in materials:
        nodes = material.node_tree.nodes if material.use_nodes and material.node_tree else []
        principled = next((node for node in nodes if node.type == "BSDF_PRINCIPLED"), None)
        if principled is None:
            missing_principled.append(material.name)
            continue
        for socket_name in ("Base Color", "Roughness", "Metallic"):
            socket = principled.inputs.get(socket_name)
            if socket and not finite(socket.default_value):
                nonfinite_pbr.append({"material": material.name, "socket": socket_name})
    return {
        "material_count": len(materials),
        "pbr_material_count": len(materials) - len(missing_principled),
        "missing_principled": missing_principled,
        "nonfinite_pbr": nonfinite_pbr,
    }


def clean_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def import_glb(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(path), import_pack_images=True)
    return [obj for obj in bpy.data.objects if obj not in before]


def roots(objects):
    listed = set(objects)
    return [obj for obj in objects if obj.parent not in listed]


def check_bounds(actual, expected, errors, asset_id):
    if not actual:
        errors.append({"id": asset_id, "error": "empty mesh bounds"})
        return
    if not finite(actual["min"]) or not finite(actual["max"]):
        errors.append({"id": asset_id, "error": "non-finite bounds"})
        return
    if any(size <= 1e-5 for size in actual["size"]):
        errors.append({"id": asset_id, "error": "degenerate bounds", "size": actual["size"]})
    if expected:
        expected_min = expected.get("min", expected.get("min_m"))
        expected_max = expected.get("max", expected.get("max_m"))
        expected_size = expected.get("size", expected.get("size_m"))
        for label, values in (("min", expected_min), ("max", expected_max), ("size", expected_size)):
            if values is not None and (not finite(values) or len(values) != 3):
                errors.append({"id": asset_id, "error": "invalid catalog bounds", "field": label})
        if expected_size and max(abs(actual["size"][i] - expected_size[i]) for i in range(3)) > 0.02:
            errors.append({"id": asset_id, "error": "bounds size mismatch", "expected": expected_size, "actual": actual["size"]})


def validate_catalog(catalog_path, report_path):
    catalog = json.loads(catalog_path.read_text(encoding="utf-8"))
    errors = []
    warnings = []
    modules = catalog.get("modules", [])
    assemblies = catalog.get("assemblies", [])
    if not modules and not assemblies:
        errors.append({"error": "catalog has no modules or assemblies"})

    ids = []
    for entry in modules + assemblies:
        asset_id = entry.get("id")
        if not isinstance(asset_id, str) or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]*", asset_id):
            errors.append({"id": asset_id, "error": "missing or invalid id"})
        ids.append(asset_id)
    seen = set()
    for asset_id in ids:
        if asset_id in seen:
            errors.append({"id": asset_id, "error": "duplicate id"})
        seen.add(asset_id)

    module_ids = {entry.get("id") for entry in modules}
    valid_connectors = set(catalog.get("connector_types", catalog.get("connectors", [])))
    assets = []
    for entry in modules + assemblies:
        asset_id = entry.get("id", "<missing>")
        raw_path = entry.get("path")
        if not raw_path:
            errors.append({"id": asset_id, "error": "missing path"})
            continue
        path = Path(raw_path)
        if not path.is_absolute():
            path = (catalog_path.parent / path).resolve()
        record = {"id": asset_id, "path": str(path), "kind": "assembly" if entry in assemblies else "module"}
        if not path.is_file():
            errors.append({"id": asset_id, "error": "missing GLB", "path": str(path)})
            assets.append(record)
            continue
        for field in ("layers", "anchors", "bounds_m"):
            if field not in entry:
                warnings.append({"id": asset_id, "warning": "catalog omits " + field})
        expected_layers = set(entry.get("layers", []))
        expected_anchors = set(entry.get("anchors", []))
        for connector in entry.get("connectors", []):
            if valid_connectors and connector not in valid_connectors:
                errors.append({"id": asset_id, "error": "unknown connector type", "connector": connector})
        clean_scene()
        try:
            imported = import_glb(path)
        except Exception as exc:
            errors.append({"id": asset_id, "error": "GLB import failed", "detail": str(exc)})
            assets.append(record)
            continue
        mesh_objects = [obj for obj in imported if obj.type == "MESH"]
        actual = object_stats(imported)
        actual["image_count"] = len(bpy.data.images)
        actual["pbr"] = pbr_stats()
        exported_instance_keys = [str(obj.get("instance_key")) for obj in imported if obj.get("instance_key")]
        actual["instance_key_count"] = len(exported_instance_keys)
        actual["instance_key_unique"] = len(set(exported_instance_keys)) == len(exported_instance_keys)
        record["stats"] = actual
        if not mesh_objects:
            errors.append({"id": asset_id, "error": "GLB contains no mesh objects"})
        check_bounds(actual["bounds_m"], entry.get("bounds_m", entry.get("bounds")), errors, asset_id)
        names = {obj.name.lower() for obj in imported}
        imported_roots = roots(imported)
        hierarchy = entry.get("source_hierarchy", entry.get("hierarchy", {})) or {}
        expected_root = hierarchy.get("root")
        if expected_root and not any(obj.name == expected_root for obj in imported_roots):
            errors.append({"id": asset_id, "error": "source hierarchy root missing", "expected": expected_root, "roots": [obj.name for obj in imported_roots]})
        required_children = hierarchy.get("required_children", hierarchy.get("children", []))
        if required_children:
            imported_names = {obj.name for obj in imported}
            absent_children = [name for name in required_children if name not in imported_names]
            if absent_children:
                errors.append({"id": asset_id, "error": "source hierarchy children missing", "children": absent_children})
        custom_layers = {str(obj.get("layer")).lower() for obj in imported if obj.get("layer")}
        custom_anchors = {str(obj.get("anchor")).lower() for obj in imported if obj.get("anchor")}
        if expected_layers and not expected_layers.issubset(custom_layers):
            missing_layers = [layer for layer in expected_layers if layer.lower() not in custom_layers and not any(layer.lower() in name for name in names)]
            if missing_layers:
                errors.append({"id": asset_id, "error": "declared layer categories absent", "layers": sorted(missing_layers)})
            else:
                warnings.append({"id": asset_id, "warning": "layer metadata preserved in names rather than custom properties", "expected": sorted(expected_layers), "found": sorted(custom_layers)})
        if expected_anchors and not expected_anchors.intersection(custom_anchors) and not any(any(anchor.lower() in name for name in names) for anchor in expected_anchors):
            errors.append({"id": asset_id, "error": "no declared connector anchor found", "anchors": sorted(expected_anchors)})
        material_ids = entry.get("material_ids", entry.get("materials", []))
        if isinstance(material_ids, dict):
            material_ids = list(material_ids)
        if material_ids and actual["materials"] <= 0:
            errors.append({"id": asset_id, "error": "declared materials absent after GLB import", "expected": material_ids})
        if actual["materials"] > 0 and actual["pbr"]["missing_principled"]:
            errors.append({"id": asset_id, "error": "imported material lacks Principled PBR node", "materials": actual["pbr"]["missing_principled"]})
        if actual["pbr"]["nonfinite_pbr"]:
            errors.append({"id": asset_id, "error": "non-finite PBR socket value", "values": actual["pbr"]["nonfinite_pbr"]})
        recipe_name = entry.get("recipe", entry.get("recipe_path"))
        if recipe_name:
            recipe_path = Path(recipe_name)
            if not recipe_path.is_absolute():
                recipe_path = (catalog_path.parent / recipe_path).resolve()
            if not recipe_path.is_file():
                errors.append({"id": asset_id, "error": "missing assembly recipe", "path": str(recipe_path)})
            else:
                recipe = json.loads(recipe_path.read_text(encoding="utf-8"))
                recipe_keys = [str(piece.get("instance_key")) for piece in recipe.get("pieces", []) if piece.get("instance_key")]
                missing_keys = sorted(set(recipe_keys) - set(exported_instance_keys))
                extra_keys = sorted(set(exported_instance_keys) - set(recipe_keys))
                if len(exported_instance_keys) != len(recipe_keys) or missing_keys or extra_keys:
                    errors.append({"id": asset_id, "error": "assembly instance_key set mismatch", "recipe_count": len(recipe_keys), "exported_count": len(exported_instance_keys), "missing": missing_keys, "extra": extra_keys})
                if not actual["instance_key_unique"]:
                    errors.append({"id": asset_id, "error": "duplicate exported instance_key"})
        expected_images = entry.get("embedded_images", entry.get("image_count"))
        if expected_images is not None and actual["image_count"] < int(expected_images):
            errors.append({"id": asset_id, "error": "embedded image count mismatch", "expected": int(expected_images), "actual": actual["image_count"]})
        expected_stats = entry.get("stats", {})
        for field in ("mesh_objects", "triangles", "uv_mesh_objects"):
            if field in expected_stats and actual[field] != expected_stats[field]:
                errors.append({"id": asset_id, "error": "round-trip count mismatch", "field": field, "expected": expected_stats[field], "actual": actual[field]})
        assets.append(record)

    # Materials/images are checked per imported file above; this catches an
    # assembly catalog that has no usable material claims at all.
    if catalog.get("materials") is not None and not catalog["materials"]:
        warnings.append({"warning": "catalog materials list is empty"})
    report = {
        "status": "passed" if not errors else "failed",
        "catalog": str(catalog_path),
        "module_count": len(modules),
        "assembly_count": len(assemblies),
        "assets": assets,
        "errors": errors,
        "warnings": warnings,
    }
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print("ORGANIC_MASTER_VALIDATION " + json.dumps({k: report[k] for k in ("status", "module_count", "assembly_count", "errors", "warnings")}, ensure_ascii=False), flush=True)
    return report


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--catalog", type=Path, default=DEFAULT_CATALOG)
    parser.add_argument("--report", type=Path, default=DEFAULT_REPORT)
    # Blender leaves its own flags in sys.argv.  When invoked with
    # ``blender -b --python this.py -- --catalog ...``, only consume the
    # arguments after the separator; direct Python invocation still works.
    argv = sys.argv
    if "--" in argv:
        argv = argv[argv.index("--") + 1:]
    else:
        argv = argv[1:]
    args = parser.parse_args(argv)
    report = validate_catalog(args.catalog.resolve(), args.report.resolve())
    raise SystemExit(0 if report["status"] == "passed" else 1)


if __name__ == "__main__":
    main()
