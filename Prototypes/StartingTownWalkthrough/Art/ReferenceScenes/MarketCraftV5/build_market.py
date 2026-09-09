"""Coherent explorable street. Anime language, original project shop designs."""
import bpy,math,sys,json,hashlib,os,random
from pathlib import Path
from mathutils import Vector
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE));sys.path.insert(0,str(HERE.parent));sys.path.insert(0,str(HERE.parent/'MarketDemoV3'))
import architecture as a
import build_reference_scenes_hq as hq
import build_reference_scenes as base
import craft_landmarks as craft
G=hq.G;PROJECT=HERE.parents[2];EXPORT=PROJECT/'assets/reference_scenes';OUT=PROJECT/'validation/market_craft_v5'
base.PALETTE.update({'tile_red':'B77F70','bread':'BD8745','bread_light':'ECD19B','pottery':'AA7861','canvas_blue':'75AEB7','canvas_rust':'A96C4C'})
CAM=Vector((5.4,-16,4.4));TARGET=Vector((-3.4,1.0,3.5));FOV=64

def asset_surface_finish():
    """Subtle tileable micro-normal assets, generated locally, embedded in GLB."""
    import numpy as np
    folder=HERE/'textures';folder.mkdir(exist_ok=True)
    n=512;u,v=np.meshgrid(np.arange(n)/n,np.arange(n)/n);textures={}
    for kind in ['stone','wood','plaster']:
        rng=np.random.default_rng(515);height=np.zeros((n,n))
        if kind=='wood':
            height=.4*np.sin(u*math.tau*46+.8*np.sin(v*math.tau*3))+.14*np.sin(u*math.tau*91+np.sin(v*math.tau*7))
        else:
            # Isotropic filtered noise avoids a visible woven/diagonal grid on stone.
            noise=rng.normal(0,1,(n,n));frequency=np.fft.fftfreq(n)*n;kx,ky=np.meshgrid(frequency,frequency);radial=kx*kx+ky*ky
            spectrum=np.fft.fft2(noise)
            height=np.fft.ifft2(spectrum*(np.exp(-radial/800)*.65+np.exp(-radial/7000)*.12)).real
        height+=rng.normal(0,.015,(n,n));dx=(np.roll(height,-1,1)-np.roll(height,1,1))*.7;dy=(np.roll(height,-1,0)-np.roll(height,1,0))*.7
        normal=np.stack([-dx,-dy,np.ones_like(dx)],axis=-1);normal/=np.linalg.norm(normal,axis=-1)[...,None];rgba=np.ones((n,n,4),dtype=np.float32);rgba[:,:,:3]=normal*.5+.5
        im=bpy.data.images.new('V5_'+kind+'_micro_normal',width=n,height=n,alpha=True);im.colorspace_settings.name='Non-Color';im.pixels.foreach_set(rgba.reshape(-1));im.filepath_raw=str(folder/(kind+'_normal.png'));im.file_format='PNG';im.save();im.pack();textures[kind]=im
    for key,mat in base.MATS.items():
        kind='wood' if key.startswith('wood') or key.startswith('shutter') else 'plaster' if key.startswith('plaster') else 'stone' if key in ['limestone','stone_light','stone_shade','paving','paving_light','paving_cool','paving_warm'] else None
        if not kind:continue
        nodes=mat.node_tree.nodes;image=nodes.new('ShaderNodeTexImage');image.image=textures[kind];normal=nodes.new('ShaderNodeNormalMap');normal.inputs['Strength'].default_value=.12 if kind=='wood' else .12
        mat.node_tree.links.new(image.outputs['Color'],normal.inputs['Color']);mat.node_tree.links.new(normal.outputs['Normal'],nodes.get('Principled BSDF').inputs['Normal'])

def street_props(col):
    g=G()
    for x,y in [(-4.8,-6.8),(-4.85,2.6),(5.2,-1.5),(5.3,8.6)]:
        hq.barrel(g,(x,y,.24),.75,False);hq.crate(g,(x+.70,y+.25,.24),(.60,.65,.40));hq.crate(g,(x+.70,y+.25,.64),(.55,.60,.32))
    # Bench with separated boards, curved iron ends and rear brace.
    for y in (9,20):
        for i in range(4):g.box((-4.9,y+(i-1.5)*.10,.70),(2.0,.09,.065),'wood')
        for side in (-1,1):
            x=-4.9+side*.77;g.tube([(x,y-.2,.24),(x,y-.13,.65),(x,y+.18,.71),(x,y+.25,1.3)],[.028]*4,'iron',10)
        for i in range(3):g.box((-4.9,y+.24,.94+i*.14),(2.0,.07,.10),'wood_light')
    # Slender hanging lanterns, glass core behind a frame and pitched hood.
    for x,y,side in [(-5.65,-7.8,-1),(6.0,-.9,1),(-5.65,10,-1),(6.25,17,1)]:
        ex=x-side*.5;g.tube([(x,y,2.8),(ex,y,2.8),(ex,y,2.57)],[.023]*3,'iron',10)
        g.box((ex,y,2.24),(.26,.26,.48),'glass_light')
        for dx in (-.15,.15):
            for dy in (-.15,.15):g.box((ex+dx,y+dy,2.24),(.025,.025,.51),'iron')
        g.curved((ex,y,2.49),[(.23,0),(0,.19)],'iron',4);g.box((ex,y,1.99),(.34,.34,.06),'iron')
    a.emit(g,col,'V5_street_furniture_joinery_and_lanterns',.007)
    # Produce stand complements the shop fronts; main circulation stays clear.
    hq.shop_stall('V5_produce_stall',col,(-4.6,14,.24),90,2.55)

