"""Original modular appearance parts for the single existing Level0 axe."""
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
    n.inputs['Roughness'].default_value=.62 if role in ('Iron','IronLight','SteelEdge','Brass') else .82
    n.inputs['Metallic'].default_value=.4 if role in ('Iron','IronLight','SteelEdge','Brass') else 0
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

def mesh_part(name,vs,fs,role,bevel=.002):
    me=bpy.data.meshes.new(name);me.from_pydata(vs,[],fs);me.update()
    bm=bmesh.new();bm.from_mesh(me);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(me);bm.free();me.update()
    o=bpy.data.objects.new(name,me);scene.collection.objects.link(o);return finish(o,name,role,bevel)

def wood_shaft(name,rings,angles,role):
    vs=[]
    for z,cx,rx,ry in rings:
        for angle in angles:vs.append((cx+math.cos(angle)*rx,math.sin(angle)*ry,z))
    n=len(angles);fs=[tuple(range(n-1,-1,-1)),tuple(range((len(rings)-1)*n,len(rings)*n))]
    fs.extend((k*n+i,k*n+(i+1)%n,(k+1)*n+(i+1)%n,(k+1)*n+i) for k in range(len(rings)-1) for i in range(n))
    return mesh_part(name,vs,fs,role,.0006)

def handle(split):
    start=len(PARTS)
    rings=[(-.285,-.020,.024,.017),(-.26,-.015,.025,.018),(-.17,.0,.018,.014),(-.04,.012,.018,.014),(.075,.008,.019,.015),(.17,.0,.019,.015),(.275,-.002,.017,.013)]
    angles=[i*math.tau/12 for i in range(12)]
    if not split:wood_shaft('Continuous ash wood haft',rings,angles,'TimberLight')
    else:
        wood_shaft('Lower worn haft',rings[:5],angles,'Timber')
        # Two actual separated timber tongues above the split; not just a painted stripe.
        upper=[(.07,.008,.019,.015),(.15,.002,.019,.015),(.275,-.002,.017,.013)]
        for side in (-1,1):
            aa=[side*math.pi/2 + math.pi*i/8 for i in range(9)]
            tongue=wood_shaft('Split upper tongue',upper,aa,'TimberLight' if side==1 else 'Timber')
            tongue.location.x=-side*.0022
        beam('Split shadow seam',(.006,-.0005,.072),(-.002,-.0005,.21),.0025,'TimberDark')
    for i in range(5):
        z=-.237+i*.013
        # Narrow natural fibre grip ties; geometry is a visual of the same existing tool.
        t=(z+.26)/.09;cx=-.015+t*.015;rx=.025-t*.007;ry=.018-t*.004
        bpy.ops.mesh.primitive_torus_add(major_segments=16,minor_segments=6,major_radius=rx+.001,minor_radius=.0025,location=(cx,0,z))
        o=bpy.context.object;o.scale.y=ry/rx;finish(o,'Grip cord','Cloth',0)
    cyl('Visible end grain',(-.002,0,.278),.014,.007,'TimberLight',vertices=12)
    wedge=box('Head fastening wedge',(-.002,0,.282),(.024,.004,.012),'TimberDark',.001)
    if split:wedge.rotation_euler.y=.28;wedge.location.z+=.006
    return PARTS[start:]

