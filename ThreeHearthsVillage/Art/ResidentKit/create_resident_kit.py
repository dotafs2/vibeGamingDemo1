"""Original static accessories fitted to measured Cropout reference bones.

Blender/MCP generation only; no character animation binding is performed.
"""
from pathlib import Path
import importlib
import json
import math
import sys
import time
import bpy
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree

OUT=Path(__file__).resolve().parent
if str(OUT) not in sys.path:sys.path.insert(0,str(OUT))
import resident_geometry as G
importlib.reload(G)
SPECS=json.loads((OUT/'module-specs.json').read_text(encoding='utf-8'))
REF=json.loads((OUT/'attachment-reference.json').read_text(encoding='utf-8'))
BONES={b['name']:b for b in REF['bones']}
surface_path=OUT.parent.parent/'Saved/ThreeHearths/ResidentKitReference/body-surface.json'
surface=json.loads(surface_path.read_text(encoding='utf-8'))
BODY_BVH=BVHTree.FromPolygons([Vector(v) for v in surface['vertices_m']],surface['triangles'],all_triangles=True)
HEAD=Vector(BONES['head']['head_m'])
FIT_SAMPLES=[]
START=time.monotonic()
for folder in ('modules','previews'):(OUT/folder).mkdir(parents=True,exist_ok=True)
for old in list(bpy.data.objects):bpy.data.objects.remove(old,do_unlink=True)
for old in list(bpy.data.collections):bpy.data.collections.remove(old)
for old in list(bpy.data.meshes):bpy.data.meshes.remove(old)
for old in list(bpy.data.materials):bpy.data.materials.remove(old)
for old in list(bpy.data.armatures):bpy.data.armatures.remove(old)
for old in list(bpy.data.actions):bpy.data.actions.remove(old)
# Fitting previews append this source library. Remove its now-unused library
# records before saving the regenerated source to the same path.
for old in list(bpy.data.libraries):bpy.data.libraries.remove(old)
scene=bpy.context.scene;scene.name='ResidentKit | original accessories'
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
MODULES=bpy.data.collections.new('ORIGINAL_ACCESSORIES');scene.collection.children.link(MODULES)
M={
    'wood':G.mat('RK_Warm_Oak','BC9368',.43),
    'wood_light':G.mat('RK_Warm_Oak_Light','D0AC7A',.44),
    'dark':G.mat('RK_Hair_Dark','41352F',.53),
    'chestnut':G.mat('RK_Hair_Chestnut','78503A',.52),
    'auburn':G.mat('RK_Hair_Auburn','A76547',.51),
    'silver':G.mat('RK_Hair_Silver','B7B5A7',.55),
    'silver_light':G.mat('RK_Hair_Silver_Edge','CFCCBE',.53),
    'sage':G.mat('RK_Sage_Cloth','779F97',.55),
    'linen':G.mat('RK_Cream_Linen','E9DCB9',.61),
    'straw':G.mat('RK_Golden_Straw','CAA771',.57),
    'plum':G.mat('RK_Plum_Cloth','816D85',.55),
    'leather':G.mat('RK_Chestnut_Leather','916547',.48),
    'leather_dark':G.mat('RK_Dark_Leather','594337',.52),
    'blue':G.mat('RK_Royal_Blue','537F9A',.54),
    'ochre':G.mat('RK_Ochre_Cloth','BB9655',.56),
    'red':G.mat('RK_Muted_Red_Cloth','B56157',.55),
    'brass':G.mat('RK_Brass','B99550',.30,.7),
}
G.configure(MODULES,M)
box,beam,mesh=G.box,G.beam,G.mesh
ASSETS={}
REPORT={'schema_version':1,'kit_id':'resident_kit_01','blender_version':bpy.app.version_string,
    'source_body_height_m':1.557142,'modules':[],'animation_binding_verified':False}


def ellipsoid(name,at,scale,material,subdivisions=2):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=subdivisions,radius=1,location=at)
    obj=G.adopt(bpy.context.view_layer.objects.active,material)
    obj.name=name;obj.scale=scale
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    G.uv_project(obj.data)
    for p in obj.data.polygons:p.use_smooth=True
    return obj


