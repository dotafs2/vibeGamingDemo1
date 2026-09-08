"""Render the actual authored geometry for review; never alters source assets."""
import bpy
import json
import math
import sys
from pathlib import Path
from mathutils import Vector

ROOT=Path(__file__).resolve().parent
bpy.ops.wm.open_mainfile(filepath=str(ROOT/'OrganicVillage_Masters.blend'))
scene=bpy.context.scene
catalog=json.loads((ROOT/'catalog.json').read_text(encoding='utf-8'))
args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
only=set(args[args.index('--only')+1].split(',')) if '--only' in args else set()
cam=scene.camera
scene.cycles.samples=24
scene.render.threads=6
for c in bpy.data.collections:
    if c.name.startswith('HOUSE |'):c.hide_render=True
review=bpy.data.collections.new('REVIEW temporary');scene.collection.children.link(review)
lights=[o for o in bpy.data.objects if o.type=='LIGHT']
for light,offset in zip(lights,[(-4,-5,8),(6,-2,6),(-4,6,7)]):
    light.location=offset
    light.data.energy*=.55
    light.rotation_euler=(Vector((0,0,1))-light.location).to_track_quat('-Z','Y').to_euler()

def copy_module(mid,position=(0,0,0),only=None,palette=None):
    root=bpy.data.objects.new('review_'+mid,None);review.objects.link(root);root.location=position
    for src in bpy.data.objects[mid].children_recursive:
        if src.type!='MESH' or (only and src.get('layer') not in only):continue
        ob=src.copy();review.objects.link(ob);ob.parent=root
        if palette and ob.get('layer') in ('finish','weathering'):
            ob.data=src.data.copy()
            for i,mat in enumerate(ob.data.materials):
                role=mat.get('semantic_slot','')
                alternate=bpy.data.materials.get(palette+' | '+role)
                if alternate:ob.data.materials[i]=alternate
    return root

def clear_review():
    for ob in list(review.objects):bpy.data.objects.remove(ob,do_unlink=True)

def point_camera(target,offset,scale):
    target=Vector(target);cam.location=target+Vector(offset)
    cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler()
    cam.data.ortho_scale=scale

thumbs=ROOT/'Previews'/'Modules';thumbs.mkdir(exist_ok=True)
scene.render.resolution_x=500;scene.render.resolution_y=430
for entry in catalog['modules']:
    if only and entry['id'] not in only:continue
    clear_review();root=copy_module(entry['id'])
    b=entry['bounds_m'];mid=Vector([(b['min'][i]+b['max'][i])/2 for i in range(3)])
    root.location.z=.02-b['min'][2]
    mid.z+=root.location.z
    point_camera(mid,(5,-8,5),max(b['size'][0]*1.35,b['size'][1]*1.4,b['size'][2]*1.65,1.35))
    scene.render.filepath=str(thumbs/(entry['id']+'.png'))
    bpy.ops.render.render(write_still=True)

# Same wall, four genuinely editable states. Structure is held fixed in state 4.
if only and 'layers' not in only:
    print('SELECTED_THUMBNAILS_COMPLETE',flush=True)
    raise SystemExit(0)
clear_review()
for i in range(4):
    offset=(i*2.8,0,0)
    copy_module('wall_window_2m',offset,only=['structure'] if i==0 else None,palette='sage_clay' if i==3 else None)
    if i>=2:
        for mid in ('overlay_lime_repair','attachment_flowerbox','overlay_ivy'):
            copy_module(mid,offset,palette='sage_clay' if i==3 else None)
scene.render.resolution_x=2800;scene.render.resolution_y=1050;scene.cycles.samples=40
point_camera((4.2,0,1.14),(2,-18,5),12.7)
for light,offset in zip(lights,[(2,-5,9),(9,-3,6),(4,6,8)]):
    light.location=offset;light.data.energy*=2
    light.rotation_euler=(Vector((4.2,0,1))-light.location).to_track_quat('-Z','Y').to_euler()
scene.render.filepath=str(ROOT/'Previews'/'Material_Layers_Raw.png')
bpy.ops.render.render(write_still=True)
if only:
    print('SELECTED_REVIEW_COMPLETE',flush=True)
    raise SystemExit(0)

# Rear views catch seams and misplaced fixtures hidden by the hero cameras.
clear_review();scene.render.resolution_x=1200;scene.render.resolution_y=1080
for entry in catalog['assemblies']:
    root=bpy.data.objects[entry['id']]
    root_col=root.users_collection[0];root_col.hide_render=False
    b=entry['bounds_m'];origin=root.location.copy()
    mid=origin+Vector([(b['min'][i]+b['max'][i])/2 for i in range(3)])
    point_camera(mid,(-11,15,10),max(11.7,b['size'][0]*1.4,b['size'][1]*1.28,b['size'][2]*1.82))
    for light,offset in zip(lights,[(-5,-7,13),(10,-3,9),(-6,9,12)]):
        light.location=origin+Vector(offset)
        light.rotation_euler=(mid-light.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath=str(ROOT/'Previews'/(entry['id']+'_rear.png'))
    bpy.ops.render.render(write_still=True)
    root_col.hide_render=True
print('REVIEW_RENDERS_COMPLETE',flush=True)
