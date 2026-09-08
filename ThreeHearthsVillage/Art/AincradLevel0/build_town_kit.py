"""Create editable original town modules and native-import sources in Blender.

Run only in an owned background Blender process. Uses no imported artwork and
does not modify the older medieval library. Runtime shells are offline batches
of these authored parts; the recipe preserves replaceable part identities.
"""
import argparse
import json
import math
import random
import sys
from pathlib import Path
import bpy
from mathutils import Vector, Matrix

OUT = Path(__file__).resolve().parent
STYLE = json.loads((OUT / 'style.json').read_text(encoding='utf-8'))
for name in ('Modules', 'Assemblies', 'Previews', 'Recipes'):
    (OUT / name).mkdir(exist_ok=True)
args = sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
parser = argparse.ArgumentParser()
parser.add_argument('--render', action='store_true')
opt = parser.parse_args(args)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
MATS = {}
def linear(c):
    c /= 255
    return c/12.92 if c <= .04045 else ((c+.055)/1.055)**2.4
def material(key):
    if key in MATS:
        return MATS[key]
    code = STYLE['palette_srgb'][key]
    color = tuple(linear(int(code[i:i+2],16)) for i in (0,2,4))+(1,)
    mat = bpy.data.materials.new('AT_'+key)
    mat.use_nodes = True
    mat.diffuse_color = color
    node = mat.node_tree.nodes.get('Principled BSDF')
    node.inputs['Base Color'].default_value = color
    node.inputs['Roughness'].default_value = .6 if key in ('Glass','Iron','Brass') else .88
    node.inputs['Metallic'].default_value = .35 if key in ('Iron','Brass') else 0
    mat['semantic_role'] = key
    MATS[key] = mat
    return mat

