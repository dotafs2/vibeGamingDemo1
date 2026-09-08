"""Build a clean Blender catalogue board for the authored MedievalLife art.

Run from Blender 5.2 in background mode::

    blender -b --python create_library_overview.py -- --samples 32

The source blends are only checked for presence.  The board imports the
declared module GLBs, which avoids bringing preview floors, cameras, lights or
other studio objects from those source files into the overview.  Each tile is
one complete module (all of its semantic layers), uniformly fitted into a
display frame.  No source blend or source mesh is edited.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector, Euler


ART = Path(__file__).resolve().parent
CATALOG = ART / "catalog.json"
SOURCE_BLEND = ART / "MedievalLife_Masters.blend"
FREIGHT_DIR = ART / "FreightKit"
FREIGHT_MANIFEST = FREIGHT_DIR / "manifest.json"
MARKET_DIR = ART / "MarketLifeKit"
MARKET_MANIFEST = MARKET_DIR / "manifest.json"
OUT = ART / "LibraryOverview"
OVERVIEW_BLEND = OUT / "MedievalLife_LibraryOverview.blend"
OVERVIEW_PNG = OUT / "MedievalLife_LibraryOverview.png"
OVERVIEW_REPORT = OUT / "MedievalLife_LibraryOverview.json"

STATIC_COUNT = 21
FREIGHT_IDS = {"cargo_planks", "cargo_beams", "cart_traces", "freight_depot_rack"}
MARKET_IDS = {"bench_low", "work_table", "tool_rack", "linen_canopy", "clay_jar"}
GRID_COLUMNS = 5
GRID_ROWS = 6
CELL_WIDTH = 5.25
CELL_DEPTH = 4.55
DISPLAY_HEIGHT = 3.25


def _args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--samples", type=int, default=32)
    parser.add_argument("--resolution", type=int, default=2000)
    values = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    return parser.parse_args(values)


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _load_sources() -> tuple[dict, dict, dict, list[dict]]:
    if not SOURCE_BLEND.is_file():
        raise RuntimeError("Missing MedievalLife source blend: " + str(SOURCE_BLEND))
    if not CATALOG.is_file() or not FREIGHT_MANIFEST.is_file() or not MARKET_MANIFEST.is_file():
        raise RuntimeError("MedievalLife catalog, FreightKit manifest, or MarketLifeKit manifest is missing")
    catalog = json.loads(CATALOG.read_text(encoding="utf-8"))
    freight = json.loads(FREIGHT_MANIFEST.read_text(encoding="utf-8"))
    market = json.loads(MARKET_MANIFEST.read_text(encoding="utf-8"))
    modules = catalog.get("modules")
    if catalog.get("schema_version") != 1 or not isinstance(modules, list):
        raise RuntimeError("MedievalLife catalog must have schema_version=1 and modules[]")
    if len(modules) != STATIC_COUNT:
        raise RuntimeError(f"Expected {STATIC_COUNT} MedievalLife modules, found {len(modules)}")
    ids = [str(row.get("id", "")) for row in modules]
    if len(set(ids)) != STATIC_COUNT or any(not item for item in ids):
        raise RuntimeError("MedievalLife module IDs must be unique and non-empty")
    board = []
    for row in modules:
        source = ART / str(row.get("path", ""))
        if source.suffix.lower() != ".glb" or not source.is_file():
            raise RuntimeError("Missing MedievalLife module GLB: " + str(source))
        layers = row.get("layers")
        if not isinstance(layers, list) or not layers:
            raise RuntimeError("Missing MedievalLife layers for " + row["id"])
        board.append({
            "id": row["id"],
            "source": source,
            "layers": [str(layer) for layer in layers],
            "kind": "MedievalLife",
        })
    if freight.get("schema_version") != 1 or not isinstance(freight.get("modules"), list):
        raise RuntimeError("FreightKit manifest must have schema_version=1 and modules[]")
    freight_rows = freight["modules"]
    if {str(row.get("id", "")) for row in freight_rows} != FREIGHT_IDS:
        raise RuntimeError("FreightKit module IDs mismatch")
    if len(freight_rows) != len(FREIGHT_IDS):
        raise RuntimeError("FreightKit module IDs are duplicated")
    for row in freight_rows:
        source = FREIGHT_DIR / str(row.get("glb", ""))
        if source.suffix.lower() != ".glb" or not source.is_file():
            raise RuntimeError("Missing FreightKit module GLB: " + str(source))
        layers = row.get("layers")
        if not isinstance(layers, list) or not layers:
            raise RuntimeError("Missing FreightKit layers for " + row["id"])
        board.append({
            "id": row["id"],
            "source": source,
            "layers": [str(layer.get("layer", "")) for layer in layers],
            "kind": "FreightKit",
        })
    if market.get("schema_version") != 1 or not isinstance(market.get("modules"), list):
        raise RuntimeError("MarketLifeKit manifest must have schema_version=1 and modules[]")
    market_rows = market["modules"]
    if {str(row.get("id", "")) for row in market_rows} != MARKET_IDS:
        raise RuntimeError("MarketLifeKit module IDs mismatch")
    if len(market_rows) != len(MARKET_IDS):
        raise RuntimeError("MarketLifeKit module IDs are duplicated")
    for row in market_rows:
        source = MARKET_DIR / str(row.get("glb", ""))
        if source.suffix.lower() != ".glb" or not source.is_file():
            raise RuntimeError("Missing MarketLifeKit module GLB: " + str(source))
        layers = row.get("layers")
        if not isinstance(layers, list) or not layers:
            raise RuntimeError("Missing MarketLifeKit layers for " + row["id"])
        board.append({
            "id": row["id"],
            "source": source,
            "layers": [str(layer.get("layer", "")) for layer in layers],
            "kind": "MarketLifeKit",
        })
    if len(board) != GRID_COLUMNS * GRID_ROWS:
        raise RuntimeError("Overview board requires exactly 30 declared modules")
    return catalog, freight, market, board


def _collection(name: str) -> bpy.types.Collection:
    collection = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(collection)
    return collection


def _activate_collection(collection: bpy.types.Collection) -> None:
    def find(layer_collection: bpy.types.LayerCollection) -> bpy.types.LayerCollection | None:
        if layer_collection.collection == collection:
            return layer_collection
        for child in layer_collection.children:
            found = find(child)
            if found:
                return found
        return None
    layer = find(bpy.context.view_layer.layer_collection)
    if layer is None:
        raise RuntimeError("Cannot activate overview collection: " + collection.name)
    bpy.context.view_layer.active_layer_collection = layer


def _import_glb(path: Path, collection: bpy.types.Collection) -> list[bpy.types.Object]:
    before = set(bpy.data.objects)
    _activate_collection(collection)
    if not bpy.ops.import_scene.gltf(filepath=str(path)):
        raise RuntimeError("Blender rejected overview GLB: " + str(path))
    imported = [obj for obj in bpy.data.objects if obj not in before]
    if not any(obj.type == "MESH" for obj in imported):
        raise RuntimeError("Overview GLB contains no mesh objects: " + str(path))
    for obj in imported:
        for old_collection in list(obj.users_collection):
            old_collection.objects.unlink(obj)
        collection.objects.link(obj)
    return imported


def _world_bounds(objects: list[bpy.types.Object]) -> tuple[Vector, Vector]:
    points = []
    for obj in objects:
        if obj.type != "MESH":
            continue
        points.extend(obj.matrix_world @ Vector(corner) for corner in obj.bound_box)
    if not points:
        raise RuntimeError("Overview item has no mesh bounds")
    return Vector((min(p.x for p in points), min(p.y for p in points), min(p.z for p in points))), Vector(
        (max(p.x for p in points), max(p.y for p in points), max(p.z for p in points)))


def _group_imported(imported: list[bpy.types.Object], collection: bpy.types.Collection,
                    module_id: str) -> tuple[bpy.types.Object, list[bpy.types.Object]]:
    group = bpy.data.objects.new("Overview__" + module_id, None)
    collection.objects.link(group)
    imported_set = set(imported)
    roots = [obj for obj in imported if obj.parent not in imported_set]
    for obj in roots:
        matrix = obj.matrix_world.copy()
        obj.parent = group
        obj.matrix_world = matrix
    return group, imported


def _material(name: str, colour: tuple[float, float, float, float], roughness: float = .8) -> bpy.types.Material:
    material = bpy.data.materials.new(name)
    material.diffuse_color = colour
    material.use_nodes = True
    principled = material.node_tree.nodes.get("Principled BSDF")
    if principled:
        principled.inputs["Base Color"].default_value = colour
        principled.inputs["Roughness"].default_value = roughness
    return material


def _cube(name: str, location: tuple[float, float, float], dimensions: tuple[float, float, float],
          material: bpy.types.Material, collection: bpy.types.Collection) -> bpy.types.Object:
    bpy.ops.mesh.primitive_cube_add(location=location)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dimensions
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    for old in list(obj.users_collection):
        old.objects.unlink(obj)
    collection.objects.link(obj)
    obj.data.materials.append(material)
    bevel = obj.modifiers.new("soft_display_edges", "BEVEL")
    bevel.width = .035
    bevel.segments = 2
    return obj


def _text(body: str, location: tuple[float, float, float], size: float,
          material: bpy.types.Material, collection: bpy.types.Collection) -> bpy.types.Object:
    bpy.ops.object.text_add(location=location)
    obj = bpy.context.object
    obj.data.body = body
    obj.data.align_x = "CENTER"
    obj.data.align_y = "CENTER"
    obj.data.size = size
    obj.data.extrude = .006
    obj.data.bevel_depth = .001
    obj.data.materials.append(material)
    for old in list(obj.users_collection):
        old.objects.unlink(obj)
    collection.objects.link(obj)
    return obj


def _look_at(camera: bpy.types.Object, target: Vector) -> None:
    camera.rotation_euler = (target - camera.location).to_track_quat("-Z", "Y").to_euler()


def _build_scene(args: argparse.Namespace, catalog: dict, freight: dict, market: dict,
                 board: list[dict]) -> dict:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = args.resolution
    board_width = GRID_COLUMNS * CELL_WIDTH + 2.0
    board_height = GRID_ROWS * CELL_DEPTH + 4.3
    scene.render.resolution_y = int(args.resolution * board_height / board_width)
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(OVERVIEW_PNG)
    scene.render.film_transparent = False
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.image_settings.color_depth = "8"
    scene.render.resolution_percentage = 100
    scene.view_settings.look = "AgX - Medium High Contrast"

    display = _collection("LIBRARY_OVERVIEW__declared_modules")
    studio = _collection("LIBRARY_OVERVIEW__studio_only")
    floor_material = _material("Overview_Floor", (.085, .105, .12, 1.0), .92)
    slab_material = _material("Overview_Slab", (.24, .29, .32, 1.0), .84)
    label_material = _material("Overview_Label", (.80, .68, .40, 1.0), .58)
    title_material = _material("Overview_Title", (.92, .84, .64, 1.0), .5)
    _cube("Overview_Background", (0, 0, -.24),
          (GRID_COLUMNS * CELL_WIDTH + 2.0, GRID_ROWS * CELL_DEPTH + 3.0, .28),
          floor_material, studio)

    records = []
    for index, item in enumerate(board):
        column = index % GRID_COLUMNS
        row = index // GRID_COLUMNS
        cx = (column - (GRID_COLUMNS - 1) / 2.0) * CELL_WIDTH
        cy = (row - (GRID_ROWS - 1) / 2.0) * CELL_DEPTH
        imported = _import_glb(item["source"], display)
        group, mesh_objects = _group_imported(imported, display, item["id"])
        bpy.context.view_layer.update()
        lo, hi = _world_bounds(mesh_objects)
        source_lo, source_hi = lo.copy(), hi.copy()
        # Each asset is an independent catalogue vignette. Frame its actual
        # camera projection, so tall models cannot hide the next row or labels.
        rotation = Euler((math.radians(-55), 0, 0)).to_matrix() @ Euler((0, 0, math.radians(-25))).to_matrix()
        points = [rotation @ (obj.matrix_world @ Vector(corner))
                  for obj in mesh_objects if obj.type == "MESH" for corner in obj.bound_box]
        lo = Vector(tuple(min(p[k] for p in points) for k in range(3)))
        hi = Vector(tuple(max(p[k] for p in points) for k in range(3)))
        size = hi - lo
        if min(size.x, size.y, size.z) <= 0:
            raise RuntimeError("Degenerate overview bounds for " + item["id"])
        scale = min((CELL_WIDTH * .75) / size.x, (CELL_DEPTH * .64) / size.y)
        group.rotation_mode = 'QUATERNION'
        group.rotation_quaternion = rotation.to_quaternion()
        group.scale = (scale, scale, scale)
        group.location = (cx - ((lo.x + hi.x) * .5) * scale,
                          cy + .20 - ((lo.y + hi.y) * .5) * scale,
                          .10 - lo.z * scale)
        group["catalog_id"] = item["id"]
        group["source_glb"] = item["source"].relative_to(ART).as_posix()
        group["semantic_layers"] = ",".join(item["layers"])
        _cube("Slab__" + item["id"], (cx, cy, .02),
              (CELL_WIDTH - .16, CELL_DEPTH - .16, .10), slab_material, studio)
        label = _text(item["id"], (cx, cy - CELL_DEPTH * .40, .115), .19, label_material, studio)
        records.append({
            "index": index,
            "id": item["id"],
            "kind": item["kind"],
            "source_glb": item["source"].relative_to(ART).as_posix(),
            "layers": item["layers"],
            "grid": {"column": column, "row": row},
            "display_scale": scale,
            "source_bounds_m": {"min": list(source_lo), "max": list(source_hi)},
            "presentation": "rotated vignette fitted to projected bounds; not game placement",
            "label_object": label.name,
        })

    bpy.ops.object.camera_add(location=(0, .6, 40.0))
    camera = bpy.context.object
    camera.name = "Overview_Camera"
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = max(board_width, board_height)
    _look_at(camera, Vector((0, .6, 0)))
    scene.camera = camera
    for obj in [camera]:
        for old in list(obj.users_collection):
            old.objects.unlink(obj)
        studio.objects.link(obj)
    title = _text("MEDIEVAL LIFE  •  LIBRARY OVERVIEW", (0, GRID_ROWS * CELL_DEPTH / 2 + 1.05, .08), .42,
                  title_material, studio)
    subtitle = _text("30 declared modules  |  21 MedievalLife + 4 FreightKit + 5 MarketLifeKit  |  complete layers",
                     (0, GRID_ROWS * CELL_DEPTH / 2 + .46, .08), .16, label_material, studio)
    # Text faces local +Z; the camera/light -Z aiming helper would turn
    # these headings away from the viewer. Keep the whole board plane legible.

    world = bpy.data.worlds.new("Overview_World")
    scene.world = world
    world.use_nodes = True
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (.018, .022, .021, 1.0)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = .45
    bpy.ops.object.light_add(type="AREA", location=(-9, -11, 21))
    key = bpy.context.object
    key.name = "Overview_Key"
    key.data.energy = 9000
    key.data.shape = "DISK"
    key.data.size = 9.0
    _look_at(key, Vector((0, 0, 0)))
    bpy.ops.object.light_add(type="AREA", location=(11, 5, 14))
    fill = bpy.context.object
    fill.name = "Overview_Fill"
    fill.data.energy = 6000
    fill.data.size = 8.0
    _look_at(fill, Vector((0, 0, .5)))
    bpy.ops.object.light_add(type="AREA", location=(0, 15, 19))
    top_fill = bpy.context.object
    top_fill.name = "Overview_UpperRowsFill"
    top_fill.data.energy = 2600
    top_fill.data.size = 10.0
    _look_at(top_fill, Vector((0, 9, .5)))
    for light in (key, fill, top_fill):
        for old in list(light.users_collection):
            old.objects.unlink(light)
        studio.objects.link(light)

    scene["overview_source_blend"] = SOURCE_BLEND.relative_to(ART).as_posix()
    scene["overview_source_catalog"] = CATALOG.relative_to(ART).as_posix()
    scene["overview_freight_manifest"] = FREIGHT_MANIFEST.relative_to(ART).as_posix()
    scene["overview_market_manifest"] = MARKET_MANIFEST.relative_to(ART).as_posix()
    scene["overview_module_count"] = len(records)
    return {
        "schema_version": 1,
        "status": "rendered_by_this_script",
        "source_blend": SOURCE_BLEND.relative_to(ART).as_posix(),
        "source_catalog": CATALOG.relative_to(ART).as_posix(),
        "source_catalog_sha256": _sha256(CATALOG),
        "freight_manifest": FREIGHT_MANIFEST.relative_to(ART).as_posix(),
        "freight_manifest_sha256": _sha256(FREIGHT_MANIFEST),
        "market_manifest": MARKET_MANIFEST.relative_to(ART).as_posix(),
        "market_manifest_sha256": _sha256(MARKET_MANIFEST),
        "grid": {"columns": GRID_COLUMNS, "rows": GRID_ROWS, "tile_count": len(records)},
        "groups": {"MedievalLife": STATIC_COUNT, "FreightKit": len(FREIGHT_IDS),
                   "MarketLifeKit": len(MARKET_IDS)},
        "display_policy": "complete declared layer composition per module; each tile uniformly fitted",
        "axis": {"up": "+Z", "front": "-Y", "units": "meters"},
        "modules": records,
        "outputs": {
            "blend": OVERVIEW_BLEND.relative_to(ART).as_posix(),
            "png": OVERVIEW_PNG.relative_to(ART).as_posix(),
        },
    }


def main() -> None:
    args = _args()
    OUT.mkdir(parents=True, exist_ok=True)
    catalog, freight, market, board = _load_sources()
    report = _build_scene(args, catalog, freight, market, board)
    bpy.ops.wm.save_as_mainfile(filepath=str(OVERVIEW_BLEND))
    bpy.context.scene.render.filepath = str(OVERVIEW_PNG)
    bpy.ops.render.render(write_still=True)
    OVERVIEW_REPORT.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n",
                               encoding="utf-8")


if __name__ == "__main__":
    main()
