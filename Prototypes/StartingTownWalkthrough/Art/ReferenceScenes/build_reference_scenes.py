"""Blender-authored two anime scene reconstructions, metres / Z-up.

No imported artwork; inferred dimensions and unseen geometry remain project infill.
One shared palette and geometry vocabulary. Run Blender --background --python this.
"""
import bpy
import json
import math
import random
import os
import hashlib
from pathlib import Path
from mathutils import Vector, Matrix

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[1]
EXPORT = PROJECT / 'assets' / 'reference_scenes'
EXPORT.mkdir(parents=True, exist_ok=True)
RNG = random.Random(90926)
STYLE = 'level0_anime_reference_geometry_v1'
PALETTE = {
    'limestone': 'C7BA93', 'stone_light': 'E1D6B3', 'stone_shade': 'A99E83',
    'mortar': '9E977F', 'paving': 'BDB7A0', 'paving_light': 'D4CEB5',
    'plaster_cream': 'E9DEBA', 'plaster_peach': 'D9B9A4', 'plaster_sage': 'BBC9B3',
    'plaster_white': 'E4E2C8', 'roof': '985E58', 'roof_light': 'B67969',
    'roof_dark': '774C4B', 'slate': '5B797E', 'wood': '796142', 'wood_light': 'A88C5C',
    'wood_dark': '494A35', 'shutter': '547963', 'glass': '466A65', 'iron': '434C47',
    'canvas_cream': 'EFE5C8', 'canvas_green': '709C79', 'canvas_gold': 'DDB16D',
    'canvas_red': 'AE6659', 'leaf': '628841', 'leaf_light': '8CAE52',
    'leaf_dark': '426739', 'grass': '8FAE57', 'grass_light': 'A8BC6B',
    'grass_dark': '718E43', 'water': '7DAFA0', 'fruit_red': 'B95D45',
    'fruit_yellow': 'D8B451', 'fruit_green': '93A455', 'skin': 'D4B495',
    'cloth_blue': '68828E', 'cloth_brown': '8B795E', 'cloth_dark': '4B5450',
    'rock': '8C977C',
}
MATS = {}

def material(key):
    if key in MATS:
        return MATS[key]
    def lin(v):
        return v/12.92 if v <= .04045 else ((v+.055)/1.055)**2.4
    color = tuple(lin(int(PALETTE[key][i:i+2], 16)/255) for i in (0, 2, 4))+(1,)
    mat = bpy.data.materials.new('REF_'+key)
    mat.diffuse_color = color
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = color
    bsdf.inputs['Roughness'].default_value = .38 if key == 'water' else .86
    bsdf.inputs['Specular IOR Level'].default_value = .18
    mat['style_id'] = STYLE
    MATS[key] = mat
    return mat

