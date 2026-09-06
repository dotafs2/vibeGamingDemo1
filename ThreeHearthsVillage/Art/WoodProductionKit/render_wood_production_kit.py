"""Render new intermediates and an asset-reuse production illustration, using CPU."""
from pathlib import Path
import importlib
import json
import math
import sys
import time
import bpy
from mathutils import Vector,Matrix
OUT=Path(__file__).resolve().parent;SAVED=OUT.parent.parent/'Saved/ThreeHearths/WoodProductionKit'
sys.dont_write_bytecode=True
if str(OUT) not in sys.path:sys.path.insert(0,str(OUT))
import wood_geometry as G
importlib.reload(G)
args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [];view=args[0] if args else 'originals'
assert view in ('originals','production');START=time.monotonic()
for o in list(bpy.data.objects):bpy.data.objects.remove(o,do_unlink=True)
for c in list(bpy.data.collections):bpy.data.collections.remove(c)
scene=bpy.context.scene;scene.name='WoodProductionKit | '+view
bpy.ops.wm.save_as_mainfile(filepath=str(SAVED/('WoodProduction_'+view+'_Scratch.blend')))
specs=json.loads((OUT/'module-specs.json').read_text(encoding='utf-8'))
reuse=json.loads((OUT/'reuse-manifest.json').read_text(encoding='utf-8'));refs={r['reference_id']:r for r in reuse['references']}
layout=json.loads((OUT/'production-layout.json').read_text(encoding='utf-8'))
tool_specs={m['id']:m for m in json.loads((OUT.parent/'ToolKit/module-specs.json').read_text(encoding='utf-8'))['modules']}
sources=bpy.data.collections.new('READ_ONLY_SOURCE_REFERENCES');scene.collection.children.link(sources)
display=bpy.data.collections.new('DISPLAY');scene.collection.children.link(display)
stage=bpy.data.collections.new('PRESENTATION | not a runtime map');scene.collection.children.link(stage)
G.configure(stage,{})
base=G.mat('Wood process preview sage','A6B09B',.67);ink=G.mat('Wood process preview ink','293D38',.60)
floor=G.mat('Wood process preview cream','DDD8C9',.75);blue=G.mat('Wood process arrows','537F9A',.52)
assets={}

def source(key,path):
    if key in assets:return assets[key]
    before=set(scene.objects);bpy.ops.import_scene.gltf(filepath=str(path));objects=[o for o in scene.objects if o not in before and o.type=='MESH'];assert len(objects)==1
    o=objects[0];bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    for c in list(o.users_collection):c.objects.unlink(o)
    sources.objects.link(o);o.hide_render=True;o.hide_viewport=True;assets[key]=o;return o

def place(key,path,at):
    original=source(key,path);o=bpy.data.objects.new(key+' | reused display',original.data);display.objects.link(o);o.location=at;return o

def label(text,at,size=.06):
    d=bpy.data.curves.new('Caption','FONT');d.body=text;d.size=size;d.align_x='CENTER';d.materials.append(ink)
    o=bpy.data.objects.new('Caption | '+text,d);stage.objects.link(o);o.location=at

def arrow(a,b):
    a,b=Vector(a),Vector(b);delta=(b-a).normalized();side=Vector((-delta.y,delta.x,0))
    for p,q in ((a,b),(b,b-delta*.12+side*.07),(b,b-delta*.12-side*.07)):
        d=bpy.data.curves.new('Resource direction','CURVE');d.dimensions='3D';d.bevel_depth=.009;d.bevel_resolution=1
        s=d.splines.new('POLY');s.points.add(1);s.points[0].co=(*p,1);s.points[1].co=(*q,1);d.materials.append(blue)
        o=bpy.data.objects.new('Resource direction',d);stage.objects.link(o)

if view=='originals':
    for i,m in enumerate(specs['modules']):
        x=(i-.5)*1.12;place(m['id'],OUT/m['asset_glb'],(x,0,0))
        G.box('Original intermediate plinth',(x,0,-.045),(1.05,.80,.09),base,.018)
        label(m['label'].upper(),(x,-.276,.005),.047)
        label('%.3f x %.3f x %.3f M'%tuple(m['nominal_size_m']),(x,-.355,.005),.038)
    label('WOOD PRODUCTION  /  TWO MISSING INTERMEDIATE STATES',(0,.63,0),.079)
    label('EXISTING LOGS, PLANKS, BEAMS, BENCH & TOOLS ARE REUSED',(0,-.63,0),.045)
    camera_at=(2.6,-4.4,4.2);target=(0,0,.06);scale=2.98;resolution=(1700,1150)
