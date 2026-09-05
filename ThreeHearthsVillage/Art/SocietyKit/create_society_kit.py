"""Build SocietyKit using Blender or Blender MCP. --no-render exports only.

Original local procedural geometry; no paid APIs, external models or textures.
"""
from pathlib import Path
import importlib
import json
import math
import sys
import time
import bpy
from mathutils import Vector

OUT=Path(__file__).resolve().parent
if str(OUT) not in sys.path:
    sys.path.insert(0,str(OUT))
import society_geometry as G
importlib.reload(G)
ARGS=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
RENDER='--no-render' not in ARGS
START=time.monotonic()
SPECS=json.loads((OUT/'module-specs.json').read_text(encoding='utf-8'))
for directory in ('modules','examples','previews'):
    (OUT/directory).mkdir(parents=True,exist_ok=True)
for old in list(bpy.data.objects):
    bpy.data.objects.remove(old,do_unlink=True)
for old in list(bpy.data.collections):
    bpy.data.collections.remove(old)
for old in list(bpy.data.meshes):
    bpy.data.meshes.remove(old)
for old in list(bpy.data.materials):
    bpy.data.materials.remove(old)
scene=bpy.context.scene
scene.name='SocietyKit | Castle, market and crafts'
scene.unit_settings.system='METRIC'
scene.unit_settings.scale_length=1
scene.render.engine='CYCLES'
scene.cycles.samples=24
scene.cycles.use_denoising=True
scene.view_settings.view_transform='AgX'
scene.render.resolution_percentage=100
scene.world=bpy.data.worlds.new('Society soft sky')
scene.world.use_nodes=True
scene.world.node_tree.nodes['Background'].inputs[0].default_value=(.5,.64,.72,1)
scene.world.node_tree.nodes['Background'].inputs[1].default_value=.45
MODULES=bpy.data.collections.new('MODULE_SOURCES | local coordinates')
scene.collection.children.link(MODULES)
M={
    'wood':G.mat('SK_Chestnut','916547',.48),
    'wood_light':G.mat('SK_Cut_Oak','BC9368',.43),
    'wood_dark':G.mat('SK_Timber_Recess','594337',.56),
    'wood_honey':G.mat('SK_Honey_Plank','B2865C',.46),
    'plaster':G.mat('SK_Cream_Lime','E9DCB9',.57),
    'stone':G.mat('SK_BlueGrey_Stone','98A5AC',.52),
    'stone_light':G.mat('SK_Stone_Edge','BAC1BA',.47),
    'stone_dark':G.mat('SK_Stone_Shadow','7B8990',.57),
    'iron':G.mat('SK_Forged_Iron','535B5E',.34,.65),
    'gold':G.mat('SK_Royal_Gold','DAAE51',.27,.8),
    'brass':G.mat('SK_Brass','B99550',.30,.7),
    'sage':G.mat('SK_Sage_Paint','779F97',.41),
    'blue':G.mat('SK_Royal_Blue','537F9A',.52),
    'red':G.mat('SK_Royal_Red','B56157',.52),
    'rope':G.mat('SK_Hemp_Rope','C5AE7E',.62),
    'leather':G.mat('SK_Leather','865744',.48),
    'coal':G.mat('SK_Charcoal','3A3C3D',.64),
    'ember':G.mat('SK_Warm_Ember','D98347',.42),
    'glass':G.mat('SK_Amber_Lantern','E3B873',.24),
    'paper':G.mat('SK_Parchment','EBDDAD',.63),
}
TILES=[G.mat('SK_Terracotta_'+str(i+1),c,r) for i,(c,r) in enumerate([
    ('BB695F',.34),('CA7C6F',.38),('AF6059',.36),('D28B79',.40)])]
G.configure(MODULES,M)
box,beam,cylinder,mesh=G.box,G.beam,G.cylinder,G.mesh
ASSETS={}
REPORT={'schema_version':1,'kit_id':'society_kit_01','blender_version':bpy.app.version_string,
        'modules':[],'examples':[],'npc_execution_ready':False,'units':'metres'}


def stone_rect(name,x0,x1,z0,z1,y=0,thickness=.4,block=.50,course=.30):
    """Solid masonry with staggered joints; skip sliver cuts smaller than25mm."""
    rows=max(1,round((z1-z0)/course))
    for row in range(rows):
        height=(z1-z0)/rows
        cursor=x0-(block/2 if row%2 else 0)
        while cursor<x1:
            lo,hi=max(x0,cursor),min(x1,cursor+block)
            if hi-lo>.025:
                obj=box(name,((lo+hi)/2,y,z0+(row+.5)*height),
                    (hi-lo-.006,thickness,height-.006),
                    [M['stone'],M['stone_light'],M['stone_dark']][(row+int(cursor*11))%3],0)
                G.bevel(obj,.025,segments=1)
            cursor+=block


def wall(battlement=False):
    stone_rect('Dressed wall',-1,1,0,2.4)
    if battlement:
        box('Parapet coping',(0,0,2.43),(2,.5,.12),M['stone_light'],.03)
        for x in (-.76,0,.76):
            box('Rounded merlon',(x,0,2.78),(.48,.50,.58),M['stone'],.038)
            box('Merlon coping',(x,0,3.07),(.5,.52,.07),M['stone_light'],.018)