class Geo:
    def __init__(self):
        self.v, self.f, self.roles = [], [], []

    def mesh(self, verts, faces, role):
        base = len(self.v)
        self.v.extend(tuple(v) for v in verts)
        self.f.extend(tuple(base+i for i in f) for f in faces)
        self.roles.extend([role]*len(faces))

    def box(self, p, s, role='limestone', rotation=None):
        verts = [Vector((x*s[0]/2, y*s[1]/2, z*s[2]/2))
                 for x,y,z in [(-1,-1,-1),(1,-1,-1),(1,1,-1),(-1,1,-1),
                               (-1,-1,1),(1,-1,1),(1,1,1),(-1,1,1)]]
        self.mesh([Vector(p)+(rotation@v if rotation else v) for v in verts],
                  [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)], role)

    def beam(self, a, b, width, role='wood', width2=None):
        delta = Vector(b)-Vector(a)
        self.box((Vector(a)+Vector(b))/2, (width, width2 or width, delta.length), role,
                 delta.to_track_quat('Z','Y').to_matrix())

    def rings(self, p, rings, role, n=20):
        vs = [(p[0]+r*math.cos(i*math.tau/n), p[1]+r*math.sin(i*math.tau/n), p[2]+z)
              for r,z in rings for i in range(n)]
        fs = [tuple(range(n-1,-1,-1)), tuple((len(rings)-1)*n+i for i in range(n))]
        for j in range(len(rings)-1):
            for i in range(n):
                a=j*n+i; b=j*n+(i+1)%n
                fs.append((a,b,b+n,a+n))
        self.mesh(vs,fs,role)

    def cylinder(self, p, r, h, role, n=16):
        self.rings(p,[(r,0),(r,h)],role,n)

    def ellipsoid(self, p, s, role, n=10, rings=6, irregular=0):
        vs=[]
        for j in range(rings+1):
            a=math.pi*j/rings
            for i in range(n):
                t=i*math.tau/n
                bump=1+RNG.uniform(-irregular,irregular)
                vs.append((p[0]+s[0]*math.sin(a)*math.cos(t)*bump,
                           p[1]+s[1]*math.sin(a)*math.sin(t)*bump,
                           p[2]+s[2]*math.cos(a)*bump))
        fs=[]
        for j in range(rings):
            for i in range(n):
                a=j*n+i; b=j*n+(i+1)%n
                fs.append((a,a+n,b+n,b))
        self.mesh(vs,fs,role)

    def arch(self, x, y, spring, radius, thickness, depth, role='stone_light', segments=16):
        for i in range(segments):
            a=math.pi*i/segments+.009; b=math.pi*(i+1)/segments-.009
            vs=[(x+r*math.cos(t), y+dy, spring+r*math.sin(t))
                for dy in (-depth/2,depth/2) for r,t in [(radius,a),(radius,b),(radius+thickness,b),(radius+thickness,a)]]
            self.mesh(vs,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],role)

    def arch_fill(self,x,y,z,width,height,role):
        r=width/2; spring=z+height-r
        vs=[(x-r,y,z),(x+r,y,z)]+[(x+r*math.cos(i*math.pi/14),y,spring+r*math.sin(i*math.pi/14)) for i in range(15)]
        self.mesh(vs,[tuple(reversed(range(len(vs))))],role)

    def add(self, other, p=(0,0,0), yaw=0):
        rot=Matrix.Rotation(math.radians(yaw),3,'Z'); base=len(self.v)
        self.v.extend(tuple(rot@Vector(v)+Vector(p)) for v in other.v)
        self.f.extend(tuple(base+i for i in f) for f in other.f)
        self.roles.extend(other.roles)

    def obj(self,name,collection,collision=True):
        me=bpy.data.meshes.new(name)
        me.from_pydata(self.v,[],self.f); me.update()
        obj=bpy.data.objects.new(name,me); collection.objects.link(obj)
        keys=list(dict.fromkeys(self.roles)); lookup={k:i for i,k in enumerate(keys)}
        for key in keys: me.materials.append(material(key))
        for poly,key in zip(me.polygons,self.roles):
            poly.material_index=lookup[key]
            if name.endswith('_canopy'):poly.use_smooth=True
        # Metric box-projected UVs are retained for the subsequent texture/shader pass.
        uv=me.uv_layers.new(name='UVMap')
        for poly in me.polygons:
            axis=max(range(3),key=lambda k:abs(poly.normal[k])); axes=[k for k in range(3) if k!=axis]
            for li in poly.loop_indices:
                co=me.vertices[me.loops[li].vertex_index].co
                uv.data[li].uv=(co[axes[0]],co[axes[1]])
        obj['collision_enabled']=collision
        obj['style_id']=STYLE
        obj['evidence']='anime_observed_form_project_dimensions'
        return obj

def window(g,x,y,z,w=1.05,h=1.9,arched=True,shutters=True):
    if arched:
        g.arch_fill(x,y-.018,z,w,h,'glass')
        g.arch(x,y-.065,z+h-w/2,w/2,.13,.17)
    else:
        g.box((x,y,z+h/2),(w,.07,h),'glass')
        g.box((x,y-.075,z+h),(w+.28,.16,.13),'stone_light')
    for side in (-1,1):
        g.box((x+side*(w/2+.07),y-.08,z+(h-w/2 if arched else h)/2),(.14,.18,h-w/2 if arched else h),'stone_light')
        if shutters:
            g.box((x+side*(w*.76+.16),y-.1,z+h*.44),(w*.42,.12,h*.84),'shutter')
            for k in range(6):
                g.box((x+side*(w*.76+.16),y-.18,z+.16+k*h*.12),(w*.4,.04,.055),'wood_dark')
    g.box((x,y-.11,z+h*.43),(.065,.13,h*.86),'wood_light')
    g.box((x,y-.11,z+h*.44),(w,.13,.055),'wood_light')
    g.box((x,y-.2,z-.10),(w+.48,.48,.18),'stone_light')

