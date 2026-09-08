"""Art-directed medieval construction kit. Run in Blender 5.2, no paid services.

The three authored houses are recipes of the SAME editable modules. Structural
cells remove internal walls. Surface palettes and physical overlays are separate
from structure; the exported GLBs use portable Principled materials and UVs.
"""
import argparse
import hashlib
import json
import math
import random
import sys
from pathlib import Path

import bpy
import bmesh
from mathutils import Vector, Matrix

OUT = Path(__file__).resolve().parent
for directory in ('Modules', 'Assemblies', 'Previews', 'Recipes'):
    (OUT / directory).mkdir(exist_ok=True)
ARGS = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
parser = argparse.ArgumentParser()
parser.add_argument('--render', action='store_true')
parser.add_argument('--samples', type=int, default=40)
parser.add_argument('--only', default='')
opt = parser.parse_args(ARGS)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1.0

PALETTES = {
    'warm_lime': {'plaster':'E8DBB8','plaster_patch':'F3E6C8','wood':'795039','wood_light':'AA7B50',
                  'wood_dark':'453329','accent':'799C91','roof_0':'BD7764','roof_1':'CF8A72',
                  'roof_2':'DDA089','roof_3':'AC675B'},
    'ochre_slate': {'plaster':'D8B777','plaster_patch':'E9CE99','wood':'614A3D','wood_light':'A17B58',
                    'wood_dark':'3C3330','accent':'517D86','roof_0':'586E80','roof_1':'728998',
                    'roof_2':'8B9AA3','roof_3':'536577'},
    'sage_clay': {'plaster':'C9D0AE','plaster_patch':'E1DFC0','wood':'795741','wood_light':'A88C63',
                  'wood_dark':'48382E','accent':'A76751','roof_0':'927368','roof_1':'B18A72',
                  'roof_2':'C2A088','roof_3':'84756D'},
}
COMMON = {'stone':'909C99','stone_light':'BBC0AB','stone_dark':'657375','iron':'444C4B',
          'glass':'426C76','glass_light':'8FB0AF','leaf':'67874A','leaf_light':'91A55F',
          'flower':'E8AF58','flower_red':'C57965','soil':'655241','grain':'D8B169',
          'cloth':'EFE0B6','moss':'7C8E54','pot':'B97256','endgrain':'C49C68'}
MATERIALS = {}

def linear(v):
    v /= 255.0
    return v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4

def material(role, palette='warm_lime'):
    key = (palette if role in PALETTES[palette] else 'shared', role)
    if key in MATERIALS:
        return MATERIALS[key]
    rgb = PALETTES[palette].get(role, COMMON.get(role, 'FFFFFF'))
    c = tuple(linear(int(rgb[i:i+2], 16)) for i in (0,2,4)) + (1,)
    mat = bpy.data.materials.new(f'{key[0]} | {role}')
    mat.diffuse_color = c
    mat.use_nodes = True
    p = mat.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value = c
    p.inputs['Roughness'].default_value = .33 if role == 'glass' else .82
    p.inputs['Metallic'].default_value = .35 if role == 'iron' else 0.0
    mat['semantic_slot'] = role
    mat['portable_surface'] = 'glTF metallic-roughness; separate geometric wear overlay'
    MATERIALS[key] = mat
    return mat

def collection(name):
    c = bpy.data.collections.new(name)
    scene.collection.children.link(c)
    return c

LIB = collection('00 | MODULAR LIBRARY - hidden in renders')
LIB.hide_render = True
LIB.hide_viewport = True
STAGE = collection('90 | presentation terrain and lights - not exported')

class Geometry:
    def __init__(self):
        self.v, self.f, self.mi, self.roles = [], [], [], []

    def mesh(self, verts, faces, role):
        base = len(self.v)
        self.v.extend([tuple(v) for v in verts])
        self.f.extend([tuple(base+i for i in f) for f in faces])
        if role not in self.roles:
            self.roles.append(role)
        self.mi.extend([self.roles.index(role)] * len(faces))

    def box(self, pos, size, role='wood', rotation=None):
        p = Vector(pos)
        verts = [Vector((sx*size[0]/2,sy*size[1]/2,sz*size[2]/2))
                 for sx,sy,sz in [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),
                                  (-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]]
        verts = [p + (rotation @ v if rotation else v) for v in verts]
        self.mesh(verts, [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],role)

    def beam(self, a, b, width=.13, depth=None, role='wood'):
        a,b = Vector(a),Vector(b)
        self.box((a+b)/2,(width,depth or width,(b-a).length),role,(b-a).to_track_quat('Z','Y').to_matrix())

    def cylinder(self, pos, radius, length, role='wood', n=10, direction=(0,0,1)):
        p = Vector(pos)
        rot = Vector(direction).to_track_quat('Z','Y').to_matrix()
        verts = [p+rot@Vector((radius*math.cos(i*2*math.pi/n),radius*math.sin(i*2*math.pi/n),z))
                 for z in (-length/2,length/2) for i in range(n)]
        faces = [tuple(reversed(range(n))),tuple(range(n,2*n))]
        faces += [(i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n)]
        self.mesh(verts,faces,role)

    def leaf(self, pos, size, role='leaf'):
        # Closed, faceted foliage clusters, deliberately sparse and legible.
        x,y,z=pos; a,b,c=size
        vs=[(x-a,y,z),(x,y-b,z),(x+a,y,z),(x,y+b,z),(x,y,z+c),(x,y,z-c)]
        self.mesh(vs,[(0,1,4),(1,2,4),(2,3,4),(3,0,4),(1,0,5),(2,1,5),(3,2,5),(0,3,5)],role)

    def object(self, name, coll, layer, palette='warm_lime', bevel=.018):
        if not self.v:
            return None
        mesh = bpy.data.meshes.new(name)
        mesh.from_pydata(self.v,[],self.f)
        mesh.update()
        for role in self.roles:
            mesh.materials.append(material(role,palette))
        for polygon, index in zip(mesh.polygons,self.mi):
            polygon.material_index=index
        bm=bmesh.new(); bm.from_mesh(mesh)
        bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces))
        bm.to_mesh(mesh); bm.free()
        # Planar UVs in metres, retained in GLB and useful for future texture work.
        uv=mesh.uv_layers.new(name='UV0_meters')
        for face in mesh.polygons:
            axis=max(range(3),key=lambda a:abs(face.normal[a]))
            axes=((1,2),(0,2),(0,1))[axis]
            for li in face.loop_indices:
                v=mesh.vertices[mesh.loops[li].vertex_index].co
                uv.data[li].uv=(v[axes[0]],v[axes[1]])
        obj=bpy.data.objects.new(name,mesh); coll.objects.link(obj)
        obj['layer']=layer
        obj['material_roles']=self.roles
        if bevel:
            mod=obj.modifiers.new('Soft crafted edges','BEVEL')
            mod.width=bevel; mod.segments=2
            mod.limit_method='ANGLE'; mod.angle_limit=.6
        return obj

