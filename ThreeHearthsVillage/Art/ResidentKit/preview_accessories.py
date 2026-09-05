"""Render an original-accessory contact sheet, with no Cropout body included."""
from pathlib import Path
import importlib
import json
import sys
import bpy
from mathutils import Vector

OUT=Path(__file__).resolve().parent
if str(OUT) not in sys.path:sys.path.insert(0,str(OUT))
import resident_geometry as G
importlib.reload(G)
SPECS=json.loads((OUT/'module-specs.json').read_text(encoding='utf-8'))['modules']
for obj in list(bpy.data.objects):bpy.data.objects.remove(obj,do_unlink=True)
for c in list(bpy.data.collections):bpy.data.collections.remove(c)
scene=bpy.context.scene
if Path(bpy.data.filepath).resolve()==(OUT/'ResidentKit.blend').resolve():
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT.parent.parent/'Saved/ThreeHearths/ResidentKitReference/ResidentKit_Scratch.blend'))
display=bpy.data.collections.new('Original accessories');scene.collection.children.link(display)
with bpy.data.libraries.load(str(OUT/'ResidentKit.blend'),link=False) as (src,dst):
    dst.objects=[s['id'] for s in SPECS]
assets={obj.name:obj for obj in dst.objects}
stage=bpy.data.collections.new('Presentation');scene.collection.children.link(stage)
G.configure(stage,{})
base=G.mat('Overview sage plinth','A0AD98',.67)
ink=G.mat('Overview charcoal text','273C36',.60)
for i,spec in enumerate(SPECS):
    obj=assets[spec['id']];display.objects.link(obj)
    obj.hide_render=False;obj.hide_viewport=False
    x=(i%4-1.5)*1.52;y=(1.5-i//4)*1.55
    bounds=[Vector(b) for b in obj.bound_box]
    cx=(min(v.x for v in bounds)+max(v.x for v in bounds))/2
    cy=(min(v.y for v in bounds)+max(v.y for v in bounds))/2
    zmin=min(v.z for v in bounds)
    obj.location=(x-cx,y-cy,.075-zmin)
    G.box('Display base',(x,y,-.025),(1.39,1.40,.1),base,.035)
    text=bpy.data.curves.new('Asset ID','FONT');text.body=spec['id'].replace('_',' ')
    text.size=.070;text.align_x='CENTER';text.materials.append(ink)
    label=bpy.data.objects.new('ID | '+spec['id'],text);stage.objects.link(label)
    label.location=(x,y-.62,.027)
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True
scene.render.resolution_x=1800;scene.render.resolution_y=1700;scene.render.resolution_percentage=100
scene.view_settings.view_transform='AgX'
scene.world=bpy.data.worlds.new('Overview soft sky');scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.52,.63,.71,1)
scene.world.node_tree.nodes['Background'].inputs[1].default_value=.5
for name,at,power,size,color in (('Warm key',(-5,-6,10),1700,7,(1,.87,.73)),('Cool fill',(5,5,9),1200,6,(.76,.89,1))):
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.size=size;data.color=color
    obj=bpy.data.objects.new(name,data);scene.collection.objects.link(obj);obj.location=at
    obj.rotation_euler=(-obj.location).to_track_quat('-Z','Y').to_euler()
data=bpy.data.cameras.new('Overview camera');data.type='ORTHO';data.ortho_scale=7.65
camera=bpy.data.objects.new('Overview camera',data);scene.collection.objects.link(camera)
camera.location=(1,-9,12)
camera.rotation_euler=(Vector((0,0,.1))-camera.location).to_track_quat('-Z','Y').to_euler();scene.camera=camera
scene.render.filepath=str(OUT/'previews/ResidentKit_Accessories.png')
bpy.ops.render.render(write_still=True)
print('RESIDENT_ACCESSORIES_RENDERED',flush=True)
