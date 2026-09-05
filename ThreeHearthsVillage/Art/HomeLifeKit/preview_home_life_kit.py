"""Render original modules and two layouts from the same placement contract."""
from pathlib import Path
import importlib
import json
import math
import sys
import time
import bpy
from mathutils import Vector

OUT=Path(__file__).resolve().parent
if str(OUT) not in sys.path:sys.path.insert(0,str(OUT))
import home_geometry as G
importlib.reload(G)
SPECS=json.loads((OUT/'module-specs.json').read_text(encoding='utf-8'))
MODULES={m['id']:m for m in SPECS['modules']}
LAYOUTS=json.loads((OUT/'example-layouts.json').read_text(encoding='utf-8'))
ARGS=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
view=ARGS[0] if ARGS else 'overview'
START=time.monotonic()
for obj in list(bpy.data.objects):bpy.data.objects.remove(obj,do_unlink=True)
for c in list(bpy.data.collections):bpy.data.collections.remove(c)
scene=bpy.context.scene;scene.name='HomeLifeKit | '+view
if Path(bpy.data.filepath).resolve()==(OUT/'HomeLifeKit.blend').resolve():
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'examples/PreviewScratch.blend'))
sources=bpy.data.collections.new('SOURCE_REFERENCES');scene.collection.children.link(sources)
with bpy.data.libraries.load(str(OUT/'HomeLifeKit.blend'),link=False) as (src,dst):
    dst.objects=list(MODULES)
assets={obj.name:obj for obj in dst.objects}
for obj in dst.objects:sources.objects.link(obj);obj.hide_render=True;obj.hide_viewport=True
display=bpy.data.collections.new('LAYOUT_GEOMETRY');scene.collection.children.link(display)
stage=bpy.data.collections.new('PRESENTATION | no gameplay collision');scene.collection.children.link(stage)
G.configure(stage,{})
base=G.mat('Home preview sage plinth','A0AD98',.67)
ink=G.mat('Home preview charcoal text','273C36',.60)
blue=G.mat('Approach reference teal','5E9297',.50)
cream=G.mat('Seat reference cream','E9DCB9',.55)
stone=G.mat('Home preview blue-grey edge','98A5AC',.55)
export_objects=[];reused=[]

def label(text,at,size=.12,vertical=False):
    data=bpy.data.curves.new('Caption','FONT');data.body=text;data.size=size;data.align_x='CENTER';data.materials.append(ink)
    obj=bpy.data.objects.new('Caption | '+text,data);stage.objects.link(obj);obj.location=at
    if vertical:obj.rotation_euler=(math.pi/2,0,0)
    return obj

def curve(name,points,radius=.009,closed=False):
    data=bpy.data.curves.new(name,'CURVE');data.dimensions='3D';data.bevel_depth=radius;data.bevel_resolution=1
    spline=data.splines.new('POLY');spline.points.add(len(points)-1)
    for p,v in zip(spline.points,points):p.co=(*v,1)
    spline.use_cyclic_u=closed;data.materials.append(blue)
    obj=bpy.data.objects.new(name,data);stage.objects.link(obj);return obj

def reuse(mid,at,yaw=0):
    source_path=OUT.parent/'VillageKit/modules'/(mid+'.glb')
    before=set(scene.objects)
    bpy.ops.import_scene.gltf(filepath=str(source_path))
    objects=[o for o in scene.objects if o not in before and o.type=='MESH']
    assert len(objects)==1
    obj=objects[0]
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True)
    bpy.context.view_layer.objects.active=obj
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    obj.rotation_mode='XYZ';obj.location=at;obj.rotation_euler=(0,0,math.radians(yaw))
    export_objects.append(obj)
    reused.append({'kit':'VillageKit','id':mid,'source_path':'../VillageKit/modules/'+mid+'.glb','position_m':list(at),'yaw_degrees':yaw})

