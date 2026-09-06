"""Independent native polished-cottage import/map and read-only reference bones."""
from pathlib import Path
import hashlib
import json
import sys
import unreal as ue
ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'Art/HearthCottage'
sys.path.insert(0,str(Path(__file__).resolve().parent))
from import_village_kit import import_one
from import_resident_kit import verify_native,source_details
SOURCE=OUT/'HearthCottage_SharedUV_Polished.glb'
OLD='/Game/ThreeHearths/Generated/HearthCottage/SM_HearthCottage_SharedUV.SM_HearthCottage_SharedUV'
MAP='/Game/ThreeHearths/Maps/L_HearthCottagePolished_Compare'
TEMPLATE='/Game/ThreeHearths/Maps/L_ThreeHearthsVillage'
REPORT=OUT/'Polished_UE_Import.json'
MAINFILE=ROOT/'Content/ThreeHearths/Maps/L_ThreeHearthsVillage.umap'

def dump(path,data):path.write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def old_audit():
    sm=ue.load_asset(OLD);assert isinstance(sm,ue.StaticMesh)
    expected=source_details(OUT/'HearthCottage_SharedUV.glb');b=sm.get_bounds()
    origin=[b.origin.x,b.origin.y,b.origin.z];extent=[b.box_extent.x,b.box_extent.y,b.box_extent.z]
    assert max(abs(a-b) for a,b in zip(origin,expected['origin_cm']))<.15
    assert max(abs(a-b) for a,b in zip(extent,expected['extent_cm']))<.15
    total=sm.get_num_nanite_triangles() or sm.get_num_triangles(0);assert total==16788
    materials=[]
    for slot in sm.get_editor_property('static_materials'):
        mat=slot.get_editor_property('material_interface')
        row={'path':mat.get_path_name(),'class':mat.get_class().get_name()}
        if 'Roof' in mat.get_name():
            parameters=[str(n) for n in ue.MaterialEditingLibrary.get_scalar_parameter_names(mat)]
            row['scalar_parameters']={n:ue.MaterialEditingLibrary.get_material_default_scalar_parameter_value(mat,n) for n in parameters}
            row['source_roughness']=.78;row['source_metallic']=0
            row['native_factor_query']='Legacy material function constants; no named scalar parameters. Original source and native artifact hashes retained.'
        materials.append(row)
    assert len(materials)==19
    return {'mesh':OLD,'triangles':total,'uv_channels':sm.get_num_tex_coords(0),'bounds_origin_cm':origin,'bounds_extent_cm':extent,'materials':materials}

def export_reference():
    path=ROOT/'Saved/ThreeHearths/ResidentKitReference';path.mkdir(parents=True,exist_ok=True)
    asset=ue.load_asset('/Game/Characters/Meshes/SKM_Villager');component=ue.SkeletalMeshComponent()
    component.set_skeletal_mesh_asset(asset)
    locals_={};globals_={}
    def local(name):
        if name not in locals_:locals_[name]=component.get_ref_pose_transform(component.get_bone_index(name))
        return locals_[name]
    def global_(name):
        if name not in globals_:
            parent=str(component.get_parent_bone(name));value=local(name)
            globals_[name]=value*global_(parent) if parent not in ('None','') else value
        return globals_[name]
    def plain(t):
        at=t.translation;q=t.rotation;scale=t.scale3d;rot=q.rotator()
        origin=t.transform_location(ue.Vector(0,0,0))
        axes=[t.transform_location(v)-origin for v in [ue.Vector(1,0,0),ue.Vector(0,1,0),ue.Vector(0,0,1)]]
        return {'translation_cm':[at.x,at.y,at.z],'rotation_quaternion_xyzw':[q.x,q.y,q.z,q.w],
           'rotation_degrees':{'pitch':rot.pitch,'yaw':rot.yaw,'roll':rot.roll},'scale':[scale.x,scale.y,scale.z],
           'matrix_column_vector_rows':[[axes[0].x,axes[1].x,axes[2].x,origin.x],
             [axes[0].y,axes[1].y,axes[2].y,origin.y],[axes[0].z,axes[1].z,axes[2].z,origin.z],[0,0,0,1]]}
    names=['head','neck','spine_02','pelvis'];bones={}
    for name in names:
        assert component.get_bone_index(name)>=0
        bones[name]={'parent':str(component.get_parent_bone(name)),'parent_space':plain(local(name)),'mesh_space':plain(global_(name))}
    report={'status':'passed','asset':asset.get_path_name(),'engine':ue.SystemLibrary.get_engine_version(),
        'source':'USkinnedMeshComponent.GetRefPoseTransform -> FReferenceSkeleton.GetRefBonePose; accumulated through parents',
        'reference_skeleton_only':True,'animation_pose_used':False,'component_registered_or_animated':False,
        'asset_modified':False,'units':'UE centimetres','bone_count':component.get_num_bones(),'bones':bones,
        'matrix_convention':'Column-vector rows [basis X, basis Y, basis Z, translation]. UE TransformLocation verified basis; not raw UE row-vector FMatrix memory.'}
    dump(path/'UE_ReferenceBoneTransforms.json',report)
    return report

