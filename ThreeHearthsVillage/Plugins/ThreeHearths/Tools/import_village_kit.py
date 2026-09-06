"""Import the local VillageKit into native editor assets using a commandlet.

Run with UnrealEditor-Cmd <project> -run=pythonscript -script=<this file>
Saved per-source hashes make an interrupted run resumable without overwriting art.
"""
import hashlib
import json
from pathlib import Path
import time
import unreal as ue

ROOT=Path(__file__).resolve().parents[3]
KIT=ROOT/'Art/VillageKit'
DEST='/Game/ThreeHearths/Generated/VillageKit'
REPORT=KIT/'UE_Import_Report.json'

def disable_nanite_for_low_poly_kit(mesh):
    """VillageKit pieces are tiny low-poly meshes; Nanite only adds material usage churn."""
    settings=mesh.get_editor_property('nanite_settings')
    if not settings.get_editor_property('enabled'):
        return False
    settings.set_editor_property('enabled',False)
    mesh.set_editor_property('nanite_settings',settings)
    return True

def import_one(source,asset_id,previous,replace_changed=False,destination_root=DEST):
    checksum=hashlib.sha256(source.read_bytes()).hexdigest()
    old=previous.get(asset_id)
    existing=ue.load_asset(old['mesh']) if old and old['source_sha256']==checksum else None
    if existing:
        if disable_nanite_for_low_poly_kit(existing):
            if not ue.get_editor_subsystem(ue.EditorAssetSubsystem).save_loaded_asset(existing,False):
                raise RuntimeError('Cannot save Nanite setting for '+asset_id)
        old['nanite']='disabled_low_poly_kit'
        return old
    destination=destination_root+'/'+asset_id
    replacing=bool(old and replace_changed and old['mesh'].startswith(destination+'/'))
    if ue.EditorAssetLibrary.does_directory_exist(destination) and not replacing:
        raise RuntimeError('Existing asset cache requires review before replacing: '+destination)
    pipeline=ue.InterchangeGenericAssetsPipeline()
    mesh=pipeline.get_editor_property('mesh_pipeline')
    for key,value in [('import_static_meshes',True),('import_skeletal_meshes',False),('collision',False)]:
        mesh.set_editor_property(key,value)
    mesh.set_editor_property('combine_static_meshes_behavior',ue.InterchangeCombineStaticMeshesBehavior.ALL)
    pipeline.get_editor_property('common_meshes_properties').set_editor_property('bake_meshes',True)
    pipeline.get_editor_property('animation_pipeline').set_editor_property('import_animations',False)
    pipeline.get_editor_property('material_pipeline').set_editor_property('import_materials',True)
    gltf=ue.InterchangeGLTFPipeline()
    params=ue.ImportAssetParameters()
    params.set_editor_property('is_automated',True)
    params.set_editor_property('replace_existing',replacing)
    params.set_editor_property('override_pipelines',[ue.SoftObjectPath(pipeline.get_path_name()),ue.SoftObjectPath(gltf.get_path_name())])
    imported=[]
    params.on_assets_import_done.bind_callable(lambda objects:imported.extend(objects))
    manager=ue.InterchangeManager.get_interchange_manager_scripted()
    started=time.monotonic()
    if not manager.import_asset(destination,manager.create_source_data(str(source)),params):
        raise RuntimeError('Interchange rejected '+asset_id)
    meshes=[obj for obj in imported if isinstance(obj,ue.StaticMesh)]
    if len(meshes)!=1 or meshes[0].get_num_lods()<1:
        raise RuntimeError('Missing renderable combined mesh: '+asset_id)
    sm=meshes[0]
    disable_nanite_for_low_poly_kit(sm)
    slots=sm.get_editor_property('static_materials')
    if not slots or any(not slot.get_editor_property('material_interface') for slot in slots):
        raise RuntimeError('Missing material slot: '+asset_id)
    extent=sm.get_bounds().box_extent
    if min(extent.x,extent.y,extent.z)<=0:
        raise RuntimeError('Zero model extent: '+asset_id)
    assets=ue.get_editor_subsystem(ue.EditorAssetSubsystem)
    for obj in imported:
        if not obj.get_path_name().startswith(destination+'/'):
            raise RuntimeError('Unexpected import destination')
        if not assets.save_loaded_asset(obj,False):
            raise RuntimeError('Cannot save '+obj.get_path_name())
    return {'id':asset_id,'source':source.relative_to(ROOT).as_posix(),'source_sha256':checksum,
        'mesh':sm.get_path_name(),'material_slots':len(slots),
        'bounds_extent_cm':[extent.x,extent.y,extent.z],
        'import_seconds':time.monotonic()-started,'saved_assets':len(imported),
        'collision':'not_generated_requires_gameplay_template','nanite':'disabled_low_poly_kit'}

def main():
    _,switches,_=ue.SystemLibrary.parse_command_line(ue.SystemLibrary.get_command_line())
    replace_changed='VillageKitReplaceChanged' in switches
    catalog=json.loads((KIT/'catalog.json').read_text(encoding='utf-8'))
    sources=[(m['id'],KIT/m['asset_glb']) for m in catalog['modules']]
    sources.extend(('example__'+e['id'],KIT/'examples'/(e['id']+'.glb')) for e in catalog['blueprints'])
    previous={}
    if REPORT.exists():
        prior=json.loads(REPORT.read_text(encoding='utf-8'))
        previous={row['id']:row for row in prior.get('resume_assets',[])+prior.get('assets',[])}
    report={'status':'running','engine':ue.SystemLibrary.get_engine_version(),
        'scope':'native editor assets; NPC behavior and collision are not enabled',
        'assets':[],'resume_assets':list(previous.values())}
    try:
        for asset_id,source in sources:
            report['assets'].append(import_one(source,asset_id,previous,replace_changed))
            REPORT.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
            ue.log('[VillageKitImport] '+asset_id+' saved')
        report['status']='passed'
        report.pop('resume_assets',None)
    except Exception as exc:
        report.update(status='failed',error=str(exc))
        ue.log_error('[VillageKitImport] '+str(exc))
        raise
    finally:
        REPORT.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__': main()
