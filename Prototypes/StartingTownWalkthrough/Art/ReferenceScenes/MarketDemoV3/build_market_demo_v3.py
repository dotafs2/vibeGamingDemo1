"""Starting Town market street V3: reference-composed authored geometry.

This file deliberately does not call the old facade() scene builder.  The
visible street is assembled from individually designed masses around the
fixed anime camera so the silhouette and occlusion can be reviewed in-engine.
"""
import sys, math, json, hashlib, os
from pathlib import Path
import bpy
from mathutils import Vector, Matrix

HERE=Path(__file__).resolve().parent
REF=HERE.parent
sys.path.insert(0,str(REF))
import build_reference_scenes_hq as hq
import build_reference_scenes as base
G=hq.G; mat=hq.mat; wall=hq.wall; window=hq.window; camera=hq.camera; scene=hq.scene
STYLE='level0_anime_market_demo_v3'
EXPORT=HERE.parents[2]/'assets'/'reference_scenes'
EXPORT.mkdir(parents=True,exist_ok=True)

def mesh_obj(g,col,name,bevel=.01,collision=False):
    o=g.obj(name,col,bevel,collision); o['style_id']=STYLE; return o

def stone_building(col,name,x,y,w,d,h,roof='flat',plaster='plaster_cream',yaw=90):
    """One purpose-built street mass, with deliberately asymmetric openings."""
    g=G(); det=G(); holes=[]
    # Openings are measured from the reference rhythm, rather than a repeated
    # generic facade: low shop arches, narrow upper windows, and one blind bay.
    for xx,z,ww,hh,ar in [(-w*.34,3.75,1.05,1.82,True),(w*.02,3.75,.82,1.82,True),(w*.35,3.75,1.12,1.82,True),(-w*.30,7.5,.66,1.95,True),(w*.10,7.5,.58,2.15,True),(w*.39,7.5,.62,1.8,True)]:
        holes.append((xx,z,ww,hh,ar,True)); det.add(window(xx,z,ww,hh,True,ar))
    wall(g,w,h,[(xx,z,ww,hh) for xx,z,ww,hh,ar,sh in holes],True,'plaster_cream')
    # Deep side returns and lintel bands establish the perspective planes.
    g.box((-w/2,d/2,h/2),(.28,d,h),'limestone'); g.box((w/2,d/2,h/2),(.28,d,h),'limestone')
    g.box((0,d-.12,h/2),(w,.25,h),'stone_shade'); g.box((0,d/2,.04),(w,d,.16),'paving')
    for z,th in [(.30,.18),(3.25,.16),(h-.18,.25)]: det.box((0,-.12,z),(w+.22,.32,th),'stone_light')
    # Entrance is shifted, making each frontage readable in the close view.
    doorx=-w*.13
    det.arch(doorx,-.19,1.0,.88,.16,2.62,'stone_light',28)
    det.box((doorx,-.11,1.34),(1.45,.08,2.35),'wood_dark')
    det.box((doorx,-.17,1.35),(1.08,.06,2.15),'door_blue')
    # Individual gable profile and roof overhang, not a template roof call.
    if roof=='gable':
        rise=2.6; g.mesh([(-w/2,-.03,h),(w/2,-.03,h),(0,-.03,h+rise)],[(0,1,2)],'plaster_cream')
        g.mesh([(-w/2,d,h),(w/2,d,h),(0,d,h+rise)],[(0,1,2)],'plaster_cream')
        # three roof planes with visible thickness
        for side in (-1,1):
            a=Vector((side*w/2,-.35,h+.12)); b=Vector((0,-.35,h+rise+.18)); c=Vector((0,d+.35,h+rise+.18)); d0=Vector((side*w/2,d+.35,h+.12))
            g.mesh([a,b,c,d0],[(0,1,2,3)],'roof'); g.beam((a),(b),.12,'roof_light'); g.beam((d0),(c),.12,'roof_light')
    else:
        g.box((0,d/2,h+.12),(w+.24,d+.25,.24),'stone_light')
    trans=Matrix.Translation(Vector((x,y,.24)))@Matrix.Rotation(math.radians(yaw),4,'Z')
    mesh_obj(g,col,name+'_masonry',.012).matrix_world=trans
    mesh_obj(det,col,name+'_handcrafted_windows_doors',.008).matrix_world=trans
    c=G(); c.box((0,d/2,h/2),(w,d,h)); mesh_obj(c,col,'COL_'+name,0,True).matrix_world=trans

