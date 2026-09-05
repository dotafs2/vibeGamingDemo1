"""Read-only UE commandlet export of project-owned Cropout fitting references.

Outputs stay in Saved and are not distributed as ResidentKit originals.
"""
from pathlib import Path
import json
import unreal as ue

OUT=Path(ue.Paths.project_saved_dir())/'ThreeHearths'/'ResidentKitReference'
OUT.mkdir(parents=True,exist_ok=True)
results=[]
for asset_path in ('/Game/Characters/Meshes/SKM_Villager','/Game/Characters/Meshes/Hats/SKM_Farmer'):
    asset=ue.load_asset(asset_path)
    if not asset:raise RuntimeError('Missing fitting reference '+asset_path)
    task=ue.AssetExportTask()
    task.object=asset
    task.filename=str(OUT/(asset.get_name()+'.fbx'))
    task.automated=True
    task.prompt=False
    task.replace_identical=True
    task.exporter=ue.SkeletalMeshExporterFBX()
    task.options=ue.FbxExportOption()
    task.options.bake_material_inputs=ue.FbxMaterialBakeMode.DISABLED
    for key,value in (('ascii',False),('level_of_detail',False),('collision',False),('export_morph_targets',False)):
        try:task.options.set_editor_property(key,value)
        except Exception:pass
    ok=ue.Exporter.run_asset_export_task(task)
    if not ok:raise RuntimeError('FBX export failed '+asset_path)
    results.append({'asset_path':asset_path,'fbx':task.filename,'bytes':Path(task.filename).stat().st_size})
(OUT/'ue-export-report.json').write_text(json.dumps({'engine':ue.SystemLibrary.get_engine_version(),'assets':results},indent=2)+'\n',encoding='utf-8')
ue.log('RESIDENT_REFERENCE_EXPORTED '+str(len(results)))