def roof(g,w,d,z,rise):
    x=w/2+.45; y=d/2+.45
    g.mesh([(-x,-y,z),(x,-y,z),(x,y,z),(-x,y,z),(0,-y,z+rise),(0,y,z+rise)],
           [(0,3,5,4),(1,4,5,2)],'roof_dark')
    rows=max(6,round(x/.48)); cols=max(8,round((2*y)/.43))
    for sign in (-1,1):
        for row in range(rows):
            xx=(row+.5)*x/rows; zz=z+rise*(1-xx/x)+.055
            for col in range(cols):
                yy=-y+(col+.5)*2*y/cols
                rot=Matrix.Rotation(sign*math.atan2(rise,x),3,'Y')
                g.box((sign*xx,yy,zz), (math.hypot(x/rows,rise/rows)+.025,2*y/cols-.045,.055),
                      RNG.choice(['roof','roof','roof_light','roof_dark']),rot)
    g.beam((0,-y-.06,z+rise+.1),(0,y+.06,z+rise+.1),.18,'roof_light')
    for sign in (-1,1):
        g.beam((-x,sign*y,z),(0,sign*y,z+rise),.18,'stone_light')
        g.beam((x,sign*y,z),(0,sign*y,z+rise),.18,'stone_light')

def house(w,d,h,role='plaster_cream',gable=True,stone=False):
    g=Geo(); fy=-d/2
    # Actual front doorway and hollow room; windows use recessed opaque glass inserts.
    g.box((0,0,.14),(w,d,.28),'paving')
    for sign in (-1,1):
        g.box((sign*(w/2-.2),0,h/2),(.4,d,h),role)
        g.box((sign*(w/4+.45),fy,h/2),(w/2-.9,.4,h),role)
    g.box((0,fy,(h+2.7)/2),(1.8,.4,h-2.7),role)
    g.box((0,d/2-.2,h/2),(w,.4,h),role)
    g.box((0,0,h),(w,d,.25),'stone_light')
    for x in (-.98,.98):g.box((x,fy-.09,1.35),(.16,.28,2.7),'stone_light')
    g.box((0,fy-.10,2.73),(2.15,.28,.18),'stone_light')
    # Door stands open inside the room; entrance remains walkable.
    g.box((.89,fy+.75,1.25),(.09,1.4,2.5),'shutter')
    g.box((0,fy-.4,.12),(2.2,.8,.24),'stone_light')
    for zz in (.45,3.7,h-.28):g.box((0,fy-.075,zz),(w+.18,.2,.17),'stone_light')
    if stone:
        for row in range(int(h/.67)):
            zz=(row+.5)*.67
            for col in range(int(w/1.4)+1):
                xx=-w/2+(col+.5)*1.4+(row%2)*.7
                if xx>w/2-.2 or (abs(xx)<1.55 and zz<3.05):continue
                # Exposed joints on the facade, with low amplitude broad color changes.
                g.box((xx,fy-.225,zz),(min(1.36,w/2-xx+.67),.055,.635),
                      RNG.choice(['limestone','limestone','stone_light','stone_shade']))
    count=max(2,int(w/3))
    for zz in [4.65,8.7,12.75]:
        if zz+2.05>h-.5:continue
        for i in range(count):
            xx=-w/2+(i+.5)*w/count
            window(g,xx,fy-.30 if stone else fy-.24,zz,1.0,2.0,True,not stone or i%2==0)
    for xx in (-w*.29,w*.29):
        window(g,xx,fy-.30,1.1,1.25,1.8,True,True)
    if gable:
        rise=w*.40
        roof(g,w,d,h,rise)
        g.mesh([(-w/2,fy-.015,h),(w/2,fy-.015,h),(0,fy-.015,h+rise)],[(0,2,1)],role)
        if rise>2.5:window(g,0,fy-.1,h+.5,.7,1.3,True,False)
    else:
        g.box((0,0,h+.18),(w+.5,d+.4,.35),'stone_light')
        for xx in [-w/2+i*1.5 for i in range(int(w/1.5)+1)]:
            g.box((xx,fy,h+.65),(.65,.65,.95),'limestone')
    return g

def awning(g,w,fy=-4.5,role='canvas_green',z=3.65):
    stripes=max(4,round(w/.65))
    for j in range(stripes):
        xa=-w/2+j*w/stripes; xb=xa+w/stripes-.013
        verts=[]
        for k in range(9):
            t=k/8; yy=fy-2.9*t; zz=z-.68*t-.27*math.sin(t*math.pi)
            verts.extend([(xa,yy,zz),(xb,yy,zz)])
        g.mesh(verts,[(k*2,k*2+1,k*2+3,k*2+2) for k in range(8)],role if j%2==0 else 'canvas_cream')
        xc=(xa+xb)/2; endz=z-.68
        g.mesh([(xa,fy-2.9,endz),(xb,fy-2.9,endz),(xb,fy-2.9,endz-.20),
                (xc,fy-2.9,endz-.32),(xa,fy-2.9,endz-.20)],[(0,1,2,3,4)],role if j%2==0 else 'canvas_cream')
    for xx in (-w/2,w/2):
        g.beam((xx,fy-2.9,0),(xx,fy-2.9,z-.45),.075,'wood')
    g.beam((-w/2,fy-2.88,z-.68),(w/2,fy-2.88,z-.68),.085,'wood')

