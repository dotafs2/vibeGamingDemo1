"""Blender MCP proof render: cumulative components with new timber roof variants."""
from pathlib import Path
import importlib.util
import json
import math
import sys
import time
import bpy
from mathutils import Vector
sys.dont_write_bytecode=True
OUT=Path(__file__).resolve().parent;SAVED=OUT.parent.parent/'Saved/ThreeHearths/Stage4HouseAudit'
sp=importlib.util.spec_from_file_location('stage4_preview_geometry',OUT.parent/'ToolKit/tool_geometry.py')
G=importlib.util.module_from_spec(sp);sp.loader.exec_module(G)
START=time.monotonic()
for o in list(bpy.data.objects):bpy.data.objects.remove(o,do_unlink=True)
for c in list(bpy.data.collections):bpy.data.collections.remove(c)
scene=bpy.context.scene;scene.name='Stage4 audit | cumulative independent component construction'
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
bpy.ops.wm.save_as_mainfile(filepath=str(SAVED/'Stage4_Audit_Scratch.blend'))
plan=json.loads((OUT/'stage4-assembly.json').read_text(encoding='utf-8'))
catalog=json.loads((OUT/'component-catalog.json').read_text(encoding='utf-8'))
components={c['component_asset_id']:c for c in catalog['components'] if c['selected_for_minimal_house']}
sources=bpy.data.collections.new('COMPONENT_SOURCES | timber roof variants');scene.collection.children.link(sources)
assets={}
for mid,component in components.items():
    before=set(scene.objects);bpy.ops.import_scene.gltf(filepath=str(OUT/component['source_glb']))
    imported=[o for o in scene.objects if o not in before and o.type=='MESH'];assert len(imported)==1
    o=imported[0];bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    for c in list(o.users_collection):c.objects.unlink(o)
    sources.objects.link(o);o.hide_render=True;o.hide_viewport=True;assets[mid]=o
stage=bpy.data.collections.new('PRESENTATION | never exported as a house');scene.collection.children.link(stage)
G.configure(stage,{})
base=G.mat('Stage preview sage','A6B09B',.67);floor=G.mat('Stage preview warm ground','DDD8C9',.75)
ink=G.mat('Stage preview ink','293D38',.60)

def label(text,at,size=.16):
    d=bpy.data.curves.new('Caption','FONT');d.body=text;d.size=size;d.align_x='CENTER';d.materials.append(ink)
    o=bpy.data.objects.new('Caption | '+text,d);stage.objects.link(o);o.location=at;return o

stages=[];names=['FOUNDATION + FLOOR','FRAME','WALL PANELS + GABLES','ROOF']
for s in range(1,5):
    x=(s-2.5)*6.0;display=bpy.data.collections.new('STAGE '+str(s)+' | cumulative component instances');scene.collection.children.link(display)
    count=0;ids=[]
    for p in plan['placements']:
        if p['stage']>s:continue
        source=assets[p['component_asset_id']]
        o=bpy.data.objects.new('Stage'+str(s)+' | '+p['component_id'],source.data);display.objects.link(o)
        o.location=Vector((x,0,0))+Vector(p['position_authoring_m']);o.rotation_euler.z=math.radians(p['yaw_degrees'])
        o['component_id']=p['component_id'];o['component_asset_id']=p['component_asset_id'];o['installed_in_stage']=p['stage']
        count+=1;ids.append(p['component_id'])
    G.box('Stage demonstration base',(x,0,-.31),(5.5,5.8,.14),base,.035)
    label('%02d  %s'%(s,names[s-1]),(x,-2.52,-.225),.205)
    added=plan['stages'][s-1]['adds_components'];label('+%d COMPONENTS  /  %d TOTAL'%(added,count),(x,-2.78,-.225),.135)
    stages.append({'stage':s,'component_count':count,'component_ids':ids,'all_previous_instances_preserved':s==1 or set(stages[-1]['component_ids'])<set(ids)})
assert [s['component_count'] for s in stages]==[8,29,39,45]
G.box('Studio ground',(0,0,-.47),(200,200,.16),floor,.01)
scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=16;scene.cycles.use_denoising=True
scene.render.threads_mode='FIXED';scene.render.threads=3
scene.render.resolution_x=2400;scene.render.resolution_y=1150;scene.render.resolution_percentage=100;scene.view_settings.view_transform='AgX'
scene.world=bpy.data.worlds.new('Soft daylight');scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.62,.70,.74,1);scene.world.node_tree.nodes['Background'].inputs[1].default_value=.48
for name,at,power,size,color in (('Warm key',(-7,-9,18),2600,12,(1,.87,.74)),('Cool fill',(10,6,15),2000,10,(.79,.91,1))):
    d=bpy.data.lights.new(name,'AREA');d.energy=power;d.size=size;d.color=color
    o=bpy.data.objects.new(name,d);scene.collection.objects.link(o);o.location=at;o.rotation_euler=(-o.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Stage audit camera');d.type='ORTHO';d.ortho_scale=26.2
c=bpy.data.objects.new('Stage audit camera',d);scene.collection.objects.link(c);c.location=(6,-28,23)
c.rotation_euler=(Vector((0,0,1.25))-c.location).to_track_quat('-Z','Y').to_euler();scene.camera=c
# Camera-space text avoids occlusion by the house geometry at later stages.
for text,y,size in (('TIMBER HOUSE  /  FOUR COMPONENT BUILD STAGES',5.25,.40),
 ('EXISTING INSTANCES RETAINED  /  NO WHOLE-HOUSE MESH OR MATERIAL SWAP',-5.45,.25)):
    o=label(text,(0,0,0),size);o.parent=c;o.location=(0,y,-10);o.rotation_euler=(0,0,0)
bpy.ops.wm.save_as_mainfile(filepath=str(SAVED/'Stage4_Components_Proof.blend'))
scene.render.filepath=str(OUT/'previews/Stage4_ComponentStages.png');bpy.ops.render.render(write_still=True)
report={'engine':'Cycles','device':'CPU','threads':3,'samples':16,'resolution':[2400,1150],
 'preview_file':'previews/Stage4_ComponentStages.png','new_geometry_created':False,'new_glbs_exported':False,
 'existing_sources_modified':False,'uses_whole_house_mesh':False,'stages':stages,'roof_variant':'timber','wall_variant':'timber',
 'seconds':round(time.monotonic()-START,3),'runtime_ue_screenshot':False}
(OUT/'render-report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('STAGE4_COMPONENT_PREVIEW_COMPLETE',flush=True)