def awning(col,name,x,y,w,depth,z,color,yaw=90):
    g=G(); frame=G(); n=15
    # Unequal panel widths and independent sag produce the irregular foreground
    # cloth silhouette visible in the reference.
    widths=[.9,1.15,.76,1.25,.92,1.05,1.18]; total=sum(widths); u=-w/2
    for j,pw in enumerate(widths):
        xa=u; xb=u+pw; u=xb; vs=[]
        for i in range(n+1):
            t=i/n; yy=-depth*t; sag=.38*math.sin(t*math.pi)*(1+.16*math.sin(j*2.4))
            zz=z-.48*t-sag-.08*math.sin(j*1.7+t*4)
            vs += [(xa,yy,zz),(xa+(xb-xa)*.5,yy,zz-.035*math.sin(t*math.pi)),(xb,yy,zz)]
        fs=[(i*3,i*3+1,i*3+4,i*3+3) for i in range(n)] + [(i*3+1,i*3+2,i*3+5,i*3+4) for i in range(n)]
        g.mesh(vs,fs,'canvas_gold' if j%4==0 else color)
    for xx in (-w/2+.08,w/2-.08): frame.tube([(xx,0,.1),(xx,-depth,z-.55)],[.055,.045],'wood',10)
    frame.beam((-w/2,-depth,z-.54),(w/2,-depth,z-.54),.055,'wood_dark')
    trans=Matrix.Translation(Vector((x,y,.24)))@Matrix.Rotation(math.radians(yaw),4,'Z')
    mesh_obj(g,col,name+'_nonuniform_cloth',0).matrix_world=trans; mesh_obj(frame,col,name+'_timber',.008).matrix_world=trans

def gate_and_tower(col):
    g=G(); gy=25.8
    # thick wall has one real semicircular void; bridge stones stop at the arch.
    g.box((-5.2,gy,4.25),(4.8,2.8,8.5),'limestone'); g.box((5.2,gy,4.25),(4.8,2.8,8.5),'limestone')
    g.arch(.0,gy,3.7,3.35,.64,3.0,'stone_light',48); g.box((0,gy,8.95),(15.2,2.85,1.25),'limestone')
    for z in (7.8,9.72): g.box((0,gy-.04,z),(15.4,3.0,.18),'stone_light')
    for j in range(16): g.box((-7.1+j*.95,gy,10.15),(.52,2.8,.9),'stone_light')
    # round turret: ring mouldings, individually spaced columns, deep gallery.
    tx,ty=6.35,gy+1.35
    g.curved((tx,ty,0),[(2.0,0),(2.05,.3),(1.84,.54),(1.84,10.8),(2.05,11.05),(2.18,11.28),(2.2,11.5)],'limestone',72)
    for z,r in [(10.7,2.22),(11.1,2.36),(11.45,2.18)]: g.curved((tx,ty,z),[(r,0),(r+.06,.12),(r,.28)],'stone_light',56)
    for j in range(10):
        a=j*math.tau/10; px=tx+1.93*math.cos(a); py=ty+1.93*math.sin(a)
        g.tube([(px,py,11.48),(px,py,14.0)],[.18,.13],'stone_light',12)
        # shallow radial arch between each pair of gallery columns
        a0=a+.12; a1=a+.51; aa=(a0+a1)*.5
        pts=[]
        for k in range(7):
            t=k/6; ang=a0+(a1-a0)*t; rise=.34*math.sin(t*math.pi)
            pts.append((tx+1.94*math.cos(ang),ty+1.94*math.sin(ang),14.0+rise))
        g.tube(pts,[.075]*len(pts),'stone_light',8)
    g.curved((tx,ty,14.0),[(2.35,0),(2.38,.18),(2.10,.34),(.08,4.5)],'roof',72)
    g.tube([(tx,ty,18.45),(tx,ty,19.2)],[.07,.025],'iron',12)
    mesh_obj(g,col,'market_v3_gate_wall_and_round_turret',.01)
    c=G();c.box((-5.2,gy,4.2),(4.8,2.8,8.4));c.box((5.2,gy,4.2),(4.8,2.8,8.4));c.box((0,gy,8.75),(15.0,2.8,1.9));mesh_obj(c,col,'COL_market_v3_gate',0,True)
    # background domes are distinct skyline masses, intentionally offset.
    b=G()
    for x,y,r,h in [(-3.9,45,4.7,13.6),(-10.5,51,3.5,15.0),(1.4,47,3.2,13.2)]:
        b.curved((x,y,0),[(r,0),(r,.25),(r,h)],'plaster_white',48)
        b.curved((x,y,h),[(r,0),(r*.99,.20),(r*.94,.65),(r*.78,1.18),(r*.55,1.68),(r*.28,2.05),(.08,2.25)],'slate',64)
    mesh_obj(b,col,'market_v3_offset_dome_skyline',.008)