MODULES={}

def module(mid, layers, anchors=None, connectors=None, notes=''):
    root=bpy.data.objects.new(mid,None); LIB.objects.link(root)
    root['module_id']=mid
    root['grid_m']=2.0
    root['notes']=notes
    children=[]
    for layer,geo in layers.items():
        ob=geo.object(f'{mid}__{layer}',LIB,layer,bevel=.018 if layer!='weathering' else .004)
        if ob: ob.parent=root; children.append(ob)
    for name,pos in (anchors or {'origin':(0,0,0)}).items():
        ob=bpy.data.objects.new(f'{mid}__anchor_{name}',None); LIB.objects.link(ob)
        ob.parent=root; ob.location=pos; ob['anchor']=name; ob.empty_display_size=.12
        children.append(ob)
    MODULES[mid]={'root':root,'children':children,'connectors':connectors or [],'notes':notes}

def framed_wall(kind, height=2.4):
    s,f=Geometry(),Geometry()
    for x in (-.93,.93): s.box((x,0,height/2),(.14,.21,height),'wood')
    for z in (.10,height-.09): s.box((0,0,z),(2,.22,.18),'wood')
    if kind=='plain':
        f.box((0,.025,height/2),(1.75,.13,height-.2),'plaster')
        s.beam((-.83,-.074,.22),(.83,-.074,height-.22),.095,.08)
    elif kind=='window':
        for x in (-.665,.665): f.box((x,.02,height/2),(.47,.14,height-.2),'plaster')
        f.box((0,.02,.46),(.87,.14,.72),'plaster')
        f.box((0,.02,2.06),(.87,.14,.50),'plaster')
        s.box((0,.03,1.34),(.83,.085,1.04),'wood_dark')
        f.box((0,-.025,1.36),(.66,.025,.86),'glass')
        for x in (-.41,.41): s.box((x,-.10,1.35),(.10,.12,1.12),'wood_light')
        for z in (.80,1.90): s.box((0,-.12,z),(.96,.19,.11),'wood_light')
        for x in (-.12,.12): s.box((x,-.067,1.36),(.035,.045,.85),'wood_light')
        s.box((0,-.073,1.40),(.69,.05,.04),'wood_light')
        # Individually boarded shutters, hinges and subtle glazing glints.
        for side in (-1,1):
            for i in range(3): f.box((side*(.51+i*.112),-.13,1.34),(.102,.075,.92),'accent')
            for z in (.99,1.69):
                s.box((side*.63,-.177,z),(.36,.035,.055),'wood_dark')
                s.cylinder((side*.47,-.20,z),.025,.025,'iron',8,(0,1,0))
        f.beam((-.26,-.044,1.5),(-.08,-.044,1.76),.024,.018,'glass_light')
    elif kind in ('door','passage'):
        for x in (-.705,.705): f.box((x,.025,height/2),(.39,.13,height-.2),'plaster')
        f.box((0,.025,2.22),(1.02,.13,.18),'plaster')
        for x in (-.5,.5): s.box((x,-.08,1.08),(.12,.20,2.15),'wood_light')
        s.box((0,-.085,2.11),(1.16,.23,.14),'wood_light')
        if kind=='door':
            for i in range(7): f.box((-.39+i*.13,-.011,1.02),(.12,.085,1.96),'wood')
            for z in (.42,1.54):
                s.box((0,-.067,z),(.84,.045,.09),'iron')
                for x in (-.32,.32): s.cylinder((x,-.10,z),.023,.025,'iron',8,(0,1,0))
            s.cylinder((.27,-.105,1.05),.072,.036,'iron',12,(0,1,0))
            f.cylinder((.27,-.13,1.05),.036,.037,'wood_dark',10,(0,1,0))
    module('wall_'+kind+'_2m',{'structure':s,'finish':f},
           {'west':(-1,0,0),'east':(1,0,0),'stack':(0,0,height),'outward':(0,-.2,height/2)},
           ['wall_edge','wall_top','facade_attachment'], 'Front is -Y; width 2m; openings are split meshes, not decals.')

for kind in ('plain','window','door','passage'): framed_wall(kind)

s,f=Geometry(),Geometry()
for x in (-.93,.93): s.box((x,0,.30),(.14,.21,.60))
s.box((0,0,.52),(2,.21,.16))
f.box((0,.02,.27),(1.75,.13,.43),'plaster')
module('wall_knee_2m',{'structure':s,'finish':f},{'west':(-1,0,0),'east':(1,0,0),'stack':(0,0,.6)},['wall_edge','wall_top'])

