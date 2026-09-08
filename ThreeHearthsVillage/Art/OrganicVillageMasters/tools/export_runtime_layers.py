"""Export OrganicVillageMasters as per-module, per-layer runtime GLBs.

Run from the source blend with Blender 5.2.1:

    blender --background OrganicVillage_Masters.blend --python \
      tools/export_runtime_layers.py -- --output RuntimeLayers

The source module roots remain untouched.  Each output contains one layer
mesh, an identity root carrying module/layer/palette metadata, and the
original source mesh coordinates.  No assemblies are exported or merged.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
import time
from pathlib import Path
from typing import Any

import bpy
from mathutils import Vector


ART = Path(__file__).resolve().parents[1]
CATALOG_PATH = ART / "catalog.json"
DEFAULT_OUTPUT = ART / "RuntimeLayers"
LAYERS = {"structure", "finish", "weathering", "attachments"}


def parse_args() -> argparse.Namespace:
    raw = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT,
                        help="runtime layer output directory")
    return parser.parse_args(raw)


def load_catalog() -> dict[str, Any]:
    return json.loads(CATALOG_PATH.read_text(encoding="utf-8-sig"))


def semantic_slot(material: bpy.types.Material) -> str | None:
    value = material.get("semantic_slot")
    if isinstance(value, str) and value:
        return value
    if "|" in material.name:
        return material.name.rsplit("|", 1)[1].strip()
    return None


def source_bounds(obj: bpy.types.Object) -> dict[str, list[float]]:
    points = [Vector(corner) for corner in obj.bound_box]
    lo = [min(v[i] for v in points) for i in range(3)]
    hi = [max(v[i] for v in points) for i in range(3)]
    return {"min": lo, "max": hi, "size": [hi[i] - lo[i] for i in range(3)]}


def material_for_palette(source: bpy.types.Material, palette: str) -> bpy.types.Material:
    slot = semantic_slot(source)
    if slot:
        candidate = bpy.data.materials.get(f"{palette} | {slot}")
        if candidate:
            return candidate
    return source


def direct_layer_meshes(root: bpy.types.Object) -> dict[str, list[bpy.types.Object]]:
    result: dict[str, list[bpy.types.Object]] = {layer: [] for layer in LAYERS}
    for obj in root.children_recursive:
        if obj.type != "MESH":
            continue
        layer = obj.get("layer")
        if isinstance(layer, str) and layer in result:
            result[layer].append(obj)
    return result


def select_only(objects: list[bpy.types.Object]) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    for obj in objects:
        obj.select_set(True)
    if objects:
        bpy.context.view_layer.objects.active = objects[0]


def export_layer(path: Path, module_id: str, layer: str, palette: str,
                 source_meshes: list[bpy.types.Object], temp_collection: bpy.types.Collection) -> dict[str, Any]:
    # A root empty makes the authored pivot explicit while keeping mesh data in
    # original local coordinates.  The source blend itself is never modified.
    root = bpy.data.objects.new(f"{module_id}__{layer}__{palette}", None)
    temp_collection.objects.link(root)
    root["module_id"] = module_id
    root["layer"] = layer
    root["palette"] = palette
    root["coordinate_system"] = "+Z up / -Y front"
    root["source_pivot_m"] = [0.0, 0.0, 0.0]
    clones: list[bpy.types.Object] = []
    points: list[Vector] = []
    triangles = 0
    material_names: set[str] = set()
    source_object_names: list[str] = []
    for index, source in enumerate(source_meshes):
        clone = source.copy()
        clone.data = source.data.copy()
        clone.name = f"{module_id}__{layer}__mesh_{index:02d}"
        temp_collection.objects.link(clone)
        # Source module roots are identity transforms, but retaining the world
        # matrix here makes the exporter robust to authored local offsets.
        clone.matrix_world = source.matrix_world.copy()
        clone.parent = root
        clone.matrix_parent_inverse = root.matrix_world.inverted()
        clone["module_id"] = module_id
        clone["layer"] = layer
        clone["palette"] = palette
        for slot_index, source_mat in enumerate(list(clone.data.materials)):
            target = material_for_palette(source_mat, palette)
            clone.data.materials[slot_index] = target
            material_names.add(target.name)
        clone.data.calc_loop_triangles()
        triangles += len(clone.data.loop_triangles)
        points.extend([clone.matrix_world @ Vector(v.co) for v in clone.data.vertices])
        source_object_names.append(source.name)
        clones.append(clone)
    if not points:
        fail(f"module {module_id} layer {layer} has no mesh vertices")
    lo = [min(p[i] for p in points) for i in range(3)]
    hi = [max(p[i] for p in points) for i in range(3)]
    bounds = {"min": lo, "max": hi, "size": [hi[i] - lo[i] for i in range(3)]}
    if any(not math.isfinite(v) for v in lo + hi) or any(v <= 1e-8 for v in bounds["size"]):
        fail(f"degenerate or non-finite bounds for {module_id}/{layer}")
    root["bounds_authoring_m"] = bounds
    root["triangle_count"] = triangles
    root["material_names"] = ",".join(sorted(material_names))
    root["source_mesh_objects"] = ",".join(source_object_names)
    path.parent.mkdir(parents=True, exist_ok=True)
    select_only([root] + clones)
    bpy.ops.export_scene.gltf(
        filepath=str(path), export_format="GLB", use_selection=True,
        export_yup=True, export_apply=True, export_texcoords=True,
        export_normals=True, export_materials="EXPORT", export_extras=True,
        export_animations=False, export_cameras=False, export_lights=False)
    record = {
        "module_id": module_id,
        "layer": layer,
        "palette": palette,
        "asset_glb": path.relative_to(ART).as_posix(),
        "source_mesh_objects": source_object_names,
        "pivot_authoring_m": [0.0, 0.0, 0.0],
        "front_axis": "-Y",
        "up_axis": "+Z",
        "bounds_authoring_m": bounds,
        "triangle_count": triangles,
        "material_names": sorted(material_names),
        "bytes": path.stat().st_size,
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
    }
    for obj in clones + [root]:
        bpy.data.objects.remove(obj, do_unlink=True)
    return record


def fail(message: str) -> None:
    raise RuntimeError(message)


def main() -> int:
    args = parse_args()
    catalog = load_catalog()
    palettes = catalog.get("palettes", {})
    if not isinstance(palettes, dict) or set(palettes) != {"warm_lime", "ochre_slate", "sage_clay"}:
        fail("catalog palettes must contain warm_lime, ochre_slate, and sage_clay")
    output = (args.output if args.output.is_absolute() else ART / args.output).resolve()
    if output == ART or output == ART / "Modules" or output == ART / "Assemblies":
        fail(f"refusing unsafe output directory: {output}")
    output.mkdir(parents=True, exist_ok=True)
    temp_collection = bpy.data.collections.new("RUNTIME_LAYER_EXPORT_TEMP")
    bpy.context.scene.collection.children.link(temp_collection)
    started = time.monotonic()
    assets: list[dict[str, Any]] = []
    module_ids: list[str] = []
    for spec in catalog.get("modules", []):
        module_id = str(spec["id"])
        root = bpy.data.objects.get(module_id)
        if root is None or root.get("module_id") != module_id:
            fail(f"source module root missing or mismatched: {module_id}")
        layers = direct_layer_meshes(root)
        expected = set(spec.get("layers", []))
        actual = {layer for layer, meshes in layers.items() if meshes}
        if actual != expected:
            fail(f"layer contract mismatch for {module_id}: catalog={sorted(expected)} source={sorted(actual)}")
        module_ids.append(module_id)
        for palette in palettes:
            for layer in sorted(actual):
                path = output / palette / f"{module_id}__{layer}.glb"
                assets.append(export_layer(path, module_id, layer, palette, layers[layer], temp_collection))
                print(f"RUNTIME_LAYER_EXPORTED {palette}/{module_id}__{layer}.glb", flush=True)
    bpy.data.collections.remove(temp_collection)
    manifest = {
        "schema_version": 1,
        "status": "exported",
        "source_blend": Path(bpy.data.filepath).resolve().relative_to(ART.parent).as_posix(),
        "source_blend_sha256": hashlib.sha256(Path(bpy.data.filepath).read_bytes()).hexdigest(),
        "source_hierarchy": "+Z up / -Y front; one module root at identity pivot",
        "module_count": len(module_ids),
        "layer_export_count": len(assets),
        "palette_count": len(palettes),
        "palettes": sorted(palettes),
        "modules": module_ids,
        "assets": assets,
        "elapsed_seconds": time.monotonic() - started,
    }
    (output / "export_manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("RUNTIME_LAYER_EXPORT_COMPLETE " + json.dumps({"modules": len(module_ids), "assets": len(assets), "output": str(output)}, sort_keys=True), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
