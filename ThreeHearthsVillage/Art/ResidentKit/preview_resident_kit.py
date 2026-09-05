"""Render ten adult look proposals on the measured existing Cropout body.

Reference-containing blend is saved only under Saved, never as new originals.
"""
from pathlib import Path
import importlib
import json
import math
import sys
import time
import bpy
from mathutils import Vector, Matrix

OUT=Path(__file__).resolve().parent
PROJECT=OUT.parent.parent
REFDIR=PROJECT/'Saved/ThreeHearths/ResidentKitReference'
if str(OUT) not in sys.path:sys.path.insert(0,str(OUT))
import resident_geometry as G
importlib.reload(G)
START=time.monotonic()
LOOKS=json.loads((OUT/'looks.json').read_text(encoding='utf-8'))['looks']
SPECS=json.loads((OUT/'module-specs.json').read_text(encoding='utf-8'))
SPEC={s['id']:s for s in SPECS['modules']}
BONES={b['name']:b for b in json.loads((OUT/'attachment-reference.json').read_text(encoding='utf-8'))['bones']}
ARGS=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
view=ARGS[0] if ARGS else 'front'
for obj in list(bpy.data.objects):bpy.data.objects.remove(obj,do_unlink=True)
for c in list(bpy.data.collections):bpy.data.collections.remove(c)
scene=bpy.context.scene;scene.name='ResidentKit reference-pose fitting review'
if Path(bpy.data.filepath).resolve()==(OUT/'ResidentKit.blend').resolve():
    bpy.ops.wm.save_as_mainfile(filepath=str(REFDIR/'ResidentKit_Scratch.blend'))
assets=bpy.data.collections.new('ACCESSORY_REFERENCES');scene.collection.children.link(assets)
with bpy.data.libraries.load(str(OUT/'ResidentKit.blend'),link=False) as (src,dst):
    dst.objects=[s['id'] for s in SPECS['modules']]
accessories={}
for obj in dst.objects:
    assets.objects.link(obj);accessories[obj.name]=obj
    obj.hide_render=True;obj.hide_viewport=True

bpy.ops.import_scene.fbx(filepath=str(REFDIR/'SKM_Villager.fbx'),automatic_bone_orientation=False)
body=next(o for o in scene.objects if o.type=='MESH' and o.name.startswith('SKM_Villager'))
world=body.matrix_world.copy()
for v in body.data.vertices:v.co=world@v.co
body.parent=None;body.matrix_world=Matrix.Identity(4)
for mod in list(body.modifiers):body.modifiers.remove(mod)
for obj in list(scene.objects):
    if obj.type=='ARMATURE':bpy.data.objects.remove(obj,do_unlink=True)
body.hide_render=True;body.hide_viewport=True

# New accessories are reused by data linkage. Existing SocietyKit references are
# imported from their original paths, not copied to this kit's modules directory.
external={}
for look in LOOKS:
    for ref in look['references']:
        if ref.get('display_only') or ref['id'] in external:continue
        before=set(scene.objects)
        bpy.ops.import_scene.gltf(filepath=str(OUT.parent/ref['kit']/'modules'/(ref['id']+'.glb')))
        imported=[o for o in scene.objects if o not in before and o.type=='MESH']
        if len(imported)!=1:raise RuntimeError('Expected one reusable attachment mesh')
        obj=imported[0]
        bpy.context.view_layer.objects.active=obj
        bpy.ops.object.select_all(action='DESELECT');obj.select_set(True)
        bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
        external[ref['id']]=obj;obj.hide_render=True;obj.hide_viewport=True

