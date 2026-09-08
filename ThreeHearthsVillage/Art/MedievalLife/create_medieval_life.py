"""Authored modular transport and royal gate masters, Blender 5.2.

Run: blender -b --python create_medieval_life.py -- --render
This generates inspectable art prototypes, not fabricated runtime evidence.
"""
import argparse
import json
import math
import sys
from pathlib import Path
import bpy
from mathutils import Vector, Matrix

OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(OUT))
from meshkit import Geo, material, COLORS

parser=argparse.ArgumentParser()
parser.add_argument('--render',action='store_true')
parser.add_argument('--samples',type=int,default=32)
opt=parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
for folder in ('Modules','Assemblies','Previews'): (OUT/folder).mkdir(parents=True,exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene;scene.unit_settings.system='METRIC'

def collection(name):
    c=bpy.data.collections.new(name);scene.collection.children.link(c);return c

LIB=collection('00 | reusable parts')
SHOW=collection('50 | authored compositions')
STAGE=collection('90 | studio, excluded from exports')
modules={};assemblies={}

def empty(name,coll,parent=None,pos=(0,0,0)):
    ob=bpy.data.objects.new(name,None);coll.objects.link(ob);ob.parent=parent;ob.location=pos;return ob

def part(mid,layers,anchors=None,notes=''):
    root=empty(mid,LIB);root['asset_id']=mid
    for layer,geo in layers.items():
        geo.object(mid+'__'+layer,LIB,root,layer,bevel=.008 if layer in ('finish','attachments') else .012)
    for name,pos in (anchors or {'origin':(0,0,0)}).items():
        a=empty(mid+'__anchor_'+name,LIB,root,pos);a['anchor']=name
    modules[mid]={'root':root,'anchors':anchors or {'origin':(0,0,0)},'notes':notes}
    return root

def instance(mid,parent,pos=(0,0,0),yaw=0,scale=(1,1,1),key=None):
    src=modules[mid]['root'];root=empty(key or mid,SHOW,parent,pos)
    root.rotation_euler.z=math.radians(yaw);root.scale=scale;root['module_id']=mid
    for child in src.children:
        ob=child.copy();SHOW.objects.link(ob);ob.parent=root;ob.matrix_basis=child.matrix_basis.copy()
    return root

def assembly(aid,label,notes):
    root=empty(aid,SHOW);assemblies[aid]={'root':root,'label':label,'notes':notes};return root

# A cart whose wheels really have axle-centred origins; body/cargo/harness are
# independent parts. Slightly uneven plank widths describe carpentry, not noise.
g=Geo();f=Geo();a=Geo()
for x in (-.66,.66):
    g.beam((x,-1.18,.61),(x,1.15,.61),.16,.19,'oak_dark')
g.beam((-1.04,0,.63),(1.04,0,.63),.14,.16,'iron')
for i in range(9):
    x=-.72+i*.18;g.box((x,0,.82),(.169,2.23,.10),'oak_light' if i%3==0 else 'oak')
for x in (-.83,.83):
    for y in (-1.1,-.35,.42,1.1):
        g.beam((x,y,.76),(x*1.13,y,1.53),.087,role='oak_dark')
    for z in (1.0,1.23,1.46):
        g.box((x*(1+(z-.8)*.18),0,z),(.065,2.29,.15),'oak_light' if z==1.46 else 'oak')
    for y in (-1.06,1.06):
        f.box((x*1.08,y,1.18),(.073,.087,.76),'iron')
        for z in (.9,1.23,1.46):
            a.tube((x*1.13,y,z),(x*1.13+(.015 if x>0 else -.015),y,z),.025,role='iron',n=8)
for y in (-1.14,1.14):
    for z in (1.0,1.23,1.46):g.box((0,y,z),(1.69,.06,.15),'oak')
g.box((0,-.79,1.63),(1.77,.38,.10),'oak_light')
for x in (-.52,.52):
    g.beam((x,-.9,.66),(x,-3.88,.79),.083,.10,'oak_dark')
    f.tube((x,-3.55,.8),(x,-3.9,.8),.052,role='leather')
part('cart_body',{'structure':g,'finish':f,'attachments':a},
     {'wheel_left':(-1.04,0,.63),'wheel_right':(1.04,0,.63),'cargo':(0,.18,.88),
      'driver':(0,-.76,1.7),'horse':(0,-2.9,0)},'Two-wheel farm cart; wheels pivot around local X. Front is -Y.')

g=Geo();f=Geo()
g.ring((0,0,0),.607,.50,.125,'oak_dark',32)
f.ring((0,0,0),.638,.604,.143,'iron',32)
for i in range(10):
    t=2*math.pi*i/10
    g.beam((0,.11*math.cos(t),.11*math.sin(t)),(0,.535*math.cos(t),.535*math.sin(t)),.065,.09,'oak_light')
    f.tube((-.075,.603*math.cos(t),.603*math.sin(t)),(-.083,.603*math.cos(t),.603*math.sin(t)),.018,role='iron',n=8)
g.tube((-.13,0,0),(.13,0,0),.142,role='oak',n=12)
f.ring((-.12,0,0),.148,.125,.036,'iron',12)
f.ring((.12,0,0),.148,.125,.036,'iron',12)
part('cart_wheel',{'structure':g,'finish':f},{'axle':(0,0,0)},'Axle origin, radius 0.638m. Rotate local X to roll.')

g=Geo();f=Geo()
for row,count in ((0,3),(1,2)):
    for i in range(count):
        x=(i-(count-1)/2)*.37;z=.2+row*.33
        g.tube((x,-.77,z),(x,.93,z),.19,.16,'oak_dark',11)
        f.tube((x,-.785,z),(x,-.798,z),.162,role='endgrain',n=11)
        for a in (-.1,.07):f.beam((x+a,-.804,z-.075),(x+a*.7,-.804,z+.09),.013,.006,'oak')
for y in (-.45,.52):
    f.beam((-.59,y,.16),(-.39,y,.68),.047,.033,'leather')
    f.beam((-.39,y,.68),(.4,y,.68),.047,.033,'leather')
    f.beam((.4,y,.68),(.59,y,.16),.047,.033,'leather')
part('cargo_logs',{'structure':g,'attachments':f},notes='Removable cargo visual; runtime inventory must authorize loading.')

g=Geo();f=Geo()
for x,y,s in [(-.38,-.35,1),(.31,-.29,.93),(-.25,.36,.84),(.34,.4,1.02)]:
    g.loft([((x,y,.035),.24*s,.25*s),((x,y,.19),.30*s,.31*s),((x+.03,y,.52*s),.22*s,.22*s),
            ((x+.03,y,.67*s),.065,.06),((x+.04,y,.75*s),.1,.10)],'linen',10)
    f.ring((x+.03,y,.66*s),.071,.057,.047,'leather',12,(0,0,1))
    f.beam((x-.15,y-.257,.18),(x-.13,y-.232,.48*s),.018,.009,'leather_light')
part('cargo_sacks',{'structure':g,'attachments':f},notes='Four tied linen sacks; no implied resource creation.')

def horse(coat):
    g=Geo();f=Geo();a=Geo()
    # Broad barrel, powerful haunches, wedge neck, bony muzzle and jointed legs.
    g.ellipsoid((0,.12,1.34),(.43,1.0,.50),coat,14,8)
    g.ellipsoid((0,.76,1.42),(.44,.49,.44),coat+'_light',12,8)
    g.ellipsoid((0,-.56,1.36),(.38,.43,.49),coat,12,8)
    g.loft([((0,-.59,1.4),.31,.38),((0,-.80,1.77),.245,.30),
            ((0,-1.05,2.13),.17,.24),((0,-1.12,2.32),.135,.18)],coat,12)
    headrot=Matrix.Rotation(math.radians(-24),3,'X')
    g.ellipsoid((0,-1.24,2.18),(.195,.285,.28),coat+'_light',12,7,headrot)
    g.loft([((0,-1.32,2.16),.16,.16),((0,-1.59,1.99),.14,.13),
            ((0,-1.73,1.94),.155,.13)],coat,12,(0,-1,-.5))
    f.ellipsoid((0,-1.75,1.94),(.153,.11,.121),coat+'_dark',12,6)
    for side in (-1,1):
        x=side*.108
        g.loft([((x,-1.105,2.36),.059,.072),((x*1.25,-1.07,2.61),.05,.055),
                ((x*1.47,-1.09,2.70),.015,.022)],coat,8)
        f.beam((x*1.08,-1.132,2.42),(x*1.39,-1.128,2.64),.03,.018,coat+'_dark')
        f.ellipsoid((side*.171,-1.36,2.22),(.027,.041,.041),'eye',10,6)
        f.ellipsoid((side*.147,-1.796,1.958),(.02,.038,.025),'eye',8,5)
        # Forelegs are straighter; hind legs have a readable hock angle.
        for front in (True,False):
            xx=side*(.27 if front else .30)
            points=[(xx,-.57,1.37),(xx,-.69,.84),(xx,-.64,.40),(xx,-.70,.12)] if front else [
                (xx,.72,1.39),(xx,.51,.92),(xx,.94,.54),(xx,.78,.13)]
            for j,(p,q) in enumerate(zip(points,points[1:])):
                g.tube(p,q,[.17,.098,.06][j],[.115,.062,.07][j],coat if j<2 else 'mane',10)
            g.ellipsoid(points[1],(.102,.105,.13),coat,10,6)
            px,py,pz=points[-1]
            f.loft([((px,py-.035,.015),.105,.15),((px,py-.01,.13),.08,.108)],'hoof',10)
        # Harness breast strap and bridle remain removable meshes.
        a.beam((side*.31,-.63,1.70),(side*.37,-.92,1.29),.072,.046,'leather')
        a.beam((side*.37,-.92,1.29),(side*.18,-1.005,1.2),.072,.046,'leather')
        a.beam((side*.19,-1.17,2.37),(side*.19,-1.45,2.08),.033,.018,'leather')
        a.beam((side*.16,-1.59,2.05),(side*.15,-1.73,1.89),.035,.02,'leather')
        a.ring((side*.198,-1.44,2.08),.045,.027,.018,'gold',12)
        a.beam((side*.2,-1.45,2.08),(side*.44,.8,1.36),.015,.015,'leather')
    # Crest follows the back edge of the neck instead of a floating hair block.
    for i in range(9):
        t=i/8
        p=(0,-.91+t*.48,2.40-t*.76)
        f.ellipsoid(p,(.086,.11,.14),'mane',8,5)
    f.tube((0,1.02,1.6),(.06,1.29,1.24),.13,.10,'mane',10)
    f.tube((.06,1.29,1.24),(.12,1.30,.58),.11,.048,'mane',10)
    a.beam((-.18,-1.005,1.2),(.18,-1.005,1.2),.072,.046,'leather')
    a.beam((-.153,-1.75,1.945),(.153,-1.75,1.945),.035,.027,'leather')
    return part('horse_'+coat,{'structure':g,'finish':f,'attachments':a},
                {'ground':(0,0,0),'withers':(0,-.48,1.83),'hitch_left':(-.44,-.63,1.38),
                 'hitch_right':(.44,-.63,1.38)},'Authored standing horse. No skeleton or walk cycle yet; removable tack.')

horse('bay');horse('grey')

# Royal livery: a modest kettle helmet, quilted blue clothing, separate spear
# and heater shield. These are equipment masters and unrigged character studies.
g=Geo();f=Geo()
g.loft([((0,0,0),.235,.22),((0,0,.095),.201,.193),((0,0,.25),.145,.14),
        ((0,0,.29),.045,.05)],'steel',16)
g.ring((0,0,.026),.31,.185,.034,'iron',16,(0,0,1))
for y in (-.19,.19):f.beam((0,y,.10),(0,y*.45,.265),.041,.023,'gold')
part('guard_helmet',{'structure':g,'finish':f},{'head':(0,0,0)},'Helmet attachment origin at forehead; adjust per skeletal head socket.')

g=Geo();f=Geo()
outline=[(-.30,0,.72),(.30,0,.72),(.28,0,.25),(0,0,-.12),(-.28,0,.25)]
g.mesh([(x,y+depth,z) for depth in (-.048,.048) for x,y,z in outline],
       [(0,4,3,2,1),(5,6,7,8,9)]+[(i,(i+1)%5,(i+1)%5+5,i+5) for i in range(5)],'blue')
for p,q in zip(outline,outline[1:]+outline[:1]):
    f.beam((p[0],-.062,p[2]),(q[0],-.062,q[2]),.035,.023,'gold')
f.box((0,-.064,.41),(.09,.025,.54),'gold')
f.box((0,-.07,.54),(.41,.026,.08),'gold')
g.beam((-.12,.095,.44),(.12,.095,.44),.045,.07,'leather')
part('guard_shield',{'structure':g,'finish':f},{'grip':(0,.095,.44)},'Separate heater shield with royal crossroads device.')

g=Geo();f=Geo()
g.tube((0,0,0),(0,0,2.17),.031,role='oak_dark',n=10)
f.tube((0,0,0),(0,0,.15),.037,role='iron',n=10)
f.tube((0,0,2.03),(0,0,2.19),.044,role='iron',n=10)
f.mesh([(-.083,0,2.25),(0,-.026,2.25),(.083,0,2.25),(0,.026,2.25),(0,0,2.60),(0,0,2.10)],
       [(0,1,4),(1,2,4),(2,3,4),(3,0,4),(1,0,5),(2,1,5),(3,2,5),(0,3,5)],'steel')
part('guard_spear',{'structure':g,'finish':f},{'grip':(0,0,1.10)},'2.6m spear, bottom origin; no attack logic is implied.')

def person(mid,guard=True):
    g=Geo();f=Geo();a=Geo()
    cloth='blue' if guard else 'red'
    for side in (-1,1):
        x=side*.12
        g.tube((x,0,.90),(x,.015,.49),.117,.087,'oak_dark',10)
        g.tube((x,.015,.49),(x,-.015,.12),.088,.075,'leather',10)
        f.ellipsoid((x,-.075,.075),(.104,.182,.073),'leather',10,6)
    g.loft([((0,0,.78),.30,.17),((0,0,1.04),.235,.15),((0,0,1.25),.30,.18),
            ((0,0,1.42),.30,.17),((0,0,1.49),.18,.13)],cloth,12)
    f.ring((0,0,1.03),.247,.222,.078,'leather',16,(0,0,1))
    f.box((.075,-.17,1.04),(.09,.035,.076),'gold')
    f.box((0,-.173,1.30),(.045,.017,.34),'gold' if guard else 'linen')
    f.box((0,-.178,1.36),(.18,.018,.043),'gold' if guard else 'linen')
    for side in (-1,1):
        shoulder=(side*.29,0,1.38);elbow=(side*.39,-.045,1.13);wrist=(side*.38,-.16,.96)
        g.tube(shoulder,elbow,.128,.096,cloth,10);g.tube(elbow,wrist,.09,.064,'linen',10)
        f.ellipsoid((side*.38,-.17,.91),(.077,.067,.105),'skin',10,6)
        if guard:f.ellipsoid((side*.29,0,1.40),(.16,.195,.12),'steel',10,6)
    g.tube((0,0,1.43),(0,0,1.60),.089,role='skin',n=10)
    g.ellipsoid((0,-.015,1.73),(.16,.15,.207),'skin',12,8)
    f.ellipsoid((0,.024,1.81),(.163,.144,.14),'hair',12,6)
    for side in (-1,1):
        f.ellipsoid((side*.064,-.151,1.764),(.023,.01,.015),'eye',8,4)
        f.beam((side*.038,-.151,1.803),(side*.09,-.132,1.808),.021,.013,'hair')
        g.ellipsoid((side*.155,-.005,1.742),(.027,.04,.06),'skin',8,5)
    g.ellipsoid((0,-.167,1.71),(.038,.048,.06),'skin_light',8,5)
    f.beam((-.039,-.154,1.647),(.04,-.154,1.647),.015,.01,'hair')
    a.box((-.2,-.12,.98),(.14,.09,.17),'leather_light')
    return part(mid,{'structure':g,'finish':f,'attachments':a},
                {'head':(0,-.008,1.89),'left_hand':(-.38,-.17,.92),'right_hand':(.38,-.17,.92),
                 'ground':(0,0,0)},'Unrigged character art study; local runtime NPC duties are a separate implementation.')

person('royal_guard_body');person('carter_body',False)

# Gate piers are independent, allowing wider/narrower passages. Stone courses
# use a fixed authored bond with mild chipped corners and two-tone repairs.
g=Geo();f=Geo()
g.box((0,0,.12),(1.3,1.25,.24),'stone_dark')
for row in range(9):
    for ix in range(2):
        for iy in range(2):
            x=(ix-.5)*.51;y=(iy-.5)*.51
            g.box((x,y,.39+row*.29),(.495,.495,.277),'stone_light' if (row+ix+iy)%5==0 else 'stone')
g.box((0,0,3.01),(1.22,1.18,.18),'stone_light')
for x in (-.40,.40):
    for y in (-.40,.40):g.box((x,y,3.23),(.38,.38,.30),'stone')
for x,y in [(-.39,-.47),(.26,.49),(.51,-.25)]:f.box((x,y,.065),(.29,.20,.025),'moss')
part('gate_pier',{'structure':g,'weathering':f},{'hinge':(.57,0,.3),'banner':(0,-.62,2.65)},'Masonry gate pier, 1.3m footprint; opening width set by assembly.')

g=Geo();f=Geo()
for i in range(7):g.box((.10+i*.188,0,1.16),(.172,.105,2.14-(.06 if i%3==0 else 0)),'oak' if i%2 else 'oak_light')
for z in (.52,1.63):g.box((.675,.08,z),(1.33,.10,.14),'oak_dark')
g.beam((.1,.08,.34),(1.24,.08,1.78),.13,.085,'oak_dark')
for z in (.45,1.75):
    f.box((.60,-.066,z),(1.2,.025,.075),'iron')
    f.tube((0,0,z-.09),(0,0,z+.09),.09,role='iron',n=12)
    for x in (.13,.6,1.1):f.tube((x,-.08,z),(x,-.09,z),.022,role='iron',n=8)
f.ring((1.11,-.10,1.07),.074,.052,.018,'iron',14,(0,1,0))
part('gate_leaf',{'structure':g,'finish':f},{'hinge':(0,0,0)},'Single hinged leaf, local Z rotation. Reverse X for opposite leaf.')

g=Geo();f=Geo()
g.beam((0,0,0),(0,0,2.1),.07,role='oak_dark')
g.beam((-.37,0,1.9),(.37,0,1.9),.043,role='iron')
vs=[(-.31,-.006,1.85),(.31,-.006,1.85),(.29,-.10,.87),(0,-.07,.69),(-.29,.025,.89)]
g.mesh(vs,[(0,1,2,3,4)],'blue')
f.box((0,-.062,1.38),(.06,.02,.55),'gold');f.box((0,-.066,1.54),(.40,.02,.06),'gold')
part('royal_banner',{'structure':g,'finish':f},notes='Royal blue hanging pennant; static cloth study, material double-sided on import.')

g=Geo();f=Geo()
for x in (-.58,.58):
    g.beam((x,-.27,.03),(x,-.27,.47),.10,role='oak_dark')
    g.beam((x,.27,.03),(x,.27,.88),.10,role='oak_dark')
for y in (-.20,0,.20):g.box((0,y,.49),(1.45,.18,.065),'oak_light')
for z in (.68,.86):g.box((0,.30,z),(1.44,.07,.11),'oak')
g.beam((-.58,0,.22),(.58,0,.22),.10,role='oak_dark')
part('watch_bench',{'structure':g},notes='Shared rest bench; supports duty rest transitions.')

g=Geo();f=Geo()
for x in (-1,1):
    for y in (-.63,.63):g.beam((x,y,0),(x,y,2.27),.14,role='oak_dark')
for y in (-.63,.63):g.beam((-1.12,y,2.20),(1.12,y,2.20),.17,.19,'oak')
for x in (-1,1):
    g.beam((x,-.63,1.68),(x,-.20,2.20),.09,role='oak')
    for y in (-1,1):g.beam((x,y*.89,2.16),(x,0,2.81),.12,role='oak_dark')
for side in (-1,1):
    for row in range(4):
        for col in range(8):
            x=-1.18+col*.337;y=side*(.125+row*.23);z=2.86-abs(y)*.73
            f.box((x,y,z),(.36,.31,.045),'roof_light' if (row+col)%4==0 else 'roof',Matrix.Rotation(-side*.63,3,'X'))
g.beam((-1.37,0,2.91),(1.37,0,2.91),.105,role='oak_dark')
part('watch_shelter',{'structure':g,'finish':f},{'bench':(0,.2,0)},'Open tiled shelter, shared snap scale with village timber kit.')

g=Geo();f=Geo()
for x in (-.62,.62):g.box((x,0,.24),(.14,.72,.48),'stone_dark')
g.box((0,0,.36),(1.55,.8,.13),'stone')
for y in (-.39,.39):g.box((0,y,.56),(1.62,.13,.45),'stone_light')
for x in (-.75,.75):g.box((x,0,.56),(.13,.7,.45),'stone')
f.box((0,0,.62),(1.34,.62,.016),'water')
part('water_trough',{'structure':g,'finish':f},notes='Hollow trough with replaceable water surface.')

g=Geo();f=Geo()
for x in (-.66,.66):
    g.beam((x,-.30,0),(x,-.30,1.03),.10,role='oak_dark')
    g.beam((x,.30,0),(x,.30,1.03),.10,role='oak_dark')
for y in (-.30,.30):
    g.beam((-.72,y,.48),(.72,y,.48),.085,role='oak')
    g.beam((-.72,y,1.03),(.72,y,1.03),.085,role='oak')
    for i in range(9):g.beam((-.60+i*.15,y*.60,.45),(-.60+i*.15,y,1.00),.037,role='oak_light')
for i in range(13):
    x=-.57+i*.09
    f.ellipsoid((x,.02,.82+(.035 if i%2 else 0)),(.073,.29,.19),'hay' if i%3 else 'hay_light',8,5)
part('hay_rack',{'structure':g,'attachments':f},notes='Hay is removable; future feeding must debit stored hay.')

g=Geo();f=Geo()
for x in (-1,1):g.beam((x,0,0),(x,0,1.28),.13,role='oak_dark')
for z in (.55,1.05):g.beam((-1.03,0,z),(1.03,0,z),.10,.085,'oak_light')
part('stable_rail',{'structure':g},{'left':(-1,0,0),'right':(1,0,0)},'2m repeatable paddock and stable rail.')

g=Geo();f=Geo()
g.beam((0,0,0),(0,0,2.45),.14,role='oak_dark')
for z,length in ((1.9,1.05),(2.19,.83)):
    g.box((.07,0,z),(length,.09,.22),'oak_light')
    tip=length/2+.07
    g.mesh([(tip,-.046,z-.11),(tip+.16,-.046,z),(tip,-.046,z+.11),
            (tip,.046,z-.11),(tip+.16,.046,z),(tip,.046,z+.11)],
           [(0,2,1),(3,4,5),(0,1,4,3),(1,2,5,4),(2,0,3,5)],'oak_light')
    # Symbols, not generated illegible typography: cargo crate and crown.
    f.box((-.15,-.06,z),(.19,.02,.14),'blue')
    f.beam((-.23,-.073,z-.055),(-.07,-.073,z+.055),.021,.012,'gold')
part('road_signpost',{'structure':g,'finish':f},notes='Pictographic road sign, no baked lettering.')

g=Geo();f=Geo()
g.tube((0,0,.1),(0,0,.7),.07,role='iron')
g.ring((0,0,.77),.31,.26,.09,'iron',16,(0,0,1))
for i in range(8):
    t=2*math.pi*i/8
    g.beam((.15*math.cos(t),.15*math.sin(t),.5),(.31*math.cos(t),.31*math.sin(t),.82),.04,role='iron')
for x,y in [(-.24,-.18),(.24,-.18),(0,.25)]:g.beam((0,0,.46),(x,y,.01),.045,role='iron')
f.ellipsoid((0,0,.65),(.20,.20,.10),'oak_dark',10,5)
for x,y,h in [(-.09,0,.34),(.1,.03,.28),(0,-.03,.47)]:
    f.loft([((x,y,.67),.08,.07),((x+.02,y,.78),.075,.06),((x-.03,y,.67+h),.003,.003)],'flame',7)
part('watch_brazier',{'structure':g,'attachments':f},notes='Removable stylized flame mesh; no runtime fire light or fuel logic yet.')

# Three distinct, reviewable compositions assembled from the same library.
cart=assembly('loaded_horse_cart','Horse, cart and removable log load','Authored stationary art master; not a runtime transport screenshot.')
instance('cart_body',cart)
instance('cart_wheel',cart,(-1.04,0,.638),key='wheel_left')
instance('cart_wheel',cart,(1.04,0,.638),key='wheel_right')
instance('cargo_logs',cart,(0,.13,.88))
instance('horse_bay',cart,(0,-2.90,0))
instance('carter_body',cart,(1.68,-1.4,0),yaw=-25)

gate=assembly('royal_gate_watch','Royal gate and watch post','Two working hinge anchors; guards shown as unrigged art studies.')
instance('gate_pier',gate,(-2,0,0))
instance('gate_pier',gate,(2,0,0))
instance('gate_leaf',gate,(-1.43,0,.06),yaw=-34)
instance('gate_leaf',gate,(1.43,0,.06),yaw=29,scale=(-1,1,1))
instance('royal_banner',gate,(-2,-.60,1.32))
instance('royal_banner',gate,(2,-.60,1.32))
instance('watch_shelter',gate,(3.92,.35,0))
instance('watch_bench',gate,(3.92,.45,0))
for idx,(x,y,yaw) in enumerate([(-2.86,-.68,8),(1.03,-1.04,-16)]):
    p=instance('royal_guard_body',gate,(x,y,0),yaw=yaw,key='guard_'+str(idx))
    instance('guard_helmet',p,(0,-.008,1.89))
    instance('guard_shield',p,(-.39,-.26,.40),yaw=-8)
    instance('guard_spear',p,(.43,-.18,0))
instance('watch_brazier',gate,(-3.8,-.68,0))
instance('road_signpost',gate,(5.30,-.35,0),yaw=-15)

stable=assembly('paddock_corner','Paddock, feeding and water','Modular rail corner; horse study remains unrigged.')
for x in (-1,1):instance('stable_rail',stable,(x,1.65,0))
instance('stable_rail',stable,(-2,.65,0),yaw=90)
instance('water_trough',stable,(1.1,-.65,0),yaw=90)
instance('hay_rack',stable,(-.65,1.06,0))
instance('horse_grey',stable,(-.4,0,0),yaw=-30)
instance('cargo_sacks',stable,(-2.55,.95,0))

def descendants(root):return [root]+list(root.children_recursive)

def stats(root):
    bpy.context.view_layer.update();inv=root.matrix_world.inverted();points=[];tri=0;meshes=[]
    dg=bpy.context.evaluated_depsgraph_get()
    for ob in descendants(root):
        if ob.type!='MESH':continue
        meshes.append(ob);ev=ob.evaluated_get(dg);me=ev.to_mesh();me.calc_loop_triangles()
        tri+=len(me.loop_triangles);points += [inv@ob.matrix_world@v.co for v in me.vertices];ev.to_mesh_clear()
    lo=[min(p[i] for p in points) for i in range(3)];hi=[max(p[i] for p in points) for i in range(3)]
    return {'triangles':tri,'meshes':len(meshes),'uv_meshes':sum(bool(o.data.uv_layers) for o in meshes),
            'bounds_m':{'min':lo,'max':hi,'size':[hi[i]-lo[i] for i in range(3)]}}

def export(root,path):
    bpy.ops.object.select_all(action='DESELECT')
    for ob in descendants(root):ob.select_set(True)
    bpy.context.view_layer.objects.active=root
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,export_apply=True,
                             export_extras=True,export_texcoords=True,export_normals=True,export_materials='EXPORT',
                             export_cameras=False,export_lights=False)

