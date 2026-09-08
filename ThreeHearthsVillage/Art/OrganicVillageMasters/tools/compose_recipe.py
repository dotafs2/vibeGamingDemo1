"""Compose an OrganicVillageMasters recipe into a new GLB and/or Blender file.

Run with Blender 5.2+:
    blender --background --python compose_recipe.py -- \
      --recipe ../Recipes/family_cluster.json \
      --output ../Composed/family_cluster_sage.glb \
      --finish-palette sage_clay --disable-layer weathering

The composer is deliberately a small, deterministic assembly tool.  It only
imports module GLBs named by the recipe, applies the recipe transforms, keeps
source layer/anchor custom properties, restores each piece palette across all
layers, and optionally applies a second finish/weathering-only override.  It
never writes Modules/ or Assemblies/.
"""
from __future__ import annotations

import argparse
import json
import math
import os
import re
import sys
from pathlib import Path
from typing import Any

import bpy


HERE = Path(__file__).resolve().parent
KIT = HERE.parent
CATALOG_PATH = KIT / "catalog.json"
VALID_LAYERS = {"structure", "finish", "weathering", "attachments"}
DISABLEABLE_LAYERS = {"weathering", "attachments"}


class ComposeError(RuntimeError):
    """A user-facing recipe or composition error."""


def fail(message: str) -> None:
    raise ComposeError(message)


def load_json(path: Path, label: str) -> dict[str, Any]:
    if not path.is_file():
        fail(f"{label} does not exist: {path}")
    try:
        data = json.loads(path.read_text(encoding="utf-8-sig"))
    except Exception as exc:
        fail(f"{label} is not valid JSON ({path}): {exc}")
    if not isinstance(data, dict):
        fail(f"{label} must contain a JSON object: {path}")
    return data


