"""CPU-limited Blender MCP rendering. Reference-bearing blends remain in Saved."""
from pathlib import Path
import importlib
import json
import math
import sys
import time
import bpy
from mathutils import Vector, Matrix
OUT=Path(__file__).resolve().parent
SAVED=OUT.parent.parent/'Saved/ThreeHearths/GoodsKitRecovery'
if str(OUT) not in sys.path:sys.path.insert(0,str(OUT))
import goods_geometry as G
importlib.reload(G)
ARGS=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
view=ARGS[0] if ARGS else 'overview'
START=time.monotonic()
for obj in list(bpy.data.objects):bpy.data.objects.remove(obj,do_unlink=True)
for c in list(bpy.data.collections):bpy.data.collections.remove(c)
scene=bpy.context.scene;scene.name='GoodsKit | '+view
# Break the save target away from the original asset .blend before adding reference.
bpy.ops.wm.save_as_mainfile(filepath=str(SAVED/('GoodsKit_'+view+'_Scratch.blend')))
specs=json.loads((OUT/'module-specs.json').read_text(encoding='utf-8'))
sources=bpy.data.collections.new('SOURCE_REFERENCES');scene.collection.children.link(sources)
with bpy.data.libraries.load(str(OUT/'GoodsKit.blend'),link=False) as (src,dst):
    dst.objects=[m['id'] for m in specs['modules']]
assets={o.name:o for o in dst.objects}
for o in dst.objects:sources.objects.link(o);o.hide_render=True;o.hide_viewport=True
display=bpy.data.collections.new('DISPLAY');scene.collection.children.link(display)
stage=bpy.data.collections.new('PRESENTATION');scene.collection.children.link(stage)
G.configure(stage,{})
base=G.mat('Preview sage stone','A6B09B',.67);ink=G.mat('Preview charcoal','293D38',.60)
floor=G.mat('Preview cream backdrop','DDD8C9',.75);ruler=G.mat('Preview ruler blue','537F9A',.5)

def label(text,at,size=.10,vertical=False):
    data=bpy.data.curves.new('Caption','FONT');data.body=text;data.size=size;data.align_x='CENTER';data.materials.append(ink)
    obj=bpy.data.objects.new('Caption | '+text,data);stage.objects.link(obj);obj.location=at
    if vertical:obj.rotation_euler=(math.pi/2,0,0)
    return obj

def line(name,a,b,r=.008):
    data=bpy.data.curves.new(name,'CURVE');data.dimensions='3D';data.bevel_depth=r;data.bevel_resolution=1
    s=data.splines.new('POLY');s.points.add(1);s.points[0].co=(*a,1);s.points[1].co=(*b,1);data.materials.append(ruler)
    obj=bpy.data.objects.new(name,data);stage.objects.link(obj)

def instance(mid,at):
    obj=bpy.data.objects.new(mid+' | display',assets[mid].data);display.objects.link(obj);obj.location=at;return obj