def arch_strip(name,inner,outer,spring,y,depth,material,segments=12):
    for j in range(segments):
        a0=math.pi*j/segments+.006
        a1=math.pi*(j+1)/segments-.006
        verts=[(math.cos(a)*r,yy,spring+math.sin(a)*r)
               for yy in (y-depth/2,y+depth/2) for r,a in ((inner,a0),(inner,a1),(outer,a1),(outer,a0))]
        obj=mesh(name,verts,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],material)
        G.bevel(obj,.012)


def castle_gate():
    stone_rect('Gate pier',-2,-.90,0,2.3,thickness=.6)
    stone_rect('Gate pier',.90,2,0,2.3,thickness=.6)
    # Fill the spandrels around the arch instead of putting a solid rectangle
    # through the opening. Inner edge follows the same semicircle exactly.
    for sign in (-1,1):
        for j in range(6):
            lo=j*.15
            hi=(j+1)*.15
            h0=2.3+math.sqrt(max(0,.90*.90-lo*lo))
            h1=2.3+math.sqrt(max(0,.90*.90-hi*hi))
            verts=[(sign*x,y,z) for y in (-.30,.30) for x,z in ((lo,h0),(hi,h1),(hi,3.8),(lo,3.8))]
            obj=mesh('Arch spandrel',verts,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],M['stone'])
            G.bevel(obj,.01)
        stone_rect('Upper gate flank',sign*.90 if sign>0 else -2,2 if sign>0 else -.90,2.3,3.8,thickness=.6)
    arch_strip('Radial gate voussoir',.90,1.17,2.3,-.34,.13,M['stone_light'])
    box('Gate top coping',(0,0,3.81),(4,.68,.14),M['stone_light'],.03)
    for x in (-1.85,1.85):
        box('Gate corner pilaster',(x,-.34,1.22),(.27,.18,2.44),M['stone_light'],.03)


def gate_pair():
    # Each leaf lies along +Y in the open pose, hinges at X=+/-0.9.
    for sign in (-1,1):
        for j in range(5):
            box('Open gate oak board',(sign*.94,.09+j*.18,1.15),(.12,.172,2.3),M['wood_honey'] if j%2 else M['wood_light'],.014)
        for z in (.40,1.8):
            box('Gate iron strap',(sign*.87,.45,z),(.035,.82,.075),M['iron'],.012)
        for z in (.26,2.02):
            cylinder('Gate hinge',(sign*.94,0,z),.038,.16,M['iron'],12)


def tower_face(front=False,full_width=False):
    if front:
        stone_rect('Tower door flank',-2,-.55,0,2.4,y=-1.825,thickness=.35)
        stone_rect('Tower door flank',.55,2,0,2.4,y=-1.825,thickness=.35)
        stone_rect('Tower door lintel',-.55,.55,2.08,2.4,y=-1.825,thickness=.35)
    else:
        half=2 if full_width else 1.65
        stone_rect('Arrow slit flank',-half,-.12,0,2.4,y=-1.825,thickness=.35)
        stone_rect('Arrow slit flank',.12,half,0,2.4,y=-1.825,thickness=.35)
        stone_rect('Below arrow slit',-.12,.12,0,1.12,y=-1.825,thickness=.35)
        stone_rect('Above arrow slit',-.12,.12,2.02,2.4,y=-1.825,thickness=.35)


def tower():
    tower_face(True)
    for angle in (90,180,270):
        previous=len(G.PARTS)
        # Rear spans complete width; side faces stop at its inner edges.
        tower_face(False,full_width=angle==180)
        for obj in G.PARTS[previous:]:
            rot=math.radians(angle)
            x,y=obj.location.x,obj.location.y
            obj.location.x=x*math.cos(rot)-y*math.sin(rot)
            obj.location.y=x*math.sin(rot)+y*math.cos(rot)
            obj.rotation_euler.z+=rot


def tower_cap():
    # Roof deck with a real hatch at X=-1/Y=0. No solid cover across the hole.
    for x0,x1,y0,y1 in ((-2,-1.5,-2,2),(-.5,2,-2,2),(-1.5,-.5,-2,-.5),(-1.5,-.5,.5,2)):
        box('Tower roof deck',((x0+x1)/2,(y0+y1)/2,.10),(x1-x0,y1-y0,.20),M['stone_light'],.02)
    for side in (-1,1):
        box('Parapet base',(0,side*1.825,.38),(4,.35,.36),M['stone'],.025)
        box('Parapet side',(side*1.825,0,.38),(.35,3.30,.36),M['stone'],.025)
        for j in range(5):
            x=-1.72+j*.86
            box('Tower merlon',(x,side*1.825,.78),(.52,.40,.44),M['stone_light'],.03)
        for j in range(3):
            y=-.86+j*.86
            box('Tower merlon',(side*1.825,y,.78),(.40,.52,.44),M['stone_light'],.03)


def buttress():
    for z,w,d,h in ((.15,.70,.70,.30),(.75,.58,.60,.90),(1.70,.40,.48,1.0),(2.30,.46,.54,.20)):
        box('Stepped stone buttress',(0,0,z),(w,d,h),M['stone_light'] if z>2 else M['stone'],.035)


def walkway():
    for j in range(8):
        box('Walkway oak plank',(-.875+j*.25,0,.18),(.244,2,.12),M['wood_light'],.012)
    for y in (-.75,.75):
        box('Walkway bearer',(0,y,.06),(2,.18,.12),M['wood'],.018)


