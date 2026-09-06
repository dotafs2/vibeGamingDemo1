"""Blender MCP closeup of exported timber roof components and wood reference."""
from pathlib import Path
import importlib.util
import json
import sys
import time
import bpy
from mathutils import Vector
sys.dont_write_bytecode=True
OUT=Path(__file__).resolve().parent;KIT=OUT.parent/'VillageKit';SAVED=OUT.parent.parent/'Saved/ThreeHearths/Stage4TimberRoof'
sp=importlib.util.spec_from_file_location('timber_roof_preview_geometry',OUT.parent/'ToolKit/tool_geometry.py')
G=importlib.util.module_from_spec(sp);sp.loader.exec_module(G)
START=time.monotonic()
for o in list(bpy.data.objects):bpy.data.objects.remove(o,do_unlink=True)
for c in list(bpy.data.collections):bpy.data.collections.remove(c)
scene=bpy.context.scene;scene.name='Timber roof exported components | PBR comparison'
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
stage=bpy.data.collections.new('Presentation only');scene.collection.children.link(stage);G.configure(stage,{})
floor=G.mat('Warm preview ground','DDD8C9',.75);base=G.mat('Sage preview base','A6B09B',.67);ink=G.mat('Preview ink','293D38',.6)
assets=[]
for mid,at in [('roof_slope_timber_2m',(-2.4,.20,.13)),('roof_ridge_timber_2m',(1.30,.20,-1.22)),('beam_timber_2m',(.9,-1.35,0))]:
    before=set(scene.objects);bpy.ops.import_scene.gltf(filepath=str(KIT/'modules'/(mid+'.glb')))
    items=[o for o in scene.objects if o not in before and o.type=='MESH'];assert len(items)==1
    o=items[0];o.location+=Vector(at);assets.append({'asset_id':mid,'preview_offset_m':at})
G.box('Display base',(-.25,0,-.09),(6.2,4.4,.18),base,.045)
G.box('Studio floor',(0,0,-.25),(200,200,.14),floor,.01)
def label(text,at,size=.14):
    d=bpy.data.curves.new('Caption','FONT');d.body=text;d.size=size;d.align_x='CENTER';d.materials.append(ink)
    o=bpy.data.objects.new('Caption | '+text,d);stage.objects.link(o);o.location=at;return o
label('SLOPE  /  3,452 TRIANGLES',(-1.25,-1.55,.013),.15)
label('2.293 x 2.000 x 1.414 m',(-1.25,-1.80,.013),.115)
label('RIDGE  /  408 TRIANGLES',(1.35,1.45,.013),.14)
label('0.300 x 2.000 x 0.189 m',(1.35,1.22,.013),.11)
label('EXISTING CHESTNUT BEAM',(1,-1.83,.013),.105)
scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=16;scene.cycles.use_denoising=True
scene.render.threads_mode='FIXED';scene.render.threads=3
scene.render.resolution_x=1600;scene.render.resolution_y=1100;scene.render.resolution_percentage=100;scene.view_settings.view_transform='AgX'
scene.world=bpy.data.worlds.new('Soft daylight');scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.62,.70,.74,1);scene.world.node_tree.nodes['Background'].inputs[1].default_value=.48
for name,at,power,size,color in [('Warm key',(-4,-5,8),700,5,(1,.87,.74)),('Cool fill',(5,3,7),550,4,(.79,.91,1))]:
    d=bpy.data.lights.new(name,'AREA');d.energy=power;d.size=size;d.color=color
    o=bpy.data.objects.new(name,d);scene.collection.objects.link(o);o.location=at;o.rotation_euler=(-o.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Component camera');d.type='ORTHO';d.ortho_scale=7.5
c=bpy.data.objects.new('Component camera',d);scene.collection.objects.link(c);c.location=(5,-8,8)
c.rotation_euler=(Vector((-.25,0,.5))-c.location).to_track_quat('-Z','Y').to_euler();scene.camera=c
for text,y,size in [('TIMBER ROOF / VILLAGEKIT',2.23,.24),('ORIGINAL DATUM + UV / SELF-CONTAINED WOOD PBR',-2.23,.13)]:
    o=label(text,(0,0,0),size);o.parent=c;o.location=(0,y,-10);o.rotation_euler=(0,0,0)
bpy.ops.wm.save_as_mainfile(filepath=str(SAVED/'TimberRoof_Components_Proof.blend'))
scene.render.filepath=str(OUT/'previews/TimberRoof_Components.png');bpy.ops.render.render(write_still=True)
(OUT/'timber-roof-render-report.json').write_text(json.dumps({'engine':'Cycles','device':'CPU','threads':3,'samples':16,
    'resolution':[1600,1100],'preview_file':'previews/TimberRoof_Components.png','source_glbs_imported':assets,
    'preview_display_offsets_only':True,'source_assets_modified':False,'runtime_ue_screenshot':False,
    'seconds':round(time.monotonic()-START,3)},indent=2)+'\n',encoding='utf-8')
print('TIMBER_ROOF_COMPONENT_PREVIEW_COMPLETE',flush=True)