def stall(g,p,yaw=0,cloth='canvas_green'):
    a=Geo()
    for xx in (-1.2,1.2):
        for yy in (-.46,.46):a.box((xx,yy,.5),(.12,.12,1),'wood_dark')
    a.box((0,0,1.05),(2.8,1.25,.18),'wood_light')
    for j in range(8):a.box((-1.25+j*.35,-.63,.58),(.31,.09,.82),'wood')
    for j in range(3):
        a.box((-.88+j*.9,0,1.2),(.78,.9,.16),'wood_dark')
        for k in range(14):a.ellipsoid((-.88+j*.9+RNG.uniform(-.3,.3),RNG.uniform(-.34,.34),1.36),(.12,.12,.11),['fruit_red','fruit_yellow','fruit_green'][j],8,4)
    g.add(a,p,yaw)

def barrel(g,p,scale=1):
    g.rings(p,[(.36*scale,0),(.43*scale,.2*scale),(.46*scale,.6*scale),(.38*scale,1.05*scale)],'wood',14)
    for z in (.12,.88):g.rings((p[0],p[1],p[2]+z*scale),[(.43*scale,0),(.43*scale,.06*scale)],'iron',14)
    g.cylinder((p[0],p[1],p[2]+1.052*scale),.36*scale,.03*scale,'wood_light',14)

def paving(g,x0,x1,y0,y1,z=.25,step=1.0):
    g.box(((x0+x1)/2,(y0+y1)/2,z-.12),(x1-x0,y1-y0,.24),'mortar')
    for row in range(math.ceil((y1-y0)/step)):
        yy=y0+row*step
        for col in range(math.ceil((x1-x0)/(step*1.5))):
            xx=x0+col*step*1.5+(row%2)*step*.7
            if xx+step*1.5>x1:continue
            g.box((xx+step*.75,yy+step*.5,z+.005),(step*1.5-.035,step-.035,.07),
                  RNG.choice(['paving','paving','paving_light','limestone']))

def figure(g,p,yaw=0,coat='cloth_brown',pose='stand'):
    a=Geo()
    # Spatial population markers; no persistent NPC identity or autonomous behavior.
    a.ellipsoid((0,0,1.63),(.135,.115,.18),'skin',10,6)
    a.ellipsoid((0,.018,1.75),(.145,.13,.115),'wood_dark',10,5)
    a.rings((0,0,.9),[(.18,0),(.23,.30),(.19,.49)],coat,8)
    for s in (-1,1):
        if pose=='sit':
            a.beam((s*.105,0,.92),(s*.105,-.35,.9),.15,'cloth_dark')
            a.beam((s*.105,-.35,.9),(s*.105,-.35,.46),.13,'cloth_dark')
            a.box((s*.105,-.43,.42),(.17,.3,.12),'wood_dark')
        else:
            a.beam((s*.10,0,.94),(s*.12,.025,.16),.15,'cloth_dark')
            a.box((s*.12,-.065,.12),(.18,.33,.18),'wood_dark')
        a.beam((s*.22,0,1.32),(s*.29,-.02,.96),.13,coat)
        a.ellipsoid((s*.29,-.04,.91),(.066,.067,.1),'skin',8,4)
    g.add(a,p,yaw)