def banner(color):
    # Solid folded cloth with a forked bottom, plus geometric royal emblem.
    verts=[(x,y,z) for y in (0,.025) for x,z in ((-.35,.15),(0,0),(.35,.15),(.35,1.55),(-.35,1.55))]
    mesh('Royal cloth',verts,[(0,4,3,2,1),(5,6,7,8,9),(0,1,6,5),(1,2,7,6),(2,3,8,7),(3,4,9,8),(4,0,5,9)],M[color])
    beam('Banner bar',(-.40,0,1.61),(.40,0,1.61),.06,.06,M['wood_light'])
    beam('Banner hanger',(0,.03,1.62),(0,.03,1.78),.035,.035,M['iron'])
    # Stylised three-hearth crown glyph, represented as raised cloth applique.
    box('Gold emblem base',(0,-.018,.91),(.31,.018,.08),M['gold'],.005)
    for x,z in ((-.115,1.05),(0,1.11),(.115,1.05)):
        beam('Gold emblem ray',(x,-.018,.93),(x,-.018,z),.075,.018,M['gold'])


def crate(w=1,d=.7,h=.65):
    for j in range(5):
        box('Crate bottom plank',(-w/2+(j+.5)*w/5,0,.045),(w/5-.007,d,.09),M['wood'],.012)
    for x in (-w/2+.035,w/2-.035):
        for y in (-d/2+.035,d/2-.035):
            box('Crate corner',(x,y,h/2),(.07,.07,h),M['wood_light'],.012)
    for row in range(3):
        z=.14+row*(h-.16)/3
        for y in (-d/2+.025,d/2-.025):
            box('Crate slat',(0,y,z),(w,.05,(h-.16)/3-.008),M['wood_honey'],.01)
        for x in (-w/2+.025,w/2-.025):
            box('Crate end slat',(x,0,z),(.05,d,(h-.16)/3-.008),M['wood_honey'],.01)


def barrel():
    n=16
    rings=[(0,.26),(.08,.29),(.45,.325),(.82,.29),(.90,.26)]
    for j in range(n):
        a0=j*math.tau/n+.007
        a1=(j+1)*math.tau/n-.007
        verts=[(math.cos(a)*r,math.sin(a)*r,z) for z,r in rings for a in (a0,a1)]
        faces=[(2*k,2*k+1,2*k+3,2*k+2) for k in range(len(rings)-1)]
        obj=mesh('Barrel stave',verts,faces,M['wood_honey'] if j%3 else M['wood_light'])
        # Solidify keeps the barrel staves physical and opening edges visible.
        bpy.context.view_layer.objects.active=obj
        mod=obj.modifiers.new('Stave thickness','SOLIDIFY')
        mod.thickness=.035
        bpy.ops.object.modifier_apply(modifier=mod.name)
    for z in (.12,.73):
        ring('Iron barrel hoop',.292,.315,z,.055,M['iron'])
    cylinder('Barrel lid',(0,0,.887),.255,.028,M['wood_light'],16)
    for x in (-.13,0,.13):
        box('Lid plank seam',(x,0,.904),(.006,.40,.004),M['wood_dark'],0)


def ring(name,inner,outer,z,depth,material,n=24):
    verts=[(math.cos(j*math.tau/n)*r,math.sin(j*math.tau/n)*r,zz)
           for zz in (z,z+depth) for r in (inner,outer) for j in range(n)]
    faces=[]
    for j in range(n):
        k=(j+1)%n
        faces += [(j,k,n+k,n+j),(2*n+j,3*n+j,3*n+k,2*n+k),
                  (j,2*n+j,2*n+k,k),(n+j,n+k,3*n+k,3*n+j)]
    return mesh(name,verts,faces,material)


def stall(color):
    for x in (-.92,.92):
        for y in (-.78,.78):
            box('Stall post',(x,y,1.12),(.12,.12,2.24),M['wood'],.02)
    for y in (-.78,.78):
        box('Stall roof rail',(0,y,2.22),(2,.13,.13),M['wood_light'],.02)
    for j in range(8):
        x0,x1=-1+j*.25,-1+(j+1)*.25
        verts=[(x,y,z) for x in (x0,x1) for y,z in ((-.95,2.14),(0,2.40),(.95,2.14))]
        obj=mesh('Striped canvas awning',verts,[(0,1,4,3),(1,2,5,4)],M[color] if j%2 else M['plaster'])
        bpy.context.view_layer.objects.active=obj
        mod=obj.modifiers.new('Canvas thickness','SOLIDIFY');mod.thickness=.022
        bpy.ops.object.modifier_apply(modifier=mod.name)
        box('Awning valance',((x0+x1)/2,-.95,2.06),(.25,.03,.16),M[color] if j%2 else M['plaster'],.006)
    for j in range(8):
        box('Market counter plank',(-.875+j*.25,-.39,.91),(.244,.88,.09),M['wood_light'],.016)
    for x in (-.72,.72):
        box('Counter support',(x,-.38,.44),(.12,.12,.88),M['wood'],.018)
    # Three small open trays communicate a trading surface, with no simulated stock.
    for x in (-.60,0,.60):
        box('Goods tray floor',(x,-.38,.98),(.49,.54,.04),M['wood_honey'],.009)
        for xx in (x-.245,x+.245):
            box('Goods tray rim',(xx,-.38,1.05),(.03,.54,.12),M['wood_light'],.006)