def shell(name,radii,center,material,phi_fn,segments=24,rows=7,thickness=.012,fit_head=False):
    # No coincident pole ring: one apex and proper ring fans avoid degenerates.
    rx,ry,rz=radii;cx,cy,cz=center
    verts=[(cx,cy,cz+rz)]
    for row in range(1,rows+1):
        for j in range(segments):
            a=j*math.tau/segments;p=phi_fn(a)*row/rows
            verts.append((cx+rx*math.sin(p)*math.cos(a),cy+ry*math.sin(p)*math.sin(a),cz+rz*math.cos(p)))
    if fit_head:
        fitted=[]
        origin=HEAD+Vector(center)
        for vertex in verts:
            direction=(Vector(vertex)-Vector(center)).normalized()
            hit,normal,index,distance=BODY_BVH.ray_cast(origin,direction,1.0)
            if hit is None:raise RuntimeError('Missing measured scalp intersection')
            fitted.append(hit-HEAD+direction*.026)
            FIT_SAMPLES.append({'distance_m':float(distance),'surface_clearance_m':.026})
        verts=fitted
    faces=[(0,1+j,1+(j+1)%segments) for j in range(segments)]
    for row in range(rows-1):
        for j in range(segments):
            a=1+row*segments+j;b=1+row*segments+(j+1)%segments
            faces.append((a,b,b+segments,a+segments))
    obj=mesh(name,verts,faces,material,range(len(faces)))
    bpy.context.view_layer.objects.active=obj
    mod=obj.modifiers.new('Accessory shell thickness','SOLIDIFY');mod.thickness=thickness
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj


def hair(color,kind='crop'):
    shell('Open-face hair cap',(.245,.275,.247),(0,.012,.060),M[color],
          lambda a:1.72-max(0,-math.sin(a))*.78,segments=48,rows=14,fit_head=True)
    if kind=='waves':
        for j in range(7):
            a=math.radians(15+j*25)
            ellipsoid('Soft hair wave',(.235*math.cos(a),.245*math.sin(a),.18+(j%2)*.025),(.065,.070,.085),M[color])
    elif kind=='braid':
        for j in range(7):
            ellipsoid('Braided hair section',((.015 if j%2 else -.015),.29+j*.012,.07-j*.066),(.056,.059,.055),M[color])
        box('Braid tie',(0,.374,-.35),(.083,.066,.035),M['sage'],.012)
    elif kind=='bun':
        ellipsoid('Bound hair bun',(0,.293,.13),(.116,.108,.112),M[color])
        beam('Bun pin',(-.13,.30,.14),(.13,.30,.14),.014,.014,M['wood_light'])
    elif kind=='swept':
        for j in range(5):
            ellipsoid('Swept silver lock',(-.14+j*.068,-.045,.276),(.088,.102,.039),M['silver_light'] if j%2 else M['silver'])


def beard():
    for x in (-.115,-.057,0,.057,.115):
        ellipsoid('Neat chin beard',(x,-.227-abs(x)*.22,-.09+abs(x)*.26),(.060,.063,.085-abs(x)*.18),M['silver'])
    for sign in (-1,1):
        ellipsoid('Short moustache',(sign*.053,-.280,-.021),(.057,.027,.026),M['silver_light'])


def headwrap():
    shell('Sage cloth headwrap',(.253,.281,.259),(0,.012,.060),M['sage'],lambda a:1.65-max(0,-math.sin(a))*.72,segments=48,rows=14,fit_head=True)
    ellipsoid('Headwrap knot',(.045,.30,.11),(.065,.060,.055),M['sage'])
    for x,z in ((.02,-.08),(.085,-.04)):
        box('Headwrap tail',(x,.318,z),(.07,.032,.28),M['sage'],.015)


def ring(name,inner,outer,z,height,material,n=32,yscale=1):
    verts=[(r*math.cos(j*math.tau/n),r*math.sin(j*math.tau/n)*yscale,zz)
           for zz in (z,z+height) for r in (inner,outer) for j in range(n)]
    faces=[]
    for j in range(n):
        k=(j+1)%n
        faces.extend(((j,k,n+k,n+j),(2*n+j,3*n+j,3*n+k,2*n+k),
                      (j,2*n+j,2*n+k,k),(n+j,n+k,3*n+k,3*n+j)))
    return mesh(name,verts,faces,material)


def straw_hat():
    ring('Wide straw brim',.239,.425,.199,.026,M['straw'],40,1.06)
    shell('Straw hat crown',(.255,.282,.185),(0,.010,.205),M['straw'],lambda a:math.pi/2,32,6,.012)
    ring('Leather hat band',.254,.265,.235,.052,M['leather'],32,1.09)


def merchant_cap():
    shell('Soft plum beret',(.282,.292,.142),(.02,.012,.280),M['plum'],lambda a:1.87,32,7,.014)
    ring('Beret brow band',.235,.251,.18,.050,M['leather_dark'],32,1.10)
    ellipsoid('Beret top button',(.05,.012,.426),(.027,.027,.020),M['plum'])


