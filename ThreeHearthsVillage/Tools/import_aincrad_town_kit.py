"""Import only the original AincradTownKit branch and save native materials."""
import json
import hashlib
from pathlib import Path
import unreal as ue

# Cold reimport must not race a background build of the old mesh description.
_world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
ue.SystemLibrary.execute_console_command(_world, "Editor.AsyncStaticMeshCompilation 0")
ue.SystemLibrary.execute_console_command(_world, "Editor.AsyncStaticMeshCompilationFinishAll")

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'Art/AincradLevel0'
DEST='/Game/ThreeHearths/Generated/AincradTownKit'
MAT='/Game/ThreeHearths/Materials/AincradLevel0'
data=json.loads((ART/'town_kit_manifest.json').read_text(encoding='utf-8'))
style=json.loads((ART/'style.json').read_text(encoding='utf-8'))
edit=ue.MaterialEditingLibrary
assets=ue.AssetToolsHelpers.get_asset_tools()
def get(name,root,cls,factory):
    obj=ue.load_asset(root+'/'+name)
    if obj is None:obj=assets.create_asset(name,root,cls,factory)
    assert isinstance(obj,cls),(name,str(type(obj)))
    return obj

master=get('M_AnimeTownSurface',MAT,ue.Material,ue.MaterialFactoryNew())
edit.delete_all_material_expressions(master)
master.set_editor_property('used_with_instanced_static_meshes',True)
custom=edit.create_material_expression(master,ue.MaterialExpressionCustom,0,0)
custom.set_editor_property('output_type',ue.CustomMaterialOutputType.CMOT_FLOAT3)
custom.set_editor_property('code','''
float3 HearthP=HearthWorldCm*.001;
float HearthBroad=.5+.25*sin(HearthP.x*.39+sin(HearthP.y*.31))+.25*sin(HearthP.y*.47+HearthP.z*.23);
float HearthGrain=.5+.5*sin(HearthUV.x*57+sin(HearthUV.y*8));
float HearthFade=saturate(1-max(length(ddx(HearthUV)),length(ddy(HearthUV)))*70);
float HearthTone=HearthKind<.5 ? .92+.16*HearthBroad : .98+.04*HearthBroad;
if(HearthKind>1.5 && HearthKind<2.5) HearthTone*=1+(.035*HearthGrain-.0175)*HearthFade;
return HearthTint*HearthTone;
''')
inputs=[]
for name in ('HearthWorldCm','HearthUV','HearthTint','HearthKind'):
    item=ue.CustomInput();item.set_editor_property('input_name',name);inputs.append(item)
custom.set_editor_property('inputs',inputs)
for key,cls,y in [('HearthWorldCm',ue.MaterialExpressionWorldPosition,0),('HearthUV',ue.MaterialExpressionTextureCoordinate,140)]:
    node=edit.create_material_expression(master,cls,-500,y);edit.connect_material_expressions(node,'',custom,key)
tint=edit.create_material_expression(master,ue.MaterialExpressionVectorParameter,-500,300)
tint.set_editor_property('parameter_name','HearthTint');tint.set_editor_property('default_value',ue.LinearColor(.4,.4,.3,1))
edit.connect_material_expressions(tint,'RGB',custom,'HearthTint')
kind=edit.create_material_expression(master,ue.MaterialExpressionScalarParameter,-500,400);kind.set_editor_property('parameter_name','HearthKind');kind.set_editor_property('default_value',1.)
edit.connect_material_expressions(kind,'',custom,'HearthKind')
edit.connect_material_property(custom,'',ue.MaterialProperty.MP_BASE_COLOR)
for key,prop,default,y in [('HearthRoughness',ue.MaterialProperty.MP_ROUGHNESS,.88,300),('HearthSpecular',ue.MaterialProperty.MP_SPECULAR,.18,430),('HearthMetallic',ue.MaterialProperty.MP_METALLIC,0.,550)]:
    node=edit.create_material_expression(master,ue.MaterialExpressionScalarParameter,0,y);node.set_editor_property('parameter_name',key);node.set_editor_property('default_value',default);edit.connect_material_property(node,'',prop)
fill=edit.create_material_expression(master,ue.MaterialExpressionScalarParameter,-300,600);fill.set_editor_property('parameter_name','HearthFill');fill.set_editor_property('default_value',.07)
mul=edit.create_material_expression(master,ue.MaterialExpressionMultiply,260,620);edit.connect_material_expressions(custom,'',mul,'A');edit.connect_material_expressions(fill,'',mul,'B');edit.connect_material_property(mul,'',ue.MaterialProperty.MP_EMISSIVE_COLOR)
edit.recompile_material(master);assert ue.EditorAssetLibrary.save_loaded_asset(master)
def srgb(code):
    vals=[int(code[i:i+2],16)/255 for i in (0,2,4)]
    return [v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4 for v in vals]
palette=dict(style['palette_srgb'])
palette.update({'Grass':'8DA372','Paving':'C3BDA9','PlasterWarm':palette['Plaster']})
materials={}
for key,code in palette.items():
    mi=get('MI_Town_'+key,MAT,ue.MaterialInstanceConstant,ue.MaterialInstanceConstantFactoryNew())
    edit.set_material_instance_parent(mi,master)
    edit.set_material_instance_vector_parameter_value(mi,'HearthTint',ue.LinearColor(*srgb(code),1))
    edit.set_material_instance_scalar_parameter_value(mi,'HearthKind',0 if key=='Grass' else 2 if 'Timber' in key else 1)
    edit.set_material_instance_scalar_parameter_value(mi,'HearthFill',3.5 if key=='LampWarm' else .015 if key in ('Grass','Paving') else .07)
    if key in ('Iron','IronLight','SteelEdge','Brass'):
        edit.set_material_instance_scalar_parameter_value(mi,'HearthMetallic',.4);edit.set_material_instance_scalar_parameter_value(mi,'HearthRoughness',.6)
    edit.update_material_instance(mi);assert ue.EditorAssetLibrary.save_loaded_asset(mi)
    materials[key]=mi

report={'style_id':data['style_id'],'assets':[],'native_collision':'complex-as-simple for static modular pieces; runtime chooses query participation'}
old_path=ART/'UE_Town_Kit_Import_Report.json'
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
        if choices:asset.set_material(index,materials[max(choices,key=len)])
    for obj in imported:assert ue.EditorAssetLibrary.save_loaded_asset(obj)
    assert ue.EditorAssetLibrary.save_loaded_asset(asset)
    bounds=asset.get_bounds();extent=bounds.box_extent
    expected=[(b-a)*100 for a,b in zip(row['bounds_min_m'],row['bounds_max_m'])]
    measured=[extent.x*2,extent.y*2,extent.z*2]
    assert all(abs(a-b)<max(2,b*.005) for a,b in zip(measured,expected)),(name,expected,measured)
    report['assets'].append({'id':name,'mesh':asset.get_path_name(),'dimensions_cm':measured,'source_sha256':checksum,'materials':[str(s.get_editor_property('material_interface')) for s in asset.get_editor_property('static_materials')]})
    ue.log('TOWN_KIT_IMPORTED '+name)
(ART/'UE_Town_Kit_Import_Report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
ue.log('TOWN_KIT_IMPORT_COMPLETE '+str(len(report['assets'])))