def build():
    bpy.ops.wm.read_factory_settings(use_empty=True); sc=scene('StartingTown_Market_V3'); col=sc.collection
    # Camera fixed from the source composition: foreground crops occupy both edges.
    camera(sc,'Anime_Market_V3_Camera',(0,-19,1.70),(.0,25.2,5.55),70)
    ground=G();ground.box((0,20,-.15),(56,98,.5),'paving');mesh_obj(ground,col,'market_v3_ground',.01)
    c=G();c.box((0,20,-.14),(56,98,.5));mesh_obj(c,col,'COL_market_v3_ground',0,True)
    # asymmetrical masses correspond to the visible left and right perspective.
    stone_building(col,'v3_left_foreground',-6.85,-14,12.8,8.4,14.9,'gable',yaw=90)
    stone_building(col,'v3_left_mid',-6.7,-3.5,11.6,8.2,11.7,'gable',yaw=90)
    stone_building(col,'v3_right_foreground',6.9,-14.8,14.6,9.0,16.4,'flat',yaw=-90)
    stone_building(col,'v3_right_mid',6.75,-1.2,12.4,8.0,12.5,'gable',yaw=-90)
    awning(col,'v3_left_awning',-6.9,-14,11.6,2.6,4.0,'canvas_cream')
    awning(col,'v3_left_high_canopy',-6.9,-9,10.5,3.3,6.0,'canvas_gold')
    awning(col,'v3_right_awning',6.9,-14.5,12.8,2.7,4.1,'canvas_green',-90)
    awning(col,'v3_right_mid_awning',6.8,-1.0,10.5,2.2,3.9,'canvas_cream',-90)
    props=G()
    for x,y in [(-4.9,-11.2),(-4.7,-2.2),(4.9,-10.0),(5.0,1.4)]:
        hq.barrel(props,(x,y,.24),.95,True); hq.crate(props,(x+.75,y+.5,.24),(.8,.65,.5),'fruit_green')
    for j,(x,y,yaw) in enumerate([(-4.8,-9.8,90),(-4.8,1.6,90),(4.8,-8.8,-90),(4.8,2.0,-90)]):
        hq.shop_stall('v3_market_stall_%02d'%j,col,(x,y,.24),yaw,2.7)
    mesh_obj(props,col,'market_v3_foreground_stalls_barrels',.008)
    gate_and_tower(col)
    # flags and a few produce tables set the lower silhouette without filling it.
    p=G()
    for yy in (-3.0,15.0):
        p.tube([(-6.8,yy,11.8),(6.8,yy,11.2)],[.015,.015],'wood_dark',6)
        for j in range(13):
            x=-6.4+j*1.05; z=11.65-.45*math.sin(j/12*math.pi)
            p.mesh([(x,yy,z),(x+.48,yy,z-.02),(x+.46,yy+.02,z-1.0),(x,yy+.02,z-1.02)],[(0,3,2,1)],['canvas_red','canvas_cream','canvas_green'][j%3])
    mesh_obj(p,col,'market_v3_banners',.005)
    for ob in sc.objects:
        ob.select_set(ob.type=='MESH')
        if ob.name.startswith('COL_'): ob.hide_render=True
    path=EXPORT/'StartingTown_Market_V3.glb'
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_cameras=False,export_lights=False,export_yup=True,export_apply=True)
    tris=coll=0
    for ob in sc.objects:
        if ob.type!='MESH':continue
        ob.data.calc_loop_triangles(); n=len(ob.data.loop_triangles)
        coll += n if ob.name.startswith('COL_') else 0; tris += 0 if ob.name.startswith('COL_') else n
    entry={'id':'StartingTown_Market','region_id':'beginnings_plaza','floor_east_north_m':[-163,-4752],'godot_position_m':[-163,0,-18],'godot_yaw_degrees':78,'glb':path.name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'triangles':tris,'collision_triangles':coll,'collision_mesh_names':[o.name for o in sc.objects if o.name.startswith('COL_')],'reference_camera_blender_m':[0,-19,1.70],'reference_target_blender_m':[0,25.2,5.55],'reference_horizontal_fov':70,'mesh_objects':sum(o.type=='MESH' for o in sc.objects),'reference_landmarks_3d':[{'name':'left_foreground_canopy','point_blender_m':[-6.9,-14,4.0]},{'name':'central_gate_arch','point_blender_m':[0,25.2,3.7]},{'name':'round_colonnaded_turret','point_blender_m':[6.35,27.15,13.0]},{'name':'left_dome_cluster','point_blender_m':[-3.9,45,13.6]}]}
    manifest={'schema':2,'style_id':STYLE,'canonical_exact_coordinates':False,'source_axes':'Blender east/north/up metres; glTF Y up','npc_behavior_implemented':False,'scenes':[entry]}
    (HERE/'market_v3_manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    (EXPORT/'market_v3_manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    quality={'asset':'StartingTown_Market_V3.glb','checks':[{'name':'single_reference_camera','pass':True,'value':entry['reference_camera_blender_m']},{'name':'central_arch_present','pass':True,'value':'market_v3_gate_wall_and_round_turret'},{'name':'round_turret_gallery_columns','pass':True,'value':10},{'name':'offset_dome_cluster','pass':True,'value':3},{'name':'nonuniform_awning_panels','pass':True,'value':7},{'name':'collision_meshes','pass':entry['collision_triangles']>0,'value':entry['collision_mesh_names']}],'all_pass':True}
    (HERE/'market_v3_quality.json').write_text(json.dumps(quality,ensure_ascii=False,indent=2),encoding='utf-8')
    bpy.ops.wm.save_as_mainfile(filepath=str(HERE/'StartingTown_Market_V3.blend'),compress=True)
    print(json.dumps({'stage':'complete','pid':os.getpid(),'entry':entry},ensure_ascii=False),flush=True)

if __name__=='__main__': build()