def bag():
    box('Crossbody satchel',(.44,-.22,-.065),(.24,.16,.30),M['leather'],.045)
    box('Satchel flap',(.44,-.308,.025),(.25,.035,.16),M['leather_dark'],.025)
    box('Satchel clasp',(.44,-.334,-.005),(.04,.012,.045),M['brass'],.006)
    beam('Chest diagonal strap',(-.21,-.292,.23),(.44,-.338,-.03),.047,.025,M['leather'])
    beam('Shoulder strap',(-.21,-.292,.23),(-.21,.36,.27),.047,.025,M['leather'])
    beam('Back diagonal strap',(-.21,.36,.27),(.44,.365,-.03),.047,.025,M['leather'])
    beam('Satchel side strap',(.44,.365,-.03),(.44,-.22,-.03),.047,.025,M['leather'])


def backpack():
    box('Leather backpack',(0,.47,.04),(.47,.22,.53),M['leather'],.055)
    box('Backpack rear pocket',(0,.594,-.07),(.31,.045,.22),M['leather_dark'],.022)
    for x in (-.13,.13):
        beam('Backpack strap',(x,.40,.31),(x,-.295,.19),.042,.025,M['leather_dark'])
        box('Backpack buckle',(x,.60,.12),(.05,.02,.08),M['brass'],.01)
    roll=G.cylinder('Linen bedroll',(0,.48,.365),.092,.58,M['linen'],16,(1,0,0))
    for x in (-.17,.17):
        box('Bedroll tie',(x,.48,.46),(.032,.15,.022),M['leather_dark'],.006)


def apron():
    verts=[(x,y,z) for y in (-.361,-.344) for x,z in ((-.17,.18),(.17,.18),(.29,-.38),(-.29,-.38))]
    mesh('Short linen apron',verts,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],M['linen'])
    for x in (-.13,.13):beam('Apron neck strap',(x,-.348,.16),(x*.75,-.175,.31),.031,.019,M['linen'])
    box('Linen apron pocket',(0,-.380,-.17),(.25,.025,.16),M['straw'],.017)


def cape():
    verts=[(x,y,z) for x,y,z in ((-.18,.20,.31),(.18,.20,.31),(.32,.38,.10),(-.32,.38,.10),(.35,.45,-.38),(-.35,.45,-.38))]
    obj=mesh('Royal shoulder cape',verts,[(0,1,2,3),(3,2,4,5)],M['blue'])
    bpy.context.view_layer.objects.active=obj
    mod=obj.modifiers.new('Cape thickness','SOLIDIFY');mod.thickness=.018
    bpy.ops.object.modifier_apply(modifier=mod.name)
    for x in (-.17,.17):ellipsoid('Cape shoulder fastener',(x,-.17,.27),(.035,.027,.035),M['brass'],1)
    for x in (-.17,.17):beam('Cape shoulder band',(x,-.17,.27),(x,.17,.30),.055,.024,M['blue'])


def shawl():
    n=48;rows=10;verts=[]
    bone=Vector(BONES['spine_02']['head_m']);center=Vector((0,.02,.08))
    for row in range(rows+1):
        t=row/rows;rx=.14+.29*t;ry=.22+.18*t;z=.32-.325*t
        for j in range(n+1):
            raw=Vector((rx*math.cos(j*math.pi/n),ry*math.sin(j*math.pi/n),z))
            direction=(raw-center).normalized()
            hit,normal,index,distance=BODY_BVH.ray_cast(bone+center,direction,1)
            if hit is None:raise RuntimeError('Missing measured shoulder intersection')
            verts.append(hit-bone+direction*.06)
    faces=[(r*(n+1)+j,r*(n+1)+j+1,(r+1)*(n+1)+j+1,(r+1)*(n+1)+j) for r in range(rows) for j in range(n)]
    obj=mesh('Ochre shoulder shawl',verts,faces,M['ochre'],range(len(faces)))
    bpy.context.view_layer.objects.active=obj
    mod=obj.modifiers.new('Shawl thickness','SOLIDIFY');mod.thickness=.015
    bpy.ops.object.modifier_apply(modifier=mod.name)
    for sign in (-1,1):
        start=(sign*.13,-.24,.28)
        anchor=verts[0 if sign==1 else n]
        beam('Shawl shoulder fold',anchor,start,.10,.022,M['ochre'])
        beam('Shawl front fall',start,(sign*.32,-.35,-.075),.10,.022,M['ochre'])