def notice():
    for x in (-.60,.60):
        box('Notice board post',(x,0,.98),(.15,.15,1.96),M['wood'],.025)
    box('Notice backboard',(0,.02,1.27),(1.42,.12,1.0),M['wood_honey'],.025)
    for ysign in (-1,1):
        beam('Notice roof edge',(-.8,ysign*.18,1.98),(.8,ysign*.18,1.98),.10,.10,M['wood_light'])
    for x,z,angle in ((-.33,1.42,-.06),(.24,1.27,.06),(-.23,.97,.03)):
        obj=box('Public parchment',(x,-.056,z),(.42,.016,.29),M['paper'],.01)
        obj.rotation_euler.y=angle
        for line in range(3):
            box('Notice ink line',(x,-.067,z+.07-line*.055),(.27-line*.04,.007,.012),M['wood_dark'],0)
        cylinder('Notice pin',(x,-.076,z+.11),.017,.008,M['brass'],8,(0,1,0))


def lantern():
    box('Lantern stone foot',(0,0,.12),(.32,.32,.24),M['stone_light'],.035)
    box('Lantern timber post',(0,0,1.2),(.14,.14,2.4),M['wood'],.023)
    beam('Lantern arm',(0,0,2.33),(.48,0,2.33),.085,.085,M['wood_light'])
    beam('Lantern brace',(0,0,2.05),(.35,0,2.33),.06,.06,M['wood_light'])
    box('Amber lantern',(.43,0,1.99),(.25,.25,.36),M['glass'],.02)
    for x in (.305,.555):
        for y in (-.125,.125):
            box('Lantern frame',(x,y,1.99),(.025,.025,.40),M['iron'],.004)
    for z in (1.77,2.21):
        box('Lantern cap',(.43,0,z),(.32,.32,.07),M['iron'],.013)
    beam('Lantern hanger',(.43,0,2.25),(.43,0,2.33),.026,.026,M['iron'])


def carpenter():
    for y in (-.25,0,.25):
        box('Carpenter heavy top',(0,-.10+y,.90),(1.85,.24,.14),M['wood_light'],.022)
    for x in (-.75,.75):
        for y in (-.33,.13):
            box('Bench legs',(x,y,.415),(.16,.16,.83),M['wood'],.025)
    box('Tool shelf',(0,-.1,.28),(1.66,.66,.09),M['wood_honey'],.014)
    for x in (-.8,.8):
        box('Tool rack post',(x,.63,.87),(.12,.12,1.74),M['wood'],.023)
    box('Tool rack board',(0,.66,1.42),(1.74,.07,.5),M['wood_light'],.02)
    for j in range(6):
        cylinder('Tool peg',(-.6+j*.24,.59,1.47),.026,.12,M['wood_dark'],10,(0,1,0))
    for j in range(3):
        box('Carpenter cut plank',(-.24+j*.18,-.07,1.005),(.15,.54,.06),M['wood_honey'],.01)
    box('Wooden vise',(.60,-.57,.84),(.36,.18,.25),M['wood_dark'],.019)
    cylinder('Vise screw',(.60,-.71,.84),.027,.22,M['iron'],12,(0,1,0))
    beam('Vise handle',(.60,-.83,.7),(.60,-.83,.98),.027,.027,M['wood_light'])


def mason():
    for x in (-.67,.67):
        box('Stone trestle',(x,0,.39),(.25,.70,.78),M['wood'],.035)
    box('Mason slab',(0,0,.85),(1.86,1.05,.18),M['stone_light'],.04)
    for x,y,z,dx,dy,dz in ((-.5,.05,1.1,.5,.6,.3),(.17,.12,1.0,.38,.42,.16),(.64,.22,1.08,.3,.36,.32)):
        box('Dressed stone in progress',(x,y,z),(dx,dy,dz),M['stone'],.035)
    for j in range(4):
        box('Stacked mason block',(-.72+j*.43,.85,.20),(.39,.38,.40),M['stone_dark'] if j%2 else M['stone_light'],.035)


def kiln():
    # Genuine front firebox opening, no black rectangle covering a solid wall.
    box('Kiln hearth',(0,0,.15),(1.8,1.8,.30),M['stone_dark'],.045)
    for x in (-.68,.68):
        box('Brick kiln pier',(x,0,.66),(.44,1.65,.72),TILES[2],.035)
    box('Kiln back',(0,.66,.72),(1.12,.30,.84),TILES[0],.035)
    box('Firebox lintel',(0,0,1.08),(1.72,1.65,.18),TILES[1],.03)
    # Upper chamber is a low domed solid, with an open flue through the chimney.
    n=16; rings=[(1.16,.86),(1.42,.78),(1.66,.58),(1.82,.27)]
    verts=[(math.cos(j*math.tau/n)*r,math.sin(j*math.tau/n)*r,z) for z,r in rings for j in range(n)]
    faces=[]
    for k in range(len(rings)-1):
        for j in range(n): faces.append((k*n+j,k*n+(j+1)%n,(k+1)*n+(j+1)%n,(k+1)*n+j))
    mesh('Kiln domed chamber',verts,faces,TILES[1],range(len(faces)))
    ring('Open kiln chimney',.14,.27,1.80,.75,TILES[2],16)
    ring('Chimney coping',.14,.31,2.48,.10,M['stone_light'],16)
    for x in (-.32,0,.32):
        cylinder('Unlit firewood',(x,0,.38),.085,.75,M['wood_dark'],10,(0,1,0))
    # Stock tiles outside the kiln communicate craft output without smoke/fire VFX.
    for j in range(3):
        G.arch_tile('Fired tile sample',(-.38+j*.25,-.85,.32),(0,-1,0),(1,0,0),(0,0,1),.32,.22,TILES[j],.04)


