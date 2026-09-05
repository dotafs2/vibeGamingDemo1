"""Independent native art maps for ResidentKit and HomeLifeKit, using island lights."""
import json
from pathlib import Path
import sys
import unreal as ue
ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(Path(__file__).resolve().parent))
import importlib
import import_resident_kit as native_import
importlib.reload(native_import)
verify_native=native_import.verify_native
TEMPLATE='/Game/ThreeHearths/Maps/L_ThreeHearthsVillage'
TARGETS={k:'/Game/ThreeHearths/Maps/L_'+k+'_Showcase' for k in ('ResidentKit','HomeLifeKit')}

def camera(kit,view='overview'):
    if kit=='ResidentKit':at,target=(-970,-1510,1100),(50,-340,105)
    elif view=='cabin':at,target=(-1220,1340,740),(-640,740,105)
    elif view=='meal':at,target=(20,-20,850),(640,700,85)
    else:at,target=(-2050,-2540,2040),(40,110,60)
    at,target=ue.Vector(*at),ue.Vector(*target)
    rotation=ue.MathLibrary.find_look_at_rotation(at,target)
    ue.get_editor_subsystem(ue.UnrealEditorSubsystem).set_level_viewport_camera_info(at,rotation)
    return {'position_cm':[at.x,at.y,at.z],'rotation':{'pitch':rotation.pitch,'yaw':rotation.yaw,'roll':rotation.roll}}