def pouch():
    for sign in (-1,1):
        box('Belt utility pouch',(sign*.34,-.305,.11),(.16,.12,.18),M['leather'],.03)
        box('Pouch flap',(sign*.34,-.37,.165),(.17,.018,.08),M['leather_dark'],.01)
        box('Pouch fastening',(sign*.34,-.385,.15),(.025,.012,.028),M['brass'],.004)


def scarf():
    ring('Loose red neck scarf',.163,.194,-.065,.08,M['red'],32,1.10)
    beam('Scarf front tail',(-.055,-.22,-.045),(-.02,-.31,-.285),.095,.027,M['red'])
    beam('Scarf second tail',(.055,-.22,-.045),(.085,-.30,-.20),.075,.024,M['red'])


BUILDERS={
    'hair_cropped_dark':lambda:hair('dark'),
    'hair_waves_chestnut':lambda:hair('chestnut','waves'),
    'hair_braid_auburn':lambda:hair('auburn','braid'),
    'hair_bun_dark':lambda:hair('dark','bun'),
    'hair_swept_silver':lambda:hair('silver','swept'),
    'beard_neat_silver':beard,'headwrap_sage':headwrap,'hat_straw_wide':straw_hat,
    'cap_merchant_plum':merchant_cap,'bag_crossbody_leather':bag,'backpack_bedroll':backpack,
    'apron_linen_short':apron,'cape_royal_blue':cape,'shawl_ochre':shawl,
    'pouch_belt_double':pouch,'scarf_red':scarf,
}


def select(objects):
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:obj.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]


for spec in SPECS['modules']:
    G.PARTS.clear();BUILDERS[spec['id']]();select(G.PARTS)
    if len(G.PARTS)>1:bpy.ops.object.join()
    obj=bpy.context.view_layer.objects.active;obj.name=spec['id'];obj.data.name=spec['id']
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    obj['asset_id']=spec['id'];obj['attach_bone']=spec['bone'];obj['axes']='body-aligned +Z up -Y front'
    path=OUT/'modules'/(spec['id']+'.glb')
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,
        export_yup=True,export_apply=True,export_texcoords=True,export_normals=True,
        export_materials='EXPORT',export_extras=True,export_animations=False,export_cameras=False,export_lights=False)
    obj.data.calc_loop_triangles()
    points=[Vector(p) for p in obj.bound_box]
    bone=Matrix(BONES[spec['bone']]['world_matrix_rows'])
    rotation=bone.to_3x3().normalized().to_4x4()
    correction=rotation.inverted()
    REPORT['modules'].append({'id':spec['id'],'file':'modules/'+path.name,'bone':spec['bone'],
        'triangles':len(obj.data.loop_triangles),'bounds_min_m':[round(min(p[i] for p in points),6) for i in range(3)],
        'bounds_max_m':[round(max(p[i] for p in points),6) for i in range(3)],
        'bone_reference_position_m':BONES[spec['bone']]['head_m'],
        'attachment_reference_to_bone_rotation_blender_rows':[[round(v,8) for v in row] for row in correction],
        'uv_layers':len(obj.data.uv_layers),'materials':[m.name for m in obj.data.materials],
        'bytes':path.stat().st_size})
    ASSETS[spec['id']]=obj

REPORT['total_module_triangles']=sum(m['triangles'] for m in REPORT['modules'])
REPORT['original_accessory_source_only']=True
REPORT['armature_datablock_count']=len(bpy.data.armatures)
REPORT['generation_seconds']=round(time.monotonic()-START,3)
REPORT['scalp_fitting']={'source':'Saved Cropout reference triangles','successful_surface_rays':len(FIT_SAMPLES),
    'outer_vertex_surface_clearance_m':.026,'misses':0}
(OUT/'model-report.json').write_text(json.dumps(REPORT,indent=2)+'\n',encoding='utf-8')

# Display copies only; no Cropout body/reference mesh is saved in this source.
DISPLAY=bpy.data.collections.new('DISPLAY | originals only');scene.collection.children.link(DISPLAY)
for i,spec in enumerate(SPECS['modules']):
    obj=bpy.data.objects.new(spec['id'],ASSETS[spec['id']].data);DISPLAY.objects.link(obj)
    obj.location=((i%8-3.5)*1.15,(.5-i//8)*1.25,.52)
MODULES.hide_render=True;MODULES.hide_viewport=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'ResidentKit.blend'))
print('RESIDENT_KIT_COMPLETE',len(ASSETS),REPORT['total_module_triangles'],flush=True)
