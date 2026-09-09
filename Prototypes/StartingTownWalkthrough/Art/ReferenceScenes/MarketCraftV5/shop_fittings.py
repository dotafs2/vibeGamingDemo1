"""Fine shop interiors for the Starting Town craft pass.

Local coordinates: facade at y=0, interior y>0, floor z=0, front faces -Y.
The caller supplies the collection and owns walls, roof, doors and collision.
"""
import math
from pathlib import Path
import sys
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent))
import build_reference_scenes_hq as hq
G=hq.G

def _emit(g,col,name):
    o=g.obj(name,col,.006); o['style_id']='level0_market_craft_v5'; o['interior_fitting']=True; return o
def _box(g,p,s,r): g.box(p,s,r)

def _open_door(g,x,width=1.25,height=2.6):
    """Two hinged leaves swung outward, leaving >1m clear passage."""
    leaf=width*.48
    for side in (-1,1):
        hinge=x+side*width/2; panel=G()
        panel.box((side*leaf/2,0,height/2),(leaf,.10,height),'wood_dark')
        for z in (.18,height-.18): panel.beam((-leaf*.46,0,z),(leaf*.46,0,z),.035,'wood_light')
        for xx in (-leaf*.42,leaf*.42): panel.beam((xx,0,.12),(xx,0,height-.12),.028,'wood_light')
        panel.box((side*leaf/2,-.065,height*.5),(.07,.06,.07),'iron')
        panel.box((side*leaf/2,.06,height*.5),(.05,.05,.16),'brass')
        # hinge knuckles remain visible at the jamb
        panel.tube([(0,-.07,.38),(0,-.07,2.20)],[.025,.025],'iron',8)
        g.add(panel,(hinge,-.06,0),side*95)
    # threshold and jamb joinery
    g.box((x,0,.07),(width+.18,.20,.14),'stone_light')
    for side in (-1,1):
        g.box((x+side*(width/2+.10),0,height/2),(.14,.22,height+.16),'wood_light')

def _service_counter(g,x,width,height=2.6):
    # Recessed service counter with sill, counter slab and side returns.
    g.box((x,-.12,1.08),(width+.18,.24,.14),'wood_dark')
    g.box((x,-.30,1.08),(width,.55,.14),'wood_light')
    g.box((x,.22,1.20),(width,.10,.10),'stone_light')
    for side in (-1,1): g.box((x+side*(width/2-.08),.05,height*.54),(.12,.45,height*.78),'wood')
    for xx in (-width*.30,0,width*.30): g.box((x+xx,-.34,.50),(.045,.045,.78),'iron')

def _shelf(g,x,y,width=2.8,height=2.15,depth=.42):
    """Freestanding timber shelf with three boards and mortise-like posts."""
    for xx in (-width/2+.07,width/2-.07):
        g.box((x+xx,y,height/2),(.12,depth,height),'wood_dark')
        for z in (.34,.98,1.62): g.box((x+xx,y,z),(.20,depth+.08,.07),'wood_light')
    for z in (.35,1.02,1.69): g.box((x,y,z),(width,depth,.11),'wood_light')
    g.box((x,y,.10),(width+.16,depth+.12,.12),'wood')

def _bread(g,p,kind='round',scale=1):
    x,y,z=p
    if kind=='loaf': g.ellipsoid((x,y,z),(.28*scale,.18*scale,.16*scale),'canvas_gold',12,6)
    elif kind=='roll': g.ellipsoid((x,y,z),(.12*scale,.12*scale,.10*scale),'canvas_gold',10,5)
    else: g.ellipsoid((x,y,z),(.22*scale,.22*scale,.12*scale),'canvas_gold',12,6)

def _jar(g,p,scale=1):
    x,y,z=p; g.curved((x,y,z),[(.14*scale,0),(.22*scale,.10*scale),(.25*scale,.45*scale),(.16*scale,.60*scale),(.12*scale,.72*scale)],'canvas_cream',20)
    g.curved((x,y,z+.70*scale),[(.12*scale,0),(.14*scale,.08*scale),(.12*scale,.14*scale)],'brass',16)