s,f=Geometry(),Geometry()
s.box((0,0,.12),(2,2,.24),'stone_dark')
f.box((0,0,.325),(2.04,2.04,.15),'stone_light')
module('foundation_cell_2m',{'structure':s,'finish':f},{'stack':(0,0,.4)},['foundation_top'])
s,f=Geometry(),Geometry()
for z in (.10,.29):
    for j in range(4):
        f.box((-.75+j*.5+(0.03 if z<.2 else -.015),-.055,z),(.48,.25,.18),('stone','stone_light','stone','stone_dark')[j])
module('stone_skirt_2m',{'finish':f},{'origin':(0,0,0)},['facade_attachment'])
s=Geometry()
for j in range(10): s.box((-.9+j*.2,0,.025),(.192,2,.05),'wood_light')
for x in (-.88,.88): s.box((x,0,-.07),(.15,2,.13))
module('floor_cell_2m',{'structure':s},{'stack':(0,0,0)},['floor_top'])

def roof(span, rise, rows):
    s,f=Geometry(),Geometry(); rng=random.Random(172+int(span))
    half=span/2+.22; slope=math.atan2(rise,span/2); length=half/math.cos(slope)
    top=rise
    for side in (-1,1):
        down=Vector((side*math.cos(slope),0,-math.sin(slope)))
        normal=Vector((side*math.sin(slope),0,math.cos(slope)))
        origin=Vector((0,0,top))
        verts=[origin+down*u+Vector((0,v,0))+normal*w for w in (-.11,0) for u,v in [(0,-1),(length,-1),(length,1),(0,1)]]
        s.mesh(verts,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],'wood_dark')
        step=length/rows; tw=.4
        for row in range(rows):
            for col in range(6):
                start=-1+col*tw-(tw/2 if row%2 else 0)
                lo,hi=max(-1,start),min(1,start+tw-.012)
                if hi-lo<.07: continue
                tlen=min(step*1.14,length-row*step+.04)
                vs=[]
                for layer in range(2):
                    for end in range(2):
                        for k in range(5):
                            v=lo+(hi-lo)*k/4
                            lift=.037+math.sin(k*math.pi/4)*.035-(.026 if layer==0 else 0)+(.009 if end else 0)
                            vs.append(origin+down*(row*step+end*tlen)+Vector((0,v,0))+normal*lift)
                fs=[]
                for layer in range(2):
                    b=layer*10
                    for k in range(4):fs.append((b+k,b+k+1,b+k+6,b+k+5))
                for end in range(2):
                    b=end*5
                    for k in range(4):fs.append((b+k,b+k+1,b+k+11,b+k+10))
                fs += [(0,5,15,10),(4,9,19,14)]
                f.mesh(vs,fs,'roof_'+str(rng.randrange(4)))
        s.beam((side*half,-1,-.18),(side*half,1,-.18),.14,.14,'wood_light')
        # Deliberately no bargeboard across each internal roof bay.
    for j in range(6):
        ya=-1+j/3; yb=ya+.35
        vs=[(math.cos(k*math.pi/6)*r,y,top+.025+math.sin(k*math.pi/6)*r) for r in (.095,.145) for y in (ya,yb) for k in range(7)]
        fs=[]
        for k in range(6): fs += [(k,k+1,k+8,k+7),(14+k,21+k,22+k,15+k),(k,14+k,15+k,k+1),(7+k,8+k,22+k,21+k)]
        fs += [(0,7,21,14),(6,20,27,13)]
        f.mesh(vs,fs,'roof_1')
    module(f'roof_gable_{span}x2m',{'structure':s,'finish':f},{'front':(0,-1,0),'rear':(0,1,0),'ridge':(0,0,rise)},['roof_bay','roof_ridge'])
    s,f=Geometry(),Geometry()
    vs=[(x,y,z) for y in (-.08,.08) for x,z in [(-span/2,0),(span/2,0),(0,rise)]]
    f.mesh(vs,[(0,2,1),(3,4,5),(0,1,4,3),(1,2,5,4),(2,0,3,5)],'plaster')
    s.box((0,-.09,.015),(span+.1,.18,.15))
    s.beam((0,-.11,0),(0,-.11,rise),.14,.16)
    for side in (-1,1):
        s.beam((0,-.10,rise+.02),(side*(span/2+.25),-.10,-.18),.18,.20,'wood_light')
        if span>2: s.beam((side*.18,-.13,.10),(side*.9,-.13,.82),.10,.10)
    if span>2:
        f.cylinder((0,-.1,.85),.25,.03,'wood_dark',12,(0,1,0))
        for x in (-.12,0,.12): s.box((x,-.15,.85),(.035,.035,.36),'wood_light')
    module(f'gable_end_{span}m',{'structure':s,'finish':f},{'origin':(0,0,0)},['gable_end'])

roof(4,1.6,8); roof(2,.9,5)

# Independent facade layers: removable repairs, weathering, vegetation, fixtures.
w=Geometry()
for x,z,sx,sz in [(-.37,.29,.50,.19),(.07,.32,.33,.22),(.35,.27,.23,.17)]:
    w.box((x,-.091,z),(sx,.018,sz),'plaster_patch')
for x,z in [(-.58,.10),(-.38,.12),(.22,.10),(.61,.12)]:
    w.box((x,-.10,z),(.18,.018,.04),'moss')
