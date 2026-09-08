"""Build an original, reusable UE material library from the checked-in art charter.

Only saves /Game/ThreeHearths/Materials/AincradStyle assets; never changes worlds,
source meshes, UVs, budgets or actor state. Re-running rebuilds the same library.
"""
import json
from pathlib import Path
import unreal as ue

ROOT=Path(__file__).resolve().parents[1]
ART=ROOT/'Art/AincradStyle'
DEST='/Game/ThreeHearths/Materials/AincradStyle'
data=json.loads((ART/'style.json').read_text(encoding='utf-8'))
code=(ART/'surface.hlsl').read_text(encoding='utf-8')
edit=ue.MaterialEditingLibrary
assets=ue.AssetToolsHelpers.get_asset_tools()

def asset(name, cls, factory):
    obj=ue.load_asset(DEST+'/'+name)
    if obj is None: obj=assets.create_asset(name,DEST,cls,factory)
    if not isinstance(obj,cls): raise RuntimeError('Wrong asset class: '+name)
    return obj

material=asset('M_AincradSurface',ue.Material,ue.MaterialFactoryNew())
edit.delete_all_material_expressions(material)
material.set_editor_property('used_with_instanced_static_meshes',True)
material.set_editor_property('two_sided',False)
custom=edit.create_material_expression(material,ue.MaterialExpressionCustom,0,0)
custom.set_editor_property('code',code)
custom.set_editor_property('output_type',ue.CustomMaterialOutputType.CMOT_FLOAT3)
inputs=[]
for name in ('HearthWorldCm','HearthFaceNormal','HearthUV','HearthAlbedo','HearthKind'):
    item=ue.CustomInput()
    item.set_editor_property('input_name',name)
    inputs.append(item)
custom.set_editor_property('inputs',inputs)
for name,cls,pin,y in [('HearthWorldCm',ue.MaterialExpressionWorldPosition,'',0),
                      ('HearthFaceNormal',ue.MaterialExpressionVertexNormalWS,'',120),
                      ('HearthUV',ue.MaterialExpressionTextureCoordinate,'',240)]:
    node=edit.create_material_expression(material,cls,-500,y)
    edit.connect_material_expressions(node,pin,custom,name)
albedo=edit.create_material_expression(material,ue.MaterialExpressionVectorParameter,-500,360)
albedo.set_editor_property('parameter_name','HearthAlbedo')
albedo.set_editor_property('default_value',ue.LinearColor(0.4,0.3,0.2,1))
edit.connect_material_expressions(albedo,'RGB',custom,'HearthAlbedo')
kind=edit.create_material_expression(material,ue.MaterialExpressionScalarParameter,-500,480)
kind.set_editor_property('parameter_name','HearthKind')
kind.set_editor_property('default_value',2.0)
edit.connect_material_expressions(kind,'',custom,'HearthKind')
rough=edit.create_material_expression(material,ue.MaterialExpressionScalarParameter,0,200)
rough.set_editor_property('parameter_name','HearthRoughness')
rough.set_editor_property('default_value',0.88)
spec=edit.create_material_expression(material,ue.MaterialExpressionConstant,0,300)
spec.set_editor_property('r',0.22)
edit.connect_material_property(custom,'',ue.MaterialProperty.MP_BASE_COLOR)
edit.connect_material_property(rough,'',ue.MaterialProperty.MP_ROUGHNESS)
edit.connect_material_property(spec,'',ue.MaterialProperty.MP_SPECULAR)
fill=edit.create_material_expression(material,ue.MaterialExpressionScalarParameter,-200,550)
fill.set_editor_property('parameter_name','HearthAmbientFill')
fill.set_editor_property('default_value',0.14)
minimum=edit.create_material_expression(material,ue.MaterialExpressionMultiply,250,500)
edit.connect_material_expressions(custom,'',minimum,'A')
edit.connect_material_expressions(fill,'',minimum,'B')
edit.connect_material_property(minimum,'',ue.MaterialProperty.MP_EMISSIVE_COLOR)
edit.layout_material_expressions(material)
edit.recompile_material(material)
if not ue.EditorAssetLibrary.save_loaded_asset(material,False):raise RuntimeError('Master save failed')
report={'style_id':data['id'],'master':material.get_path_name(),'materials':[],'world_mutation':False,'uv_mutation':False}
for row in data['materials']:
    mi=asset('MI_'+row['id'],ue.MaterialInstanceConstant,ue.MaterialInstanceConstantFactoryNew())
    edit.set_material_instance_parent(mi,material)
    edit.set_material_instance_vector_parameter_value(mi,'HearthAlbedo',ue.LinearColor(*row['color'],1))
    edit.set_material_instance_scalar_parameter_value(mi,'HearthKind',row['kind'])
    edit.set_material_instance_scalar_parameter_value(mi,'HearthRoughness',row['roughness'])
    edit.set_material_instance_scalar_parameter_value(mi,'HearthAmbientFill',0.02 if row['id'] in ('Grass','Paving') else 0.14)
    edit.update_material_instance(mi)
    if not ue.EditorAssetLibrary.save_loaded_asset(mi,False):raise RuntimeError('Instance save failed: '+row['id'])
    report['materials'].append({'id':row['id'],'asset':mi.get_path_name(),'kind':row['kind'],'roughness':row['roughness'],'ambient_fill':0.02 if row['id'] in ('Grass','Paving') else 0.14})
if edit.get_material_property_input_node(material,ue.MaterialProperty.MP_WORLD_POSITION_OFFSET) is not None:
    raise RuntimeError('Displacement would violate the style-only contract')
report['status']='assets_saved; runtime_shader_and_visual_validation_required'
(ART/'UE_Material_Report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
ue.log('AINCRAD_MATERIAL_LIBRARY_SAVED '+str(len(report['materials'])))
