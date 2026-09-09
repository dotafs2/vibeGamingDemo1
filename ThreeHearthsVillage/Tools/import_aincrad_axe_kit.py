"""Import the four authored appearance parts for the existing Aincrad axe.

Run only in an owned Unreal Editor Python session. The parts share one tool
origin and are installed as flat static meshes; this script does not create
items, repair state, inventory, or world instances.
"""
import hashlib
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'Art/AincradLevel0'
DEST = '/Game/ThreeHearths/Generated/AincradTownKit'
MAT = '/Game/ThreeHearths/Materials/AincradLevel0'
data = json.loads((ART / 'axe_kit_manifest.json').read_text(encoding='utf-8'))
style = json.loads((ART / 'style.json').read_text(encoding='utf-8'))

world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
assert world, 'axe-kit import requires an editor world'
ue.SystemLibrary.execute_console_command(world, 'Editor.AsyncStaticMeshCompilation 0')
ue.SystemLibrary.execute_console_command(world, 'Editor.AsyncStaticMeshCompilationFinishAll')

materials = {}
for key in style['palette_srgb']:
    material = ue.load_asset(MAT + '/MI_Town_' + key)
    assert isinstance(material, ue.MaterialInstanceConstant), key
    materials[key] = material

# Source manifests carry Blender material names (AT_TimberLight), while
# the shared UE material map is keyed by semantic role (TimberLight).
for row in data.get('assets', []):
    row['material_roles'] = [name.removeprefix('AT_') for name in row.get('materials', [])]
    assert row['material_roles'] and all(role in materials for role in row['material_roles']), row['id']

report = {
    'style_id': data['style_id'],
    'revision': data.get('revision'),
    'destination': DEST,
    'assets': [],
    'scope': data.get('scope'),
}
old_path = ART / 'UE_Axe_Kit_Import_Report.json'
old_rows = {}
if old_path.is_file():
    old_rows = {row['id']: row for row in json.loads(old_path.read_text(encoding='utf-8')).get('assets', [])}


def material_names(asset):
    names = []
    for slot in asset.get_editor_property('static_materials'):
        material = slot.get_editor_property('material_interface')
        names.append(str(slot.get_editor_property('material_slot_name')) + ' ' + (material.get_name() if material else ''))
    return names


def bounds_match(asset, row):
    extent = asset.get_bounds().box_extent
    measured = [extent.x * 2, extent.y * 2, extent.z * 2]
    expected = [(b - a) * 100 for a, b in zip(row['bounds_min_m'], row['bounds_max_m'])]
    matched = all(abs(actual - want) < max(2, want * .005) for actual, want in zip(measured, expected))
    return matched, measured


def cached_asset_is_valid(row, previous):
    asset = ue.load_asset(previous.get('mesh', '')) if previous else None
    if not isinstance(asset, ue.StaticMesh):
        return False
    slots = asset.get_editor_property('static_materials')
    if not slots or any(not isinstance(slot.get_editor_property('material_interface'), ue.MaterialInstanceConstant) for slot in slots):
        return False
    names = material_names(asset)
    if not names or not all(any(('MI_Town_' + key in text) or ('AT_' + key in text)
                                for key in row['material_roles']) for text in names):
        return False
    matched, _ = bounds_match(asset, row)
    return matched


for row in data.get('assets', []):
    source = ART / row['glb']
    name = row['id']
    assert source.is_file(), f'missing axe-kit source: {source}'
    checksum = hashlib.sha256(source.read_bytes()).hexdigest()
    previous = old_rows.get(name)
    if (previous and previous.get('source_sha256') == checksum and previous.get('materials')
            and previous.get('source_bounds_min_m') and previous.get('source_bounds_max_m')
            and cached_asset_is_valid(row, previous)):
        report['assets'].append(previous)
        continue

    pipeline = ue.InterchangeGenericAssetsPipeline()
    pipeline.set_editor_property('use_source_name_for_asset', True)
    mesh_pipeline = pipeline.get_editor_property('mesh_pipeline')
    mesh_pipeline.set_editor_property('import_static_meshes', True)
    mesh_pipeline.set_editor_property('import_skeletal_meshes', False)
    mesh_pipeline.set_editor_property('collision', False)
    mesh_pipeline.set_editor_property('combine_static_meshes_behavior', ue.InterchangeCombineStaticMeshesBehavior.ALL)
    pipeline.get_editor_property('common_meshes_properties').set_editor_property('bake_meshes', True)
    pipeline.get_editor_property('animation_pipeline').set_editor_property('import_animations', False)
    gltf = ue.InterchangeGLTFPipeline()
    params = ue.ImportAssetParameters()
    params.set_editor_property('is_automated', True)
    params.set_editor_property('replace_existing', True)
    params.set_editor_property('override_pipelines', [ue.SoftObjectPath(pipeline.get_path_name()), ue.SoftObjectPath(gltf.get_path_name())])
    imported = []
    params.on_assets_import_done.bind_callable(lambda objects: imported.extend(objects))
    manager = ue.InterchangeManager.get_interchange_manager_scripted()
    destination = DEST + '/' + name
    assert manager.import_asset(destination, manager.create_source_data(str(source)), params), name
    ue.SystemLibrary.execute_console_command(world, 'Editor.AsyncStaticMeshCompilationFinishAll')
    meshes = [obj for obj in imported if isinstance(obj, ue.StaticMesh)]
    assert len(meshes) == 1, (name, len(meshes))
    asset = meshes[0]
    settings = asset.get_editor_property('nanite_settings')
    settings.set_editor_property('enabled', False)
    asset.set_editor_property('nanite_settings', settings)
    body = asset.get_editor_property('body_setup')
    if body:
        body.set_editor_property('collision_trace_flag', ue.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    bound_materials = []
    for index, slot in enumerate(asset.get_editor_property('static_materials')):
        material = slot.get_editor_property('material_interface')
        names = str(slot.get_editor_property('material_slot_name')) + ' ' + (material.get_name() if material else '')
        choices = [key for key in row['material_roles']
                   if ('AT_' + key in names) or ('MI_Town_' + key in names)]
        assert choices, (name, names)
        selected = materials[max(choices, key=len)]
        asset.set_material(index, selected)
        bound_materials.append(str(selected))
    assert bound_materials, name
    for obj in imported:
        assert ue.EditorAssetLibrary.save_loaded_asset(obj)
    assert ue.EditorAssetLibrary.save_loaded_asset(asset)
    ue.SystemLibrary.execute_console_command(world, 'Editor.AsyncStaticMeshCompilationFinishAll')
    matched, measured = bounds_match(asset, row)
    assert matched, (name, row['bounds_min_m'], row['bounds_max_m'], measured)
    report['assets'].append({
        'id': name,
        'mesh': asset.get_path_name(),
        'dimensions_cm': measured,
        'source_sha256': checksum,
        'source_bounds_min_m': row['bounds_min_m'],
        'source_bounds_max_m': row['bounds_max_m'],
        'materials': bound_materials,
    })
    ue.log('AXE_KIT_IMPORTED ' + name)

ue.SystemLibrary.execute_console_command(world, 'Editor.AsyncStaticMeshCompilationFinishAll')
(ART / 'UE_Axe_Kit_Import_Report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
ue.log('AXE_KIT_IMPORT_COMPLETE ' + str(len(report['assets'])))