catalog={'schema_version':1,'units':'meters','authoring_up':'+Z','front':'-Y',
         'layers':['structure','finish','attachments','weathering'],'materials':COLORS,'modules':[],'assemblies':[],
         'limitations':['Authored art prototypes; runtime integration reported separately.',
                        'Characters and horses are unrigged, with no armature or animation yet.',
                        'Portable PBR colours and planar UVs; no baked texture maps or authored LODs.']}
for mid,spec in modules.items():
    path=OUT/'Modules'/f'{mid}.glb';export(spec['root'],path)
    catalog['modules'].append({'id':mid,'path':path.relative_to(OUT).as_posix(),'anchors':spec['anchors'],
                               'notes':spec['notes'],'layers':sorted({c.get('layer') for c in spec['root'].children if c.type=='MESH'}),**stats(spec['root'])})
for aid,spec in assemblies.items():
    path=OUT/'Assemblies'/f'{aid}.glb';export(spec['root'],path)
    pieces=[{'key':c.name,'parent_key':c.parent.name if c.parent!=spec['root'] else None,
             'module':c['module_id'],'position_m':list(c.location),'yaw_degrees':math.degrees(c.rotation_euler.z),
             'scale':list(c.scale)} for c in spec['root'].children_recursive if c.get('module_id')]
    catalog['assemblies'].append({'id':aid,'label':spec['label'],'notes':spec['notes'],
                                  'path':path.relative_to(OUT).as_posix(),'pieces':pieces,**stats(spec['root'])})