def forge():
    box('Forge base',(0,.25,.39),(1.50,1.25,.78),M['stone_dark'],.04)
    box('Coal bed',(0,.25,.81),(1.12,.90,.08),M['coal'],.02)
    for x in (-.66,.66):
        box('Forge side',(x,.30,1.00),(.24,1.24,.44),M['stone'],.035)
    box('Forge back',(0,.8,1.18),(1.50,.20,.80),M['stone'],.035)
    # Four-sided tapered hood, empty underside; real visible forge mouth.
    verts=[(x,y,z) for z,w,d in ((1.45,.83,.67),(2.12,.27,.26)) for x,y in ((-w,-d),(w,-d),(w,d),(-w,d))]
    obj=mesh('Forge extraction hood',verts,[(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],M['iron'])
    obj.location.y=.25
    bpy.context.view_layer.objects.active=obj
    mod=obj.modifiers.new('Hood thickness','SOLIDIFY');mod.thickness=.04
    bpy.ops.object.modifier_apply(modifier=mod.name)
    ring('Forge flue',.18,.24,2.1,.42,M['iron'],12).location.y=.25
    box('Anvil stump',(0,-.64,.25),(.55,.46,.50),M['wood'],.05)
    box('Anvil foot',(0,-.64,.54),(.48,.35,.08),M['iron'],.02)
    box('Anvil waist',(0,-.64,.66),(.25,.24,.20),M['iron'],.025)
    box('Anvil face',(0,-.64,.79),(.63,.31,.09),M['iron'],.025)
    verts=[(x,y,z) for y in (-.76,-.52) for x,z in ((.30,.74),(.60,.76),(.30,.84))]
    mesh('Anvil horn',verts,[(0,2,1),(3,4,5),(0,1,4,3),(1,2,5,4),(2,0,3,5)],M['iron'])


def plank_goods(beams=False):
    length,width,height=(1.20,.20,.12) if beams else (.90,.18,.065)
    layers=2 if beams else 3
    for level in range(layers):
        for row in (-1,1):
            box('Portable cut timber',(0,row*(width/2+.01),height/2+level*(height+.005)),
                (length,width,height),M['wood_light'] if level%2 else M['wood_honey'],.013)
    total=layers*(height+.005)
    for x in (-length*.28,length*.28):
        for y in (-width-.015,width+.015):
            box('Bundle rope side',(x,y,total/2),(.035,.025,total+.025),M['rope'],.006)
        box('Bundle rope top',(x,0,total),(.035,width*2+.055,.025),M['rope'],.006)


def tile_goods():
    crate(.65,.45,.30)
    for x in (-.16,.13):
        for j in range(3):
            box('Portable fired brick',(x,0,.115+j*.075),(.26,.25,.065),TILES[j%4],.011)
    for j in range(3):
        G.arch_tile('Portable roof tile',(-.2,-.10+j*.075,.35),(1,0,0),(0,1,0),(0,0,1),.40,.14,TILES[j],.035)


def paint_goods():
    for x,color in ((-.17,'sage'),(.17,'plaster')):
        cylinder('Paint bucket',(x,0,.17),.14,.30,M['wood_light'],14)
        ring('Paint bucket rim',.13,.15,.30,.035,M['iron'],14).location.x=x
        cylinder('Pigment surface',(x,0,.325),.128,.014,M[color],20)
        for xx in (x-.13,x+.13):
            beam('Pail bail',(xx,0,.24),(xx,0,.46),.024,.024,M['iron'])
        beam('Pail hand grip',(x-.13,0,.46),(x+.13,0,.46),.028,.028,M['wood_dark'])


def crown():
    ring('Royal crown circlet',.102,.125,0,.065,M['gold'],32)
    n=8
    for j in range(n):
        a0=(j-.32)*math.tau/n;a1=(j+.32)*math.tau/n;am=j*math.tau/n
        verts=[(math.cos(a)*r,math.sin(a)*r,z) for r in (.109,.126)
               for a,z in ((a0,.058),(a1,.058),(am,.15 if j%2==0 else .12))]
        mesh('Crown point',verts,[(0,2,1),(3,4,5),(0,1,4,3),(1,2,5,4),(2,0,3,5)],M['gold'])
        if j%2==0:
            bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1,radius=.022,
                location=(math.cos(am)*.123,math.sin(am)*.123,.047))
            obj=G.adopt(bpy.context.view_layer.objects.active,M['blue'] if j%4 else M['red'])
            G.uv_project(obj.data)


