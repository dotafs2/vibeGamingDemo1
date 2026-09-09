"""Original cold smithy forge fixtures. Run with Blender --background --python this_file.
Static visual modules; no fuel, inventory or functional simulation changes.
Does not create simulation inventory, contracts, animation, or resident actions.
"""
import bpy, json, math
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

def mesh_part(name,verts,faces,role,bevel=.004):
    mesh=bpy.data.meshes.new(name);mesh.from_pydata(verts,[],faces);mesh.update()
    o=bpy.data.objects.new(name,mesh);scene.collection.objects.link(o)
    return finish(o,name,role,bevel)

def masonry():
    start=len(PARTS)
    # An actual open ash chamber: side piers, back, arch and hearth slab.
    for z in range(3):
        for x in (-.43,.43):
            for y in (-.33,.06,.45):
                box('Dressed pier course', (x,y,.115+z*.218),(.29,.378,.21),'StoneLight' if (z+int(x>0))%3==0 else 'Stone',.018)
        for x in (-.21,.21):box('Back wall course',(x,-.465,.115+z*.218),(.41,.17,.21),'Stone',.018)
    radius=.305;thick=.19;depth=.26;center_z=.42
    for i in range(9):
        a=i*math.pi/9+.016;b=(i+1)*math.pi/9-.016
        verts=[]
        for y in (.41-depth/2,.41+depth/2):
            for r,t in ((radius,a),(radius,b),(radius+thick,b),(radius+thick,a)):
                verts.append((r*math.cos(t),y,center_z+r*math.sin(t)))
        mesh_part('Arch wedge',verts,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],'StoneLight' if i==4 else 'Stone',.006)
    box('Ash chamber floor',(0,0,.035),(.61,1.03,.07),'Stone',.012)
    box('Hearth bearing slab',(0,0,.935),(1.2,1.2,.17),'StoneLight',.016)
    # Cold, empty pan, recessed grate and raised brick wind screen.
    box('Cold pan',(0,.045,1.032),(.93,.85,.035),'Iron',.006)
    for x in (-.405,.405):box('Pan rim',(x,.045,1.065),(.035,.79,.07),'Iron',.004)
    for y in (-.335,.425):box('Pan rim',(0,y,1.065),(.83,.035,.07),'Iron',.004)
    for x in (-.18,-.12,-.06,0,.06,.12,.18):box('Air grate bar',(x,.04,1.056),(.027,.30,.015),'Iron',.002)
    for row in range(3):
        for j in range(4):
            box('Brick fire back',((j-1.5)*.28,-.49,1.08+row*.14),(.27,.18,.13),'Terracotta' if (j+row)%3 else 'TerracottaLight',.012)
        for x in (-.49,.49):
            for y in (-.28,-.055):box('Brick side cheek',(x,y,1.08+row*.14),(.18,.217,.13),'Terracotta',.012)
    for x in (-.35,.35):cyl('Slab fixing pin',(x,.48,1.027),.019,.015,'Iron')
    return PARTS[start:]

