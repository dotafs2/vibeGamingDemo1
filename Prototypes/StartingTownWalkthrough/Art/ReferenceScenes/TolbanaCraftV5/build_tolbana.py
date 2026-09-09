"""Upgrade Tolbana in its own world region, retaining archived HQ distant context."""
import bpy,sys,json,hashlib,math,os
from pathlib import Path
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE));sys.path.insert(0,str(HERE.parent));sys.path.insert(0,str(HERE.parent/'MarketCraftV5'))
import architecture as a
import build_reference_scenes_hq as hq
import build_reference_scenes as base
import residences,landscape,garden_details
G=hq.G;PROJECT=HERE.parents[2];EXPORT=PROJECT/'assets/reference_scenes'
base.PALETTE.update({'tile_red':'B77F70','bread':'BD8745','bread_light':'ECD19B','pottery':'AA7861','canvas_blue':'75AEB7','canvas_rust':'A96C4C'})

def surfaces():
    textures={}
    for kind in ['stone','plaster','wood']:
        path=HERE.parent/'MarketCraftV5/textures'/(kind+'_normal.png')
        image=bpy.data.images.load(str(path),check_existing=True);image.colorspace_settings.name='Non-Color';image.pack();textures[kind]=image
    for key,mat in base.MATS.items():
        kind='wood' if key.startswith(('wood','shutter')) else 'plaster' if key.startswith('plaster') else 'stone' if key in ['limestone','stone_light','stone_shade','paving','paving_light','paving_cool','paving_warm'] else None
        if kind:
            nodes=mat.node_tree.nodes;im=nodes.new('ShaderNodeTexImage');im.image=textures[kind];normal=nodes.new('ShaderNodeNormalMap');normal.inputs['Strength'].default_value=.12
            mat.node_tree.links.new(im.outputs['Color'],normal.inputs['Color']);mat.node_tree.links.new(normal.outputs['Normal'],nodes.get('Principled BSDF').inputs['Normal'])