if view=='overview':
    for i,spec in enumerate(SPECS['modules']):
        obj=bpy.data.objects.new(spec['id'],assets[spec['id']].data);display.objects.link(obj)
        x=(i%4-1.5)*3.15;y=(1-i//4)*3.25;obj.location=(x,y,0)
        G.box('Original module display',(x,y,-.085),(3.02,3.02,.17),base,.035)
        label(spec['id'].replace('_',' '),(x,y-1.35,.01),.13)
    camera_at=(7,-14,17);target=(0,0,.25);scale=15.4;resolution=(2300,1800)
else:
    example=next(x for x in LAYOUTS['examples'] if x['id']==view)
    for p in example['placements']:
        obj=bpy.data.objects.new(p['instance_id'],assets[p['module_id']].data);display.objects.link(obj)
        obj.location=p['position_m'];obj.rotation_euler.z=math.radians(p['yaw_degrees'])
        obj['module_id']=p['module_id'];obj['creates_inventory']=False
        export_objects.append(obj)
    bpy.context.view_layer.update()
    for p in example['placements']:
        obj=bpy.data.objects[p['instance_id']]
        for interaction in MODULES[p['module_id']]['interaction_points']:
            if interaction['id'] in p.get('disabled_interactions',[]):continue
            at=obj.matrix_world@Vector(interaction['position_m'])
            if interaction['kind']=='approach_ground':
                radius=interaction['clearance_radius_m']
                curve('Standing clearance | '+p['instance_id'],[(at.x+radius*math.cos(j*math.tau/32),at.y+radius*math.sin(j*math.tau/32),.008) for j in range(32)],closed=True)
            elif interaction['kind']=='sitting_contact':
                G.cylinder('Seat contact marker',(at.x,at.y,at.z+.009),.055,.012,cream,20)
    if view=='cabin_living_4x4m':
        for x in (-1,1):
            for y in (-1,1):reuse('floor_timber_2m',(x,y,-.16))
        # VillageKit's structural datum is .16m below its finished floor. Shift
        # the complete reference shell so these furniture contracts use floor Z=0.
        for x in (-1,1):reuse('wall_window_plaster_2m',(x,2,-.16))
        for y in (-1,1):reuse('wall_plaster_2m',(-2,y,-.16),90)
        for x,y in ((-2,-2),(-2,0),(-2,2),(0,2),(2,2)):reuse('post_timber_2_4m',(x,y,-.16))
        G.box('Cutaway foundation',(0,0,-.23),(4.22,4.22,.14),stone,.035)
        label('4M X 4M  /  LIVING CORNER',(0,-2.53,-.14),.17)
        label('1.05M CENTRAL AISLE',(-.16,-.15,.014),.095)
        ruler_x,ruler_y=2.48,1.35
        camera_at=(6,-8,7);target=(0,0,.75);scale=7.8;resolution=(1900,1700)
    else:
        G.box('Public dining ground',(0,.1,-.13),(9.8,5.8,.26),base,.055)
        label('10 SEATS  /  COMMON TABLE',(0,-3.26,-.12),.22)
        label('FOOD VISUALS REQUIRE EXISTING STOCK',(0,-3.60,-.12),.13)
        ruler_x,ruler_y=4.85,2.3
        camera_at=(9,-12,11);target=(0,.05,.38);scale=13.6;resolution=(2200,1650)
    curve('Measured resident height',[(ruler_x,ruler_y,0),(ruler_x,ruler_y,1.557142)],.013)
    for z in (0,.5,1,1.557142):curve('Height tick',[(ruler_x-.10,ruler_y,z),(ruler_x+.10,ruler_y,z)],.009)
    label('RESIDENT 1.557M',(ruler_x,ruler_y-.07,1.72),.11,True)
    bpy.ops.object.select_all(action='DESELECT')
    for obj in export_objects:obj.select_set(True)
    bpy.context.view_layer.objects.active=export_objects[0]
    bpy.ops.export_scene.gltf(filepath=str(OUT/'examples'/(view+'.glb')),export_format='GLB',use_selection=True,
        export_yup=True,export_apply=True,export_texcoords=True,export_normals=True,export_materials='EXPORT',
        export_extras=True,export_animations=False,export_cameras=False,export_lights=False)

scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True
scene.render.resolution_x,scene.render.resolution_y=resolution;scene.render.resolution_percentage=100
scene.view_settings.view_transform='AgX'
scene.world=bpy.data.worlds.new('Home soft sky');scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.52,.63,.71,1)
scene.world.node_tree.nodes['Background'].inputs[1].default_value=.5
for name,at,power,size,color in (('Warm key',(-7,-9,14),2200,9,(1,.87,.73)),('Cool fill',(8,6,12),1700,7,(.76,.89,1))):
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.size=size;data.color=color
    obj=bpy.data.objects.new(name,data);scene.collection.objects.link(obj);obj.location=at
    obj.rotation_euler=(-obj.location).to_track_quat('-Z','Y').to_euler()
data=bpy.data.cameras.new('Home life camera');data.type='ORTHO';data.ortho_scale=scale
camera=bpy.data.objects.new('Home life camera',data);scene.collection.objects.link(camera);camera.location=camera_at
camera.rotation_euler=(Vector(target)-camera.location).to_track_quat('-Z','Y').to_euler();scene.camera=camera
if view!='overview':bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'examples'/(view+'.blend')))
scene.render.filepath=str(OUT/'previews'/('HomeLifeKit_'+view+'.png'))
bpy.ops.render.render(write_still=True)
path=OUT/'preview-report.json'
report=json.loads(path.read_text(encoding='utf-8')) if path.exists() else {'views':{},'cropout_character_mesh_copied':False}
report['views'][view]={'file':'previews/'+Path(scene.render.filepath).name,'seconds':round(time.monotonic()-START,3),
    'reference_modules':reused,'layout_placements_match_json':view!='overview','interaction_markers_excluded_from_glb':True}
path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('HOME_LIFE_PREVIEW_COMPLETE',view,flush=True)
