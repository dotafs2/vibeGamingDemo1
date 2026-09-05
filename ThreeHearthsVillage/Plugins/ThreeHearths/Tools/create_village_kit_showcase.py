"""Create a separate art review map with the village's existing island lighting.

Run in an editor commandlet after import_village_kit.py. Never edits the template.
"""
import json
from pathlib import Path
import unreal as ue

ROOT=Path(__file__).resolve().parents[3]
KIT=ROOT/'Art/VillageKit'
TARGET='/Game/ThreeHearths/Maps/L_VillageKit_Showcase'
TEMPLATE='/Game/ThreeHearths/Maps/L_ThreeHearthsVillage'

def main(rebuild=False):
    report=json.loads((KIT/'UE_Import_Report.json').read_text(encoding='utf-8'))
    assert report['status']=='passed'
    assets={row['id']:row['mesh'] for row in report['assets']}
    _,switches,_=ue.SystemLibrary.parse_command_line(ue.SystemLibrary.get_command_line())
    if ue.EditorAssetLibrary.does_asset_exist(TARGET):
        if not rebuild and 'VillageKitRebuildShowcase' not in switches:
            raise RuntimeError('Review the existing showcase; pass -VillageKitRebuildShowcase to replace its kit actors')
        if not ue.EditorLevelLibrary.load_level(TARGET):
            raise RuntimeError('Cannot load existing showcase')
    elif not ue.EditorLevelLibrary.new_level_from_template(TARGET,TEMPLATE):
        raise RuntimeError('Could not create independent showcase map')
    actors=ue.get_editor_subsystem(ue.EditorActorSubsystem)
    for actor in actors.get_all_level_actors():
        label=actor.get_actor_label()
        if isinstance(actor,ue.HearthVillage) or label in ('Hearth Cottage - Shared UV','Cropout | Original material reference') or label.startswith('VillageKit | '):
            if not actors.destroy_actor(actor): raise RuntimeError('Cannot clear template demo actors')
    placed=[]
    # One neutral review floor keeps every example grounded across the island's shoreline.
    floor=actors.spawn_actor_from_class(ue.StaticMeshActor,ue.Vector(350,-400,6))
    floor.set_actor_label('VillageKit | Review floor')
    floor.set_folder_path('VillageKit/Display')
    floor.static_mesh_component.set_static_mesh(ue.load_asset('/Engine/BasicShapes/Cube'))
    floor.static_mesh_component.set_mobility(ue.ComponentMobility.MOVABLE)
    floor.static_mesh_component.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION)
    stone=ue.load_asset(assets['foundation_stone_2m']).get_editor_property('static_materials')[0].get_editor_property('material_interface')
    floor.static_mesh_component.set_material(0,stone)
    floor.set_actor_scale3d(ue.Vector(43,36,0.12))
    def place(asset_id,location,yaw=0):
        mesh=ue.load_asset(assets[asset_id])
        if mesh is None: raise RuntimeError('Missing '+asset_id)
        actor=actors.spawn_actor_from_class(ue.StaticMeshActor,ue.Vector(*location),ue.Rotator(pitch=0,yaw=yaw,roll=0))
        if actor is None: raise RuntimeError('Cannot place '+asset_id)
        actor.static_mesh_component.set_static_mesh(mesh)
        actor.set_actor_label('VillageKit | '+asset_id)
        actor.set_folder_path('VillageKit/Examples' if asset_id.startswith('example__') else 'VillageKit/Modules')
        component=actor.static_mesh_component
        component.set_mobility(ue.ComponentMobility.MOVABLE)
        component.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION)
        placed.append({'id':asset_id,'location_cm':location,'yaw_degrees':yaw})
    for asset_id,x in [('example__cottage_terracotta',-1200),('example__longhouse_slateblue',0),('example__townhouse_terracotta',1200)]:
        place(asset_id,[x,1000,36],90)
    reference=actors.spawn_actor_from_class(ue.StaticMeshActor,ue.Vector(2200,1000,22))
    if reference is None: raise RuntimeError('Cannot place reference')
    reference.static_mesh_component.set_static_mesh(ue.load_asset('/Game/Environment/Meshes/Building/SM_House_04'))
    reference.set_actor_label('Cropout | Original material reference')
    reference.set_folder_path('VillageKit/Reference')
    reference.static_mesh_component.set_mobility(ue.ComponentMobility.MOVABLE)
    catalog=json.loads((KIT/'catalog.json').read_text(encoding='utf-8'))
    for index,module in enumerate(catalog['modules']):
        place(module['id'],[-1100+(index%8)*420,-50-(index//8)*420,36 if module['category']=='foundation' else 12],90)
    ue.get_editor_subsystem(ue.UnrealEditorSubsystem).set_level_viewport_camera_info(
        ue.Vector(-2400,-3500,2600),ue.Rotator(pitch=-31,yaw=55,roll=0))
    if not ue.EditorLevelLibrary.save_current_level():
        raise RuntimeError('Cannot save showcase map')
    output={'status':'saved','map':TARGET,'template':TEMPLATE,
        'lighting':'copied from current village template','placements':placed,
        'reference_mesh':'/Game/Environment/Meshes/Building/SM_House_04',
        'purpose':'art review; collision, NPC navigation and construction are not enabled'}
    (KIT/'UE_Showcase_Report.json').write_text(json.dumps(output,indent=2)+'\n',encoding='utf-8')
    ue.log('[VillageKitShowcase] saved '+TARGET)

if __name__=='__main__': main()