def cap():
    # Real open underside, with a bill pointing -Y.
    n=24
    verts=[(math.cos(j*math.tau/n)*r,math.sin(j*math.tau/n)*r,z)
           for r,z in ((.145,0),(.15,.055),(.12,.115),(.05,.145)) for j in range(n)]
    faces=[]
    for k in range(3):
        for j in range(n):faces.append((k*n+j,k*n+(j+1)%n,(k+1)*n+(j+1)%n,(k+1)*n+j))
    faces.append(tuple(3*n+j for j in range(n)))
    mesh('Carpenter cloth cap',verts,faces,M['sage'],range(len(faces)-1))
    ring('Cap band',.137,.152,0,.03,M['leather'],24)
    box('Cap bill',(0,-.15,.012),(.25,.14,.024),M['leather'],.015)


def hood():
    # Open face towards -Y and open bottom; no sealed head-volume sphere.
    n=16
    angles=[math.radians(-25+j*230/n) for j in range(n+1)]
    verts=[(math.cos(a)*r,math.sin(a)*r,z) for r,z in ((.155,-.10),(.16,.035),(.15,.15),(.105,.22)) for a in angles]
    faces=[]
    stride=n+1
    for k in range(3):
        for j in range(n):faces.append((k*stride+j,k*stride+j+1,(k+1)*stride+j+1,(k+1)*stride+j))
    crown_vertex=len(verts)
    verts.append((0,0,.255))
    for j in range(n):faces.append((3*stride+j,3*stride+j+1,crown_vertex))
    obj=mesh('Mason open hood',verts,faces,M['plaster'],range(len(faces)))
    # Missing205..335 degree sector is centred on front -Y.
    bpy.context.view_layer.objects.active=obj
    mod=obj.modifiers.new('Cloth thickness','SOLIDIFY');mod.thickness=.008
    bpy.ops.object.modifier_apply(modifier=mod.name)
    for x in (-.11,.11):
        beam('Hood tie',(x,-.11,-.02),(x*.8,-.12,-.10),.012,.012,M['rope'])


def apron():
    verts=[(x,y,z) for y in (0,.015) for x,z in ((-.16,-.06),(.16,-.06),(.26,-.87),(0,-.95),(-.26,-.87))]
    mesh('Smith leather apron',verts,[(0,4,3,2,1),(5,6,7,8,9),(0,1,6,5),(1,2,7,6),(2,3,8,7),(3,4,9,8),(4,0,5,9)],M['leather'])
    for x in (-.12,.12):
        beam('Apron neck strap',(x,0,-.10),(x*.55,0,.05),.035,.022,M['wood_dark'])
    box('Apron tool pocket',(0,-.025,-.60),(.28,.028,.22),M['wood_dark'],.012)
    for x in (-.14,.14):
        cylinder('Apron rivet',(x,-.048,-.50),.011,.01,M['brass'],8,(0,1,0))


def hammer():
    beam('Hammer oak handle',(0,0,0),(0,0,.36),.035,.045,M['wood_light'])
    box('Hammer iron head',(0,0,.36),(.24,.085,.085),M['iron'],.018)
    box('Hammer grip',(0,0,.085),(.043,.052,.16),M['leather'],.01)


def saw():
    # Thin solid toothed blade and a genuine open wooden handle.
    outline=[(-.20,.18),(.24,.16),(.20,.02)]
    for j in reversed(range(12)):
        x=-.20+j*.034
        outline.extend(((x+.017,0),(x,.02)))
    verts=[(x,y,z) for y in (-.012,.012) for x,z in outline]
    n=len(outline)
    faces=[tuple(reversed(range(n))),tuple(n+j for j in range(n))]
    faces += [(j,(j+1)%n,n+(j+1)%n,n+j) for j in range(n)]
    mesh('Toothed saw blade',verts,faces,M['iron'])
    for x in (-.29,-.20):
        box('Saw handle side',(x,0,.12),(.055,.06,.20),M['wood_light'],.016)
    for z in (.035,.205):
        box('Saw handle bridge',(-.245,0,z),(.14,.06,.05),M['wood_light'],.012)


def chisel():
    cylinder('Chisel handle',(0,0,.20),.030,.18,M['wood_light'],12)
    cylinder('Chisel ferrule',(0,0,.125),.031,.035,M['iron'],12)
    box('Chisel steel blade',(0,0,.055),(.035,.018,.11),M['iron'],.004)


def trowel():
    verts=[(x,y,z) for z in (0,.016) for x,y in ((-.075,.10),(.075,.10),(0,-.18))]
    mesh('Trowel steel blade',verts,[(0,2,1),(3,4,5),(0,1,4,3),(1,2,5,4),(2,0,3,5)],M['iron'])
    beam('Trowel neck',(0,0,.01),(0,.06,.085),.025,.025,M['iron'])
    cylinder('Trowel oak grip',(0,.04,.10),.025,.17,M['wood_light'],12,(0,1,0))


BUILDERS={
    'castle_wall_stone_2m':wall,'castle_wall_battlement_2m':lambda:wall(True),
    'castle_gate_arch_4m':castle_gate,'castle_gate_oak_pair':gate_pair,
    'castle_tower_storey_4m':tower,'castle_tower_battlement_cap_4m':tower_cap,
    'castle_buttress_stone':buttress,'castle_walkway_timber_2m':walkway,
    'kingdom_banner_blue':lambda:banner('blue'),'kingdom_banner_red':lambda:banner('red'),
    'market_stall_blue_2m':lambda:stall('blue'),'market_stall_red_2m':lambda:stall('red'),
    'market_crate_oak':crate,'market_barrel_oak':barrel,
    'village_notice_board':notice,'village_lantern_post':lantern,
    'workshop_carpenter_station':carpenter,'workshop_mason_station':mason,
    'workshop_tile_kiln':kiln,'workshop_blacksmith_forge':forge,
    'goods_planks_bundle':plank_goods,'goods_beams_bundle':lambda:plank_goods(True),
    'goods_bricks_tiles_crate':tile_goods,'goods_paint_pails':paint_goods,
    'regalia_king_crown':crown,'profession_carpenter_cap':cap,
    'profession_mason_hood':hood,'profession_smith_apron':apron,
    'tool_carpenter_hammer':hammer,'tool_carpenter_saw':saw,
    'tool_mason_chisel':chisel,'tool_mason_trowel':trowel,
}