def _bag(g,p,scale=1):
    x,y,z=p; g.ellipsoid((x,y,z),(.26*scale,.18*scale,.42*scale),'canvas_cream',12,7)
    g.tube([(x-.10*scale,y,z+.35*scale),(x,y,z+.62*scale),(x+.10*scale,y,z+.35*scale)],[.018*scale]*3,'wood_dark',7)

def _bakery(col,prefix):
    g=G(); floor=G();
    # floorboards run into the building, leaving the central 1.7m route open
    for i in range(14): floor.box((-.0, .35+i*.48, .035),(9.7,.43,.07),'wood_light')
    _open_door(g,-2.9,1.25,2.6); _service_counter(g,1.35,3.8)
    # counter display: bread remains below sightline of the service opening
    for i in range(7): _bread(g,(.05+i*.40,-.57,1.24),'loaf',.85)
    for i in range(6): _bread(g,(.25+i*.52,-.38,1.22),'roll',.8)
    # three shelves on the right wall; their depth stays clear of central route
    for i in range(3): _shelf(g,4.0,1.25+i*1.35,1.45,2.05,.40)
    for p in [(3.55,1.02,.525),(4.05,1.02,.525),(3.8,2.37,.525),(4.15,2.37,1.195),(3.55,3.72,1.195)]: _bread(g,p,'round',.9)
    for p in [(3.7,2.45,.405),(4.2,2.45,1.075),(3.8,3.82,1.745),(4.2,5.15,1.075)]: _jar(g,p,.9)
    for p in [(3.55,1.20,.825),(4.05,1.20,.825),(3.65,2.55,1.495),(4.1,4.0,1.495)]: _bag(g,p,.95)
    # small side cabinet, kept against the right wall
    g.box((4.1,5.9,1.05),(2.5,.65,2.1),'wood_dark')
    for z in (.52,1.05,1.58): g.box((4.1,5.54,z),(2.25,.08,.06),'brass')
    _emit(floor,col,prefix+'_bakery_floorboards'); _emit(g,col,prefix+'_bakery_woodwork_and_goods')
    return {'kind':'bakery','door_center':(-2.9,0,0),'service_center':(1.35,0,2.6),'shelves':3,'central_route_width':1.7}

def _outfitter(col,prefix):
    g=G(); floor=G()
    for i in range(12): floor.box((0,.35+i*.50,.035),(7.7,.45,.07),'wood_light')
    _open_door(g,2.15,1.25,2.6); _service_counter(g,-1.2,3.6)
    # rolls of cloth on pegs and shelves, plus backpacks and rope coils
    for j,color in enumerate(['canvas_green','canvas_gold','canvas_red','canvas_green']):
        g.curved((-3.0+j*.48,1.10,1.35),[(.20,0),(.28,.12),(.30,.72),(.24,.90),(.16,1.05)],color,20)
    for i in range(3): _shelf(g,-3.15,2.6+i*1.25,1.40,1.95,.42)
    for p in [(-3.65,2.35,.825),(-3.0,2.35,.825),(-2.65,2.35,.825),(-3.35,3.60,1.495),(-2.65,4.85,1.495)]: _bag(g,p,.85)
    for j in range(5):
        x=-.2+j*.48; g.tube([(x,1.0,.40),(x+.12,1.0,.48),(x+.18,1.0,.68),(x+.05,1.0,.78)],[.075,.075,.075,.075],'wood_dark',10)
    for p in [(-.2,4.6,.35),(.5,4.6,.35),(1.2,4.6,.35)]:
        g.box(p,(.75,.65,.55),'wood_dark'); g.box((p[0],p[1]-.34,p[2]),(.55,.04,.38),'brass')
    _emit(floor,col,prefix+'_outfitter_floorboards'); _emit(g,col,prefix+'_outfitter_woodwork_and_goods')
    return {'kind':'outfitter','door_center':(2.15,0,0),'service_center':(-1.2,0,2.6),'shelves':3,'central_route_width':1.6}

def build(col,prefix,kind='bakery'):
    """Build one detailed, walkable shop fitting set."""
    if kind=='bakery': return _bakery(col,prefix)
    if kind=='outfitter': return _outfitter(col,prefix)
    raise ValueError('unsupported shop fitting kind: '+str(kind))