def finite_number(value: Any, label: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        fail(f"{label} must be a finite number; got {value!r}")
    value = float(value)
    if not math.isfinite(value):
        fail(f"{label} must be finite; got {value!r}")
    return value


def path_within(path: Path, parent: Path) -> bool:
    try:
        path.resolve().relative_to(parent.resolve())
        return True
    except ValueError:
        return False


def validate_recipe(recipe_path: Path, recipe: dict[str, Any], catalog: dict[str, Any], disabled: set[str], finish_palette: str | None) -> list[dict[str, Any]]:
    modules = {m.get("id"): m for m in catalog.get("modules", []) if isinstance(m, dict) and m.get("id")}
    palettes = catalog.get("palettes", {})
    if not isinstance(palettes, dict):
        fail("catalog.palettes must be an object")
    if finish_palette and finish_palette not in palettes:
        fail(f"unknown --finish-palette {finish_palette!r}; valid palettes: {', '.join(sorted(palettes))}")
    if disabled - DISABLEABLE_LAYERS:
        bad = ", ".join(sorted(disabled - DISABLEABLE_LAYERS))
        fail(f"--disable-layer only supports weathering or attachments; got {bad}")
    pieces = recipe.get("pieces")
    if not isinstance(pieces, list) or not pieces:
        fail(f"recipe must contain a non-empty pieces array: {recipe_path}")
    default_palette = recipe.get("palette")
    if default_palette is not None and default_palette not in palettes:
        fail(f"recipe palette {default_palette!r} is not in catalog.palettes")
    checked: list[dict[str, Any]] = []
    seen_instance_keys: set[str] = set()
    for index, piece in enumerate(pieces):
        if not isinstance(piece, dict):
            fail(f"pieces[{index}] must be an object")
        mid = piece.get("module")
        if not isinstance(mid, str) or not mid:
            fail(f"pieces[{index}].module must be a non-empty module id")
        if mid not in modules:
            fail(f"pieces[{index}] references unknown module {mid!r}; check catalog.json")
        instance_key = piece.get("instance_key")
        if not isinstance(instance_key, str) or not re.fullmatch(r"[0-9a-f]{20}", instance_key):
            fail(f"pieces[{index}].instance_key must be a unique 20-character lowercase hex hash")
        if instance_key in seen_instance_keys:
            fail(f"pieces[{index}].instance_key duplicates an earlier piece: {instance_key}")
        seen_instance_keys.add(instance_key)
        spec = modules[mid]
        module_path = KIT / str(spec.get("path", ""))
        if not module_path.is_file():
            fail(f"module {mid!r} path from catalog does not exist: {module_path}")
        t = piece.get("translation_m", [0, 0, 0])
        if not isinstance(t, list) or len(t) != 3:
            fail(f"pieces[{index}].translation_m must contain exactly three numbers")
        translation = [finite_number(v, f"pieces[{index}].translation_m[{j}]") for j, v in enumerate(t)]
        yaw = finite_number(piece.get("yaw_degrees", 0), f"pieces[{index}].yaw_degrees")
        palette = piece.get("palette", default_palette or "warm_lime")
        if not isinstance(palette, str) or palette not in palettes:
            fail(f"pieces[{index}].palette {palette!r} is not in catalog.palettes")
        available = set(spec.get("layers", []))
        if not available or not available <= VALID_LAYERS:
            fail(f"catalog module {mid!r} has invalid layers: {sorted(available)}")
        layers = piece.get("layers", sorted(available))
        if not isinstance(layers, list) or not layers or any(not isinstance(x, str) for x in layers):
            fail(f"pieces[{index}].layers must be a non-empty array of layer names")
        selected = set(layers)
        invalid = selected - available
        if invalid:
            fail(f"pieces[{index}] asks for layers absent from module {mid!r}: {', '.join(sorted(invalid))}")
        if selected - VALID_LAYERS:
            fail(f"pieces[{index}] uses invalid layer names: {', '.join(sorted(selected - VALID_LAYERS))}")
        checked.append({"index": index, "instance_key": instance_key, "module": mid, "path": module_path,
                        "translation_m": translation, "yaw_degrees": yaw,
                        "palette": palette, "layers": layers,
                        "purpose": str(piece.get("purpose", ""))})
    return checked


def clear_scene() -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for coll in list(bpy.data.collections):
        if coll.name != "Collection":
            bpy.data.collections.remove(coll)
    for mat in list(bpy.data.materials):
        bpy.data.materials.remove(mat)


def srgb_to_linear(channel: int) -> float:
    value = channel / 255.0
    return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4


def rgba_from_hex(value: str) -> tuple[float, float, float, float]:
    text = value.strip().lstrip("#")
    if len(text) != 6 or any(c not in "0123456789abcdefABCDEF" for c in text):
        fail(f"palette value must be a six-digit hex colour: {value!r}")
    return tuple(srgb_to_linear(int(text[i:i + 2], 16)) for i in (0, 2, 4)) + (1.0,)


def semantic_slot(material: bpy.types.Material) -> str | None:
    slot = material.get("semantic_slot")
    if isinstance(slot, str) and slot:
        return slot
    # Blender's glTF importer preserves source material names such as
    # "warm_lime | plaster" even when extras are not exposed as custom props.
    if "|" in material.name:
        return material.name.rsplit("|", 1)[1].strip()
    return None


def remap_materials(obj: bpy.types.Object, palette_name: str, palette: dict[str, str], layer: str,
                    allowed_layers: set[str] | None = None) -> int:
    if obj.type != "MESH":
        return 0
    changed = 0
    for slot_index, source in enumerate(list(obj.data.materials)):
        if source is None:
            continue
        mat = source.copy()
        slot = semantic_slot(source)
        if (allowed_layers is None or layer in allowed_layers) and slot in palette:
            rgba = rgba_from_hex(str(palette[slot]))
            mat.name = f"{palette_name} | {slot}"
            mat.diffuse_color = rgba
            mat["semantic_slot"] = slot
            nodes = mat.node_tree.nodes if mat.node_tree else None
            bsdf = nodes.get("Principled BSDF") if nodes else None
            if bsdf and "Base Color" in bsdf.inputs:
                bsdf.inputs["Base Color"].default_value = rgba
            changed += 1
        obj.data.materials[slot_index] = mat
    return changed


def object_layer(obj: bpy.types.Object) -> str | None:
    layer = obj.get("layer")
    if isinstance(layer, str):
        return layer
    parts = obj.name.split("__")
    for value in parts:
        if value in VALID_LAYERS:
            return value
    return None


def preserve_anchor_property(obj: bpy.types.Object) -> None:
    if "anchor" in obj:
        return
    marker = "__anchor_"
    if marker in obj.name:
        obj["anchor"] = obj.name.rsplit(marker, 1)[1]


def import_piece(spec: dict[str, Any], parent_root: bpy.types.Object, composed: bpy.types.Collection,
                 palette_table: dict[str, dict[str, str]], disabled: set[str], counters: dict[str, int]) -> None:
    before = set(bpy.data.objects)
    try:
        bpy.ops.import_scene.gltf(filepath=str(spec["path"]))
    except Exception as exc:
        fail(f"failed to import module {spec['module']!r} from {spec['path']}: {exc}")
    imported = [obj for obj in bpy.data.objects if obj not in before]
    if not imported:
        fail(f"module {spec['module']!r} imported no objects")
    imported_set = set(imported)
    for obj in imported:
        preserve_anchor_property(obj)
    removed = 0
    survivors: list[bpy.types.Object] = []
    recoloured = 0
    for obj in imported:
        layer = object_layer(obj)
        if obj.type == "MESH" and layer in disabled:
            bpy.data.objects.remove(obj, do_unlink=True)
            removed += 1
            continue
        if layer and layer not in set(spec["layers"]):
            if obj.type == "MESH":
                bpy.data.objects.remove(obj, do_unlink=True)
                removed += 1
                continue
        if layer:
            # First restore the recipe's authored palette across every layer.
            # Modules are stored in warm_lime, while recipes can intentionally
            # use sage_clay or ochre_slate for their structure and attachments.
            recoloured += remap_materials(obj, spec["palette"], palette_table[spec["palette"]], layer)
            # An explicit finish override is a second pass and is deliberately
            # limited to replaceable finish/weathering surfaces.
            if spec["finish_palette_override"] and layer in {"finish", "weathering"}:
                recoloured += remap_materials(obj, spec["finish_palette"], palette_table[spec["finish_palette"]], layer,
                                              {"finish", "weathering"})
        survivors.append(obj)
    # Do not touch removed StructRNA handles; Blender invalidates them as soon
    # as bpy.data.objects.remove() returns.
    imported = survivors
    imported_set = set(imported)
    for obj in imported:
        for collection in list(obj.users_collection):
            collection.objects.unlink(obj)
        composed.objects.link(obj)
    # Keep the imported hierarchy intact; only source roots become children of
    # the recipe piece root.  Anchors therefore remain attached to their mesh.
    for obj in imported:
        if obj.parent not in imported_set:
            obj.parent = parent_root
    counters["objects"] += len(imported)
    counters["mesh_objects"] += sum(o.type == "MESH" for o in imported)
    counters["removed_mesh_objects"] += removed
    counters["recoloured_material_slots"] += recoloured


def make_root(collection: bpy.types.Collection, recipe_id: str) -> bpy.types.Object:
    root = bpy.data.objects.new(recipe_id, None)
    root["recipe_id"] = recipe_id
    root["coordinate_system"] = "+Z up / -Y front"
    collection.objects.link(root)
    return root


def select_all(root: bpy.types.Object) -> None:
    bpy.ops.object.select_all(action="DESELECT")
    root.select_set(True)
    for child in root.children_recursive:
        child.select_set(True)
    bpy.context.view_layer.objects.active = root


def write_outputs(output: Path, root: bpy.types.Object, scene: bpy.types.Scene) -> list[Path]:
    output = output.resolve()
    validate_output_path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    select_all(root)
    glb = output if output.suffix.lower() == ".glb" else output.with_suffix(".glb")
    blend = output if output.suffix.lower() == ".blend" else output.with_suffix(".blend")
    bpy.ops.export_scene.gltf(filepath=str(glb), export_format="GLB", use_selection=True,
                              export_yup=True, export_apply=True, export_texcoords=True,
                              export_normals=True, export_materials="EXPORT", export_extras=True,
                              export_animations=False, export_cameras=False, export_lights=False)
    bpy.ops.wm.save_as_mainfile(filepath=str(blend))
    return [glb, blend]


def validate_output_path(output: Path) -> None:
    output = output.resolve()
    if path_within(output, KIT / "Modules") or path_within(output, KIT / "Assemblies"):
        fail(f"refusing to write inside source Modules/ or Assemblies/: {output}")
    if output.suffix.lower() not in {".glb", ".blend"}:
        fail(f"--output must end in .glb or .blend: {output}")


def parse_args() -> argparse.Namespace:
    raw = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recipe", required=True, help="recipe JSON path")
    parser.add_argument("--output", required=True, help="new .glb or .blend output path")
    parser.add_argument("--disable-layer", action="append", default=[], help="disable weathering or attachments; repeatable")
    parser.add_argument("--finish-palette", default=None, help="palette used for finish/weathering material slots")
    return parser.parse_args(raw)


def main() -> int:
    args = parse_args()
    recipe_path = Path(args.recipe).expanduser().resolve()
    validate_output_path(Path(args.output).expanduser().resolve())
    recipe = load_json(recipe_path, "recipe")
    catalog = load_json(CATALOG_PATH, "OrganicVillageMasters catalog")
    disabled = set(args.disable_layer)
    checked = validate_recipe(recipe_path, recipe, catalog, disabled, args.finish_palette)
    palette_table = catalog["palettes"]
    for piece in checked:
        piece["finish_palette"] = args.finish_palette or piece["palette"]
        piece["finish_palette_override"] = bool(args.finish_palette)
    clear_scene()
    scene = bpy.context.scene
    scene.name = f"Composed_{recipe.get('id', recipe_path.stem)}"
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0
    composed = bpy.data.collections.new("COMPOSED | recipe instances")
    scene.collection.children.link(composed)
    recipe_id = str(recipe.get("id") or recipe_path.stem)
    root = make_root(composed, recipe_id)
    root["source_recipe"] = recipe_path.as_posix()
    root["finish_palette_override"] = args.finish_palette or "recipe piece palettes"
    root["disabled_layers"] = ",".join(sorted(disabled))
    counters = {"objects": 0, "mesh_objects": 0, "removed_mesh_objects": 0, "recoloured_material_slots": 0}
    for piece in checked:
        p_root = bpy.data.objects.new(f"{recipe_id}__instance_{piece['instance_key']}__{piece['module']}", None)
        p_root["module_id"] = piece["module"]
        p_root["instance_key"] = piece["instance_key"]
        p_root["translation_m"] = piece["translation_m"]
        p_root["yaw_degrees"] = piece["yaw_degrees"]
        p_root["palette"] = piece["palette"]
        p_root["finish_palette"] = piece["finish_palette"]
        p_root["layers"] = ",".join(piece["layers"])
        p_root["purpose"] = piece["purpose"]
        composed.objects.link(p_root)
        p_root.parent = root
        p_root.location = piece["translation_m"]
        p_root.rotation_euler[2] = math.radians(piece["yaw_degrees"])
        import_piece(piece, p_root, composed, palette_table, disabled, counters)
    outputs = write_outputs(Path(args.output), root, scene)
    result = {"recipe": recipe_id, "pieces": len(checked), "disabled_layers": sorted(disabled),
              "outputs": [str(p) for p in outputs], **counters}
    print("COMPOSE_RECIPE_COMPLETE " + json.dumps(result, sort_keys=True), flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ComposeError as exc:
        print(f"COMPOSE_RECIPE_ERROR: {exc}", file=sys.stderr, flush=True)
        raise SystemExit(2)