class Geo:
    def __init__(self):
        self.v=[]; self.f=[]; self.roles=[]; self.parts=[]; self.direct_faces=[]
    def mesh(self, v, f, role):
        base=len(self.v); self.v.extend(tuple(x) for x in v)
        self.direct_faces.extend(range(len(self.f),len(self.f)+len(f)))
        self.f.extend(tuple(base+i for i in face) for face in f)
        self.roles.extend([role]*len(f))
    def box(self, p, s, role='Timber', rotation=None):
        corners=[(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),(-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]
        verts=[Vector((a*s[0]/2,b*s[1]/2,c*s[2]/2)) for a,b,c in corners]
        verts=[Vector(p)+(rotation@v if rotation else v) for v in verts]
        self.mesh(verts,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],role)
    def beam(self,a,b,width,role='Timber'):
        delta=Vector(b)-Vector(a)
        self.box((Vector(a)+Vector(b))/2,(width,width,delta.length),role,delta.to_track_quat('Z','Y').to_matrix())
    def cylinder(self,p,r,h,role='Timber',n=12):
        vs=[(p[0]+r*math.cos(i*2*math.pi/n),p[1]+r*math.sin(i*2*math.pi/n),p[2]+z*h/2) for z in (-1,1) for i in range(n)]
        faces=[tuple(range(n-1,-1,-1)),tuple(range(n,2*n))]
        faces += [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
        self.mesh(vs,faces,role)
    def ellipsoid(self,p,s,role='Leaf',seed=0):
        rng=random.Random(seed); rings=5; sectors=9
        vs=[]
        for j in range(rings+1):
            latitude=math.pi*j/rings
            for i in range(sectors):
                t=i*2*math.pi/sectors
                bump=1 if j in (0,rings) else rng.uniform(.9,1.08)
                vs.append((p[0]+s[0]*math.sin(latitude)*math.cos(t)*bump,p[1]+s[1]*math.sin(latitude)*math.sin(t)*bump,p[2]+s[2]*math.cos(latitude)*bump))
        for j in range(rings):
            for i in range(sectors):
                self.mesh([vs[j*sectors+i],vs[j*sectors+(i+1)%sectors],vs[(j+1)*sectors+(i+1)%sectors],vs[(j+1)*sectors+i]],[(3,2,1,0)],role)
    def add(self,other,p=(0,0,0),yaw=0,key='part',module='authored'):
        r=Matrix.Rotation(math.radians(yaw),3,'Z')
        base=len(self.v)
        self.v.extend(tuple(r@Vector(v)+Vector(p)) for v in other.v)
        self.f.extend(tuple(base+i for i in f) for f in other.f)
        self.roles.extend(other.roles)
        self.parts.append({'id':key,'module':module,'position_m':list(p),'yaw_degrees':yaw})
    def object(self,name):
        mesh=bpy.data.meshes.new(name)
        mesh.from_pydata(self.v,[],self.f); mesh.update()
        obj=bpy.data.objects.new(name,mesh); scene.collection.objects.link(obj)
        keys=list(dict.fromkeys(self.roles))
        for k in keys: mesh.materials.append(material(k))
        uv=mesh.uv_layers.new(name='UVMap')
        for poly,role in zip(mesh.polygons,self.roles):
            poly.material_index=keys.index(role)
            # Each face starts at its own metric origin; equal tiles share UVs.
            coords=[mesh.vertices[mesh.loops[i].vertex_index].co for i in poly.loop_indices]
            origin=coords[0]
            u=(coords[1]-origin).normalized()
            normal=poly.normal
            v=normal.cross(u).normalized()
            for index,co in zip(poly.loop_indices,coords):
                rel=co-origin;uv.data[index].uv=(rel.dot(u),rel.dot(v))
        obj['style_id']=STYLE['id'];obj['axis']='meters,+Z up,+Y facade'
        obj['source_parts']=len(self.parts)
        return obj
    def details(self):
        result=Geo(); used=sorted({v for i in self.direct_faces for v in self.f[i]})
        mapping={old:new for new,old in enumerate(used)}
        result.v=[self.v[i] for i in used]
        result.f=[tuple(mapping[v] for v in self.f[i]) for i in self.direct_faces]
        result.roles=[self.roles[i] for i in self.direct_faces]
        return result

LIB={}
def wall(window=False,door=False):
    g=Geo(); thickness=.22; height=3.2
    if door:
        for s in (-1,1):g.box((s*.91,0,1.6),(.18,thickness,height),'Plaster')
        g.box((0,0,2.8),(1.64,thickness,.8),'Plaster')
        for s in (-1,1):g.box((s*.85,.03,1.21),(.1,.31,2.42),'TimberDark')
        g.box((0,.03,2.47),(1.8,.31,.12),'TimberDark')
    elif window:
        g.box((0,0,.55),(2,thickness,1.1),'Plaster')
        g.box((0,0,2.85),(2,thickness,.7),'Plaster')
        for s in (-1,1):g.box((s*.77,0,1.8),(.46,thickness,1.4),'Plaster')
        g.box((0,0,1.8),(1.05,.06,1.35),'Glass')
        for x in (-.55,0,.55):g.box((x,.14,1.8),(.065,.08,1.45),'TimberDark')
        for z in (1.07,1.8,2.53):g.box((0,.14,z),(1.18,.08,.075),'TimberDark')
        g.box((0,.26,1.03),(1.35,.5,.12),'StoneLight')
        for s in (-1,1):
            g.box((s*.77,.15,1.8),(.3,.08,1.4),'Slate')
            for z in (1.3,2.25):g.box((s*.77,.205,z),(.31,.04,.045),'TimberDark')
    else:g.box((0,0,1.6),(2,thickness,height),'Plaster')
    g.box((0,.02,.06),(2,.27,.12),'Stone')
    g.box((0,0,3.13),(2,.29,.14),'Timber')
    return g
LIB['wall_plain_2m']=wall()
LIB['wall_window_2m']=wall(window=True)
LIB['wall_entry_2m']=wall(door=True)
g=Geo();g.box((0,0,-.1),(2,2,.2),'TimberLight');LIB['floor_2m']=g
g=Geo();g.box((0,0,.15),(2,.32,.3),'Stone');LIB['stone_base_2m']=g
g=Geo();g.box((0,0,1.6),(.18,.25,3.2),'Timber');LIB['post_3m2']=g
g=Geo();g.box((0,0,0),(2,.18,.18),'TimberDark');LIB['eave_2m']=g
g=Geo();g.box((0,0,0),(.44,.66,.045),'Slate');LIB['tile_shared_uv']=g

def roof(g,w,d,h,slate=True):
    half=w/2+.45; length=d+1; rise=half*math.tan(math.radians(36))
    slope=rise/half; base=h-.16
    # Solid backing stops light leaks; shared repeated tile faces stay metric.
    for sign in (-1,1):
        a=(0,-length/2,base+rise);b=(sign*half,-length/2,base);c=(sign*half,length/2,base);e=(0,length/2,base+rise)
        g.mesh([a,b,c,e],[(0,1,2,3) if sign>0 else (3,2,1,0)],'TimberDark')
        rows=math.ceil(half/.55); columns=math.ceil(length/.43)
        rot=Matrix.Rotation(sign*math.atan(slope),3,'Y')
        for row in range(rows):
            x=sign*(row+.5)*half/rows
            for col in range(columns):
                y=-length/2+(col+.5)*length/columns
                # Sparse seeded differences avoid a diagonal checkerboard.
                variation=random.Random(row*73856093 ^ col*19349663 ^ 643).random()<.17
                role=('SlateLight' if variation else 'Slate') if slate else ('TerracottaLight' if variation else 'Terracotta')
                g.box((x,y,base+rise-abs(x)*slope+.035),(half/rows/math.cos(math.atan(slope))+.06,length/columns+.02,.045),role,rot)
        g.beam((sign*half,-length/2,base),(sign*half,length/2,base),.16,'TimberDark')
        for y in (-length/2,length/2):g.beam((0,y,base+rise),(sign*half,y,base),.17,'TimberDark')
    for col in range(math.ceil(length/.55)):
        g.box((0,-length/2+(col+.5)*length/math.ceil(length/.55),base+rise+.08),(.22,.59,.12),'Slate' if slate else 'Terracotta')
    # Plastered gable panels close the upper facade, with framing on both ends.
    for sign in (-1,1):
        y=sign*d/2
        g.mesh([(-w/2,y,h),(w/2,y,h),(0,y,h+w/2*slope)],[(0,1,2) if sign<0 else (2,1,0)],'Plaster')
        g.beam((0,y,h),(0,y,h+w/2*slope),.16)
        for x in (-w*.25,w*.25):g.beam((x,y,h),(0,y,h+w*.2*slope),.13)

def table():
    g=Geo();g.box((0,0,.82),(1.8,.85,.12),'TimberLight')
    for x in (-.7,.7):
        for y in (-.3,.3):g.box((x,y,.4),(.1,.1,.8),'Timber')
    return g
LIB['work_table']=table()
g=Geo()
for x in (-.7,.7):
    for y in (-.24,.24):g.box((x,y,1),(.08,.08,2),'Timber')
for z in (.15,.8,1.45,2):g.box((0,0,z),(1.5,.6,.09),'TimberLight')
LIB['storage_shelf']=g

g=Geo();g.box((0,0,.45),(1.7,.38,.11),'TimberLight')
for x in (-.62,.62):g.box((x,0,.2),(.14,.3,.4),'Timber')
LIB['guest_bench']=g
g=Geo();g.cylinder((0,0,0),.11,.26,'LampWarm',8)
for z in (-.17,.17):g.cylinder((0,0,z),.17,.06,'Iron',8)
for x in (-.115,.115):
    for y in (-.07,.07):g.box((x,y,0),(.025,.025,.34),'Iron')
g.beam((0,0,.2),(0,0,.52),.035,'Iron');LIB['lantern_hanging']=g

def building(w,d,floors,kind):
    g=Geo();cols=int(w/2);depth=int(d/2)
    for level in range(floors):
        for side in (-1,1):
            for c in range(cols):
                x=-w/2+1+2*c;front=side==1
                # An even-bay facade needs two short walls around a centered door.
                if level==0 and front and abs(x)<1.1:
                    if x==0:
                        g.add(LIB['wall_entry_2m'],(0,d/2,0),key='front_entry',module='wall_entry_2m')
                    if x<0:
                        for sx in (-1,1):
                            g.box((sx*1.45,d/2,1.6),(1.1,.22,3.2),'Plaster')
                        g.box((0,d/2,2.85),(1.8,.22,.7),'Plaster')
                        for sx in (-1,1):g.box((sx*.88,d/2+.02,1.2),(.14,.3,2.4),'TimberDark')
                        g.box((0,d/2+.02,2.45),(1.9,.3,.12),'TimberDark')
                    continue
                module='wall_window_2m' if (c+level)%3!=2 else 'wall_plain_2m'
                g.add(LIB[module],(x,side*d/2,level*3.2),0 if front else 180,f'facade_{level}_{side}_{c}',module)
            for c in range(depth):
                y=-d/2+1+2*c
                module='wall_window_2m' if c%3==1 else 'wall_plain_2m'
                g.add(LIB[module],(side*w/2,y,level*3.2),-90 if side==1 else 90,f'side_{level}_{side}_{c}',module)
        # The first floor stays open and usable; only the upper floor is closed.
        for ix in range(cols):
            for iy in range(depth):g.add(LIB['floor_2m'],(-w/2+1+2*ix,-d/2+1+2*iy,level*3.2),key=f'floor_{level}_{ix}_{iy}',module='floor_2m')
        for x in (-w/2,0,w/2):
            for side in (-1,1):
                if x==0 and level==0 and side==1:continue
                g.add(LIB['post_3m2'],(x,side*(d/2+.03),level*3.2),key=f'post_{level}_{x}_{side}',module='post_3m2')
    for x in (-w/2,w/2):g.box((x,0,.18),(.36,d+.3,.36),'Stone')
    for side in (-1,1):
        if side<0:g.box((0,side*d/2,.18),(w+.3,.36,.36),'Stone')
        else:
            for sx in (-1,1):g.box((sx*(w/4+.5),d/2,.18),(w/2-1,.36,.36),'Stone')
    roof(g,w,d,floors*3.2,kind!='carpentry')
    # Open door is placed against the interior side; the central 1.6m is clear.
    g.box((-.87,d/2-.72,1.2),(.09,1.45,2.3),'Timber')
    g.box((-.8,d/2-1.22,1.13),(.08,.08,.08),'Brass')
    g.box((w*.32,-d*.25,floors*3.2+1.8),(.65,.75,3.6),'Stone')
    for z in (floors*3.2+3.4,floors*3.2+3.65):g.box((w*.32,-d*.25,z),(.85,.95,.18),'StoneLight')
    # Furniture sits out of the straight entrance/work route.
    g.add(LIB['work_table'],(w*.28,d*.15,0),key='working_table',module='work_table')
    if kind=='inn':
        g.box((-w*.28,d*.15,.57),(2.8,.65,1.14),'Timber')
        for x in (-w*.28,w*.28):g.add(LIB['work_table'],(x,-d*.28,0),key=f'guest_table_{x}',module='work_table')
        for x in (-w*.28,w*.28):
            for side in (-1,1):g.add(LIB['guest_bench'],(x,-d*.28+side*.75,0),key=f'bench_{x}_{side}',module='guest_bench')
        g.add(LIB['storage_shelf'],(-w*.32,-d/2+.5,0),key='inn_shelf',module='storage_shelf')
        # Balcony and brackets give the inn a distinct street-facing silhouette.
        g.box((0,d/2+.63,3.17),(w*.66,1.5,.18),'Timber')
        for x in (-w*.31,w*.31):g.beam((x,d/2,2.25),(x,d/2+1.25,3.08),.13)
        for x in range(-int(w*.3/.5),int(w*.3/.5)+1):g.box((x*.5,d/2+1.34,3.65),(.065,.065,.85),'TimberDark')
        g.box((0,d/2+1.34,4.1),(w*.65,.12,.12),'Timber')
    elif kind=='smithy':
        g.box((-w*.28,-d*.25,.52),(1.6,1.5,1.04),'Stone')
        g.box((-w*.28,-d*.25,1.15),(1.1,1.05,.22),'Iron')
    elif kind=='carpentry':
        for i in range(5):g.box((-w*.3,-d*.15+i*.25,.2),(.24,3.2,.22),'TimberLight')
    if kind in ('inn','smithy','carpentry'):
        for side in (-1,1):g.add(LIB['lantern_hanging'],(side*w*.25,0,2.62),key=f'ceiling_lantern_{side}',module='lantern_hanging')
    # Hanging shop sign: support + plate. Semantic shop name remains metadata.
    g.beam((w*.28,d/2,2.8),(w*.28,d/2+1.2,2.8),.07,'Iron')
    g.box((w*.28,d/2+.95,2.35),(.08,.8,.65),'TimberDark')
    g.box((w*.28+.047,d/2+.95,2.35),(.014,.63,.45),'Brass')
    return g

ASSETS=[]
def export(name,g,folder):
    obj=g.object(name)
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);bpy.context.view_layer.objects.active=obj
    path=OUT/folder/(name+'.glb')
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,export_apply=True,export_yup=True,export_materials='EXPORT')
    lo=[min(v[i] for v in g.v) for i in range(3)];hi=[max(v[i] for v in g.v) for i in range(3)]
    row={'id':name,'glb':path.relative_to(OUT).as_posix(),'vertices':len(g.v),'triangles':sum(len(f)-2 for f in g.f),'bounds_min_m':lo,'bounds_max_m':hi,'parts':len(g.parts),'uv':'UV0 metric, repeated tile shared'}
    ASSETS.append(row)
    if g.parts:(OUT/'Recipes'/(name+'.json')).write_text(json.dumps({'asset':name,'parts':g.parts,'initial_world_art':True},ensure_ascii=False,indent=2),encoding='utf-8')
    obj.hide_set(True);obj.hide_render=True
    return obj

