"""Native Black Iron Palace surface, following the coordinator's actual review."""
import unreal as ue
root='/Game/ThreeHearths/Materials/AincradLevel0'
name='M_BlackIron'
assets=ue.AssetToolsHelpers.get_asset_tools()
m=ue.load_asset(root+'/'+name)
if m is None:m=assets.create_asset(name,root,ue.Material,ue.MaterialFactoryNew())
assert m
edit=ue.MaterialEditingLibrary
edit.delete_all_material_expressions(m)
m.set_editor_property('used_with_instanced_static_meshes',True)
base=edit.create_material_expression(m,ue.MaterialExpressionConstant3Vector,-420,0)
base.set_editor_property('constant',ue.LinearColor(.025,.035,.048,1))
edit.connect_material_property(base,'',ue.MaterialProperty.MP_BASE_COLOR)
for prop,value,y in [(ue.MaterialProperty.MP_METALLIC,.8,100),(ue.MaterialProperty.MP_ROUGHNESS,.38,220)]:
    node=edit.create_material_expression(m,ue.MaterialExpressionConstant,-420,y)
    node.set_editor_property('r',value)
    edit.connect_material_property(node,'',prop)
fill=edit.create_material_expression(m,ue.MaterialExpressionConstant3Vector,-420,340)
fill.set_editor_property('constant',ue.LinearColor(.006,.009,.014,1))
edit.connect_material_property(fill,'',ue.MaterialProperty.MP_EMISSIVE_COLOR)
edit.recompile_material(m)
assert ue.EditorAssetLibrary.save_loaded_asset(m)
ue.log('LEVEL0_BLACK_IRON_SAVED '+root+'/'+name)