module('overlay_lime_repair',{'weathering':w},{'origin':(0,0,0)},['facade_attachment'],'Physical overlay offset from plaster; survives GLB/FBX without custom shaders.')
w=Geometry()
for x,z in [(-.68,.15),(-.55,.46),(-.61,.78),(-.44,1.00),(-.65,1.29)]:
    w.beam((x,-.19,z-.20),(x+.08,-.2,z+.18),.023,.025,'wood_dark')
    for side in (-1,1):w.leaf((x+side*.09,-.23,z),(.16,.045,.13),'leaf_light' if side>0 else 'leaf')
module('overlay_ivy',{'weathering':w},{'origin':(0,0,0)},['facade_attachment'])
a=Geometry()
a.box((0,-.28,.73),(.92,.32,.13),'wood_dark')
a.box((0,-.46,.77),(1.04,.075,.22),'accent')
for x in (-.48,.48):a.box((x,-.29,.77),(.075,.36,.22),'accent')
a.box((0,-.29,.85),(.87,.25,.025),'soil')
for i in range(7):
    x=-.38+i*.125
    a.leaf((x,-.27,.95),(.13,.17,.15),'leaf' if i%2 else 'leaf_light')
    if i%2==0:a.leaf((x,-.33,1.09),(.055,.055,.055),'flower')
module('attachment_flowerbox',{'attachments':a},{'origin':(0,0,0)},['facade_attachment'])
a=Geometry()
for z in (.09,.24,.39):
    depth={.09:1.14,.24:.78,.39:.40}[z]
    a.box((0,-depth/2,z), (1.28,depth,.18),'stone_light')
module('attachment_steps_040',{'attachments':a},{'landing':(0,0,.48)},['door_step'])
a=Geometry()
for i in range(5):a.box((0,-(5-i)*.17,.08+i*.16),(1.32,(5-i)*.34,.16),'stone_light')
module('attachment_steps_080',{'attachments':a},{'landing':(0,0,.80)},['door_step'])

a=Geometry()
for j in range(7):
    z=.12+j*.23
    for side in (-1,1):
        a.box((side*.275,0,z),(.15,.68,.215),'stone_light' if j%3==0 else 'stone')
        a.box((0,side*.275,z),(.40,.15,.215),'stone')
for side in (-1,1):
    a.box((side*.31,0,1.70),(.18,.8,.15),'stone_light')
    a.box((0,side*.31,1.70),(.45,.18,.15),'stone_light')
a.box((0,0,1.42),(.39,.39,.07),'stone_dark')
module('attachment_chimney',{'attachments':a},{'base':(0,0,0)},['roof_attachment'],'Hollow rim with recessed dark flue.')

a=Geometry()
for x in (-.91,.91):
    a.box((x,-.82,1.15),(.16,.16,2.30))
    a.box((x,-.82,.1),(.25,.25,.2),'stone')
    a.beam((x,-.82,1.70),(x,-.23,2.22),.12)
for y,z in [(-.9,2.22),(1,2.69)]:a.box((0,y,z),(2.15,.17,.17),'wood_light')
for j in range(10):
    x=-.99+j*.22
    a.beam((x,-1.03,2.2),(x,1.03,2.71),.212,.075,'wood_light' if j%3 else 'wood')
module('attachment_work_canopy_2m',{'attachments':a},{'rear':(0,1,2.7)},['canopy_edge'])

a=Geometry()
for i in range(11):a.box((-.94+i*.188,0,0),(.181,1.12,.11),'wood_light')
for x in (-.93,.93):
    a.box((x,-.5,.48),(.12,.12,1.05))
    a.beam((x,.43,-.67),(x,-.50,-.08),.13)
a.box((0,-.52,.98),(2.1,.13,.13),'wood_light')
for x in [-.75+i*.25 for i in range(7)]:a.box((x,-.52,.5),(.07,.08,.90),'accent')
module('attachment_balcony_2m',{'attachments':a},{'rear':(0,.56,0)},['floor_attachment'])

a=Geometry()
for i in range(8):
    x=-.91+i*.26
    for j in range(6):
        y=-.82+j*.29; t=j/5; z=1.98+.34*t-.055*math.sin(math.pi*t)
        a.box((x,y,z),(.255,.315,.035),'accent' if i%2==0 else 'cloth')
for x in (-1.02,1.02):a.box((x,-.92,1.0),(.09,.09,2.0),'wood')
a.box((0,-.9,1.95),(2.15,.09,.10),'wood_light')
a.box((0,.77,2.32),(2.13,.08,.08),'wood_light')
for x in (-1.02,1.02):a.beam((x,.72,2.33),(x,1.03,2.37),.07,.07,'iron')
module('attachment_market_awning',{'attachments':a},{'rear':(0,1.03,2.37)},['facade_attachment'])

a=Geometry()
for x in (-.72,.72):
    for y in (-.27,.27):a.box((x,y,.43),(.12,.12,.83))
for j in range(4):a.box((0,-.3+j*.20,.90),(1.7,.19,.13),'wood_light')
a.box((.48,-.40,.76),(.28,.19,.15),'wood_dark')
a.cylinder((.48,-.49,.75),.03,.24,'iron',8,(0,1,0))
for i in range(3):a.box((-.28+i*.22,0,1.0),(.13,.48,.055),'endgrain')
module('prop_workbench',{'attachments':a},{'origin':(0,0,0)},['ground_prop'])
a=Geometry()
for row in range(3):
    for i in range(4-row):
        x=(i-(3-row)/2)*.26;z=.16+row*.23
        a.cylinder((x,0,z),.14,1.18,'wood',10,(0,1,0))
        for y in (-.597,.597):
            a.cylinder((x,y,z),.115,.012,'endgrain',10,(0,1,0))
            a.cylinder((x,y*1.014,z),.041,.014,'wood_light',10,(0,1,0))