def bellows():
    start=len(PARTS)
    # Pointed leather bellows. The plywood boards and four folds share one contour.
    outline=[(-.025,.38),(-.13,.22),(-.21,-.06),(-.20,-.25),(-.12,-.36),(0,-.395),(.12,-.36),(.20,-.25),(.21,-.06),(.13,.22),(.025,.38)]
    def height(y,top):return .18+(.12 if top else -.12)*(1-(y+.4)/.8)
    for top in (False,True):
        verts=[]
        for dz in (-.025,.025):
            verts.extend((x,y,height(y,top)+dz) for x,y in outline)
        n=len(outline);faces=[tuple(range(n-1,-1,-1)),tuple(range(n,2*n))]+[(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
        mesh_part('Bellows upper board' if top else 'Bellows lower board',verts,faces,'Timber',.008)
    verts=[];n=len(outline);rings=9
    for row in range(rings):
        t=row/(rings-1);bulge=.016 if row%2 else -.004
        for x,y in outline:
            k=1+bulge/.21
            verts.append((x*k,y,height(y,False)*(1-t)+height(y,True)*t))
    faces=[]
    for row in range(rings-1):
        for i in range(n):faces.append((row*n+i,row*n+(i+1)%n,(row+1)*n+(i+1)%n,(row+1)*n+i))
    mesh_part('Folded leather chamber',verts,faces,'TimberDark',.001)
    for y in (-.20,.12):
        for x in (-.135,.135):cyl('Leather fastening tack',(x,y,height(y,True)+.033),.009,.008,'Brass')
    for top in (False,True):
        beam('Bellows handle',(0,-.31,height(-.31,top)),(0,-.70,height(-.31,top)+(.04 if top else -.04)),.05,'TimberLight')
    cyl('Air nozzle',(0,.49,.18),.033,.25,'Iron','Y')
    for x in (-.12,.12):
        box('Bellows rack leg',(x,-.12,-.35),(.065,.075,.80),'TimberDark',.009)
    box('Bellows rack saddle',(0,-.12,.05),(.37,.14,.07),'Timber',.008)
    return PARTS[start:]

def cold_forge():
    start=len(PARTS)
    transform(masonry(),(.23,0,0))
    transform(bellows(),(-.70,-.02,.75),-90)
    # Pipe connects the bellows outlet to the cold forge's rear tuyere.
    beam('Tuyere connection',(-.18,-.02,.93),(-.08,-.10,1.03),.066,'Iron')
    return PARTS[start:]

def export(name,items,role):
    bpy.ops.object.select_all(action='DESELECT')
    for o in items:o.select_set(True)
    bpy.context.view_layer.objects.active=items[0]
    dest=OUT/'Modules'/(name+'.glb')
    bpy.ops.export_scene.gltf(filepath=str(dest),export_format='GLB',use_selection=True,export_apply=True,export_yup=True,export_materials='EXPORT')
    pts=[o.matrix_world@v.co for o in items for v in o.data.vertices]
    ASSETS.append({'id':name,'glb':dest.relative_to(OUT).as_posix(),'role':role,'vertices':sum(len(o.data.vertices) for o in items),'triangles':sum(sum(len(f.vertices)-2 for f in o.data.polygons) for o in items),'objects':len(items),'bounds_min_m':[min(v[i] for v in pts) for i in range(3)],'bounds_max_m':[max(v[i] for v in pts) for i in range(3)],'uv':'metric projected UV0; common town material palette','materials':sorted({m.name for o in items for m in o.data.materials})})
    col=bpy.data.collections.new(name);scene.collection.children.link(col)
    for o in items:
        for old in list(o.users_collection):old.objects.unlink(o)
        col.objects.link(o);o.hide_render=True;o.hide_set(True)
    return items

export('forge_masonry_hearth',masonry(),'reusable unlit masonry hearth with open ash arch')
export('forge_hand_bellows',bellows(),'reusable stationary leather bellows and rack')
assembly=export('smithy_cold_forge',cold_forge(),'replacement visual for the existing smithy gray pedestal; no lit fuel or inventory')
manifest={'style_id':STYLE['id'],'revision':1,'source_axis':STYLE['source_axis'],'scope':'Static original cold forge fixtures. No new inventory, fuel, coins, work claims, heat or crafting capability. Installation is separate from generation.','assets':ASSETS}
(OUT/'forge_kit_manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
for o in assembly:o.hide_render=False;o.hide_set(False)
box('Studio ground',(0,0,-.08),(4.8,4.8,.13),'StoneLight',.015)
world=bpy.data.worlds.new('Cold forge studio');world.use_nodes=True
world.node_tree.nodes['Background'].inputs['Color'].default_value=(.62,.70,.82,1)
world.node_tree.nodes['Background'].inputs['Strength'].default_value=.55;scene.world=world
for name,loc,power,size in [('Key',(-3,4,5),650,4),('Fill',(4,2,3),500,4),('Rim',(-1,-3,5),700,3)]:
    light=bpy.data.lights.new(name,'AREA');light.energy=power;light.shape='DISK';light.size=size
    o=bpy.data.objects.new(name,light);scene.collection.objects.link(o);o.location=loc;o.rotation_euler=(Vector((0,0,.8))-o.location).to_track_quat('-Z','Y').to_euler()
cam=bpy.data.objects.new('Cold forge camera',bpy.data.cameras.new('Cold forge camera'));scene.collection.objects.link(cam);scene.camera=cam
cam.location=(-3,4,2.8);cam.rotation_euler=(Vector((-.1,0,.7))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=2.9
scene.render.engine='CYCLES';scene.cycles.samples=40;scene.cycles.use_denoising=True;scene.render.threads_mode='FIXED';scene.render.threads=6
scene.render.resolution_x=1300;scene.render.resolution_y=1100;scene.render.resolution_percentage=100;scene.view_settings.view_transform='AgX'
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'AincradForgeKit.blend'))
scene.render.filepath=str(OUT/'Previews/forge_source.png');bpy.ops.render.render(write_still=True)
print('AINCRAD_FORGE_KIT_EXPORTED',len(ASSETS),flush=True)