def tree(col,p,scale=1,name='tree'):
    trunk=Geo(); leaves=Geo()
    trunk.rings(p,[(.65*scale,0),(.48*scale,2.2*scale),(.30*scale,4.8*scale),(.1*scale,7*scale)],'wood',12)
    for j in range(9):
        angle=j*2.399; r=RNG.uniform(2.4,4.2)*scale
        end=Vector((p[0]+math.cos(angle)*r,p[1]+math.sin(angle)*r,p[2]+RNG.uniform(6.0,8.7)*scale))
        start=Vector((p[0],p[1],p[2]+(2.7+j*.28)*scale))
        trunk.beam(start,end,.20*scale,'wood')
        for k in range(4):
            q=end+Vector((RNG.uniform(-1.8,1.8)*scale,RNG.uniform(-1.8,1.8)*scale,RNG.uniform(-.8,1.2)*scale))
            leaves.ellipsoid(q,(RNG.uniform(1.2,2.1)*scale,RNG.uniform(1.2,2.1)*scale,1.3*scale),
                             ['leaf_dark','leaf','leaf','leaf_light'][k],10,6,.15)
    # Small leaf groups break the silhouette; bounded original geometry, no alpha texture.
    for j in range(260):
        angle=RNG.uniform(0,math.tau); r=math.sqrt(RNG.random())*5.1*scale
        q=(p[0]+math.cos(angle)*r,p[1]+math.sin(angle)*r,p[2]+(7.5+RNG.uniform(-1.3,1.5))*scale)
        leaves.ellipsoid(q,(.36*scale,.24*scale,.17*scale),RNG.choice(['leaf','leaf_light']),6,3)
    trunk.obj(name+'_trunk',col,True);leaves.obj(name+'_canopy',col,False)

def new_scene(name):
    sc=bpy.data.scenes.new(name); sc.unit_settings.system='METRIC'
    sc['style_id']=STYLE;sc['canonical_complete_reconstruction']=False
    bpy.context.window.scene=sc
    return sc

def camera(sc,name,p,target,lens=36):
    data=bpy.data.cameras.new(name);ob=bpy.data.objects.new(name,data);sc.collection.objects.link(ob)
    ob.location=p; ob.rotation_euler=(Vector(target)-ob.location).to_track_quat('-Z','Y').to_euler()
    data.lens=lens;data.clip_end=2000;sc.camera=ob
    return ob

def scene_setup(sc):
    world=bpy.data.worlds.new(sc.name+'_Daylight');world.use_nodes=True
    world.node_tree.nodes['Background'].inputs[0].default_value=(.45,.62,.76,1)
    world.node_tree.nodes['Background'].inputs[1].default_value=.4;sc.world=world
    ld=bpy.data.lights.new(sc.name+'_Sun','SUN');lo=bpy.data.objects.new(sc.name+'_Sun',ld)
    sc.collection.objects.link(lo);lo.rotation_euler=(math.radians(30),math.radians(-25),math.radians(-35));ld.energy=2;ld.angle=.04
    sc.render.resolution_x=1600;sc.render.resolution_y=1000;sc.render.resolution_percentage=100
    sc.view_settings.view_transform='Standard'