def head_mesh(chipped):
    start=len(PARTS)
    # Broad cheek slopes down into a compact bearded cutting edge.
    edge=[(.229,.090),(.236,.137),(.239,.184),(.236,.232),(.222,.273)]
    if chipped:edge=[(.229,.090),(.233,.122),(.217,.139),(.237,.150),(.239,.184),(.220,.205),(.235,.218),(.222,.273)]
    outline=[(-.050,.140),(.018,.135),(.067,.105)]+edge+[(.085,.247),(.017,.252),(-.05,.248)]
    vs=[]
    for side in (-1,1):
        for x,z in outline:
            taper=max(0,min(1,(x-.055)/.18));thick=.021*(1-taper)+.0035*taper
            vs.append((x,side*thick,z))
    n=len(outline);fs=[tuple(range(n-1,-1,-1)),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
    head=mesh_part('Chipped forged axe head' if chipped else 'Sound forged axe head',vs,fs,'IronLight',.0015)
    # A real rounded eye receives the same shaft; this is not a solid head
    # intersecting the wood. Keep the shared origin and outer edge silhouette.
    bpy.ops.mesh.primitive_cube_add(size=1,location=(0,0,.20))
    cutter=bpy.context.object;cutter.name='Temporary axe eye bore';cutter.dimensions=(.044,.034,.5)
    cutter.data.materials.append(MATS['IronLight'])
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    rounded=cutter.modifiers.new('Rounded eye corners','BEVEL');rounded.width=.004;rounded.segments=3
    bpy.ops.object.modifier_apply(modifier=rounded.name)
    bpy.context.view_layer.objects.active=head
    bore=head.modifiers.new('Shaft receiving eye','BOOLEAN');bore.operation='DIFFERENCE';bore.solver='EXACT';bore.object=cutter
    bpy.ops.object.modifier_apply(modifier=bore.name)
    bpy.data.objects.remove(cutter,do_unlink=True)
    bm=bmesh.new();bm.from_mesh(head.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
    assert all(e.is_manifold for e in bm.edges),'Axe head must remain a closed surface around its bore'
    bm.to_mesh(head.data);bm.free()
    uv=head.data.uv_layers.active.data
    for poly in head.data.polygons:
        axes=sorted(range(3),key=lambda i:abs(poly.normal[i]))[:2]
        for li in poly.loop_indices:
            v=head.data.vertices[head.data.loops[li].vertex_index].co;uv[li].uv=(v[axes[0]],v[axes[1]])
    # A continuous bevel follows the actual edge profile, including each notch.
    for side in (-1,1):
        verts=[]
        for x,z in edge:verts.extend([(x,side*.005,z),(x-.015,side*.0075,z)])
        faces=[(2*i,2*i+1,2*i+3,2*i+2) for i in range(len(edge)-1)]
        # Two-sided thin solid strips have outward normals after extrusion.
        me=bpy.data.meshes.new('Cutting bevel');me.from_pydata(verts,[],faces);me.update();o=bpy.data.objects.new('Worn cutting bevel' if chipped else 'Fresh cutting bevel',me);scene.collection.objects.link(o);o.data.materials.append(MATS['SteelEdge']);PARTS.append(o)
        solid=o.modifiers.new('Bevel thickness','SOLIDIFY');solid.thickness=.0008;bpy.context.view_layer.objects.active=o;bpy.ops.object.modifier_apply(modifier=solid.name)
        bm=bmesh.new();bm.from_mesh(o.data);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(o.data);bm.free()
    return PARTS[start:]

def export(name,items,role):
    bpy.context.view_layer.update();bpy.ops.object.select_all(action='DESELECT')
    for o in items:o.select_set(True)
    bpy.context.view_layer.objects.active=items[0]
    p=OUT/'Modules'/(name+'.glb');bpy.ops.export_scene.gltf(filepath=str(p),export_format='GLB',use_selection=True,export_apply=True,export_yup=True,export_materials='EXPORT')
    pts=[o.matrix_world@v.co for o in items for v in o.data.vertices]
    ASSETS.append({'id':name,'glb':p.relative_to(OUT).as_posix(),'role':role,'vertices':sum(len(o.data.vertices) for o in items),'triangles':sum(sum(len(f.vertices)-2 for f in o.data.polygons) for o in items),'bounds_min_m':[min(v[i] for v in pts) for i in range(3)],'bounds_max_m':[max(v[i] for v in pts) for i in range(3)],'materials':sorted({m.name for o in items for m in o.data.materials}),'pivot':'shared tool origin; shaft center; +Z shaft, +X cutting edge','uv':'metric projection for solid parts; shared town material palette'})
    for o in items:o.hide_render=True;o.hide_set(True)
    return items

sound=export('axe_handle_sound',handle(False),'same existing tool: handle=100 visual')
split=export('axe_handle_split',handle(True),'same existing tool: handle<100 visual; split is geometric')
sharp=export('axe_head_sharp',head_mesh(False),'same existing tool: edge=100 visual')
chipped=export('axe_head_chipped',head_mesh(True),'same existing tool: edge<100 visual; two real edge notches')
(OUT/'axe_kit_manifest.json').write_text(json.dumps({'style_id':STYLE['id'],'revision':2,'source_axis':STYLE['source_axis'],'scope':'Four independently replaceable appearance components for one existing axe. No new item, material inventory, condition, skill, ownership, currency or repair changes.','assets':ASSETS},ensure_ascii=False,indent=2),encoding='utf-8')
# Contact sheet displays the four condition combinations, after local-pivot exports.
for i,(hh,bb) in enumerate([(split,chipped),(sound,chipped),(split,sharp),(sound,sharp)]):
 for original in hh+bb:
  o=original.copy();o.data=original.data;scene.collection.objects.link(o);o.hide_render=False;o.hide_set(False);o.location.x+=i*.65
scene.world.color=(.4,.4,.4)
bpy.ops.object.camera_add(location=(1.7,3.7,1.7));camera=bpy.context.object;camera.rotation_euler=(Vector((1.02,0,.02))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=2.8;scene.camera=camera
for loc,power,size in [((0,3,3),750,4),((3,-2,2),500,3)]:
 bpy.ops.object.light_add(type='AREA',location=loc);light=bpy.context.object;light.data.energy=power;light.data.size=size;light.rotation_euler=(Vector((1,0,0))-light.location).to_track_quat('-Z','Y').to_euler()
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.render.resolution_x=1500;scene.render.resolution_y=680;scene.render.resolution_percentage=100;scene.view_settings.view_transform='AgX';scene.render.image_settings.file_format='PNG';scene.render.filepath=str(OUT/'Previews/axe_conditions_source.png')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'AincradAxeKit.blend'));bpy.ops.render.render(write_still=True)
print('AXE_KIT_COMPLETE '+str(len(ASSETS)))
