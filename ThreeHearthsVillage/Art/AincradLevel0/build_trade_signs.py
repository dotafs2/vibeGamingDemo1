"""Original reusable double-sided trade signs. Blender background source build."""
import bpy, bmesh, json, math
from pathlib import Path
from mathutils import Vector, Matrix
OUT=Path(__file__).resolve().parent
STYLE=json.loads((OUT/'style.json').read_text(encoding='utf-8'))
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene
scene.unit_settings.system='METRIC'
MATS={}
def lin(v): return v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4
for role,code in STYLE['palette_srgb'].items():
    m=bpy.data.materials.new('AT_'+role); m.use_nodes=True
    rgb=[lin(int(code[i:i+2],16)/255) for i in (0,2,4)]
    m.diffuse_color=(*rgb,1)
    n=m.node_tree.nodes.get('Principled BSDF'); n.inputs['Base Color'].default_value=(*rgb,1)
    n.inputs['Roughness'].default_value=.62 if role in ('Iron','Brass') else .82
    n.inputs['Metallic'].default_value=.4 if role in ('Iron','Brass') else 0
    MATS[role]=m
PARTS=[]
def finish(o,name,role,bevel=.006):
    o.name=name; o.data.materials.append(MATS[role])
    bpy.context.view_layer.objects.active=o
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    if bevel:
        b=o.modifiers.new('Crafted edge','BEVEL'); b.width=bevel; b.segments=2
        bpy.ops.object.modifier_apply(modifier=b.name)
    if not o.data.uv_layers: o.data.uv_layers.new(name='UVMap')
    uv=o.data.uv_layers.active.data
    for poly in o.data.polygons:
        axes=sorted(range(3),key=lambda i:abs(poly.normal[i]))[:2]
        for li in poly.loop_indices:
            v=o.data.vertices[o.data.loops[li].vertex_index].co
            uv[li].uv=(v[axes[0]],v[axes[1]])
    PARTS.append(o); return o

def box(name,loc,size,role='Timber',bevel=.006):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc)
    o=bpy.context.object; o.dimensions=size
    return finish(o,name,role,bevel)
def cyl(name,loc,radius,depth,role='Iron',axis='Z',vertices=16):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=radius,depth=depth,location=loc)
    o=bpy.context.object
    if axis=='Y':o.rotation_euler.x=math.pi/2
    if axis=='X':o.rotation_euler.y=math.pi/2
    return finish(o,name,role,.002)
def beam(name,a,b,width,role='Timber'):
    mid=(Vector(a)+Vector(b))*.5; d=Vector(b)-Vector(a)
    o=box(name,mid,(width,width,d.length),role,min(.007,width*.12))
    o.rotation_euler=d.to_track_quat('Z','Y').to_euler();return o
def transform(items,loc=(0,0,0),yaw=0):
    mat=Matrix.Translation(Vector(loc))@Matrix.Rotation(math.radians(yaw),4,'Z')
    for o in items:o.matrix_world=mat@o.matrix_world



ASSETS=[]

