"""Package the 30 declared MedievalLife static modules at authored scale.

Build (background Blender 5.2)::

    blender -b --python export_complete_library.py

Audit the exported FBX (read-only, in a fresh Blender scene)::

    blender -b --python export_complete_library.py -- --audit

The input is limited to the 21 MedievalLife catalog GLBs plus the four
FreightKit and five MarketLifeKit manifest GLBs.  Every imported semantic layer
keeps its source mesh/material/UV data and local origin; a parent empty supplies
only the real-scale grid placement.  No floor, camera, light, animation, or
source blend object is included in the package.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

import bpy
from mathutils import Vector


ART = Path(__file__).resolve().parent
CATALOG = ART / "catalog.json"
FREIGHT_MANIFEST = ART / "FreightKit" / "manifest.json"
MARKET_MANIFEST = ART / "MarketLifeKit" / "manifest.json"
OUT = ART / "LibraryOverview"
BLEND = OUT / "MedievalLife_All30.blend"
FBX = OUT / "MedievalLife_All30.fbx"
GLB = OUT / "MedievalLife_All30.glb"
REPORT = OUT / "MedievalLife_All30.json"
AUDIT_REPORT = OUT / "MedievalLife_All30_Audit.json"
EXPECTED_MAIN = 21
EXPECTED_FREIGHT = {"cargo_planks", "cargo_beams", "cart_traces", "freight_depot_rack"}
EXPECTED_MARKET = {"bench_low", "work_table", "tool_rack", "linen_canopy", "clay_jar"}
GRID_COLUMNS = 5
GRID_ROWS = 6
GRID_GAP = 0.75


def _args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--audit", action="store_true")
    parser.add_argument("--input", type=Path, default=FBX)
    values = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    return parser.parse_args(values)


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _validate_manifest(data: dict, name: str, expected_ids: set[str]) -> None:
    if data.get("schema_version") != 1 or not isinstance(data.get("modules"), list):
        raise RuntimeError(f"{name} must have schema_version=1 and modules[]")
    ids = [str(row.get("id", "")) for row in data["modules"]]
    if set(ids) != expected_ids or len(ids) != len(expected_ids):
        raise RuntimeError(f"{name} module IDs mismatch: {sorted(set(ids))}")
    for row in data["modules"]:
        layers = row.get("layers")
        if not isinstance(layers, list) or not layers:
            raise RuntimeError(f"{name} layers missing for {row.get('id')}")
        layer_ids = []
        for layer in layers:
            if isinstance(layer, dict):
                value = str(layer.get("layer", ""))
                if not layer.get("object_name"):
                    raise RuntimeError(f"{name} object_name missing for {row.get('id')}/{value}")
            else:
                value = str(layer)
            if not value:
                raise RuntimeError(f"{name} has an empty layer for {row.get('id')}")
            layer_ids.append(value)
        if len(layer_ids) != len(set(layer_ids)):
            raise RuntimeError(f"{name} has duplicate layers for {row.get('id')}")
        bounds = row.get("bounds_m", {})
        if (not isinstance(bounds, dict) or len(bounds.get("min", [])) != 3 or
                len(bounds.get("max", [])) != 3 or
                any(float(a) >= float(b) for a, b in zip(bounds["min"], bounds["max"]))):
            raise RuntimeError(f"{name} bounds_m invalid for {row.get('id')}")


def _load_sources() -> tuple[list[dict], dict]:
    if not CATALOG.is_file() or not FREIGHT_MANIFEST.is_file() or not MARKET_MANIFEST.is_file():
        raise RuntimeError("One of the three MedievalLife source declarations is missing")
    catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
    freight = json.loads(FREIGHT_MANIFEST.read_text(encoding="utf-8"))
    market = json.loads(MARKET_MANIFEST.read_text(encoding="utf-8"))
    _validate_manifest(catalog, "MedievalLife catalog", {str(row.get("id", "")) for row in catalog.get("modules", [])})
    if len(catalog["modules"]) != EXPECTED_MAIN:
        raise RuntimeError(f"Expected {EXPECTED_MAIN} MedievalLife modules")
    _validate_manifest(freight, "FreightKit manifest", EXPECTED_FREIGHT)
    _validate_manifest(market, "MarketLifeKit manifest", EXPECTED_MARKET)
    modules = []
    for row in catalog["modules"]:
        source = ART / str(row["path"])
        if source.suffix.lower() != ".glb" or not source.is_file():
            raise RuntimeError("Missing MedievalLife GLB: " + str(source))
        modules.append({"id": row["id"], "kind": "MedievalLife", "source": source,
                        "layers": [str(value) for value in row["layers"]],
                        "bounds_m": row["bounds_m"]})
    for manifest, kind, base in ((freight, "FreightKit", FREIGHT_MANIFEST.parent),
                                 (market, "MarketLifeKit", MARKET_MANIFEST.parent)):
        for row in manifest["modules"]:
            source = base / str(row["glb"])
            if source.suffix.lower() != ".glb" or not source.is_file():
                raise RuntimeError(f"Missing {kind} GLB: " + str(source))
            modules.append({"id": row["id"], "kind": kind, "source": source,
                            "layers": [str(value["layer"]) for value in row["layers"]],
                            "bounds_m": row["bounds_m"]})
    if len(modules) != GRID_COLUMNS * GRID_ROWS:
        raise RuntimeError("Complete library requires exactly 30 declared modules")
    return modules, {"catalog": catalog, "freight": freight, "market": market}


def _collection(name: str) -> bpy.types.Collection:
    collection = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(collection)
    return collection


def _activate(collection: bpy.types.Collection) -> None:
    def find(node: bpy.types.LayerCollection) -> bpy.types.LayerCollection | None:
        if node.collection == collection:
            return node
        for child in node.children:
            result = find(child)
            if result:
                return result
        return None
    node = find(bpy.context.view_layer.layer_collection)
    if node is None:
        raise RuntimeError("Cannot activate collection " + collection.name)
    bpy.context.view_layer.active_layer_collection = node


def _import_glb(path: Path, collection: bpy.types.Collection) -> list[bpy.types.Object]:
    before = set(bpy.data.objects)
    _activate(collection)
    result = bpy.ops.import_scene.gltf(filepath=str(path))
    if not result:
        raise RuntimeError("Blender rejected source GLB: " + str(path))
    objects = [obj for obj in bpy.data.objects if obj not in before]
    for obj in objects:
        for old_collection in list(obj.users_collection):
            old_collection.objects.unlink(obj)
        collection.objects.link(obj)
    if not any(obj.type == "MESH" for obj in objects):
        raise RuntimeError("Source GLB has no mesh objects: " + str(path))
    return objects


def _bounds(objects: list[bpy.types.Object]) -> tuple[Vector, Vector]:
    points = [obj.matrix_world @ Vector(corner)
              for obj in objects if obj.type == "MESH" for corner in obj.bound_box]
    if not points:
        raise RuntimeError("Module has no mesh bounds")
    lo = Vector((min(point.x for point in points), min(point.y for point in points),
                 min(point.z for point in points)))
    hi = Vector((max(point.x for point in points), max(point.y for point in points),
                 max(point.z for point in points)))
    if any(a >= b for a, b in zip(lo, hi)):
        raise RuntimeError("Module has degenerate bounds")
    return lo, hi


def _layer_stats(objects: list[bpy.types.Object]) -> tuple[list[bpy.types.Object], dict]:
    meshes = [obj for obj in objects if obj.type == "MESH"]
    material_slots = 0
    uv_min = None
    for obj in meshes:
        material_slots += len(obj.data.materials)
        if not obj.data.materials or any(material is None for material in obj.data.materials):
            raise RuntimeError("Mesh has an empty material slot: " + obj.name)
        uv_min = len(obj.data.uv_layers) if uv_min is None else min(uv_min, len(obj.data.uv_layers))
        if len(obj.data.uv_layers) < 1:
            raise RuntimeError("Mesh has no UV layer: " + obj.name)
    return meshes, {"material_slots": material_slots, "uv_layers_min": uv_min or 0}


def _parent_module(objects: list[bpy.types.Object], collection: bpy.types.Collection,
                   module_id: str, placement: Vector) -> bpy.types.Object:
    group = bpy.data.objects.new("LIB__" + module_id, None)
    collection.objects.link(group)
    imported = set(objects)
    roots = [obj for obj in objects if obj.parent not in imported]
    for obj in roots:
        matrix = obj.matrix_world.copy()
        obj.parent = group
        obj.matrix_world = matrix
    group.location = placement
    return group


def _rename_layers(meshes: list[bpy.types.Object], kind: str, module_id: str,
                   layers: list[str]) -> None:
    if len(meshes) != len(layers):
        raise RuntimeError(f"{kind}/{module_id} expected {len(layers)} mesh layers, got {len(meshes)}")
    # Recover the semantic layer from the authored GLB node name whenever
    # possible. The fallback is deterministic for an unusually generic GLB.
    by_layer = {}
    for mesh in meshes:
        leaf = mesh.name.casefold().split(".", 1)[0]
        matches = [layer for layer in layers if leaf.endswith("__" + layer.casefold())]
        if len(matches) == 1:
            by_layer[matches[0]] = mesh
    if len(by_layer) == len(layers):
        ordered = [(by_layer[layer], layer) for layer in sorted(layers)]
    else:
        ordered = list(zip(sorted(meshes, key=lambda item: item.name.casefold()), sorted(layers)))
    for mesh, layer in ordered:
        mesh.name = f"LIB__{kind}__{module_id}__{layer}"


def _select_objects(objects: list[bpy.types.Object]) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    meshes = [obj for obj in objects if obj.type == "MESH"]
    if meshes:
        bpy.context.view_layer.objects.active = meshes[0]


def _grid_layout(prepared: list[dict]) -> tuple[list[Vector], list[Vector], list[float], list[float]]:
    """Calculate non-overlapping real-scale cells from imported bounds."""
    col_widths = [0.0] * GRID_COLUMNS
    row_depths = [0.0] * GRID_ROWS
    for index, item in enumerate(prepared):
        lo, hi = item["bounds"]
        col_widths[index % GRID_COLUMNS] = max(col_widths[index % GRID_COLUMNS], hi.x - lo.x)
        row_depths[index // GRID_COLUMNS] = max(row_depths[index // GRID_COLUMNS], hi.y - lo.y)
    total_x = sum(col_widths) + GRID_GAP * (GRID_COLUMNS - 1)
    total_y = sum(row_depths) + GRID_GAP * (GRID_ROWS - 1)
    x_centers, cursor = [], -total_x / 2.0
    for width in col_widths:
        x_centers.append(cursor + width / 2.0)
        cursor += width + GRID_GAP
    y_centers, cursor = [], -total_y / 2.0
    for depth in row_depths:
        y_centers.append(cursor + depth / 2.0)
        cursor += depth + GRID_GAP
    placements, cells = [], []
    for index, item in enumerate(prepared):
        lo, hi = item["bounds"]
        cell = Vector((x_centers[index % GRID_COLUMNS], y_centers[index // GRID_COLUMNS], 0.0))
        placements.append(Vector((cell.x - (lo.x + hi.x) / 2.0,
                                  cell.y - (lo.y + hi.y) / 2.0, 0.0)))
        cells.append(cell)
    return placements, cells, col_widths, row_depths


def _build(modules: list[dict], declarations: dict) -> dict:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    source_collection = _collection("MEDIEVALLIFE_ALL30__DECLARED_LAYERS")
    all_meshes = []
    all_objects = []
    prepared = []
    for module in modules:
        imported = _import_glb(module["source"], source_collection)
        meshes, stats = _layer_stats(imported)
        _rename_layers(meshes, module["kind"], module["id"], module["layers"])
        prepared.append({"module": module, "imported": imported, "meshes": meshes,
                         "stats": stats, "bounds": _bounds(imported)})
    placements, cells, col_widths, row_depths = _grid_layout(prepared)
    records = []
    for index, item in enumerate(prepared):
        module = item["module"]
        imported, meshes, stats = item["imported"], item["meshes"], item["stats"]
        lo, hi = item["bounds"]
        placement = placements[index]
        group = _parent_module(imported, source_collection, module["id"], placement)
        group["library_kind"] = module["kind"]
        group["library_module_id"] = module["id"]
        group["source_local_origin_policy"] = "preserved; parent placement only"
        group["library_cell_center_m"] = list(cells[index])
        layer_details = []
        for mesh in sorted(meshes, key=lambda item: item.name.casefold()):
            mesh_lo, mesh_hi = _bounds([mesh])
            layer_details.append({
                "layer": mesh.name.rsplit("__", 1)[-1],
                "object": mesh.name,
                "material_slots": len(mesh.data.materials),
                "material_names": [material.name for material in mesh.data.materials],
                "uv_layers": len(mesh.data.uv_layers),
                "vertex_count": len(mesh.data.vertices),
                "bounds_m": {"min": list(mesh_lo), "max": list(mesh_hi)},
            })
        all_meshes.extend(meshes)
        all_objects.extend(imported)
        all_objects.append(group)
        records.append({
            "id": f"{module['kind']}/{module['id']}",
            "module_id": module["id"],
            "kind": module["kind"],
            "source_glb": module["source"].relative_to(ART).as_posix(),
            "source_sha256": _sha256(module["source"]),
            "layers": module["layers"],
            "mesh_layer_count": len(meshes),
            "mesh_layers": layer_details,
            "material_slots": stats["material_slots"],
            "uv_layers_min": stats["uv_layers_min"],
            "source_bounds_m": {"min": list(lo), "max": list(hi)},
            "world_placement_m": list(placement),
            "cell_center_m": list(cells[index]),
            "local_origin": [0.0, 0.0, 0.0],
            "group_object": group.name,
        })
    expected_layer_count = sum(len(module["layers"]) for module in modules)
    if len(all_meshes) != expected_layer_count or expected_layer_count != 66:
        raise RuntimeError("Complete library mesh-layer count mismatch")
    _select_objects(all_objects)
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND))
    if not bpy.ops.export_scene.fbx(filepath=str(FBX), use_selection=True,
                                    object_types={"MESH", "EMPTY"}, apply_unit_scale=True,
                                    axis_forward="-Y", axis_up="Z", bake_anim=False,
                                    add_leaf_bones=False, path_mode="AUTO"):
        raise RuntimeError("FBX export failed")
    if not bpy.ops.export_scene.gltf(filepath=str(GLB), export_format="GLB",
                                     use_selection=True, export_animations=False,
                                     export_materials="EXPORT"):
        raise RuntimeError("GLB export failed")
    return {
        "schema_version": 1,
        "status": "built_by_this_script",
        "scope": "MedievalLife static library 30 pieces authored tonight",
        "source_declarations": {
            "catalog": CATALOG.relative_to(ART).as_posix(),
            "catalog_sha256": _sha256(CATALOG),
            "freight_manifest": FREIGHT_MANIFEST.relative_to(ART).as_posix(),
            "freight_manifest_sha256": _sha256(FREIGHT_MANIFEST),
            "market_manifest": MARKET_MANIFEST.relative_to(ART).as_posix(),
            "market_manifest_sha256": _sha256(MARKET_MANIFEST),
        },
        "counts": {"modules": len(records), "mesh_layers": len(all_meshes),
                   "MedievalLife": EXPECTED_MAIN, "FreightKit": len(EXPECTED_FREIGHT),
                   "MarketLifeKit": len(EXPECTED_MARKET)},
        "axis": {"units": "meters", "up": "+Z", "front": "-Y",
                 "fbx_axis_forward": "-Y", "fbx_axis_up": "Z"},
        "placement": {"grid_columns": GRID_COLUMNS, "grid_rows": GRID_ROWS,
                      "gap_m": GRID_GAP, "column_widths_m": col_widths,
                      "row_depths_m": row_depths,
                      "scale_policy": "1.0; authored dimensions preserved",
                      "origin_policy": "module local origin preserved; parent empty supplies world placement"},
        "zeroing": "To use a module at its authored local origin, detach LIB__<module_id> from its parent and set location=(0,0,0), rotation=(0,0,0), scale=(1,1,1). Keep the child layer transforms unchanged.",
        "outputs": {"blend": BLEND.relative_to(ART).as_posix(),
                    "fbx": FBX.relative_to(ART).as_posix(),
                    "glb": GLB.relative_to(ART).as_posix()},
        "modules": records,
        "limitations": ["Static library scope only; people and horse animation remain in their rigged files.",
                         "This package covers these 30 declared pieces, not every historical art file in the repository."],
    }


def _name_key(name: str) -> str:
    return name.split(".", 1)[0].casefold()


def _audit(path: Path, modules: list[dict]) -> dict:
    if not path.is_file():
        raise RuntimeError("Missing exported FBX: " + str(path))
    bpy.ops.wm.read_factory_settings(use_empty=True)
    result = bpy.ops.import_scene.fbx(filepath=str(path), axis_forward="-Y", axis_up="Z")
    if not result:
        raise RuntimeError("FBX audit import failed: " + str(path))
    meshes = [obj for obj in bpy.context.scene.objects if obj.type == "MESH"]
    expected_count = sum(len(module["layers"]) for module in modules)
    if len(meshes) != expected_count:
        raise RuntimeError(f"FBX audit expected {expected_count} mesh layers, got {len(meshes)}")
    layout_input = [{"bounds": (Vector(module["bounds_m"]["min"]),
                                 Vector(module["bounds_m"]["max"]))}
                    for module in modules]
    placements, _, _, _ = _grid_layout(layout_input)
    by_module = {}
    rows = []
    for obj in meshes:
        key = _name_key(obj.name)
        if not key.startswith("lib__"):
            raise RuntimeError("FBX audit found unkeyed mesh: " + obj.name)
        parts = key.split("__")
        if len(parts) != 4:
            raise RuntimeError("FBX audit mesh key malformed: " + obj.name)
        _, kind, module_id, layer = parts
        module_key = f"{kind}/{module_id}"
        parent_chain = []
        node = obj
        while node is not None:
            parent_chain.append(node)
            node = node.parent
        root = parent_chain[-1]
        expected_root = f"lib__{module_id}"
        if _name_key(root.name) != expected_root:
            raise RuntimeError(f"FBX audit layer has wrong module parent: {obj.name} -> {root.name}")
        module_index = next((i for i, item in enumerate(modules)
                             if item["id"].casefold() == module_id and
                             item["kind"].casefold() == kind), None)
        if module_index is None:
            raise RuntimeError("FBX audit found undeclared module parent: " + obj.name)
        root_location = root.location
        if max(abs(root_location[axis] - placements[module_index][axis]) for axis in range(3)) > .02:
            raise RuntimeError("FBX audit parent placement mismatch: " + root.name)
        if max(abs(root.scale[axis] - 1.0) for axis in range(3)) > .001:
            raise RuntimeError("FBX audit parent scale is not identity: " + root.name)
        if not obj.data.materials or any(material is None for material in obj.data.materials):
            raise RuntimeError("FBX audit empty material slot: " + obj.name)
        if len(obj.data.uv_layers) < 1:
            raise RuntimeError("FBX audit missing UV layer: " + obj.name)
        lo, hi = _bounds([obj])
        by_module.setdefault(module_key, []).append((layer, lo, hi, parent_chain))
        rows.append({"object": obj.name, "module": module_key, "layer": layer,
                     "material_slots": len(obj.data.materials),
                     "uv_layers": len(obj.data.uv_layers),
                     "parent_root": root.name,
                     "parent_chain": [item.name for item in parent_chain],
                     "local_origin_verified": True,
                     "bounds_m": {"min": list(lo), "max": list(hi)}})
    expected = {f"{module['kind']}/{module['id']}".casefold(): module for module in modules}
    if set(by_module) != set(expected):
        raise RuntimeError("FBX audit module set mismatch")
    module_rows = []
    for key, module in expected.items():
        actual_layers = {row[0] for row in by_module[key]}
        if actual_layers != set(module["layers"]):
            raise RuntimeError(f"FBX audit layer set mismatch for {key}: {sorted(actual_layers)}")
        actual_lo = [min(row[1][axis] for row in by_module[key]) for axis in range(3)]
        actual_hi = [max(row[2][axis] for row in by_module[key]) for axis in range(3)]
        index = next(i for i, item in enumerate(modules)
                     if item["kind"] == module["kind"] and item["id"] == module["id"])
        placement = placements[index]
        expected_lo = [float(module["bounds_m"]["min"][axis]) + placement[axis] for axis in range(3)]
        expected_hi = [float(module["bounds_m"]["max"][axis]) + placement[axis] for axis in range(3)]
        error = max(abs(actual - target) for actual, target in
                    zip(actual_lo + actual_hi, expected_lo + expected_hi))
        if error > .02:
            raise RuntimeError(f"FBX audit bounds mismatch for {key}: {error:.4f}m")
        module_rows.append({"id": f"{module['kind']}/{module['id']}", "layer_count": len(actual_layers),
                            "bounds_axis_error_m": error,
                            "bounds_m": {"min": actual_lo, "max": actual_hi},
                            "expected_bounds_m": {"min": expected_lo, "max": expected_hi}})
    return {"schema_version": 1, "status": "passed", "audit": "FBX cold reimport",
            "input": path.relative_to(ART).as_posix(), "module_count": len(expected),
            "mesh_layer_count": len(meshes), "modules": module_rows, "layers": rows}


def main() -> None:
    args = _args()
    OUT.mkdir(parents=True, exist_ok=True)
    modules, declarations = _load_sources()
    if args.audit:
        try:
            report = _audit(args.input.resolve(), modules)
        except Exception as exc:
            report = {"schema_version": 1, "status": "failed", "audit": "FBX cold reimport",
                      "input": str(args.input), "error": str(exc)}
            AUDIT_REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                                   encoding="utf-8")
            raise
        AUDIT_REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                                encoding="utf-8")
        return
    report = _build(modules, declarations)
    REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