for name,g in LIB.items():export(name,g,'Modules')
ASSEMBLIES={}
for name,w,d,f,kind in [('SM_Inn',12,16,2,'inn'),('SM_Smithy',10,14,2,'smithy'),('SM_Carpentry',10,14,2,'carpentry'),('SM_TownHouse',8,10,2,'house'),('SM_TownHouse3',8,10,3,'house')]:
    geometry=building(w,d,f,kind)
    ASSEMBLIES[name]=export(name,geometry,'Assemblies')
    export(name+'_Details',geometry.details(),'Assemblies')

for name in ('SM_StreetTree','SM_CourtyardTree','SM_Shrub','SM_FlowerBed'):
    g=Geo()
    if 'Tree' in name:
        tall=1 if name=='SM_StreetTree' else .72
        g.cylinder((0,0,2*tall),.16*tall,4*tall,'Timber',10)
        for i in range(7):
            a=i*2.399;z=(3.4+(i%3)*.65)*tall
            p=(math.cos(a)*1.15*tall,math.sin(a)*1.15*tall,z)
            g.beam((0,0,z-.9),p,.1*tall)
            g.ellipsoid(p,(1.25*tall,1.15*tall,.95*tall),'LeafLight' if i%3==0 else 'Leaf',i)
    elif name=='SM_Shrub':
        for i in range(5):g.ellipsoid(((i-2)*.3,.1*(i%2),.45),(.42,.4,.4),'LeafLight' if i%2 else 'Leaf',i)
    else:
        g.box((0,0,.09),(2,.8,.18),'Timber')
        g.box((0,0,.18),(1.88,.68,.1),'Soil')
        for i in range(13):
            x=(i%7-3)*.25;y=(-.18 if i<7 else .18);z=.4+(i%3)*.07
            g.beam((x,y,.2),(x,y,z),.025,'Leaf')
            g.ellipsoid((x,y,z),(.09,.09,.045),'FlowerRose' if i%3==0 else 'Flower',i)
    export(name,g,'Modules')