(OUT/'catalog.json').write_text(json.dumps(catalog,ensure_ascii=False,indent=2),encoding='utf-8')
LIB.hide_render=True;LIB.hide_viewport=True
for root,pos in [(cart,(-4,-1,0)),(gate,(1.3,5.0,0)),(stable,(4,-2.1,0))]:root.location=pos

g=Geo();g.box((0,0,-.14),(200,200,.25),'earth');g.object('Studio ground',STAGE,bevel=0)
world=bpy.data.worlds.new('Pale daylight');world.use_nodes=True
world.node_tree.nodes['Background'].inputs['Color'].default_value=(.73,.81,.92,1)
world.node_tree.nodes['Background'].inputs['Strength'].default_value=.40;scene.world=world
lights=[]
for name,p,energy,size,color in [('Warm key',(-6,-9,15),2300,9,(1,.9,.76)),
                                ('Sky fill',(10,-3,11),1800,10,(.77,.87,1)),
                                ('Sun rim',(-4,10,13),2000,8,(1,.95,.84))]:
    d=bpy.data.lights.new(name,'AREA');d.energy=energy;d.shape='DISK';d.size=size;d.color=color
    o=bpy.data.objects.new(name,d);STAGE.objects.link(o);o.location=p
    o.rotation_euler=(Vector((0,0,1))-o.location).to_track_quat('-Z','Y').to_euler();lights.append(o)
