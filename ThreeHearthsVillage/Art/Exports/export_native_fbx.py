"""Read-only UE export of every Git-tracked mesh and current procedural recipes.

Run after compiling the current editor plugin, with -HearthDisableApi,
-HearthNoWorldPersistence, -d3d11 -RenderOffscreen -AllowCommandletRendering
-run=pythonscript -script=<this file>. Skeletal export requires an RHI.
Does not load a saved simulation or save any project asset.
"""
from pathlib import Path
import json
import hashlib
import subprocess
import unreal as ue

PROJECT = Path(__file__).resolve().parents[2]
REPO = PROJECT.parent
OUT = PROJECT / 'Saved/ThreeHearths/CombinedFbx'
OUT.mkdir(parents=True, exist_ok=True)
tracked = subprocess.check_output(['git', 'ls-files', '-z'], cwd=REPO,
    creationflags=subprocess.CREATE_NO_WINDOW).decode('utf-8').split('\0')
commit = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=REPO,
    creationflags=subprocess.CREATE_NO_WINDOW).decode().strip()
sources = [p for p in tracked if Path(p).suffix.lower() in ('.glb', '.gltf', '.blend', '.fbx', '.obj')]
prefix = 'ThreeHearthsVillage/Content/'
packages = {'/Game/' + p[len(prefix):-7] for p in tracked if p.startswith(prefix) and p.endswith('.uasset')}
world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
ue.SystemLibrary.execute_console_command(world, 'Hearth.ExportArtCatalog')
catalog_path = PROJECT / 'Saved/ThreeHearths/ArtCatalog/catalog.json'
catalog = json.loads(catalog_path.read_text(encoding='utf-8-sig'))
assert len(catalog['entries']) == 12
(OUT / 'catalog.json').write_text(json.dumps(catalog, ensure_ascii=False, indent=2), encoding='utf-8')
registry = ue.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(synchronous_search=True)
mesh_paths = {}
classes = {}
for ad in registry.get_assets_by_path('/Game', recursive=True):
    cls = str(ad.asset_class_path.asset_name)
    package = str(ad.package_name)
    if package in packages:
        classes[cls] = classes.get(cls, 0) + 1
        if cls in ('StaticMesh', 'SkeletalMesh'):
            mesh_paths[package] = {'in_git': True, 'class': cls}
for entry in catalog['entries']:
    for part in entry['parts']:
        package = part['mesh_path'].split('.')[0]
        mesh_paths.setdefault(package, {'in_git': package in packages, 'class': 'StaticMesh'})

records = []
for index, (path, meta) in enumerate(sorted(mesh_paths.items())):
    asset = ue.load_asset(path)
    assert asset, path
    key = path.strip('/').replace('/', '__')
    # Stable compact filenames avoid the Windows path-length limit.
    filename = OUT / (hashlib.sha256(path.encode()).hexdigest()[:12] + '_' + asset.get_name()[:48] + '.fbx')
    task = ue.AssetExportTask()
    task.object = asset
    task.filename = str(filename)
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    task.exporter = ue.SkeletalMeshExporterFBX() if meta['class'] == 'SkeletalMesh' else ue.StaticMeshExporterFBX()
    task.options = ue.FbxExportOption()
    task.options.bake_material_inputs = ue.FbxMaterialBakeMode.DISABLED
    task.options.ascii = False
    task.options.level_of_detail = False
    task.options.collision = False
    task.options.export_source_mesh = True
    task.options.force_front_x_axis = False
    task.options.vertex_color = True
    task.options.export_morph_targets = True
    assert ue.Exporter.run_asset_export_task(task), path
    record = dict(meta, path=path, key=key, fbx=str(filename), bytes=filename.stat().st_size)
    if meta['class'] == 'StaticMesh':
        bounds = asset.get_bounds()
        record['bounds_origin_cm'] = [bounds.origin.x, bounds.origin.y, bounds.origin.z]
        record['bounds_extent_cm'] = [bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z]
    records.append(record)
    ue.log('COMBINED_FBX_NATIVE %d/%d %s' % (index+1, len(mesh_paths), path))
report = {'commit': commit, 'engine': ue.SystemLibrary.get_engine_version(),
    'tracked_model_sources': sources, 'tracked_content_classes': classes,
    'meshes': records, 'catalog_entries': len(catalog['entries']),
    'material_note': 'Simple constants/textures exported; Unreal procedural shader graphs are not representable by FBX.'}
(OUT/'native-export-report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
ue.log('COMBINED_FBX_NATIVE_COMPLETE ' + str(len(records)))
