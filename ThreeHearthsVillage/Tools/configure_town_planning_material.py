"""UE editor script: enable instanced drawing on the existing village tint material.

Run with UnrealEditor-Cmd <project> -run=pythonscript -script=<this file>.
Only the named material is saved; no world, inventory or budget state is touched.
"""
import unreal

path = '/Game/ThreeHearths/Materials/M_VillageTint'
material = unreal.load_asset(path)
if not material:
    raise RuntimeError('Missing existing village tint material')
material.set_editor_property('used_with_instanced_static_meshes', True)
unreal.MaterialEditingLibrary.recompile_material(material)
if not material.get_editor_property('used_with_instanced_static_meshes'):
    raise RuntimeError('Instanced material usage did not stick')
if not unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False):
    raise RuntimeError('Could not save the planning material usage')
unreal.log('TOWN_PLANNING_MATERIAL_READY ' + path)
