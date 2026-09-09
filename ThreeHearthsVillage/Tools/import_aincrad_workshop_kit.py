"""Import the six workshop modules, binding the existing town materials unchanged."""
import json, hashlib
from pathlib import Path
import unreal as ue
ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'Art/AincradLevel0'
DEST='/Game/ThreeHearths/Generated/AincradTownKit'
MAT='/Game/ThreeHearths/Materials/AincradLevel0'
data=json.loads((ART/'workshop_kit_manifest.json').read_text(encoding='utf-8'))
style=json.loads((ART/'style.json').read_text(encoding='utf-8'))
materials={}
for key in style['palette_srgb']:
    obj=ue.load_asset(MAT+'/MI_Town_'+key)
    assert isinstance(obj,ue.MaterialInstanceConstant),key
    materials[key]=obj
report={'style_id':data['style_id'],'assets':[],'native_collision':'complex-as-simple for static modular pieces; runtime chooses query participation'}
old_path=ART/'UE_Workshop_Kit_Import_Report.json'
old_rows={r['id']:r for r in json.loads(old_path.read_text(encoding='utf-8'))['assets']} if old_path.exists() else {}
for row in data['assets']:
    source=ART/row['glb'];name=row['id'];destination=DEST+'/'+name
    checksum=hashlib.sha256(source.read_bytes()).hexdigest()
    previous=old_rows.get(name)
    if previous and previous['source_sha256']==checksum and isinstance(ue.load_asset(previous['mesh']),ue.StaticMesh):
        report['assets'].append(previous)
        continue
    pipeline=ue.InterchangeGenericAssetsPipeline()
    pipeline.set_editor_property('use_source_name_for_asset',True)
    mesh=pipeline.get_editor_property('mesh_pipeline')
    mesh.set_editor_property('import_static_meshes',True);mesh.set_editor_property('import_skeletal_meshes',False);mesh.set_editor_property('collision',False)
    mesh.set_editor_property('combine_static_meshes_behavior',ue.InterchangeCombineStaticMeshesBehavior.ALL)
    pipeline.get_editor_property('common_meshes_properties').set_editor_property('bake_meshes',True)
    pipeline.get_editor_property('animation_pipeline').set_editor_property('import_animations',False)
    gltf=ue.InterchangeGLTFPipeline()
    params=ue.ImportAssetParameters();params.set_editor_property('is_automated',True);params.set_editor_property('replace_existing',True)
    params.set_editor_property('override_pipelines',[ue.SoftObjectPath(pipeline.get_path_name()),ue.SoftObjectPath(gltf.get_path_name())])
    imported=[];params.on_assets_import_done.bind_callable(lambda objects:imported.extend(objects))
    manager=ue.InterchangeManager.get_interchange_manager_scripted()
    assert manager.import_asset(destination,manager.create_source_data(str(source)),params),name
    meshes=[o for o in imported if isinstance(o,ue.StaticMesh)]
    assert len(meshes)==1,(name,len(meshes))
    asset=meshes[0]
    settings=asset.get_editor_property('nanite_settings');settings.set_editor_property('enabled',False);asset.set_editor_property('nanite_settings',settings)
    body=asset.get_editor_property('body_setup')
    if body:body.set_editor_property('collision_trace_flag',ue.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    for index,slot in enumerate(asset.get_editor_property('static_materials')):
        mat=slot.get_editor_property('material_interface')
        names=str(slot.get_editor_property('material_slot_name'))+' '+(mat.get_name() if mat else '')
        choices=[key for key in materials if 'AT_'+key in names]
        assert choices,(name,names)
        asset.set_material(index,materials[max(choices,key=len)])
    for obj in imported:assert ue.EditorAssetLibrary.save_loaded_asset(obj)
    assert ue.EditorAssetLibrary.save_loaded_asset(asset)
    bounds=asset.get_bounds();extent=bounds.box_extent
    expected=[(b-a)*100 for a,b in zip(row['bounds_min_m'],row['bounds_max_m'])]
    measured=[extent.x*2,extent.y*2,extent.z*2]
    assert all(abs(a-b)<max(2,b*.005) for a,b in zip(measured,expected)),(name,expected,measured)
    report['assets'].append({'id':name,'mesh':asset.get_path_name(),'dimensions_cm':measured,'source_sha256':checksum,'materials':[str(s.get_editor_property('material_interface')) for s in asset.get_editor_property('static_materials')]})
    ue.log('WORKSHOP_KIT_IMPORTED '+name)
(ART/'UE_Workshop_Kit_Import_Report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
ue.log('WORKSHOP_KIT_IMPORT_COMPLETE '+str(len(report['assets'])))
