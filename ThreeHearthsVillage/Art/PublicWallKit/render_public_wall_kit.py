"""Actual Blender MCP CPU previews, sourced only from exported GLB modules."""
from pathlib import Path
import importlib.util
import json
import sys
import time
import bpy
from mathutils import Vector
sys.dont_write_bytecode=True
OUT=Path(__file__).resolve().parent;SAVED=OUT.parent.parent/'Saved/ThreeHearths/PublicWallKit'
sp=importlib.util.spec_from_file_location('public_wall_render_geometry',OUT.parent/'ToolKit/tool_geometry.py')
G=importlib.util.module_from_spec(sp);sp.loader.exec_module(G)
MODE=globals().get('PREVIEW_MODE','components');START=time.monotonic()
for o in list(bpy.data.objects):bpy.data.objects.remove(o,do_unlink=True)
for c in list(bpy.data.collections):bpy.data.collections.remove(c)
scene=bpy.context.scene;scene.name='Public wall | '+MODE;scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
stage=bpy.data.collections.new('Presentation only');scene.collection.children.link(stage);G.configure(stage,{})
floor=G.mat('PW preview ground','DDD8C9',.75);base=G.mat('PW preview sage','A6B09B',.67);ink=G.mat('PW preview ink','293D38',.60)
def label(text,at,size=.15):
    d=bpy.data.curves.new('Caption','FONT');d.body=text;d.size=size;d.align_x='CENTER';d.materials.append(ink)
    o=bpy.data.objects.new('Caption | '+text,d);stage.objects.link(o);o.location=at;return o
catalog=json.loads((OUT/'catalog.json').read_text());specs={m['id']:m for m in catalog['modules']}
sources=bpy.data.collections.new('Hidden GLB source meshes');scene.collection.children.link(sources);assets={}
for mid,m in specs.items():
    before=set(scene.objects);bpy.ops.import_scene.gltf(filepath=str(OUT/m['asset_glb']))
    os=[o for o in scene.objects if o not in before and o.type=='MESH'];assert len(os)==1;o=os[0]
    for c in list(o.users_collection):c.objects.unlink(o)
    sources.objects.link(o);o.hide_render=True;o.hide_viewport=True;assets[mid]=o
def instance(mid,at,name):
    o=bpy.data.objects.new(name,assets[mid].data);stage.objects.link(o);o.location=at;return o
if MODE=='components':
    ids=['public_wall_foundation_2m','public_wall_stone_2m','public_wall_walkway_2m','public_wall_parapet_2m']
    names=['STONE FOUNDATION','STONE WALL','TIMBER WALKWAY','TIMBER PARAPET']
    for i,mid in enumerate(ids):
        x=(i-1.5)*3.1;instance(mid,(x,0,0),mid+' display')
        G.box('Module display base',(x,0,-.07),(2.9,2.8,.14),base,.035)
        label(names[i],(x,-1.03,.01),.17)
        inputs=specs[mid]['resource_inputs'];label(' / '.join(str(v)+' '+k.upper() for k,v in inputs.items()),(x,-1.25,.01),.12)
    at=(6,-14,10);look=(0,0,1);scale=14.2;resolution=(2100,1100)
    title='FIRST PUBLIC WALL / FOUR INDEPENDENT COMPONENTS';footer='MATERIAL BUNDLES ARE CARRIED / COMPONENTS ARE INSTALLED ON SITE'
    component_count=4
else:
    plan=json.loads((OUT/'assembly.json').read_text())
    for p in plan['placements']:instance(p['component_asset_id'],p['position_authoring_m'],p['component_id'])
    G.box('Six metre segment display base',(0,-.35,-.08),(7.5,3.3,.16),base,.04)
    for x,key in ((-2,'stone'),(0,'planks'),(2,'beams')):
        ref=OUT/catalog['resources'][key]['npc_carry_asset'];before=set(scene.objects);bpy.ops.import_scene.gltf(filepath=str(ref))
        os=[o for o in scene.objects if o not in before and o.type=='MESH'];assert len(os)==1;o=os[0]
        bottom=min((o.matrix_world@v.co).z for v in o.data.vertices);o.location+=Vector((x,-1.28,-bottom))
        label(key.upper()+' CARRY INPUT',(x,-1.76,.01),.14)
    G.box('1.557m comparison ruler',(3.48,0,.7785),(.045,.045,1.557),ink,.003)
    for h in (0,.5,1,1.557):G.box('Ruler tick',(3.48,0,h),(.18,.045,.018),ink,.002)
    label('1.557 m',(3.48,-.35,.01),.105)
    at=(8,-12,8);look=(0,-.1,1.6);scale=10.6;resolution=(1900,1250)
    title='FIRST PUBLIC WALL / THREE 2m BAYS';footer='15 SEPARATE INSTALLATIONS / 36 STONE + 21 PLANKS + 12 BEAMS'
    component_count=len(plan['placements'])
G.box('Studio floor',(0,0,-.26),(200,200,.16),floor,.01)
scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=16;scene.cycles.use_denoising=True
scene.render.threads_mode='FIXED';scene.render.threads=3;scene.render.resolution_x,scene.render.resolution_y=resolution;scene.render.resolution_percentage=100
scene.view_settings.view_transform='AgX';scene.world=bpy.data.worlds.new('PW soft daylight');scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.62,.70,.74,1);scene.world.node_tree.nodes['Background'].inputs[1].default_value=.48
for name,loc,power,size,color in [('Warm key',(-6,-7,11),1300,7,(1,.87,.74)),('Cool fill',(7,4,9),1000,6,(.79,.91,1))]:
    d=bpy.data.lights.new(name,'AREA');d.energy=power;d.size=size;d.color=color
    o=bpy.data.objects.new(name,d);scene.collection.objects.link(o);o.location=loc;o.rotation_euler=(-o.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('PW preview camera');d.type='ORTHO';d.ortho_scale=scale
c=bpy.data.objects.new('PW preview camera',d);scene.collection.objects.link(c);c.location=at
c.rotation_euler=(Vector(look)-c.location).to_track_quat('-Z','Y').to_euler();scene.camera=c
height=scale*resolution[1]/resolution[0]
for text,y,size in [(title,height*.43,scale*.021),(footer,-height*.44,scale*.013)]:
    o=label(text,(0,0,0),size);o.parent=c;o.location=(0,y,-10);o.rotation_euler=(0,0,0)
bpy.ops.wm.save_as_mainfile(filepath=str(SAVED/('PublicWall_'+MODE+'_Proof.blend')))
filename='previews/PublicWall_'+MODE+'.png';scene.render.filepath=str(OUT/filename);bpy.ops.render.render(write_still=True)
(OUT/('render-'+MODE+'-report.json')).write_text(json.dumps({'engine':'Cycles','device':'CPU','threads':3,'samples':16,'resolution':resolution,
 'preview_file':filename,'module_instances':component_count,'source_glbs_used':True,'source_assets_modified':False,
 'whole_wall_glb_created':False,'runtime_ue_screenshot':False,'seconds':round(time.monotonic()-START,3)},indent=2)+'\n',encoding='utf-8')
print('PUBLIC_WALL_PREVIEW_COMPLETE',MODE,flush=True)