module('prop_log_stack',{'attachments':a},{'origin':(0,0,0)},['ground_prop'])
a=Geometry()
for i in range(12):
    t=i*2*math.pi/12
    verts=[]
    for z,r in [(0,.27),(.18,.32),(.54,.32),(.72,.27)]:
        for da in (-.238,.238):verts.append((r*math.cos(t+da),r*math.sin(t+da),z))
    a.mesh(verts,[(0,1,3,2),(2,3,5,4),(4,5,7,6)],'wood_light' if i%3 else 'wood')
for z in (.13,.57):
    # Closed annular iron hoops.
    vs=[(r*math.cos(i*2*math.pi/12),r*math.sin(i*2*math.pi/12),zz) for zz in (z-.025,z+.025) for r in (.312,.335) for i in range(12)]
    fs=[]
    for i in range(12):
        j=(i+1)%12
        fs +=[(i,j,12+j,12+i),(24+i,36+i,36+j,24+j),(12+i,12+j,36+j,36+i),(i,24+i,24+j,j)]
    a.mesh(vs,fs,'iron')
a.cylinder((0,0,.714),.268,.025,'endgrain',12)
module('prop_barrel',{'attachments':a},{'origin':(0,0,0)},['ground_prop'])
a=Geometry()
for j in range(5):a.box((0,-.28+j*.14,.53),(1.75,.13,.10),'wood_light')
for x in (-.74,.74):
    for y in (-.23,.23):a.box((x,y,.26),(.12,.12,.52))
module('prop_bench',{'attachments':a},{'origin':(0,0,0)},['ground_prop'])
a=Geometry()
for x in (-.93,.93): a.box((x,0,.47),(.13,.13,.94))
for z in (.25,.70): a.box((0,0,z),(2,.08,.09),'wood_light')
for x in [-.8+i*.20 for i in range(9)]: a.box((x,0,.46),(.09,.07,.79),'accent')
module('attachment_garden_fence_2m',{'attachments':a},{'west':(-1,0,0),'east':(1,0,0)},['fence_edge'])

a=Geometry()
a.box((0,0,.035),(.64,.46,.07),'wood_dark')
for x in (-.3,.3):a.box((x,0,.15),(.05,.47,.29),'wood_light')
for y in (-.22,.22):
    for z in (.10,.23):a.box((0,y,z),(.64,.045,.09),'wood_light')
for i in range(4):
    for j in range(3):a.leaf((-.22+i*.145,-.13+j*.13,.20+((i+j)%2)*.04),(.084,.079,.085),'flower' if (i+j)%3 else 'leaf_light')
module('prop_produce_crate',{'attachments':a},{'origin':(0,0,0)},['ground_prop'])

a=Geometry()
a.beam((0,0,0),(0,-.90,0),.08,.08,'iron')
a.beam((0,0,.38),(0,-.62,0),.055,.055,'iron')
for y in (-.45,-.78):a.beam((0,y,-.02),(0,y,-.28),.025,.025,'iron')
# Sign is read from the side of the bracket. An abstract sheaf is geometry.
a.box((0,-.63,-.49),(.09,.64,.47),'wood_dark')
a.box((.054,-.63,-.49),(.032,.55,.38),'accent')
for side in (-1,1):
    a.beam((side*.08,-.63,-.64),(side*.08,-.63,-.35),.021,.021,'grain')
    for i in range(3):
        for direction in (-1,1):a.leaf((side*.09,-.63+direction*.06,-.39-i*.06),(.022,.065,.04),'grain')
module('attachment_shop_sign',{'attachments':a},{'wall':(0,0,0)},['facade_attachment'])

ASSEMBLIES=[]
CURRENT=None

def start_house(mid,label,palette,origin):
    global CURRENT
    coll=collection('HOUSE | '+label)
    root=bpy.data.objects.new(mid,None);coll.objects.link(root);root.location=origin
    root['assembly_id']=mid;root['palette']=palette
    CURRENT={'id':mid,'label':label,'palette':palette,'origin':origin,'collection':coll,'root':root,'pieces':[],
             'boundary_walls':[],'cells':[],'omitted_internal_wall_edges':0}
    ASSEMBLIES.append(CURRENT)

def put(mid,pos=(0,0,0),yaw=0,palette=None,layers=None,purpose=''):
    recipe=CURRENT; spec=MODULES[mid]; pal=palette or recipe['palette']
    index=len(recipe['pieces'])
    root=bpy.data.objects.new(f'{recipe["id"]}__{index:03d}__{mid}',None)
    recipe['collection'].objects.link(root);root.parent=recipe['root'];root.location=pos;root.rotation_euler.z=math.radians(yaw)
    root['module_id']=mid;root['purpose']=purpose
    for src in spec['children']:
        if src.type=='MESH' and layers and src.get('layer') not in layers: continue
        ob=src.copy();recipe['collection'].objects.link(ob);ob.parent=root
        ob.name=root.name+'__'+str(src.get('layer','anchor_'+str(src.get('anchor','origin'))))
        if ob.type=='MESH':
            # Palettes replace semantic slots, never the form or attachment layout.
            ob.data=src.data.copy()
            for i,mat in enumerate(ob.data.materials): ob.data.materials[i]=material(mat['semantic_slot'],pal)
    piece={'module':mid,'translation_m':list(pos),'yaw_degrees':yaw,'palette':pal,
           'layers':layers or sorted({c.get('layer') for c in spec['children'] if c.type=='MESH'}),'purpose':purpose}
    identity={k:piece[k] for k in ('module','translation_m','yaw_degrees','palette','layers')}
    piece['instance_key']=hashlib.sha256(json.dumps(identity,sort_keys=True,separators=(',',':')).encode()).hexdigest()[:20]
    root['instance_key']=piece['instance_key']
    recipe['pieces'].append(piece)
    return root

