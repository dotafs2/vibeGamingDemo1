"""Actual CPU-limited overview and grip-guide renders through Blender MCP."""
from pathlib import Path
import importlib
import json
import math
import sys
import time
import bpy
from mathutils import Vector
OUT=Path(__file__).resolve().parent;SAVED=OUT.parent.parent/'Saved/ThreeHearths/ToolKit'
sys.dont_write_bytecode=True
if str(OUT) not in sys.path:sys.path.insert(0,str(OUT))
import tool_geometry as G
importlib.reload(G)
args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
view=args[0] if args else 'overview';assert view in ('overview','grips')
START=time.monotonic()
for o in list(bpy.data.objects):bpy.data.objects.remove(o,do_unlink=True)
for c in list(bpy.data.collections):bpy.data.collections.remove(c)
scene=bpy.context.scene;scene.name='ToolKit | '+view
bpy.ops.wm.save_as_mainfile(filepath=str(SAVED/('ToolKit_'+view+'_Scratch.blend')))
specs=json.loads((OUT/'module-specs.json').read_text(encoding='utf-8'))
sources=bpy.data.collections.new('ORIGINAL_SOURCE_REFERENCES');scene.collection.children.link(sources)
with bpy.data.libraries.load(str(OUT/'ToolKit.blend'),link=False) as (src,dst):dst.objects=[m['id'] for m in specs['modules']]
assets={o.name:o for o in dst.objects}
for o in dst.objects:sources.objects.link(o);o.hide_render=True;o.hide_viewport=True
display=bpy.data.collections.new('ORIGINAL_TOOL_DISPLAY');scene.collection.children.link(display)
stage=bpy.data.collections.new('PRESENTATION | excluded from GLBs');scene.collection.children.link(stage)
G.configure(stage,{})
base=G.mat('Tool review sage plinth','A6B09B',.67);ink=G.mat('Tool review charcoal','293D38',.60)
floor=G.mat('Tool review warm backdrop','DDD8C9',.75);blue=G.mat('Grip reference blue','537F9A',.45)
tipmat=G.mat('Working tip reference amber','C59444',.46);cream=G.mat('Grip reference cream','E9DCB9',.61)

def label(text,at,size=.055):
    data=bpy.data.curves.new('Caption','FONT');data.body=text;data.size=size;data.align_x='CENTER';data.materials.append(ink)
    obj=bpy.data.objects.new('Caption | '+text,data);stage.objects.link(obj);obj.location=at;return obj

def line(name,a,b,material=blue):
    data=bpy.data.curves.new(name,'CURVE');data.dimensions='3D';data.bevel_depth=.004;data.bevel_resolution=1
    s=data.splines.new('POLY');s.points.add(1);s.points[0].co=(*a,1);s.points[1].co=(*b,1);data.materials.append(material)
    obj=bpy.data.objects.new(name,data);stage.objects.link(obj)

placements=[]
for i,m in enumerate(specs['modules']):
    x=(i%4-1.5)*.92;y=(.5-i//4)*1.45
    obj=bpy.data.objects.new(m['id']+' | display',assets[m['id']].data);display.objects.link(obj)
    obj.rotation_euler.x=-math.pi/2;bpy.context.view_layer.update()
    points=[obj.matrix_world@Vector(p) for p in obj.bound_box]
    lo=Vector([min(p[k] for p in points) for k in range(3)]);hi=Vector([max(p[k] for p in points) for k in range(3)])
    obj.location=Vector((x,y+.045,.018))-Vector(((lo.x+hi.x)/2,(lo.y+hi.y)/2,lo.z))
    G.box('Individual tool plinth',(x,y,-.040),(.87,1.38,.08),base,.018)
    label(m['label'].upper(),(x,y-.515,.003),.055)
    dims=m['nominal_size_m'];label('%.3f x %.3f x %.3f M'%tuple(dims),(x,y-.618,.003),.040)
    bpy.context.view_layer.update()
    if view=='grips':
        for name,point,material in (('Primary grip',m['grip_anchor']['position_m'],blue),('Working tip',m['working_tip_m'],tipmat)):
            at=obj.matrix_world@Vector(point)
            G.cylinder(name+' marker',(at.x,at.y,at.z+.044),.021,.006,material,20)
        if 'secondary_grip_anchor_m' in m:
            at=obj.matrix_world@Vector(m['secondary_grip_anchor_m'])
            G.cylinder('Secondary grip marker',(at.x,at.y,at.z+.044),.015,.007,cream,16)
    placements.append({'id':m['id'],'display_translation_m':list(obj.location),'display_rotation_euler_degrees':[-90,0,0],'display_scale':[1,1,1]})
if view=='overview':
    label('TOOLS  /  WORKING LIFE IN THE VILLAGE',(0,1.64,0),.107)
    label('EIGHT INDEPENDENT TOOLS  /  WARM OAK, CREAM GRIPS & FORGED IRON',(0,-1.64,0),.055)
else:
    label('TOOL GRIPS  /  AUTHORING GUIDE',(0,1.64,0),.107)
    label('BLUE: PRIMARY GRIP   /   CREAM: SECONDARY GRIP   /   AMBER: WORKING TIP',(0,-1.64,0),.055)
    label('LOCAL ANCHOR PROPOSALS  /  NO SKELETON OR WORK ANIMATION BOUND',(0,-1.80,0),.043)
G.box('Studio ground',(0,0,-.17),(200,200,.16),floor,.008)
scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=24;scene.cycles.use_denoising=True
scene.render.threads_mode='FIXED';scene.render.threads=3
scene.render.resolution_x=1800;scene.render.resolution_y=1450;scene.render.resolution_percentage=100
scene.view_settings.view_transform='AgX';scene.world=bpy.data.worlds.new('Tool soft daylight');scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.62,.70,.74,1)
scene.world.node_tree.nodes['Background'].inputs[1].default_value=.45
for name,at,power,size,color in (('Warm key',(-3,-4,7),620,5,(1,.87,.74)),('Cool fill',(4,3,6),470,4,(.79,.91,1))):
    d=bpy.data.lights.new(name,'AREA');d.energy=power;d.size=size;d.color=color
    o=bpy.data.objects.new(name,d);scene.collection.objects.link(o);o.location=at;o.rotation_euler=(-o.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Tool camera');d.type='ORTHO';d.ortho_scale=5.47
c=bpy.data.objects.new('Tool camera',d);scene.collection.objects.link(c);c.location=(2.2,-5.5,8.5)
c.rotation_euler=(Vector((0,0,.04))-c.location).to_track_quat('-Z','Y').to_euler();scene.camera=c
bpy.ops.wm.save_as_mainfile(filepath=str(SAVED/('ToolKit_'+view+'_Review.blend')))
scene.render.filepath=str(OUT/'previews'/('ToolKit_'+view+'.png'));bpy.ops.render.render(write_still=True)
path=OUT/'render-report.json';report=json.loads(path.read_text(encoding='utf-8')) if path.exists() else {'engine':'Cycles','device':'CPU','threads':3,'samples':24,'views':{}}
report['views'][view]={'file':'previews/'+Path(scene.render.filepath).name,'seconds':round(time.monotonic()-START,3),
 'resolution':[1800,1450],'all_tools_at_authored_scale':True,'guide_markers_in_exported_assets':False,'placements':placements}
path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('TOOL_PREVIEW_COMPLETE',view,flush=True)