def select(objects):
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:obj.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]


def export(path,objects):
    select(objects)
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,
        export_yup=True,export_apply=True,export_texcoords=True,export_normals=True,
        export_materials='EXPORT',export_extras=True,export_animations=False,
        export_cameras=False,export_lights=False)


def stats(obj):
    data=obj.data;data.calc_loop_triangles()
    coords=[obj.matrix_world@Vector(v) for v in obj.bound_box]
    low=[round(min(v[i] for v in coords),5) for i in range(3)]
    high=[round(max(v[i] for v in coords),5) for i in range(3)]
    return {'triangles':len(data.loop_triangles),'vertices':len(data.vertices),
        'bounds_min_m':low,'bounds_max_m':high,
        'dimensions_m':[round(high[i]-low[i],5) for i in range(3)],
        'materials':[m.name for m in data.materials if m],
        'uv_layers':len(data.uv_layers),'smooth_polygons':sum(p.use_smooth for p in data.polygons)}


for spec in SPECS['modules']:
    G.PARTS.clear()
    BUILDERS[spec['id']]()
    select(G.PARTS)
    if len(G.PARTS)>1:bpy.ops.object.join()
    obj=bpy.context.view_layer.objects.active
    obj.name=spec['id']
    obj.data.name=spec['id']
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    scene.cursor.location=(0,0,0)
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    obj['asset_id']=spec['id'];obj['category']=spec['category']
    obj['npc_portable']=spec['category'] in ('goods','tool')
    obj['fitting_anchor']=spec.get('fitting_anchor','centre_bottom')
    path=OUT/'modules'/(spec['id']+'.glb')
    export(path,[obj]);ASSETS[spec['id']]=obj
    REPORT['modules'].append({'id':spec['id'],'file':'modules/'+path.name,'bytes':path.stat().st_size,**stats(obj)})
    print('SOCIETY_MODULE',spec['id'],REPORT['modules'][-1]['triangles'],flush=True)

EXAMPLES={};LAYOUTS=[]


def instance(collection,module_id,at=(0,0,0),yaw=0):
    obj=bpy.data.objects.new(module_id,ASSETS[module_id].data)
    collection.objects.link(obj)
    obj.location=at;obj.rotation_euler.z=math.radians(yaw);obj['module_id']=module_id
    return obj


def example(name):
    collection=bpy.data.collections.new('EXAMPLE | '+name)
    scene.collection.children.link(collection);EXAMPLES[name]=collection
    layout={'id':name,'placements':[],'npc_execution_ready':False}
    LAYOUTS.append(layout)
    def add(module_id,at,yaw=0):
        layout['placements'].append({'instance_id':name+'_'+str(len(layout['placements'])+1).zfill(3),
            'module_id':module_id,'position_m':list(at),'yaw_degrees':yaw})
        return instance(collection,module_id,at,yaw)
    return collection,add


collection,add=example('kings_gate_courtyard')
add('castle_gate_arch_4m',(0,-4,0))
add('castle_gate_oak_pair',(0,-4,0))
for side in (-1,1):
    for x in (3,5):add('castle_wall_battlement_2m',(side*x,-4,0))
    for y in (-3,-1):add('castle_wall_battlement_2m',(side*6,y,0),90)
    for level in range(2):add('castle_tower_storey_4m',(side*4,2,level*2.4),-side*90 if level else 0)
    add('castle_tower_battlement_cap_4m',(side*4,2,4.8))
    add('castle_buttress_stone',(side*5.65,-3.7,0))
    add('kingdom_banner_blue',(side*1.5,-4.39,1.5))
    add('village_lantern_post',(side*1.7,-5.1,0))
for x in (-1,1):add('castle_wall_battlement_2m',(x,4,0))
for x in (-3,-1,1,3):add('castle_walkway_timber_2m',(x,2,2.16))
add('village_notice_board',(-2.7,-1.9,0),20)
add('market_barrel_oak',(2.8,-1.4,0))
add('market_crate_oak',(3.4,-.4,0))

collection,add=example('guild_market_yard')
for mid,x in (('workshop_carpenter_station',-3.3),('workshop_mason_station',0),('workshop_tile_kiln',2.3)):
    add(mid,(x,2.6,0))
add('workshop_blacksmith_forge',(4.2,.1,0),20)
add('market_stall_blue_2m',(-2.0,-1.5,0))
add('market_stall_red_2m',(1.0,-1.5,0))
add('village_notice_board',(-4.0,-1.8,0),25)
add('village_lantern_post',(3.5,-3.0,0))
add('market_barrel_oak',(-.3,-3.1,0))
add('market_crate_oak',(4,-1.5,0))
add('goods_planks_bundle',(-3.6,1.2,0))
add('goods_beams_bundle',(-2.5,1.2,0))
add('goods_bricks_tiles_crate',(3.2,1.1,0))
add('goods_paint_pails',(-1.8,-1.7,.99))
add('tool_carpenter_hammer',(-3.45,2.4,.98),20)
add('tool_mason_trowel',(.25,2.6,.98),15)