def relief(name,outline,y,depth,role):
    n=len(outline)
    vs=[(x,y+d,z) for d in (-depth/2,depth/2) for x,z in outline]
    fs=[tuple(range(n-1,-1,-1)),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(vs,[],fs);mesh.update()
    bm=bmesh.new();bm.from_mesh(mesh);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(mesh);bm.free();mesh.update()
    o=bpy.data.objects.new(name,mesh);scene.collection.objects.link(o)
    return finish(o,name,role,.004)

def ring(name,loc,major,minor,role='Iron'):
    bpy.ops.mesh.primitive_torus_add(major_segments=16,minor_segments=6,location=loc,rotation=(math.pi/2,0,0),major_radius=major,minor_radius=minor)
    return finish(bpy.context.object,name,role,0)

def plaque():
    start=len(PARTS)
    outline=[(-.51,-.20),(-.43,-.36),(.43,-.36),(.51,-.20),(.51,.24),(.43,.34),(-.43,.34),(-.51,.24)]
    relief('Carved dark timber rim',outline,0,.095,'TimberDark')
    for i in range(4):
        left=-.45+i*.225;right=left+.22
        bottom=-.29 if i in (1,2) else -.25;top=.27 if i in (1,2) else .245
        relief('Recessed sign board',[(left,bottom),(right,bottom),(right,top),(left,top)],0,.106,'Timber' if i%2 else 'TimberLight')
    for face in (-1,1):
        for x in (-.435,.435):
            for z in (-.18,.20):cyl('Hand forged corner nail',(x,face*.060,z),.014,.012,'Iron','Y',8)
        # A small lower maker stripe makes each face readable from both street directions.
        box('Lower brass stripe',(0,face*.063,-.272),(.45,.009,.015),'Brass',.002)
    for x in (-.30,.30):
        ring('Suspension eye',(x,0,.38),.044,.009)
        ring('Suspension link',(x,0,.462),.044,.009)
    # Bracket: board is in XZ, wall attachment at x=-.68; rotate at placement.
    beam('Forged hanging arm',(-.70,0,.535),(.53,0,.535),.040,'Iron')
    beam('Bracket diagonal',(-.68,0,.78),(-.18,0,.535),.025,'Iron')
    box('Wall bracket plate',(-.71,0,.625),(.028,.12,.37),'Iron',.004)
    for z in (.50,.74):cyl('Bracket fixing',(-.731,0,z),.018,.014,'Iron','X',8)
    return PARTS[start:]

def icon(role,face):
    y=face*.068
    if role=='inn':
        # A bed and small roof, a language-independent inn emblem.
        box('Bed mattress',(0,y,-.035),(.60,.025,.105),'Brass',.015)
        for x,z,h in [(-.30,-.035,.32),(.30,-.065,.24)]:box('Bed post',(x,y,z),(.045,.031,h),'Brass',.006)
        box('Pillow',(-.18,y+face*.014,.039),(.16,.028,.06),'Plaster',.018)
        for x in (-.27,.27):beam('Shelter roof',(x,y,.18),(0,y,.30),.032,'Brass')
    elif role=='smithy':
        outline=[(-.32,-.07),(-.25,-.14),(-.08,-.14),(-.06,-.20),(-.21,-.22),(-.21,-.27),(.18,-.27),(.18,-.22),(.06,-.20),(.08,-.12),(.29,-.05),(.34,.025),(-.32,.025)]
        relief('Anvil emblem',outline,y,.027,'Brass')
        beam('Hammer handle',(-.03,y,.055),(.12,y,.24),.031,'Brass')
        h=box('Hammer head',(.125,y,.235),(.22,.032,.092),'Brass',.013);h.rotation_euler.y=-.58
    elif role=='carpentry':
        # A hand plane above a joiner's try square.
        relief('Hand plane emblem',[(-.30,-.04),(.24,-.04),(.30,.025),(.26,.08),(-.30,.08)],y,.029,'Brass')
        relief('Plane tote',[(-.20,.08),(-.19,.19),(-.13,.22),(-.075,.19),(-.10,.08)],y,.027,'Brass')
        box('Plane front knob',(.17,y,.116),(.06,.028,.10),'Brass',.009)
        beam('Plane cutter',(-.015,y,.055),(.045,y,.18),.028,'Iron')
        box('Try square long arm',(-.06,y,-.16),(.52,.027,.035),'Brass',.003)
        box('Try square stock',(-.303,y,-.19),(.065,.031,.19),'Brass',.006)

def export(role):
    start=len(PARTS);plaque()
    for f in (-1,1):icon(role,f)
    items=PARTS[start:];name=role+'_trade_sign'
    bpy.context.view_layer.update()
    bpy.ops.object.select_all(action='DESELECT')
    for o in items:o.select_set(True)
    bpy.context.view_layer.objects.active=items[0]
    path=OUT/'Modules'/(name+'.glb')
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,export_apply=True,export_yup=True,export_materials='EXPORT')
    pts=[o.matrix_world@v.co for o in items for v in o.data.vertices]
    ASSETS.append({'id':name,'glb':path.relative_to(OUT).as_posix(),'role':role+' identification fixture; no inventory or new shop identity','vertices':sum(len(o.data.vertices) for o in items),'triangles':sum(sum(len(f.vertices)-2 for f in o.data.polygons) for o in items),'bounds_min_m':[min(v[i] for v in pts) for i in range(3)],'bounds_max_m':[max(v[i] for v in pts) for i in range(3)],'materials':sorted({m.name for o in items for m in o.data.materials}),'uv':'metric projected UV0; shared town palette','pivot':'plaque center; local front +Y, reverse face -Y; wall bracket at X=-0.71m'})
    return items

for i,role in enumerate(('inn','smithy','carpentry')):
    items=export(role)
    # Move only the source contact sheet display after exporting local-pivot modules.
    transform(items,(i*1.6,0,0))
(OUT/'trade_sign_manifest.json').write_text(json.dumps({'style_id':STYLE['id'],'revision':1,'source_axis':STYLE['source_axis'],'scope':'Original double-sided trade identification signs. Static fixtures replacing blank sign plates. No shops, inventory, transactions or resident-built claims.','assets':ASSETS},ensure_ascii=False,indent=2),encoding='utf-8')
scene.world.color=(.35,.35,.35)
bpy.ops.object.camera_add(location=(2.2,5.6,2.1));cam=bpy.context.object;cam.rotation_euler=(Vector((1.5,0,.17))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=4.9;scene.camera=cam
for loc,power,size in [((0,4,5),1100,5),((4,-2,3),800,4)]:
    bpy.ops.object.light_add(type='AREA',location=loc);light=bpy.context.object;light.data.energy=power;light.data.shape='DISK';light.data.size=size;light.rotation_euler=(Vector((1.5,0,.1))-light.location).to_track_quat('-Z','Y').to_euler()
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.render.resolution_x=1400;scene.render.resolution_y=640;scene.render.resolution_percentage=100
scene.view_settings.view_transform='AgX';scene.render.image_settings.file_format='PNG';scene.render.filepath=str(OUT/'Previews/trade_signs_source.png')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'AincradTradeSigns.blend'))
bpy.ops.render.render(write_still=True)
print('TRADE_SIGNS_COMPLETE '+str(len(ASSETS)))