def export_materials():
    asset=ue.load_asset('/Game/Characters/Meshes/SKM_Villager')
    slots=[];materials={}
    for i,slot in enumerate(asset.get_editor_property('materials')):
        mat=slot.get_editor_property('material_interface')
        slots.append({'index':i,'slot_name':str(slot.get_editor_property('material_slot_name')),
           'material':mat.get_path_name() if mat else None})
        if mat:materials[mat.get_path_name()]=mat
    for name in ('MI_WoodCut','MI_Farming'):
        mat=ue.load_asset('/Game/Characters/Materials/'+name)
        if mat:materials[mat.get_path_name()]=mat
    results=[]
    for mat in materials.values():
        row={'material':mat.get_path_name(),'class':mat.get_class().get_name(),'vectors':{}}
        if isinstance(mat,ue.MaterialInstanceConstant):row['parent']=mat.get_editor_property('parent').get_path_name()
        for name in ue.MaterialEditingLibrary.get_vector_parameter_names(mat):
            color=(ue.MaterialEditingLibrary.get_material_instance_vector_parameter_value(mat,name)
              if isinstance(mat,ue.MaterialInstanceConstant) else ue.MaterialEditingLibrary.get_material_default_vector_parameter_value(mat,name))
            row['vectors'][str(name)]={'effective_default_linear_rgba':[color.r,color.g,color.b,color.a]}
        results.append(row)
    path=ROOT/'Saved/ThreeHearths/ResidentKitReference/ue-material-parameters.json'
    dump(path,{'status':'passed','mesh':asset.get_path_name(),'slots':slots,'materials':results,
       'read_only':True,'material_instances_modified':False,'note':'Vector names and effective saved defaults only. Confirm each parameter skin/clothing region against parent material masks before tinting.'})
    return {'status':'passed','materials':len(results)}

def build():
    assert 'HearthNoWorldPersistence' in ue.SystemLibrary.get_command_line()
    original=sha(MAINFILE);old_files={str(p):sha(p) for p in (ROOT/'Content/ThreeHearths/Generated/HearthCottage').rglob('*.uasset')}
    previous={}
    if REPORT.exists():
        prior=json.loads(REPORT.read_text());previous={r['id']:r for r in prior.get('assets',[])}
    row=import_one(SOURCE,'HearthCottage_SharedUV_Polished',previous,destination_root='/Game/ThreeHearths/Generated/HearthCottagePolished')
    report={'status':'running','assets':[row],'main_map_sha256_before':original,'old_asset_hashes':old_files,
       'engine':ue.SystemLibrary.get_engine_version(),'api_enabled':False,'world_persistence_disabled':True}
    dump(REPORT,report);verify_native(row,SOURCE);report['old_native']=old_audit()
    assert not ue.EditorAssetLibrary.does_asset_exist(MAP),'Dedicated map already exists; refusing implicit overwrite'
    assert ue.EditorLevelLibrary.new_level_from_template(MAP,TEMPLATE)
    actors=ue.get_editor_subsystem(ue.EditorActorSubsystem)
    for actor in actors.get_all_level_actors():
        if isinstance(actor,ue.HearthVillage):assert actors.destroy_actor(actor)
    cottage=next(a for a in actors.get_all_level_actors() if a.get_actor_label()=='Hearth Cottage - Shared UV')
    cottage.set_actor_label('Hearth Cottage | Polished comparison');cottage.set_folder_path('PolishedComparison')
    assert cottage.static_mesh_component.static_mesh.get_path_name()==OLD
    configure('polished','house')
    assert ue.EditorLevelLibrary.save_current_level()
    try:report['reference_bones']=export_reference()['status']
    except Exception as exc:report['reference_bones_error']=repr(exc)
    try:report['reference_materials']=export_materials()['status']
    except Exception as exc:report['reference_materials_error']=repr(exc)
    assert sha(MAINFILE)==original
    assert all(sha(Path(p))==digest for p,digest in old_files.items())
    report.update(status='passed',map=MAP,main_map_and_original_assets_unchanged=True)
    dump(REPORT,report);ue.log('POLISHED_UE_IMPORT_MAP_PASSED')

def configure(state,view='house'):
    assert state in ('original','polished')
    actors=ue.get_editor_subsystem(ue.EditorActorSubsystem)
    cottage=next(a for a in actors.get_all_level_actors() if a.get_actor_label()=='Hearth Cottage | Polished comparison')
    row=json.loads(REPORT.read_text())['assets'][0]
    comp=cottage.static_mesh_component;comp.set_editor_property('override_materials',[])
    comp.set_static_mesh(ue.load_asset(OLD if state=='original' else row['mesh']))
    assert len(comp.get_editor_property('override_materials'))==0
    comp.set_mobility(ue.ComponentMobility.MOVABLE)
    position=cottage.get_actor_location()
    target=position+ue.Vector(0,0,230 if view=='roof' else 175)
    at=position+ue.Vector(-670,-760,590 if view=='roof' else 590)
    rotation=ue.MathLibrary.find_look_at_rotation(at,target)
    capture=next((a for a in actors.get_all_level_actors() if a.get_actor_label()=='Polished | Comparison camera'),None)
    if capture is None:capture=actors.spawn_actor_from_class(ue.CameraActor,at)
    capture.set_actor_label('Polished | Comparison camera');capture.set_folder_path('PolishedComparison')
    capture.set_actor_location(at,False,False);capture.set_actor_rotation(rotation,False)
    component=capture.get_component_by_class(ue.CameraComponent);component.set_field_of_view(37 if view=='roof' else 52)
    pp=component.get_editor_property('post_process_settings');pp.set_editor_property('override_auto_exposure_bias',True)
    pp.set_editor_property('auto_exposure_bias',0.0);component.set_editor_property('post_process_settings',pp)
    component.set_editor_property('post_process_blend_weight',1.0)
    ue.get_editor_subsystem(ue.UnrealEditorSubsystem).set_level_viewport_camera_info(at,rotation)
    return capture

def capture(state,view='house'):
    assert ue.EditorLevelLibrary.load_level(MAP)
    cam=configure(state,view);ue.EditorLevelLibrary.save_current_level()
    return ue.AutomationLibrary.take_high_res_screenshot(1920,1080,str(OUT/('Polished_UE_'+view+'_'+state+'.png')),camera=cam,delay=4.0)

if __name__=='__main__':build()