for name,collection in EXAMPLES.items():
    path=OUT/'examples'/(name+'.glb')
    export(path,list(collection.objects))
    REPORT['examples'].append({'id':name,'file':'examples/'+path.name,'bytes':path.stat().st_size,
        'module_instances':len(collection.objects),'npc_execution_ready':False})
(OUT/'example-layouts.json').write_text(json.dumps({'schema_version':1,'units':'metres','examples':LAYOUTS},indent=2)+'\n',encoding='utf-8')

PRESENT=bpy.data.collections.new('PRESENTATION | never exported');scene.collection.children.link(PRESENT)
ground=G.mat('Society_Presentation_Sage','9CAC91',.65)
ink=G.mat('Society_Presentation_Ink','263C36',.6)


def stage(name,at,dims):
    obj=box(name,at,dims,ground,.04)
    MODULES.objects.unlink(obj);PRESENT.objects.link(obj)
    return obj


def caption(text,at,size=.20):
    data=bpy.data.curves.new('Caption','FONT');data.body=text;data.size=size;data.align_x='CENTER';data.materials.append(ink)
    obj=bpy.data.objects.new('Caption | '+text,data);PRESENT.objects.link(obj);obj.location=at


def camera(at,target,scale):
    data=bpy.data.cameras.new('Society presentation camera');data.type='ORTHO';data.ortho_scale=scale
    obj=bpy.data.objects.new('Society presentation camera',data);scene.collection.objects.link(obj)
    obj.location=at;obj.rotation_euler=(Vector(target)-obj.location).to_track_quat('-Z','Y').to_euler();scene.camera=obj
    return obj


def area(name,at,power,size,color):
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.size=size;data.shape='DISK';data.color=color
    obj=bpy.data.objects.new(name,data);scene.collection.objects.link(obj);obj.location=at
    obj.rotation_euler=(-obj.location).to_track_quat('-Z','Y').to_euler()
    return obj


MODULES.hide_render=True;MODULES.hide_viewport=True
for c in EXAMPLES.values():c.hide_render=True;c.hide_viewport=True
for i,spec in enumerate(SPECS['modules']):
    x,y=(i%8-3.5)*5.2,(1.5-i//8)*5.0
    # Wearables/tools intentionally use larger display scale, clearly labelled.
    scale=3 if spec['category'] in ('wearable','tool') else 1
    z=.30+(3 if spec['id']=='profession_smith_apron' else 0)
    obj=instance(PRESENT,spec['id'],(x,y,z));obj.scale=(scale,scale,scale)
    stage('Display plinth',(x,y,.07),(4.8,4.65,.14))
    caption(spec['id'].replace('_',' ')+('  [display x3]' if scale==3 else ''),(x,y-2.10,.15),.18)
cam=camera((9,-47,64),(0,0,.7),52)
lights=[area('Warm large key',(-10,-16,25),4500,15,(1,.86,.68)),area('Cool fill',(15,8,20),2700,13,(.72,.88,1))]
scene.render.resolution_x=2600;scene.render.resolution_y=1600
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'SocietyKit.blend'))
if RENDER:
    scene.render.filepath=str(OUT/'previews'/'SocietyKit_Modules.png');bpy.ops.render.render(write_still=True)

for obj in PRESENT.objects:obj.hide_render=True;obj.hide_viewport=True
for name,collection in EXAMPLES.items():
    collection.hide_render=False;collection.hide_viewport=False
    footprint=(13,11) if name=='kings_gate_courtyard' else (11,9)
    platform=stage('Example terrain',(0,0,-.16),(footprint[0],footprint[1],.20))
    cam.location=(14,-19,16) if name=='kings_gate_courtyard' else (11,-15,13)
    target=Vector((0,0,1.4 if name=='kings_gate_courtyard' else .8))
    cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler()
    cam.data.ortho_scale=21.5 if name=='kings_gate_courtyard' else 15
    scene.render.resolution_x=2000;scene.render.resolution_y=1400
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/('SocietyKit_'+name+'.blend')))
    if RENDER:
        scene.render.filepath=str(OUT/'previews'/('SocietyKit_'+name+'.png'));bpy.ops.render.render(write_still=True)
    collection.hide_render=True;collection.hide_viewport=True;platform.hide_render=True;platform.hide_viewport=True

REPORT['total_module_triangles']=sum(m['triangles'] for m in REPORT['modules'])
REPORT['elapsed_seconds']=round(time.monotonic()-START,3)
REPORT['materials']=[{'name':m.name,'roughness':round(m.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value,4),
    'metallic':round(m.node_tree.nodes['Principled BSDF'].inputs['Metallic'].default_value,4)} for m in list(M.values())+TILES]
REPORT['rendered_during_generation']=RENDER
(OUT/'model-report.json').write_text(json.dumps(REPORT,indent=2)+'\n',encoding='utf-8')
print('SOCIETY_KIT_COMPLETE',len(ASSETS),REPORT['total_module_triangles'],REPORT['elapsed_seconds'],flush=True)