def build(kit):
    target=TARGETS[kit];path=ROOT/'Art'/kit
    imported=json.loads((path/'UE_Import_Report.json').read_text(encoding='utf-8'));assert imported['status']=='passed'
    rows={r['id']:r for r in imported['assets']}
    specs=json.loads((path/'module-specs.json').read_text(encoding='utf-8'))
    assert not ue.EditorAssetLibrary.does_asset_exist(target),'Existing dedicated map needs an explicit rebuild review'
    assert ue.EditorLevelLibrary.new_level_from_template(target,TEMPLATE)
    actors=ue.get_editor_subsystem(ue.EditorActorSubsystem)
    for actor in actors.get_all_level_actors():
        if isinstance(actor,ue.HearthVillage) or actor.get_actor_label()=='Hearth Cottage - Shared UV':assert actors.destroy_actor(actor)
    assert not any(isinstance(a,ue.HearthVillage) for a in actors.get_all_level_actors())
    placed=[]
    neutral=ue.load_asset('/Game/ThreeHearths/Generated/SocietyKit/castle_wall_stone_2m/castle_wall_stone_2m.castle_wall_stone_2m')
    material=neutral.get_editor_property('static_materials')[0].get_editor_property('material_interface')
    def floor(at,scale,label):
        actor=actors.spawn_actor_from_class(ue.StaticMeshActor,ue.Vector(*at),ue.Rotator(pitch=0,yaw=0,roll=0))
        actor.set_actor_label(kit+' | '+label);actor.set_folder_path(kit+'/Display')
        comp=actor.static_mesh_component;comp.set_static_mesh(ue.load_asset('/Engine/BasicShapes/Cube'))
        comp.set_mobility(ue.ComponentMobility.MOVABLE);comp.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION)
        comp.set_material(0,material);actor.set_actor_scale3d(ue.Vector(*scale))
    def text(label,at,size=14):
        actor=actors.spawn_actor_from_class(ue.TextRenderActor,ue.Vector(*at),ue.Rotator(pitch=90,yaw=180,roll=0))
        actor.set_actor_label(kit+' | Label '+label);actor.set_folder_path(kit+'/Labels')
        component=actor.get_component_by_class(ue.TextRenderComponent)
        component.set_text(label);component.set_world_size(size);component.set_text_render_color(ue.Color(45,56,58,255))
        component.set_horizontal_alignment(ue.HorizTextAligment.EHTA_CENTER)
    def place(mid,xy,display_scale=1):
        row=rows[mid];verify_native(row,ROOT/row['source'])
        mesh=ue.load_asset(row['mesh']);b=mesh.get_bounds()
        at=[xy[0],xy[1],12-(b.origin.z-b.box_extent.z)*display_scale]
        actor=actors.spawn_actor_from_class(ue.StaticMeshActor,ue.Vector(*at),ue.Rotator(pitch=0,yaw=90,roll=0))
        actor.set_actor_label(kit+' | '+mid);actor.set_folder_path(kit+('/Examples' if mid.startswith('example__') else '/Modules'))
        comp=actor.static_mesh_component;comp.set_static_mesh(mesh);comp.set_mobility(ue.ComponentMobility.MOVABLE)
        comp.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION);actor.set_actor_scale3d(ue.Vector(display_scale,display_scale,display_scale))
        placed.append({'id':mid,'mesh':row['mesh'],'actor_position_cm':at,'yaw_degrees':90,
            'display_scale':display_scale,'imported_asset_origin_changed':False,'native_origin_cm':row['bounds_origin_cm']})
    if kit=='ResidentKit':
        floor((30,-345,0),(12.5,10.5,.24),'Review floor')
        for i,m in enumerate(specs['modules']):
            x=-345+(i%4)*230;y=-40-(i//4)*225
            place(m['id'],(x,y));text(m['id'].replace('_',' '),(x,y-83,13),11)
        reference=actors.spawn_actor_from_class(ue.SkeletalMeshActor,ue.Vector(550,-45,12),ue.Rotator(pitch=0,yaw=90,roll=0))
        reference.set_actor_label('ResidentKit | Existing Cropout body reference');reference.set_folder_path(kit+'/Reference')
        component=reference.get_component_by_class(ue.SkeletalMeshComponent)
        component.set_skeletal_mesh_asset(ue.load_asset('/Game/Characters/Meshes/SKM_Villager'))
        component.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION)
        text('Cropout body reference',(525,-180,13),13)
    else:
        floor((0,70,0),(24,24,.24),'Review floor')
        place('example__cabin_living_4x4m',(-640,740));place('example__common_meal_10_seats',(640,740))
        text('4m living corner',(-640,450,13),20);text('10 seat common meal',(640,140,13),20)
        for i,m in enumerate(specs['modules']):
            x=-525+(i%4)*350;y=-180-(i//4)*350
            place(m['id'],(x,y));text(m['id'].replace('_',' '),(x,y-145,13),14)
    camera_info=camera(kit)
    assert ue.EditorLevelLibrary.save_current_level()
    output={'status':'passed','map':target,'template_read_only':TEMPLATE,'lighting':'copied village island lighting',
        'native_assets_verified':len(placed),'origin_and_dimensions_verified':True,'pbr_and_uv_verified':True,
        'skeletal_binding_and_animation_verified':False,'collision_and_navigation_enabled':False,
        'camera':camera_info,'placements':placed,'scope':'Dedicated native art review map; no world simulation actor'}
    (path/'UE_Showcase_Report.json').write_text(json.dumps(output,indent=2)+'\n',encoding='utf-8')
    ue.log('['+kit+'Showcase] saved '+target)

def request_capture(kit,view='overview'):
    assert kit in TARGETS and 'HearthNoWorldPersistence' in ue.SystemLibrary.get_command_line()
    assert ue.EditorLevelLibrary.load_level(TARGETS[kit]);info=camera(kit,view)
    actors=ue.get_editor_subsystem(ue.EditorActorSubsystem)
    capture=next((a for a in actors.get_all_level_actors() if a.get_actor_label()==kit+' | Capture camera'),None)
    if capture is None:capture=actors.spawn_actor_from_class(ue.CameraActor,ue.Vector(*info['position_cm']))
    capture.set_actor_label(kit+' | Capture camera');capture.set_folder_path(kit+'/Display')
    capture.set_actor_location(ue.Vector(*info['position_cm']),False,False)
    capture.set_actor_rotation(ue.Rotator(**info['rotation']),False)
    component=capture.get_component_by_class(ue.CameraComponent)
    component.set_field_of_view(52 if view=='cabin' else 55 if view=='meal' else 58)
    pp=component.get_editor_property('post_process_settings')
    pp.set_editor_property('override_auto_exposure_bias',True);pp.set_editor_property('auto_exposure_bias',0.0)
    component.set_editor_property('post_process_settings',pp);component.set_editor_property('post_process_blend_weight',1.0)
    assert ue.EditorLevelLibrary.save_current_level()
    suffix='' if view=='overview' else '_'+view
    filename=ROOT/'Art'/kit/'previews'/(kit+suffix+'_InUE.png')
    return ue.AutomationLibrary.take_high_res_screenshot(1920,1080,str(filename),camera=capture,delay=4.0)

def audit_imports():
    output=[]
    for kit in TARGETS:
        path=ROOT/'Art'/kit/'UE_Import_Report.json';report=json.loads(path.read_text(encoding='utf-8'))
        assert report['status']=='passed'
        for row in report['assets']:verify_native(row,ROOT/row['source'])
        path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        output.append({'kit':kit,'source_triangles':sum(r['source_triangles'] for r in report['assets']),
           'nanite_triangles':sum(r['native_nanite_triangles'] for r in report['assets']),
           'fallback_triangles':sum(r['native_lod0_fallback_triangles'] for r in report['assets'])})
    return output

if __name__=='__main__':
    build('ResidentKit');build('HomeLifeKit')