DIRECTIONS=[(0,-1,0),(1,0,90),(0,1,180),(-1,0,270)]

def shell(cells,z=.4, doors=(), plain=(), foundation=False, knee=False):
    cells=set(cells);CURRENT['cells'].append({'z_m':z,'cells':[list(c) for c in sorted(cells)],'wall_height_m':.6 if knee else 2.4})
    for x,y in sorted(cells):
        if not knee: put('floor_cell_2m',(x*2+1,y*2+1,z))
        if foundation:
            courses=round(z/.4)
            for level in range(courses):put('foundation_cell_2m',(x*2+1,y*2+1,level*.4))
        for dx,dy,ang in DIRECTIONS:
            if (x+dx,y+dy) in cells:
                CURRENT['omitted_internal_wall_edges']+=1
                continue
            key=(x,y,ang)
            kind='door' if key in doors else 'plain' if key in plain else 'window'
            pos=(x*2+1+dx,y*2+1+dy,z)
            put('wall_knee_2m' if knee else 'wall_'+kind+'_2m',pos,ang,purpose='boundary only; interior shared wall omitted')
            CURRENT['boundary_walls'].append({'cell':[x,y],'outward_degrees':ang,'z_m':z,'kind':'knee' if knee else kind})
            if foundation:
                for level in range(round(z/.4)):put('stone_skirt_2m',(pos[0],pos[1],level*.4),ang)
            if not knee and kind=='window' and (x+y+ang//90)%3==0:
                put('attachment_flowerbox',pos,ang)
            if not knee and kind!='door' and (x*3+y+ang//90)%3==1:
                put('overlay_lime_repair',pos,ang)

def roof_run(span, bays, center, eave, yaw=0):
    x,y=center
    rot=Matrix.Rotation(math.radians(yaw),3,'Z')
    for i in range(bays):
        p=Vector((x,y,eave))+rot@Vector((0,-bays+1+2*i,0))
        put(f'roof_gable_{span}x2m',p,yaw)
    for sign in (-1,1):
        p=Vector((x,y,eave))+rot@Vector((0,sign*bays,0))
        put(f'gable_end_{span}m',p,yaw+(180 if sign>0 else 0))

def porch(cells, height):
    """Open, supported platform. No phantom walls around an open workshop."""
    cells=set(cells)
    for x,y in sorted(cells):
        for level in range(round(height/.4)):
            put('foundation_cell_2m',(x*2+1,y*2+1,level*.4))
        put('floor_cell_2m',(x*2+1,y*2+1,height))
        for dx,dy,ang in DIRECTIONS:
            if (x+dx,y+dy) not in cells and dy<=0:
                for level in range(round(height/.4)):
                    put('stone_skirt_2m',(x*2+1+dx,y*2+1+dy,level*.4),ang)

# 01: a workshop grows around a lower timber store, with an open working court.
main={(0,0),(1,0),(0,1),(1,1)}
start_house('carpenter_court','Carpenter - working courtyard','warm_lime',(-10,0,0))
shell(main|{(-1,-1),(-1,0),(-1,1)},doors={(0,0,0),(-1,-1,0)},plain={(-1,0,270),(1,1,180)},foundation=True)
shell(main,2.8,knee=True)
roof_run(4,2,(2,2),3.4)
roof_run(2,3,(-1,1),2.8)
porch({(0,-1),(1,-1)},.4)
put('attachment_work_canopy_2m',(1,-1,.4))
put('attachment_work_canopy_2m',(3,-1,.4))
put('attachment_steps_040',(1,-2.10,0))
put('attachment_steps_040',(-1,-2.13,0))
put('prop_workbench',(3,-1.07,.4),0,purpose='carpenter occupation')
put('prop_log_stack',(-2.8,-2.15,0),0,purpose='raw timber storage; clear doorway')
put('prop_log_stack',(-2.65,1.0,0),0)
put('prop_barrel',(4.5,1.2,0))
put('attachment_chimney',(3.25,2.9,3.75))
put('overlay_ivy',(-2,2.8,.4),270)

# 02: compact upper rooms over the shop; balcony and lower side stockroom.
start_house('merchant_steps','Merchant - raised shop and balcony','ochre_slate',(1,5,0))
shell(main|{(2,0)},.8,doors={(0,0,0),(2,0,0)},plain={(0,1,180)},foundation=True)
shell(main,3.2,doors={(0,0,0)},plain={(0,1,180)})
roof_run(4,2,(2,2),5.6)
roof_run(2,1,(5,1),3.2)
porch({(0,-1),(1,-1)},.8)
put('attachment_balcony_2m',(1,-.6,3.2))
put('attachment_balcony_2m',(3,-.6,3.2))
put('attachment_market_awning',(3,-1.0,.8))
put('attachment_steps_080',(1,-2.12,0))
put('attachment_steps_080',(5,-.15,0))
put('prop_workbench',(3,-1.1,.8),0,purpose='shop display counter')
put('prop_produce_crate',(2.5,-1.1,1.77))
put('prop_produce_crate',(3.3,-1.1,1.77))
put('attachment_shop_sign',(6.1,.3,2.85),90)
put('prop_barrel',(6.6,.8,0))
put('prop_barrel',(6.7,1.6,0))
put('attachment_chimney',(.6,2.8,5.6))

# 03: two low wings embrace a court; a later bedroom rises at the rear corner.
start_house('family_cluster','Family - three wings and bedroom annex','sage_clay',(4,-7,0))
family=main|{(-1,-1),(-1,0),(2,-1),(2,0),(2,1)}
shell(family,.4,doors={(0,0,0),(2,-1,270)},plain={(-1,-1,270),(2,1,90)},foundation=True)
shell({(2,1)},2.8,plain={(2,1,270)})
roof_run(4,2,(2,2),2.8,90)
roof_run(2,2,(-1,0),2.8)
roof_run(2,2,(5,0),2.8)
roof_run(2,1,(5,3),5.2)
put('attachment_steps_040',(1,-.14,0))
put('attachment_steps_040',(4.10,-1,0),270)
put('prop_bench',(2.8,-2.1,0),0,purpose='household shared courtyard; main and annex doors clear')
put('prop_barrel',(-2.7,-.9,0))
put('attachment_garden_fence_2m',(3,-2.7,0))
put('attachment_garden_fence_2m',(-1,-3.0,0))
put('attachment_chimney',(1.0,3.25,3.3))
put('overlay_ivy',(6,2.8,.4),90)
put('overlay_ivy',(-2,1.0,.4),270)

# Two smaller stages use exactly the same rules and pieces as the final family
# house. At each extension shell() removes the newly internal facade; we do not
# simply push another closed prefab through the existing wall.
start_house('family_starter','Family growth 01 - first home','sage_clay',(-10,-15,0))
shell(main,.4,doors={(0,0,0)},foundation=True)
roof_run(4,2,(2,2),2.8,90)
put('attachment_steps_040',(1,-.14,0))
put('attachment_chimney',(1.0,3.25,3.3))

start_house('family_side_wing','Family growth 02 - lower side wing','sage_clay',(-1,-18,0))
shell(main|{(-1,-1),(-1,0)},.4,doors={(0,0,0)},plain={(-1,-1,270)},foundation=True)
roof_run(4,2,(2,2),2.8,90)
roof_run(2,2,(-1,0),2.8)
put('attachment_steps_040',(1,-.14,0))
put('attachment_chimney',(1.0,3.25,3.3))

def descendants(root):
    return [root]+list(root.children_recursive)

def stats(root):
    was_hidden=LIB.hide_viewport
    LIB.hide_viewport=False
    bpy.context.view_layer.update()
    obs=descendants(root);meshes=[o for o in obs if o.type=='MESH']
    dg=bpy.context.evaluated_depsgraph_get();points=[];tri=0
    for o in meshes:
        ev=o.evaluated_get(dg);me=ev.to_mesh();me.calc_loop_triangles();tri+=len(me.loop_triangles)
        inv=root.matrix_world.inverted()
        points += [inv@o.matrix_world@v.co for v in me.vertices]
        ev.to_mesh_clear()
    lo=[min(p[i] for p in points) for i in range(3)];hi=[max(p[i] for p in points) for i in range(3)]
    result={'bounds_m':{'min':lo,'max':hi,'size':[hi[i]-lo[i] for i in range(3)]},
            'stats':{'mesh_objects':len(meshes),'triangles':tri,'uv_mesh_objects':sum(bool(o.data.uv_layers) for o in meshes)}}
    LIB.hide_viewport=was_hidden
    bpy.context.view_layer.update()
    return result

def export(root,path):
    bpy.ops.object.select_all(action='DESELECT')
    LIB.hide_viewport=False
    orig=root.location.copy();root.location=(0,0,0);bpy.context.view_layer.update()
    for ob in descendants(root):ob.select_set(True)
    bpy.context.view_layer.objects.active=root
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,export_apply=True,
                               export_extras=True,export_yup=True,export_texcoords=True,export_normals=True,
                               export_materials='EXPORT',export_cameras=False,export_lights=False)
    root.location=orig;bpy.context.view_layer.update()
    LIB.hide_viewport=True

catalog={'schema_version':1,'units':'meters','authoring_up':'+Z','front':'-Y','module_grid_m':2.0,
         'story_height_m':2.4,'palettes':PALETTES,'materials':list(COMMON),
         'layer_contract':{'structure':'load-bearing frame and roof substrate','finish':'replaceable plaster, tiles, shutters, stone facing',
                           'weathering':'optional separate repair/moss/ivy mesh, not destructive paint','attachments':'optional anchored fixtures and occupational props'},
         'connector_types':sorted({c for spec in MODULES.values() for c in spec['connectors']}),
         'modules':[],'assemblies':[],
         'limitations':['These are authored visual prototypes. Resident selection is not yet wired to these recipes in the live game.',
                        'No collision meshes or LODs yet. Portable PBR colours and separate overlays; no baked texture maps.',
                        'Roof intersections are authored per recipe; arbitrary unrestricted roof intersections are not supported.']}

bpy.context.view_layer.update()
for mid,spec in MODULES.items():
    path=OUT/'Modules'/f'{mid}.glb'
    export(spec['root'],path)
    catalog['modules'].append({'id':mid,'path':path.relative_to(OUT).as_posix(),
                               'source_hierarchy':{'root':mid},'layers':sorted({c.get('layer') for c in spec['children'] if c.type=='MESH'}),
                               'anchors':[c.get('anchor') for c in spec['children'] if c.get('anchor')],
                               'connectors':spec['connectors'],'notes':spec['notes'],**stats(spec['root'])})
all_recipes={}
for house in ASSEMBLIES:
    root=house['root'];path=OUT/'Assemblies'/f'{house["id"]}.glb'
    export(root,path)
    evidence=stats(root)
    recipe={k:house[k] for k in ('id','label','palette','pieces','cells','boundary_walls','omitted_internal_wall_edges')}
    assert len({p['instance_key'] for p in recipe['pieces']})==len(recipe['pieces']),house['id']
    recipe['kind']='growth_stage' if house['id'] in ('family_starter','family_side_wing') else 'authored_master'
    recipe.update(evidence)
    all_recipes[house['id']]=recipe
    (OUT/'Recipes'/f'{house["id"]}.json').write_text(json.dumps(recipe,ensure_ascii=False,indent=2),encoding='utf-8')
    catalog['assemblies'].append({'id':house['id'],'label':house['label'],'path':path.relative_to(OUT).as_posix(),
                                  'source_hierarchy':{'root':house['id']},'layers':['structure','finish','weathering','attachments'] if any(p['module'].startswith('overlay') for p in house['pieces']) else ['structure','finish','attachments'],
                                  'anchors':['origin'],'recipe':f'Recipes/{house["id"]}.json',**evidence})
growth={'id':'family_house_expansion','source':'authored art demonstration, not live NPC history',
        'stages':['family_starter','family_side_wing','family_cluster'],
        'rules':['Preserve stable module instance keys across stages.',
                 'Dismantle newly internal facade modules before adding the wing; retain reusable materials.',
                 'Remove the old roof over a room before raising its upper floor.',
                 'Only use these authored roof joins until a new join is visually reviewed.',
                 'A future resident decision must fund the added components and have a valid site.'],
        'transitions':[]}
for previous,current in zip(growth['stages'],growth['stages'][1:]):
    a={p['instance_key']:p for p in all_recipes[previous]['pieces']}
    b={p['instance_key']:p for p in all_recipes[current]['pieces']}
    growth['transitions'].append({'from':previous,'to':current,'retain':sorted(a.keys()&b.keys()),
                                   'dismantle':[a[k] for k in sorted(a.keys()-b.keys())],
                                   'add':[b[k] for k in sorted(b.keys()-a.keys())]})
(OUT/'Recipes'/'family_growth.json').write_text(json.dumps(growth,ensure_ascii=False,indent=2),encoding='utf-8')
catalog['growth_sequences']=['Recipes/family_growth.json']
(OUT/'catalog.json').write_text(json.dumps(catalog,ensure_ascii=False,indent=2),encoding='utf-8')
print('MASTER_EXPORTS_COMPLETE',len(MODULES),len(ASSEMBLIES),flush=True)

# Real geometry presentation terrain is excluded from asset exports. It is a
# stage, not a claim about the live game's pathfinding or terrain implementation.
g=Geometry();g.box((0,0,-.23),(200,200,.35),'leaf')
stage_ground=g.object('Studio ground',STAGE,'presentation',bevel=0)
stage_ground.data.materials.clear()
stage_mat=material('plaster','sage_clay').copy();stage_mat.name='STAGE | muted meadow'
stage_mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value=(.235,.292,.215,1)
stage_mat.diffuse_color=(.235,.292,.215,1);stage_ground.data.materials.append(stage_mat)
world=bpy.data.worlds.new('Soft daylight');world.use_nodes=True
world.node_tree.nodes['Background'].inputs['Color'].default_value=(.73,.81,.9,1)
world.node_tree.nodes['Background'].inputs['Strength'].default_value=.42;scene.world=world
def area(name,loc,energy,size,color):
    d=bpy.data.lights.new(name,'AREA');d.energy=energy;d.shape='DISK';d.size=size;d.color=color
    o=bpy.data.objects.new(name,d);STAGE.objects.link(o);o.location=loc
    o.rotation_euler=(Vector((0,0,1.7))-o.location).to_track_quat('-Z','Y').to_euler()
    return o
lights=[area('Key - warm',(0,-12,18),2300,9,(1,.88,.73)),
        area('Fill - sky',(13,-4,11),1500,11,(.77,.86,1)),
        area('Rim - sunlight',(-8,11,15),2400,8,(1,.93,.81))]
camdata=bpy.data.cameras.new('Architecture camera');camdata.type='ORTHO'
cam=bpy.data.objects.new('Camera',camdata);STAGE.objects.link(cam);scene.camera=cam
scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=opt.samples
scene.cycles.use_denoising=True;scene.render.threads_mode='FIXED';scene.render.threads=6
scene.render.resolution_x=1500;scene.render.resolution_y=1350;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.view_settings.view_transform='AgX'
scene.view_settings.look='AgX - Medium High Contrast'
for h in ASSEMBLIES:h['collection'].hide_render=False
target=Vector((0,1,2));cam.location=target+Vector((22,-29,22));cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();camdata.ortho_scale=30
for screen in bpy.data.screens:
    for a in screen.areas:
        if a.type=='VIEW_3D':
            a.spaces.active.shading.type='MATERIAL'
            a.spaces.active.region_3d.view_rotation=cam.rotation_euler.to_quaternion()
            a.spaces.active.region_3d.view_distance=35
            a.spaces.active.region_3d.view_location=target
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'OrganicVillage_Masters.blend'))
if opt.render:
    for h in ASSEMBLIES:
        if opt.only and h['id'] not in opt.only.split(','):continue
        for other in ASSEMBLIES:other['collection'].hide_render=other is not h
        origin=Vector(h['origin']);root=h['root'];bounds=stats(root)['bounds_m']
        mid=Vector([(bounds['min'][i]+bounds['max'][i])/2 for i in range(3)])
        target=origin+mid+Vector((0,0,-.12))
        cam.location=target+Vector((11,-15,10));cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler()
        camdata.ortho_scale=max(11.7,bounds['size'][0]*1.4,bounds['size'][1]*1.28,bounds['size'][2]*1.82)
        for light,offset in zip(lights,[(-5,-7,13),(10,-3,9),(-6,9,12)]):
            light.location=origin+Vector(offset)
            light.rotation_euler=(target-light.location).to_track_quat('-Z','Y').to_euler()
        scene.render.filepath=str(OUT/'Previews'/f'{h["id"]}.png')
        bpy.ops.render.render(write_still=True)
        print('MASTER_RENDER_COMPLETE',h['id'],flush=True)
print('ORGANIC_MASTERS_COMPLETE',flush=True)