if view=='overview':
    for i,m in enumerate(specs['modules']):
        x=(i%4-1.5)*1.04;y=(.5-i//4)*1.04
        instance(m['id'],(x,y,0))
        G.box('Individual sage plinth',(x,y,-.045),(.98,.97,.09),base,.02)
        label(m['label'].upper(),(x,y-.35,.006),.048)
        label(m['commodity_id'],(x,y-.435,.006),.036)
    label('GOODS  /  MATERIALS FOR A LIVING VILLAGE',(0,1.23,.0),.105)
    label('EIGHT INDEPENDENT CARRY PROPS   /   WARM OAK, CLAY & FORGED IRON',(0,-1.25,.0),.060)
    camera_at=(3.5,-6.5,7.0);target=(0,.06,.08);scale=5.12;resolution=(1750,1250)
    reference=False
else:
    assert view=='scale'
    for i,m in enumerate(specs['modules']):
        x=(i%4)*.94-.72;y=(.5-i//4)*.98
        instance(m['id'],(x,y,0))
        G.box('Same-scale goods plinth',(x,y,-.04),(.88,.90,.08),base,.018)
        dims=m['nominal_size_m']
        label(m['label'].upper(),(x,y-.31,.009),.038)
        label('%.2f x %.2f x %.2f M'%tuple(dims),(x,y-.392,.009),.044)
    ref=OUT.parent.parent/'Saved/ThreeHearths/ResidentKitReference/SKM_Villager.fbx'
    bpy.ops.import_scene.fbx(filepath=str(ref),automatic_bone_orientation=False)
    body=next(o for o in scene.objects if o.type=='MESH' and o.name.startswith('SKM_Villager'))
    world=body.matrix_world.copy()
    for v in body.data.vertices:v.co=world@v.co
    body.parent=None;body.matrix_world=Matrix.Identity(4)
    for mod in list(body.modifiers):body.modifiers.remove(mod)
    for obj in list(scene.objects):
        if obj.type=='ARMATURE':bpy.data.objects.remove(obj,do_unlink=True)
    skin=G.mat('Reference warm skin','D4AD84',.58);outfit=G.mat('Reference sage tunic','7D9990',.58)
    eyes=G.mat('Reference charcoal eyes','302D28',.30);boots=G.mat('Reference leather boots','594337',.52)
    body.data.materials.clear()
    for material in (outfit,skin,eyes,boots):body.data.materials.append(material)
    groups={g.index:g.name for g in body.vertex_groups}
    for p in body.data.polygons:
        weights={}
        for index in p.vertices:
            for g in body.data.vertices[index].groups:
                key=groups[g.group];weights[key]=weights.get(key,0)+g.weight
        dominant=max(weights,key=weights.get) if weights else ''
        if dominant.startswith(('eye_','brow_')):p.material_index=2
        elif dominant.startswith(('head','nose','neck','hand','thumb','index','lowerarm')):p.material_index=1
        elif dominant.startswith(('foot','ball','calf')):p.material_index=3
        else:p.material_index=0
    heights=[v.co.z for v in body.data.vertices];height=max(heights)
    # The established resident height uses the ground datum, not max-min: the
    # reference shoe sole sits 2.10 mm above Z=0. Preserve its authored placement.
    assert abs(height-1.557142)<.00001 and abs(min(heights)-.0021)<.00001,('Reference scale changed',min(heights),height)
    body.name='LICENSED CROPOUT REFERENCE | never included in kit source';body.location=(-2.09,.0,0)
    G.box('Reference plinth',(-2.09,0,-.05),(1.48,1.94,.10),base,.02)
    label('CROPOUT RESIDENT',(-2.09,-.78,.008),.062)
    label('1.557 M  /  REFERENCE POSE',(-2.09,-.90,.008),.044)
    rx,ry=-2.91,.10
    line('Measured reference height',(rx,ry,0),(rx,ry,height))
    for z in (0,.5,1,height):line('Height ruler tick',(rx-.07,ry,z),(rx+.07,ry,z),.006)
    label('1.557 M',(rx,ry-.06,height+.10),.06,True)
    label('REAL-WORLD SCALE  /  ONE METRE IS ONE METRE',(-.25,1.24,.0),.095)
    label('DIMENSIONS: WIDTH x DEPTH x HEIGHT  /  ANCHORS ARE PROPOSALS; NO HOLD ANIMATION BOUND',(-.25,-1.27,.0),.044)
    camera_at=(2.9,-7.8,6.6);target=(-.20,.03,.35);scale=6.50;resolution=(1850,1400)
    reference=True
G.box('Studio ground',(0,0,-.19),(200,200,.16),floor,.01)
scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=24;scene.cycles.use_denoising=True
scene.render.threads_mode='FIXED';scene.render.threads=3
scene.render.resolution_x,scene.render.resolution_y=resolution;scene.render.resolution_percentage=100
scene.view_settings.view_transform='AgX'
scene.world=bpy.data.worlds.new('Soft daylight');scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.62,.70,.74,1)
scene.world.node_tree.nodes['Background'].inputs[1].default_value=.45
for name,at,power,size,color in (('Warm key',(-3,-4,7),620,5,(1,.87,.74)),('Cool fill',(4,3,6),470,4,(.79,.91,1))):
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.size=size;data.color=color
    obj=bpy.data.objects.new(name,data);scene.collection.objects.link(obj);obj.location=at
    obj.rotation_euler=(-obj.location).to_track_quat('-Z','Y').to_euler()
data=bpy.data.cameras.new('Goods camera');data.type='ORTHO';data.ortho_scale=scale
camera=bpy.data.objects.new('Goods camera',data);scene.collection.objects.link(camera);camera.location=camera_at
camera.rotation_euler=(Vector(target)-camera.location).to_track_quat('-Z','Y').to_euler();scene.camera=camera
bpy.ops.wm.save_as_mainfile(filepath=str(SAVED/('GoodsKit_'+view+'_Review.blend')))
scene.render.filepath=str(OUT/'previews'/('GoodsKit_'+view+'.png'))
bpy.ops.render.render(write_still=True)
report_path=OUT/'render-report.json'
report=json.loads(report_path.read_text(encoding='utf-8')) if report_path.exists() else {'views':{},'engine':'Cycles','device':'CPU','threads':3,'samples':24}
report['views'][view]={'file':'previews/'+Path(scene.render.filepath).name,'resolution':list(resolution),
  'seconds':round(time.monotonic()-START,3),'licensed_cropout_reference_visible':reference,
  'reference_geometry_retained_only_under_saved':True,'holding_animation_shown':False,'saved_scene_in_art':False}
if reference:report['views'][view].update({'reference_ground_to_crown_m':height,'reference_sole_above_ground_m':min(heights),'reference_vertex_extent_m':max(heights)-min(heights)})
report_path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('GOODS_PREVIEW_COMPLETE',view,flush=True)