def background(col):
    specs=[('left_inn',8.4,7.5,9.3,(-6.0,9.3,.24),90,'plaster_cream','terrace'),('left_end',8.2,7.3,7.4,(-6.0,20,.24),90,'plaster_white','gable'),('right_inn',9.0,7.5,10.0,(6.3,14.5,.24),-90,'plaster_sage','terrace'),('right_end',7.2,7.0,7.5,(6.3,24,.24),-90,'plaster_peach','crossgable')]
    for name,w,d,h,p,yaw,role,roof in specs:
        openings=[(x,4.15,1.05,1.85,True,True) for x in (-w*.31,0,w*.31)]
        if h>8:openings += [(x,7.4,.78,1.45,True,False) for x in (-w*.31,0,w*.31)]
        hq.facade('V5_'+name,col,w,d,h,openings,p,yaw,False,role,roof,-w*.23,False,False)
    craft.build_gate(col,(.0,34,.24),15.0,8.0,5.6,6.0,2.6)
    craft.build_tower(col,(6.0,35,.24),1.45,9.0,3.1,3.4)
    # Correct the reused tower's old collision origin (base, not centre).
    old=bpy.data.objects.get('COL_market_v3_round_tower')
    if old:bpy.data.objects.remove(old,do_unlink=True)
    c=G();c.cylinder((6,35,.24),1.45,9,'limestone',32);a.emit(c,col,'COL_V5_round_tower',0,True)
    craft.build_dome_cluster(col,[(-6,45,.24)],3.9,10.9,2.9)

def main():
    print(json.dumps({'pid':os.getpid(),'stage':'normal_architecture_v5'}),flush=True)
    bpy.ops.wm.read_factory_settings(use_empty=True);sc=hq.scene('StartingTown_Market_CraftV5');col=sc.collection
    hq.camera(sc,'Market_Craft_View',CAM,TARGET,FOV)
    ground=G();ground.box((0,22,-.1),(55,116,.5),'paving');a.emit(ground,col,'V5_ground',.005);a.emit(ground,col,'COL_V5_ground',0,True)
    hq.stone_path('V5_street_stone_paving',col,(-6.15,6.5,-20,53),.20,.72)
    shops=[a.build_house(col,'V5_Bakery','bakery',9.8,7.3,6.95,(-6,-3,.24),90),a.build_house(col,'V5_Outfitter','outfitter',7.8,6.5,6.9,(6.3,3,.24),-90)]
    background(col);street_props(col)
    # Few neutral visual proxies; do not put a duplicate crowd in the foreground.
    for idx,(x,y,yaw) in enumerate([(-2.9,-1,40),(3.1,4,245),(-1,11,100),(2.2,17,180),(-2,24,210)]):
        g=G();g.box((0,0,.4),(.12,.12,.7),'wood');ob=a.emit(g,col,'StartingTown_Market_visual_person_%02d'%idx,0);ob.location=(x,y,.24);ob.rotation_euler.z=math.radians(yaw)
    asset_surface_finish()
    for ob in sc.objects:ob.select_set(ob.type=='MESH')
    path=EXPORT/'StartingTown_Market_CraftV5.glb';bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_cameras=False,export_lights=False,export_yup=True,export_apply=True)
    visual=collision=0
    for ob in sc.objects:
        if ob.type=='MESH':
            ob.data.calc_loop_triangles();count=len(ob.data.loop_triangles)
            if ob.name.startswith('COL_'):collision+=count;ob.hide_render=True;ob.hide_set(True)
            else:visual+=count
    entry={'id':'StartingTown_Market','region_id':'beginnings_plaza','floor_east_north_m':[-163,-4752],'godot_position_m':[-163,0,-18],'godot_yaw_degrees':78,'glb':path.name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'triangles':visual,'collision_triangles':collision,'collision_mesh_names':[o.name for o in sc.objects if o.name.startswith('COL_')],'reference_camera_blender_m':list(CAM),'reference_target_blender_m':list(TARGET),'reference_horizontal_fov':FOV,'mesh_objects':sum(o.type=='MESH' for o in sc.objects),'reference_landmarks_3d':{'gate_opening_crown':[0,32.7,6.24]},'shops':shops,'visual_actor_count':5,'walkable_gate':True,'layout_is_project_infill':True,'camera_matching_required':False}
    manifest={'schema':3,'style_id':'level0_market_craft_v5','scenes':[entry],'visual_approval':False,'direction':'coherent_detailed_3d_architecture_not_single_frame_reconstruction','source_files_sha256':{name:hashlib.sha256((HERE/name).read_bytes()).hexdigest() for name in ['build_market.py','architecture.py','interiors.py']}}
    for folder in [HERE,EXPORT]:(folder/'market_craft_v5_manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    bpy.ops.wm.save_as_mainfile(filepath=str(HERE/'StartingTown_Market_CraftV5.blend'),compress=True)
    print(json.dumps({'stage':'exported','triangles':visual,'shops':shops,'sha256':entry['sha256']}),flush=True)

if __name__=='__main__':main()
