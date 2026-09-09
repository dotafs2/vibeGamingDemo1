"""Import the reviewed Aincrad V2 characters through UE's legacy FBX factory.

This script is intentionally limited to the three explicit manifest entries and
their six explicit animation FBX files.  It never searches Saved/ or touches the
existing SKM_Villager asset.
"""
import hashlib
import json
from pathlib import Path

import unreal as ue


ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / "Art" / "AincradCharacters" / "V2"
EXPORTS = ART / "Exports"
MANIFEST = json.loads((ART / "manifest.json").read_text(encoding="utf-8"))
DEST_ROOT = "/Game/ThreeHearths/Generated/AincradCharactersV2"
REPORT_PATH = ART / "UE_Import_Report.json"
ASSET_TOOLS = ue.AssetToolsHelpers.get_asset_tools()
EDIT = ue.MaterialEditingLibrary


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def save(asset):
    assert asset, asset
    asset.modify()
    assert ue.EditorAssetLibrary.save_loaded_asset(asset, False), asset


def import_fbx(source, destination, name, options):
    assert source.is_file(), source
    task = ue.AssetImportTask()
    task.filename = str(source)
    task.destination_path = destination
    task.destination_name = name
    task.replace_existing = True
    task.replace_existing_settings = True
    task.automated = True
    task.save = True
    task.factory = ue.FbxFactory()  # Pin the legacy importer; do not use Interchange.
    task.options = options
    ASSET_TOOLS.import_asset_tasks([task])
    paths = list(task.get_editor_property("imported_object_paths"))
    assert paths, f"FBX import produced no assets: {source}"
    objects = [ue.load_asset(path) for path in paths]
    assert all(objects), (source, paths)
    return objects