def market():
    sc=new_scene('StartingTown_Market');col=sc.collection
    ground=Geo();paving(ground,-7.0,7.0,-26,91,.27,.90)
    ground.box((0,32,-.10),(72,126,.5),'paving')
    for s in (-1,1):ground.box((s*7.2,31,.33),(.3,114,.32),'stone_light')
    ground.obj('market_street_paving',col)
    props=Geo();people=Geo()
    for side in (-1,1):
        for i,(yy,w,h) in enumerate([(-18,12,16),(-5,13,18),(8,12,14),(35,13,16),(49,14,17),(68,13,14),(82,14,16)]):
            d=9; yaw=90 if side<0 else -90
            x=side*(7.7+d/2)
            shell=house(w,d,h,'limestone',False,True)
            shell.obj('market_house_%s_%02d'%('west' if side<0 else 'east',i),col).matrix_world=Matrix.Translation(Vector((x,yy,.25)))@Matrix.Rotation(math.radians(yaw),4,'Z')
            if i<5:
                a=Geo();awning(a,w-.6,-d/2,'canvas_gold' if side<0 and i==0 else 'canvas_green' if i%2==0 else 'canvas_cream')
                if side<0 and i==0:awning(a,w+.25,-d/2,'canvas_gold',8.0)
                props.add(a,(x,yy,.25),yaw)
                stall(props,(side*5.55,yy+2,.3),yaw)
                barrel(props,(side*5.5,yy-3,.32))
    # Far stone bridge: a real arch opening, crenellated parapet, and turret.
    gate=Geo();gy=22
    for s in (-1,1):gate.box((s*6.1,gy,5.2),(5.6,3,10.4),'limestone')
    gate.arch(0,gy,4.1,3.3,.75,3.25,'stone_light',22)
    for j in range(24):
        xx=-3.3+(j+.5)*6.6/24
        lower=4.1+math.sqrt(4.05**2-xx**2)
        gate.box((xx,gy,(lower+8.3)/2),(.30,3,8.3-lower),'limestone')
    gate.box((0,gy,9.7),(18,3.5,3.0),'limestone')
    for xx in range(-9,10,2):gate.box((xx,gy,11.65),(.8,3.5,1),'stone_light')
    for xx in (-6,-3,0,3,6):window(gate,xx,gy-1.8,8.8,.58,1.15,True,False)
    gate.rings((9,gy,0),[(2.6,0),(2.6,15),(3.2,15.4),(3.25,16.0),(3.1,16.3)],'limestone',28)
    gate.cylinder((9,gy,16),2.9,3.5,'slate',28)
    for j in range(12):
        t=j*math.tau/12
        gate.cylinder((9+2.8*math.cos(t),gy+2.8*math.sin(t),16),.16,3.4,'stone_light',8)
    gate.rings((9,gy,19.3),[(3.45,0),(3.4,.35),(.1,6.0)],'roof',28)
    gate.cylinder((9,gy,25.15),.1,1.6,'iron',8)
    gate.obj('market_bridge_and_turret',col)
    backdrop=Geo()
    for x,y,r,h in [(-8,60,7,24),(-19,65,4,22)]:
        backdrop.cylinder((x,y,0),r,h,'plaster_cream',24)
        backdrop.rings((x,y,h),[(r*math.cos(j*math.pi/16),r*.7*math.sin(j*math.pi/16)) for j in range(9)],'slate',32)
        for j in range(10):window(backdrop,x-r*.75+j*r*.15,y-r-.08,h-4,.48,1.65,True,False)
    backdrop.obj('market_distant_roofline',col)
    # Bunting is actual geometry, so it remains readable from moving viewpoints.
    for yy,zz in [(-3,14.8),(31,15),(66,13.3)]:
        props.beam((-8,yy,zz),(8,yy,zz+.4),.025,'wood_dark')
        for j in range(13):
            xx=-7.5+j*1.2; top=zz+(xx+8)/16*.4
            props.mesh([(xx,yy,top),(xx+.7,yy,top+.018),(xx+.7,yy,top-1.65),(xx,yy,top-1.67)],[(0,3,2,1)],['canvas_red','canvas_cream','canvas_green'][j%3])
    props.obj('market_awnings_stalls_flags',col,True)
    for j,(x,y) in enumerate([(-3,-7),(3,-5),(-1,4),(2,9),(-3,15),(1,19),(-1,25),(3,30),(0,34),(-2,40),(2,45),(-3,54)]):
        figure(people,(x,y,.33),RNG.uniform(-80,80),['cloth_blue','cloth_brown','cloth_dark'][j%3])
    people.obj('market_scale_figures_no_npc',col,False)
    camera(sc,'Reference_Market',(-.5,-18,2.65),(0,28,7.4),29)
    scene_setup(sc)
    return sc

def fountain(g,p):
    a=Geo()
    a.cylinder((0,0,0),3.7,.22,'stone_shade',16)
    # Closed profile around an open basin: outer wall, lip, inner wall, basin floor.
    a.rings((0,0,0),[(3.4,.2),(3.4,.7),(3.55,.78),(3.55,.93),(3.08,.93),(3.08,.43),(0,.43)],'stone_light',16)
    a.cylinder((0,0,.55),3.07,.04,'water',40)
    a.rings((0,0,.55),[(.7,0),(.5,.3),(.4,1.5),(.68,1.7),(1.8,1.92),(1.9,2.15),(1.7,2.3),(.45,2.3)],'limestone',20)
    a.cylinder((0,0,2.83),1.7,.03,'water',32)
    a.rings((0,0,2.85),[(.3,0),(.22,.6),(.35,.9),(.92,1.12),(1.05,1.3),(.85,1.4),(.2,1.4)],'stone_light',20)
    a.cylinder((0,0,4.27),.88,.025,'water',28)
    a.rings((0,0,4.28),[(.16,0),(.2,.25),(.08,.45)],'limestone',16)
    g.add(a,p)