cd=bpy.data.cameras.new('Review camera');cd.type='ORTHO'
cam=bpy.data.objects.new('Review camera',cd);STAGE.objects.link(cam);scene.camera=cam
scene.render.engine='CYCLES';scene.cycles.device='CPU';scene.cycles.samples=opt.samples;scene.cycles.use_denoising=True
scene.render.threads_mode='FIXED';scene.render.threads=6
scene.render.resolution_x=1800;scene.render.resolution_y=1300;scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG';scene.view_settings.view_transform='AgX'
scene.view_settings.look='AgX - Medium High Contrast'

def camera(target,scale,offset=(12,-18,12)):
    target=Vector(target);cam.location=target+Vector(offset)
    cam.rotation_euler=(target-cam.location).to_track_quat('-Z','Y').to_euler();cd.ortho_scale=scale

camera((.4,.8,1.1),20)
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type=='VIEW_3D':
            area.spaces.active.region_3d.view_rotation=cam.rotation_euler.to_quaternion()
            area.spaces.active.region_3d.view_distance=23
            area.spaces.active.region_3d.view_location=(0,0,1)
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'MedievalLife_Masters.blend'))
if opt.render:
    scene.render.filepath=str(OUT/'Previews'/'MedievalLife_Overview.png');bpy.ops.render.render(write_still=True)
    for aid,spec in assemblies.items():
        for other in assemblies.values():
            for ob in descendants(other['root']):ob.hide_render=other is not spec
        bounds=stats(spec['root'])['bounds_m'];origin=spec['root'].location.copy()
        target=origin+Vector([(bounds['min'][i]+bounds['max'][i])/2 for i in range(3)])
        camera(target,max(bounds['size'][0]*1.48,bounds['size'][1]*1.23,bounds['size'][2]*2.1),offset=(10,-15,9))
        for light,offset in zip(lights,[(-6,-8,12),(8,-4,9),(-4,8,12)]):
            light.location=target+Vector(offset)
            light.rotation_euler=(target-light.location).to_track_quat('-Z','Y').to_euler()
        scene.render.filepath=str(OUT/'Previews'/f'{aid}.png');bpy.ops.render.render(write_still=True)
        print('MEDIEVAL_RENDER_COMPLETE',aid,flush=True)
    for other in assemblies.values():
        for ob in descendants(other['root']):ob.hide_render=False
print('MEDIEVAL_MASTERS_COMPLETE',len(modules),len(assemblies),flush=True)