(OUT/'town_kit_manifest.json').write_text(json.dumps({'style_id':STYLE['id'],'status':'exported','source_axis':STYLE['source_axis'],'assets':ASSETS},ensure_ascii=False,indent=2),encoding='utf-8')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'AincradTownKit.blend'))
if opt.render:
    obj=ASSEMBLIES['SM_Inn'];obj.hide_set(False);obj.hide_render=False
    g=Geo();g.box((0,0,-.22),(55,55,.2),'StoneLight');g.object('Preview ground')
    world=bpy.data.worlds.new('Anime daylight');world.use_nodes=True;world.node_tree.nodes['Background'].inputs['Color'].default_value=(.65,.77,.9,1);world.node_tree.nodes['Background'].inputs['Strength'].default_value=.5;scene.world=world
    for name,loc,power,size in [('Key',(-9,9,20),2400,8),('Fill',(10,6,11),1800,10)]:
        data=bpy.data.lights.new(name,'AREA');data.energy=power;data.shape='DISK';data.size=size
        light=bpy.data.objects.new(name,data);scene.collection.objects.link(light);light.location=loc;light.rotation_euler=(Vector((0,0,3))-light.location).to_track_quat('-Z','Y').to_euler()
    camera=bpy.data.objects.new('Camera',bpy.data.cameras.new('Camera'));scene.collection.objects.link(camera);scene.camera=camera
    camera.location=(22,30,15);camera.rotation_euler=(Vector((0,0,4))-camera.location).to_track_quat('-Z','Y').to_euler();camera.data.type='ORTHO';camera.data.ortho_scale=27
    scene.render.engine='CYCLES';scene.cycles.samples=24;scene.cycles.use_denoising=True;scene.render.threads_mode='FIXED';scene.render.threads=4
    scene.render.resolution_x=1400;scene.render.resolution_y=1100;scene.render.resolution_percentage=100
    scene.view_settings.view_transform='AgX';scene.render.filepath=str(OUT/'Previews/inn_source.png')
    bpy.ops.render.render(write_still=True)
print('AINCRAD_TOWN_KIT_EXPORTED',len(ASSETS),flush=True)