def mesh_options():
    options = ue.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.original_import_type = ue.FBXImportType.FBXIT_SKELETAL_MESH
    options.mesh_type_to_import = ue.FBXImportType.FBXIT_SKELETAL_MESH
    options.import_as_skeletal = True
    options.import_mesh = True
    options.import_animations = False
    options.import_materials = False
    options.import_textures = False
    options.create_physics_asset = False
    mesh_import_data = options.get_editor_property("skeletal_mesh_import_data")
    assert mesh_import_data, "FbxImportUI has no skeletal mesh import data"
    mesh_import_data.set_editor_property(
        "normal_import_method", ue.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return options


def animation_options(skeleton, animation_name):
    options = ue.FbxImportUI()
    options.automated_import_should_detect_type = False
    options.original_import_type = ue.FBXImportType.FBXIT_ANIMATION
    options.mesh_type_to_import = ue.FBXImportType.FBXIT_ANIMATION
    options.import_as_skeletal = True
    options.import_mesh = False
    options.import_animations = True
    options.import_materials = False
    options.import_textures = False
    options.create_physics_asset = False
    options.override_animation_name = animation_name
    options.skeleton = skeleton
    return options


def srgb(code):
    values = [int(code[i:i + 2], 16) / 255.0 for i in (0, 2, 4)]
    return [v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4 for v in values]


def create_master(path):
    name = "M_AincradCharacterSurface"
    master = ue.load_asset(path + "/" + name)
    if master is None:
        master = ASSET_TOOLS.create_asset(name, path, ue.Material, ue.MaterialFactoryNew())
    assert isinstance(master, ue.Material)
    master.set_editor_property("used_with_skeletal_mesh", True)
    EDIT.delete_all_material_expressions(master)
    tint = EDIT.create_material_expression(master, ue.MaterialExpressionVectorParameter, -500, 0)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", ue.LinearColor(1, 1, 1, 1))
    rough = EDIT.create_material_expression(master, ue.MaterialExpressionScalarParameter, -500, 180)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", .82)
    spec = EDIT.create_material_expression(master, ue.MaterialExpressionScalarParameter, -500, 300)
    spec.set_editor_property("parameter_name", "Specular")
    spec.set_editor_property("default_value", .15)
    strength = EDIT.create_material_expression(master, ue.MaterialExpressionConstant, -220, 470)
    strength.set_editor_property("r", .035)
    emissive = EDIT.create_material_expression(master, ue.MaterialExpressionMultiply, 220, 80)
    EDIT.connect_material_expressions(tint, "RGB", emissive, "A")
    EDIT.connect_material_expressions(strength, "", emissive, "B")
    EDIT.connect_material_property(tint, "RGB", ue.MaterialProperty.MP_BASE_COLOR)
    EDIT.connect_material_property(rough, "", ue.MaterialProperty.MP_ROUGHNESS)
    EDIT.connect_material_property(spec, "", ue.MaterialProperty.MP_SPECULAR)
    EDIT.connect_material_property(emissive, "", ue.MaterialProperty.MP_EMISSIVE_COLOR)
    EDIT.recompile_material(master)
    save(master)
    return master


def make_materials(name, palette, path, master):
    colors = dict(palette)
    colors.update({"White": "F5F1EA", "Ink": "332E38", "Mouth": "A76562"})
    result = {}
    for role, code in colors.items():
        mi_name = f"MI_{name}_{role}"
        mi = ue.load_asset(path + "/" + mi_name)
        if mi is None:
            mi = ASSET_TOOLS.create_asset(mi_name, path, ue.MaterialInstanceConstant, ue.MaterialInstanceConstantFactoryNew())
        assert isinstance(mi, ue.MaterialInstanceConstant)
        EDIT.set_material_instance_parent(mi, master)
        EDIT.set_material_instance_vector_parameter_value(mi, "Tint", ue.LinearColor(*srgb(code), 1))
        EDIT.update_material_instance(mi)
        save(mi)
        result[role] = mi
    return result


def assign_materials(mesh, name, materials):
    slots = list(mesh.materials)
    mapping = {}
    for index, slot in enumerate(slots):
        slot_name = str(slot.material_slot_name)
        prefix = f"AC_{name}_"
        assert slot_name.startswith(prefix), (name, slot_name)
        role = slot_name[len(prefix):]
        assert role in materials, (name, role)
        old = slots[index]
        slots[index] = ue.SkeletalMaterial(
            material_interface=materials[role],
            material_slot_name=old.material_slot_name,
            uv_channel_data=old.uv_channel_data)
        mapping[slot_name] = materials[role].get_path_name()
    assert mapping and len(mapping) == len(slots)
    mesh.modify()
    mesh.set_editor_property("materials", slots)
    assigned = list(mesh.get_editor_property("materials"))
    assert all(slot.material_interface for slot in assigned), (name, assigned)
    save(mesh)
    return mapping


def import_character(row):
    name = row["id"]
    assert row.get("roles"), (name, "manifest roles missing")
    assert set(MANIFEST["palette"][name]).issubset(set(row["roles"])), (name, row["roles"])
    source = EXPORTS / Path(row["fbx"]).name
    folder = f"{DEST_ROOT}/{name}"
    material_folder = folder + "/Materials"
    source_hash = sha256(source)
    objects = import_fbx(source, folder, f"SK_{name}", mesh_options())
    meshes = [obj for obj in objects if isinstance(obj, ue.SkeletalMesh)]
    assert len(meshes) == 1, (name, [obj.get_path_name() for obj in objects])
    mesh = meshes[0]
    skeleton = mesh.skeleton
    assert isinstance(skeleton, ue.Skeleton)
    assert not mesh.physics_asset, f"Unexpected physics asset on {mesh.get_path_name()}"
    bounds = mesh.get_bounds()
    dimensions_cm = [bounds.box_extent.x * 2, bounds.box_extent.y * 2, bounds.box_extent.z * 2]
    measured_height_m = dimensions_cm[2] / 100.0
    assert abs(measured_height_m - row["height_m"]) / row["height_m"] <= .05, (name, measured_height_m, row["height_m"])
    master = create_master(material_folder)
    materials = make_materials(name, MANIFEST["palette"][name], material_folder, master)
    material_slots = assign_materials(mesh, name, materials)
    save(skeleton)

    animations = []
    for clip in ("Idle", "Walk"):
        clip_source = EXPORTS / f"{name}_{clip}.fbx"
        clip_hash = sha256(clip_source)
        clip_objects = import_fbx(clip_source, folder, f"AC_{name}_{clip}", animation_options(skeleton, f"AC_{name}_{clip}"))
        sequences = [obj for obj in clip_objects if isinstance(obj, ue.AnimSequence)]
        assert len(sequences) == 1, (name, clip, [obj.get_path_name() for obj in clip_objects])
        sequence = sequences[0]
        assert sequence.get_editor_property("skeleton") == skeleton
        duration = float(sequence.get_editor_property("sequence_length"))
        assert duration > 0.0, (name, clip, duration)
        save(sequence)
        animations.append({"clip": clip, "path": sequence.get_path_name(), "duration_s": duration, "source_sha256": clip_hash})

    # Animation imports share the destination; persist slot overrides after all FBX tasks.
    material_slots = assign_materials(mesh, name, materials)

    return {
        "id": name,
        "mesh": mesh.get_path_name(),
        "skeleton": skeleton.get_path_name(),
        "mesh_bounds_cm": dimensions_cm,
        "manifest_height_m": row["height_m"],
        "measured_height_m": measured_height_m,
        "source_sha256": source_hash,
        "animations": animations,
        "material_slots": material_slots,
    }


def main():
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    ue.SystemLibrary.execute_console_command(world, "Interchange.FeatureFlags.Import.FBX 0")
    rows = {row["id"]: row for row in MANIFEST["assets"]}
    assert set(rows) == {"Aileen", "Takuma", "Kashiwagi"}, set(rows)
    report = {"revision": 2, "destination": DEST_ROOT, "assets": []}
    for name in ("Aileen", "Takuma", "Kashiwagi"):
        report["assets"].append(import_character(rows[name]))
    assert len(report["assets"]) == 3
    REPORT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    ue.log("AINCRAD_CHARACTERS_V2_IMPORT_COMPLETE")


if __name__ == "__main__":
    main()