def main():
    print(json.dumps({'pid':os.getpid(),'stage':'tolbana_normal_craft_v5'}),flush=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    with bpy.data.libraries.load(str(HERE.parent/'Level0_Anime_Reference_Scenes_HQ.blend'),link=False) as (src,dst):dst.scenes=['Tolbana_Plaza']
    sc=dst.scenes[0];bpy.context.window.scene=sc;sc.name='Tolbana_Plaza_CraftV5';col=sc.collection
    prefixes=['tolbana_west_tall','tolbana_west_low','tolbana_peach_bread_front','tolbana_sage_front','COL_tolbana_west_tall','COL_tolbana_west_low','COL_tolbana_peach_bread_front','COL_tolbana_sage_front','tolbana_tree','COL_tolbana_tree','tolbana_fountain_molded','tolbana_fountain_water','COL_tolbana_fountain','tolbana_curved_grass','tolbana_shrubs_and_benches']
    for ob in list(sc.objects):
        if any(ob.name.startswith(s) for s in prefixes) or '_visual_person_' in ob.name or ob.type=='CAMERA':bpy.data.objects.remove(ob,do_unlink=True)
    # Restore the retained front-to-rear fountain approach collision (prefix removal is deliberately explicit).
    c=G();c.box((9.5,8,.16),(3,20,.16));a.emit(c,col,'COL_TolbanaV5_fountain_approach',0,True)
    houses=[residences.build(col,'TolbanaV5_WestInn',8.1,9,10.8,(-22,26,.22),'plaster_cream','gable',0,True),
        residences.build(col,'TolbanaV5_WhiteResidence',8,8,6.5,(-13.8,25,.22),'plaster_white','cross',-.3),
        residences.build(col,'TolbanaV5_PeachResidence',8.9,8,6.6,(-5.2,25.7,.22),'plaster_peach','cross',1.3),
        residences.build(col,'TolbanaV5_SageResidence',10.5,8.6,7.8,(4.7,25.2,.22),'plaster_cream','cross',-1.6,False,True)]
    landscape.tree(col);landscape.grass(col);landscape.fountain_paving(col);garden=garden_details.build(col)
    # Complete the west lane's enclosing volume; distant supporting architecture stays simpler.
    for name,w,d,h,p,yaw,role in [('west_lane_house',9,8,8.4,(-28.4,28,.22),90,'plaster_peach'),('west_return',10.5,8,7.6,(-29,-.2,.22),90,'plaster_cream')]:
        hq.facade('TolbanaV5_'+name,col,w,d,h,[(x,4.2,.90,1.65,True,True) for x in (-w*.3,0,w*.3)],p,yaw,False,role,'crossgable',0)
    # Porch walk extensions are flush at .26, joining the existing rear pavement.
    hq.stone_path('TolbanaV5_house_forecourt',col,(-26.2,20.0,23.0,26.0),.22,.69)
    for idx,(x,y,yaw) in enumerate([(12.7,-6,25),(-7,10,120),(1,-14.5,80)]):
        g=G();g.box((0,0,.4),(.12,.12,.7),'wood');ob=a.emit(g,col,'Tolbana_Plaza_visual_person_%02d'%idx,0);ob.location=(x,y,.24);ob.rotation_euler.z=math.radians(yaw)
    cam=[20,-30,7.5];target=[-3,13,6.8];hq.camera(sc,'Tolbana_Craft_View',cam,target,64)
    surfaces()
    for ob in sc.objects:
        ob.hide_set(False);ob.select_set(ob.type=='MESH')
    path=EXPORT/'Tolbana_Plaza_CraftV5.glb';bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_cameras=False,export_lights=False,export_yup=True,export_apply=True)
    visual=collision=0
    for ob in sc.objects:
        if ob.type=='MESH':
            ob.data.calc_loop_triangles();count=len(ob.data.loop_triangles)
            if ob.name.startswith('COL_'):collision+=count;ob.hide_render=True;ob.hide_set(True)
            else:visual+=count
    entry={'id':'Tolbana_Plaza','region_id':'tolbana','floor_east_north_m':[0,2900],'godot_position_m':[0,0,-7670],'godot_yaw_degrees':0,'glb':path.name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'triangles':visual,'collision_triangles':collision,'collision_mesh_names':[o.name for o in sc.objects if o.name.startswith('COL_')],'reference_camera_blender_m':cam,'reference_target_blender_m':target,'reference_horizontal_fov':64,'mesh_objects':sum(o.type=='MESH' for o in sc.objects),'buildings':houses,'garden':garden,'visual_actor_count':3,'layout_is_project_infill':True,'camera_matching_required':False}
    manifest={'schema':3,'style_id':'level0_tolbana_craft_v5','scenes':[entry],'visual_approval':False,'global_style_approved':False,'direction':'coherent_detailed_3d_architecture_not_single_frame_reconstruction','source_files_sha256':{name:hashlib.sha256((HERE/name).read_bytes()).hexdigest() for name in ['build_tolbana.py','residences.py','landscape.py','garden_details.py']}}
    for folder in [HERE,EXPORT]:(folder/'tolbana_craft_v5_manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    combined=json.loads((EXPORT/'scene_manifest.json').read_text(encoding='utf-8'))
    combined['scenes']=[json.loads((EXPORT/'market_craft_v5_manifest.json').read_text(encoding='utf-8'))['scenes'][0],entry]
    combined['style_id']='level0_two_towns_craft_v5'
    combined['visual_approval']=False;combined['global_style_approved']=False
    (EXPORT/'craft_scene_manifest.json').write_text(json.dumps(combined,ensure_ascii=False,indent=2),encoding='utf-8')
    bpy.ops.wm.save_as_mainfile(filepath=str(HERE/'Tolbana_Plaza_CraftV5.blend'),compress=True)
    print(json.dumps({'stage':'exported','triangles':visual,'sha256':entry['sha256']}),flush=True)

if __name__=='__main__':main()