def tolbana():
    sc=new_scene('Tolbana_Plaza');col=sc.collection
    g=Geo();g.box((0,3,-.15),(170,175,.5),'grass')
    paving(g,-33,33,-24,-17,.26,1.05)
    paving(g,-33,-27,-17,34,.26,1.05);paving(g,26,32,-17,34,.26,1.05)
    paving(g,-33,33,24,31,.26,1.05);paving(g,1.5,6.5,-17,24,.26,1.05)
    paving(g,6.5,26,-4,2.5,.26,1.05)
    # Lawn curbs, interrupted at paths; front strip frames the reference composition.
    for x,w in [(-12.75,28.5),(16.25,19.5)]:
        g.box((x,-16.85,.32),(w,.23,.27),'stone_light')
        g.box((x,23.85,.32),(w,.23,.27),'stone_light')
    g.obj('tolbana_plaza_ground_paths',col)
    waterworks=Geo();fountain(waterworks,(13,-.7,.28));waterworks.obj('tolbana_fountain',col)
    tree(col,(-10,6,.22),1.92,'tolbana_old_tree')
    plants=Geo()
    for x,y in [(-23,-11),(-19,19),(-2,19),(21,19),(22,6),(20,-10),(-22,8),(-3,-9)]:
        plants.ellipsoid((x,y,.9),(.9,.8,1.05),'leaf',10,6,.2)
    for j in range(2400):
        x=RNG.uniform(-26,25.5);y=RNG.uniform(-16.4,23.5)
        if 1<x<7 or (x>6 and -4.5<y<3) or math.hypot(x-13,y+.7)<4:continue
        z=.23;h=RNG.uniform(.14,.42);w=.1
        plants.mesh([(x-w,y,z),(x,y,z+h),(x+w,y,z),(x,y-w,z),(x,y,z+h*.8),(x,y+w,z)],[(0,2,1),(3,4,5)],RNG.choice(['grass','grass_light','grass_dark']))
    plants.obj('tolbana_grass_and_shrubs',col,False)
    houses=[(-26,37,0,11,11,13,'plaster_cream',0),(-14,34,0,11,10,10,'plaster_white',0),
            (-2,36,0,12,10,10,'plaster_peach',0),(11,35,0,12,10,11,'plaster_sage',0),
            (26,35,0,13,12,14,'plaster_cream',0),(-38,15,0,11,13,14,'plaster_white',90),
            (39,12,0,12,14,13,'plaster_sage',-90),(-27,54,4,13,12,12,'plaster_peach',0),
            (-12,54,5,13,12,13,'plaster_cream',0),(3,55,6,12,11,14,'plaster_white',0),
            (18,55,7,13,13,12,'plaster_cream',0),(33,57,8,12,12,11,'plaster_peach',0),
            (-10,73,9,13,12,13,'plaster_sage',0),(7,73,12,15,12,12,'plaster_cream',0),(26,75,13,13,12,12,'plaster_white',0)]
    for j,(x,y,z,w,d,h,mat,yaw) in enumerate(houses):
        a=house(w,d,h,mat,True,False)
        if z:a.box((0,0,-z/2),(w+.8,d+.8,z),'stone_shade')
        ob=a.obj('tolbana_house_%02d'%j,col)
        ob.matrix_world=Matrix.Translation(Vector((x,y,z+.24)))@Matrix.Rotation(math.radians(yaw),4,'Z')
    # Landmark right-hand square turret visible in the reference.
    turret=house(7,8,16,'plaster_white',False,False)
    turret.rings((0,0,16.4),[(6.0,0),(0,7.1)],'roof',4)
    turret.obj('tolbana_square_turret',col).location=(34,34,.25)
    props=Geo();people=Geo()
    for p,rot in [((-24,-22,.3),0),((27,-14,.3),90)]:
        stall(props,p,rot)
        a=Geo();a.box((0,0,1.17),(3.2,1.45,.06),'canvas_red');props.add(a,p,rot)
    for x,y in [(-17,-11),(22,12),(-19,20)]:
        props.box((x,y,.68),(2.6,.72,.16),'wood_light')
        for xx in (-.95,.95):props.box((x+xx,y,.35),(.16,.5,.7),'wood_dark')
    for j,(x,y,pose) in enumerate([(9,-4,'stand'),(16,-4,'stand'),(10,1.8,'sit'),(-3,10,'stand'),(-21,-20,'stand'),(25,16,'stand')]):
        figure(people,(x,y,.32 if pose=='stand' else .30),25,['cloth_brown','cloth_blue','cloth_dark'][j%3],pose)
    props.obj('tolbana_market_and_benches',col,True);people.obj('tolbana_scale_figures_no_npc',col,False)
    # Actual surrounding valley geometry, not a backdrop image or floating stage.
    land=Geo()
    n=54;extent=440
    vs=[]
    for iy in range(n+1):
        y=-extent+iy*extent*2/n
        for ix in range(n+1):
            x=-extent+ix*extent*2/n
            hill=max(0,(abs(x)-65)/7)+max(0,(y-105)/8)
            z=hill*(.75+.25*math.sin(x*.026+y*.018))
            if abs(x)<85 and -84<y<91:z=-.24
            vs.append((x,y,z-.3))
    fs=[]
    for iy in range(n):
        for ix in range(n):
            a=iy*(n+1)+ix;fs.append((a,a+1,a+n+2,a+n+1))
    land.mesh(vs,fs,'grass_dark');land.obj('tolbana_valley_terrain',col)
    roads=Geo();paving(roads,-3,3,-95,-24,.24,2);paving(roads,-3,3,84,112,.24,2);roads.obj('tolbana_north_south_approach',col)
    camera(sc,'Reference_Tolbana',(21,-31,6.3),(-1,15,7.3),31)
    scene_setup(sc)
    return sc

