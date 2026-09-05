"""Create and validate a dedicated SocietyKit art review map, preserving the world map."""
import hashlib
import json
from pathlib import Path
import unreal as ue

ROOT=Path(__file__).resolve().parents[3]
KIT=ROOT/'Art/SocietyKit'
TARGET='/Game/ThreeHearths/Maps/L_SocietyKit_Showcase'
TEMPLATE='/Game/ThreeHearths/Maps/L_ThreeHearthsVillage'

def main():
    report=json.loads((KIT/'UE_Import_Report.json').read_text(encoding='utf-8'))
    specs=json.loads((KIT/'module-specs.json').read_text(encoding='utf-8'))
    source_report=json.loads((KIT/'model-report.json').read_text(encoding='utf-8'))
    assert report['status']=='passed' and len(report['assets'])==34
    assert not ue.EditorAssetLibrary.does_asset_exist(TARGET),'Existing showcase must be reviewed before any rebuild'
    assert ue.EditorLevelLibrary.new_level_from_template(TARGET,TEMPLATE)
    assets={row['id']:row for row in report['assets']}
    dimensions={m['id']:[hi-lo for lo,hi in zip(m['bounds_min_m'],m['bounds_max_m'])] for m in source_report['modules']}
    actors=ue.get_editor_subsystem(ue.EditorActorSubsystem)
    for actor in actors.get_all_level_actors():
        if isinstance(actor,ue.HearthVillage) or actor.get_actor_label()=='Hearth Cottage - Shared UV':
            assert actors.destroy_actor(actor)
    placed=[]
    def place(asset_id,location,scale=1):
        row=assets[asset_id]
        assert hashlib.sha256((ROOT/row['source']).read_bytes()).hexdigest()==row['source_sha256']
        mesh=ue.load_asset(row['mesh']);assert isinstance(mesh,ue.StaticMesh)
        assert all(slot.get_editor_property('material_interface') for slot in mesh.get_editor_property('static_materials'))
        bounds=mesh.get_bounds()
        location=list(location)
        location[2]=6-(bounds.origin.z-bounds.box_extent.z)*scale
        if asset_id in dimensions:
            extent=mesh.get_bounds().box_extent
            native=sorted([extent.x*2,extent.y*2,extent.z*2]);source=sorted(x*100 for x in dimensions[asset_id])
            assert all(abs(a-b)<0.5 for a,b in zip(native,source)),(asset_id,native,source)
        actor=actors.spawn_actor_from_class(ue.StaticMeshActor,ue.Vector(*location),ue.Rotator(pitch=0,yaw=90,roll=0))
        assert actor
        actor.set_actor_label('SocietyKit | '+asset_id)
        actor.set_folder_path('SocietyKit/Examples' if asset_id.startswith('example__') else 'SocietyKit/Modules')
        component=actor.static_mesh_component
        component.set_static_mesh(mesh);component.set_mobility(ue.ComponentMobility.MOVABLE)
        component.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION);actor.set_actor_scale3d(ue.Vector(scale,scale,scale))
        rot=actor.get_actor_rotation();assert abs(rot.pitch)<0.01 and abs(rot.roll)<0.01
        placed.append({'id':asset_id,'position_cm':location,'yaw_degrees':90,'display_scale':scale})
    floor=actors.spawn_actor_from_class(ue.StaticMeshActor,ue.Vector(650,-150,0))
    floor.set_actor_label('SocietyKit | Review floor');floor.set_folder_path('SocietyKit/Display')
    floor.static_mesh_component.set_static_mesh(ue.load_asset('/Engine/BasicShapes/Cube'))
    floor.static_mesh_component.set_mobility(ue.ComponentMobility.MOVABLE)
    floor.static_mesh_component.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION)
    stone=ue.load_asset(assets['castle_wall_stone_2m']['mesh']).get_editor_property('static_materials')[0].get_editor_property('material_interface')
    floor.static_mesh_component.set_material(0,stone);floor.set_actor_scale3d(ue.Vector(65,63,.12))
    for asset_id,location in [('example__kings_gate_courtyard',[-1000,1350,6]),('example__guild_market_yard',[1300,1350,6])]:
        place(asset_id,location)
    for i,m in enumerate(specs['modules']):
        # Enlarge only the clearly marked review display; runtime imports remain true scale.
        scale=3 if m['category'] in ('wearable','tool') else 1
        place(m['id'],[-1700+(i%8)*620,-450-(i//8)*640,65 if m['category']=='wearable' else 6],scale)
    reference=actors.spawn_actor_from_class(ue.StaticMeshActor,ue.Vector(3200,1350,6))
    reference.static_mesh_component.set_static_mesh(ue.load_asset('/Game/Environment/Meshes/Building/SM_House_04'))
    reference.set_actor_label('Cropout | Original material reference')
    reference.set_folder_path('SocietyKit/Reference')
    ue.get_editor_subsystem(ue.UnrealEditorSubsystem).set_level_viewport_camera_info(ue.Vector(-4200,-5800,4800),ue.Rotator(pitch=-36,yaw=52,roll=0))
    assert ue.EditorLevelLibrary.save_current_level()
    output={'status':'passed','map':TARGET,'template':TEMPLATE,'native_meshes_verified':34,
        'source_hashes_verified':34,'module_dimensions_verified':32,'upright_placements_verified':34,
        'lighting':'same village island lights','placements':placed,
        'scope':'Art review; NPC jobs, ownership, collisions and navigation are separate runtime work'}
    (KIT/'UE_Showcase_Report.json').write_text(json.dumps(output,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    ue.log('[SocietyKitShowcase] '+TARGET+' saved and verified')

if __name__=='__main__':main()