display=bpy.data.collections.new('LOOKS | licensed reference body plus original accessories')
scene.collection.children.link(display)
stage=bpy.data.collections.new('PRESENTATION');scene.collection.children.link(stage)
G.configure(stage,{'wood':G.mat('Review warm wood','916547',.48)})
ground=G.mat('Review sage plinth','9CAC91',.65)
ink=G.mat('Review label ink','263C36',.60)
eyes=G.mat('Review eye charcoal','302D28',.30)
boots=G.mat('Review leather boots','594337',.52)
fitting=[]
for i,look in enumerate(LOOKS):
    offset=Vector(((i%5-2)*2.55,(.5-i//5)*2.9,0))
    figure=bpy.data.objects.new('Cropout reference | '+look['id'],body.data.copy())
    display.objects.link(figure);figure.location=offset
    figure.data.materials.clear()
    skin=G.mat('Review skin | '+look['id'],look['skin_color'],.58)
    outfit=G.mat('Review tunic | '+look['id'],look['body_color'],.58)
    for m in (outfit,skin,eyes,boots):figure.data.materials.append(m)
    group_names={g.index:g.name for g in body.vertex_groups}
    for polygon in figure.data.polygons:
        weights={}
        for index in polygon.vertices:
            for g in body.data.vertices[index].groups:
                key=group_names[g.group];weights[key]=weights.get(key,0)+g.weight
        dominant=max(weights,key=weights.get) if weights else ''
        if dominant.startswith(('eye_','brow_')):polygon.material_index=2
        elif dominant.startswith(('head','nose','neck','hand','thumb','index','lowerarm')):polygon.material_index=1
        elif dominant.startswith(('foot','ball','calf')):polygon.material_index=3
        else:polygon.material_index=0
    placed=[]
    for mid in look['attachments']:
        obj=bpy.data.objects.new(look['id']+' | '+mid,accessories[mid].data)
        display.objects.link(obj);obj.location=offset+Vector(BONES[SPEC[mid]['bone']]['head_m'])
        obj['bone']=SPEC[mid]['bone'];placed.append({'id':mid,'bone':SPEC[mid]['bone']})
    for ref in look['references']:
        if ref.get('display_only'):continue
        obj=bpy.data.objects.new(look['id']+' | referenced '+ref['id'],external[ref['id']].data)
        display.objects.link(obj);obj.location=offset+Vector(BONES[ref['bone']]['head_m'])+Vector(ref['position_m'])
        obj.scale=(ref['scale'],)*3;placed.append({'id':ref['id'],'kit':ref['kit'],'bone':ref['bone']})
    plinth=G.box('Look review base',(offset.x,offset.y,-.06),(2.18,2.10,.12),ground,.04)
    data=bpy.data.curves.new('Look caption','FONT')
    data.body=look['id'].replace('_01','').upper()+' / '+str(look['adult_age'])+' '+('F' if look['presentation']=='female' else 'M')
    data.size=.14;data.align_x='CENTER';data.materials.append(ink)
    label=bpy.data.objects.new('Caption',data);stage.objects.link(label);label.location=(offset.x,offset.y-1.0,.01)
    fitting.append({'look_id':look['id'],'adult_age':look['adult_age'],'attachments':placed,
        'reference_pose_only':True,'animation_binding_verified':False})

scene.render.engine='CYCLES';scene.cycles.samples=24;scene.cycles.use_denoising=True
scene.render.resolution_x=2400;scene.render.resolution_y=1300;scene.render.resolution_percentage=100
scene.view_settings.view_transform='AgX'
scene.world=bpy.data.worlds.new('Review soft sky');scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.50,.64,.72,1)
scene.world.node_tree.nodes['Background'].inputs[1].default_value=.45
for name,at,power,size,color in (('Warm key',(-7,-10,15),2400,10,(1,.87,.72)),('Cool fill',(9,5,12),1800,8,(.73,.89,1))):
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.size=size;data.color=color
    light=bpy.data.objects.new(name,data);scene.collection.objects.link(light);light.location=at
    light.rotation_euler=(-light.location).to_track_quat('-Z','Y').to_euler()
data=bpy.data.cameras.new('Fitting camera');data.type='ORTHO';data.ortho_scale=15.0
camera=bpy.data.objects.new('Fitting camera',data);scene.collection.objects.link(camera)
camera.location=(3,-15,12) if view=='front' else (-3,15,12)
camera.rotation_euler=(Vector((0,0,.7))-camera.location).to_track_quat('-Z','Y').to_euler();scene.camera=camera
bpy.ops.wm.save_as_mainfile(filepath=str(REFDIR/('ResidentKit_Fitting_'+view+'.blend')))
scene.render.filepath=str(OUT/'previews'/('ResidentKit_TenAdults_'+view+'.png'))
bpy.ops.render.render(write_still=True)
path=OUT/'fitting-report.json'
report=json.loads(path.read_text(encoding='utf-8')) if path.exists() else {'scope':'existing Cropout reference-pose visual fit','looks':fitting,'views':{}}
report['views'][view]={'preview':'previews/'+Path(scene.render.filepath).name,'seconds':round(time.monotonic()-START,3)}
path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('RESIDENT_FITTING_RENDERED',view,flush=True)