else:
    for branch in layout['branches']:
        at=Vector(branch['bench_world_m']);ref=refs['carpenter_bench'];place('carpenter_bench',OUT/ref['asset_glb'],at)
        m=next(s for s in specs['modules'] if s['id']==branch['intermediate_id'])
        wp=at+Vector(branch['intermediate_bench_local_m']);place(m['id'],OUT/m['asset_glb'],wp)
        for key,field in ((branch['input_reference'],'input_world_m'),(branch['output_reference'],'output_world_m')):
            ref=refs[key];p=Vector(branch[field]);p.z=ref['floor_placement_z_offset_m']
            place(key,OUT/ref['asset_glb'],p)
            dims=ref['measured_size_m'];G.box('Existing stock display pad',(p.x,p.y,-.035),(max(1.06,dims[0]+.12),.70,.07),base,.018)
            label('EXISTING '+('LOG STOCK' if key=='input_logs' else ('PLANK STOCK' if key=='output_planks' else 'BEAM STOCK')),(p.x,p.y-.29,.009),.047)
        if branch['tool_reference']=='saw':
            ref=refs['saw'];o=place('saw',OUT/ref['asset_glb'],(0,0,0))
            o.location=wp+Vector(m['work_anchor_m'])-Vector(tool_specs['tool_saw']['working_tip_m'])
        else:
            ref=refs['axe'];o=place('axe',OUT/ref['asset_glb'],(0,0,0))
            rotation=Matrix.Rotation(-math.pi/2,4,'Z')@Matrix.Rotation(-math.pi/2,4,'X');o.rotation_euler=rotation.to_euler()
            bpy.context.view_layer.update();points=[o.matrix_world@Vector(p) for p in o.bound_box]
            lo=Vector([min(p[k] for p in points) for k in range(3)]);hi=Vector([max(p[k] for p in points) for k in range(3)])
            o.location=at+Vector((.20,.235,.86))-Vector(((lo.x+hi.x)/2,(lo.y+hi.y)/2,lo.z))
        label('LOGS TO '+('PLANKS' if branch['id']=='logs_to_planks' else 'TIMBER BEAMS'),(branch['output_world_m'][0],at.y-.62,.006),.064)
        arrow((-1.0,at.y-.53,.014),(-.50,at.y-.53,.014));arrow((.57,at.y-.53,.014),(1.08,at.y-.53,.014))
    label('REUSE-FIRST WOODWORK  /  TWO PRODUCTION BRANCHES',(.20,2.45,.0),.102)
    label('NEW: TWO IN-PROCESS PIECES  /  EXISTING STOCK, BENCH & TOOLS PRESERVED',(.20,-1.90,.0),.055)
    label('STATIC ASSET ILLUSTRATION  /  NO RECIPE, INVENTORY OR WORK ANIMATION IMPLEMENTED HERE',(.20,-2.09,.0),.043)
    camera_at=(4.6,-7.5,8.6);target=(.20,0,.30);scale=6.85;resolution=(1900,1450)
G.box('Studio ground',(0,0,-.17),(200,200,.16),floor,.008)
scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=24;scene.cycles.use_denoising=True
scene.render.threads_mode='FIXED';scene.render.threads=3
scene.render.resolution_x,scene.render.resolution_y=resolution;scene.render.resolution_percentage=100;scene.view_settings.view_transform='AgX'
scene.world=bpy.data.worlds.new('Soft daylight');scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.62,.70,.74,1);scene.world.node_tree.nodes['Background'].inputs[1].default_value=.45
for name,at,power,size,color in (('Warm key',(-3,-4,7),620,5,(1,.87,.74)),('Cool fill',(4,3,6),470,4,(.79,.91,1))):
    d=bpy.data.lights.new(name,'AREA');d.energy=power;d.size=size;d.color=color
    o=bpy.data.objects.new(name,d);scene.collection.objects.link(o);o.location=at;o.rotation_euler=(-o.location).to_track_quat('-Z','Y').to_euler()
d=bpy.data.cameras.new('Wood process camera');d.type='ORTHO';d.ortho_scale=scale
c=bpy.data.objects.new('Wood process camera',d);scene.collection.objects.link(c);c.location=camera_at
c.rotation_euler=(Vector(target)-c.location).to_track_quat('-Z','Y').to_euler();scene.camera=c
bpy.ops.wm.save_as_mainfile(filepath=str(SAVED/('WoodProduction_'+view+'_Review.blend')))
scene.render.filepath=str(OUT/'previews'/('WoodProduction_'+view+'.png'));bpy.ops.render.render(write_still=True)
path=OUT/'render-report.json';report=json.loads(path.read_text(encoding='utf-8')) if path.exists() else {'engine':'Cycles','device':'CPU','threads':3,'samples':24,'views':{}}
report['views'][view]={'file':'previews/'+Path(scene.render.filepath).name,'resolution':list(resolution),'seconds':round(time.monotonic()-START,3),
 'source_assets_reused_by_path':view=='production','existing_source_files_modified':False,'runtime_integration_shown':False}
path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('WOOD_PRODUCTION_PREVIEW_COMPLETE',view,flush=True)