def main():
    print(json.dumps({'blender_pid':os.getpid(),'stage':'build'}),flush=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scs=[market(),tolbana()]
    empty=bpy.data.scenes.get('Scene')
    if empty:bpy.data.scenes.remove(empty)
    manifest={'schema':1,'style_id':STYLE,'palette_srgb':PALETTE,'blender_version':bpy.app.version_string,
              'units':'metres','source_axes':'Blender X right/east, Y forward/north, Z up; glTF export Y up',
              'geography_source':'ThreeHearthsVillage/Plugins/ThreeHearths/Source/ThreeHearths/Private/HearthAincradFloorPlan.cpp',
              'floor_origin_east_north_m':[0,0],'prototype_origin_east_north_m':[0,-4770],
              'canonical_exact_coordinates':False,'npc_behavior_implemented':False,'scenes':[]}
    locations=[('beginnings_plaza',[-163,-4752],[-163,0,-18],78),('tolbana',[0,2900],[0,0,-7670],0)]
    for sc,(region,globalxy,godotxyz,yaw) in zip(scs,locations):
        bpy.context.window.scene=sc
        sc['region_id']=region;sc['floor_east_north_m']=globalxy;sc['placement_yaw_degrees']=yaw
        # Export only authored meshes, with no Blender camera/light overrides in the engine.
        bpy.ops.object.select_all(action='DESELECT')
        for ob in sc.objects:
            if ob.type=='MESH':ob.select_set(True)
        file=EXPORT/(sc.name+'.glb')
        bpy.ops.export_scene.gltf(filepath=str(file),export_format='GLB',use_selection=True,
                                  export_extras=True,export_cameras=False,export_lights=False,
                                  export_yup=True,export_apply=True)
        objects=[o for o in sc.objects if o.type=='MESH']
        tris=0
        for o in objects:o.data.calc_loop_triangles();tris+=len(o.data.loop_triangles)
        manifest['scenes'].append({'id':sc.name,'region_id':region,'floor_east_north_m':globalxy,
            'godot_position_m':godotxyz,'godot_yaw_degrees':yaw,'glb':file.name,'sha256':hashlib.sha256(file.read_bytes()).hexdigest(),
            'mesh_objects':len(objects),'triangles':tris,'collision_mesh_names':[o.name for o in objects if o.get('collision_enabled')],
            'reference_camera_blender_m':list(sc.camera.location),
            'objects':[{'id':o.name,'collision':bool(o.get('collision_enabled'))} for o in objects]})
    # Additional Blender view places linked meshes in the actual shared floor coordinate system.
    # Editing a source mesh updates the geographic overview as well.
    geographic=new_scene('Level0_Geographic_Placement')
    geographic['coordinate_policy']='existing Level0 approximate anchors; metres east/north/up'
    for sc,(region,globalxy,godotxyz,yaw) in zip(scs,locations):
        anchor=bpy.data.objects.new(region+'_anchor',None);geographic.collection.objects.link(anchor)
        anchor.location=(globalxy[0],globalxy[1],0);anchor.rotation_euler.z=math.radians(yaw)
        for ob in sc.objects:
            if ob.type!='MESH':continue
            linked=ob.copy();linked.data=ob.data;geographic.collection.objects.link(linked)
            linked.parent=anchor;linked.matrix_parent_inverse=Matrix.Identity(4)
    camera(geographic,'Floor_Overview',(6500,-5500,8500),(0,-700,0),35)
    scene_setup(geographic)
    bpy.context.window.scene=scs[0]
    bpy.ops.wm.save_as_mainfile(filepath=str(HERE/'Level0_Anime_Reference_Scenes.blend'))
    (HERE/'scene_manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    (EXPORT/'scene_manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'stage':'complete','scenes':[{k:s[k] for k in ('id','mesh_objects','triangles')} for s in manifest['scenes']]}),flush=True)

if __name__=='__main__':main()
