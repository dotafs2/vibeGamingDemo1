"""Tolbana fountain, seating and restrained garden details.

Local world coordinates are metres.  This module contains only freestanding
props; the caller owns buildings, terrain and navigation.
"""
import math, sys
from pathlib import Path
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent))
import build_reference_scenes_hq as hq
G=hq.G

def _emit(g,col,name,collision=False):
    o=g.obj(name,col,.007,collision); o['style_id']='level0_tolbana_craft_v5'; o['garden_detail']=True; return o

def _fountain(col):
    cx,cy,base=8.9,-5.0,.22;stone=G();water=G()
    # Outer wall folds inward through its coping to the real basin floor.
    stone.curved((cx,cy,base),[(2.70,0),(2.82,.10),(2.84,.20),(2.70,.29),(2.66,.65),(2.79,.71),(2.83,.81),(2.80,.90),(2.47,.90),(2.39,.79),(2.38,.34),(.52,.34)],'stone_light',96)
    # Carved central pedestal, first bowl, shaft, second bowl and finial.
    stone.curved((cx,cy,base+.34),[(.61,0),(.70,.13),(.61,.26),(.44,.43),(.33,.92),(.38,1.12),(.53,1.23),(1.14,1.43),(1.34,1.55),(1.40,1.65),(1.38,1.79),(1.24,1.83),(1.17,1.67),(.27,1.64)],'limestone',80)
    stone.curved((cx,cy,base+1.98),[(.29,0),(.35,.12),(.25,.26),(.20,.54),(.26,.74),(.62,.91),(.79,1.04),(.82,1.15),(.77,1.25),(.65,1.25),(.59,1.12),(.13,1.10)],'stone_light',72)
    stone.curved((cx,cy,base+3.08),[(.16,0),(.22,.10),(.15,.24),(.12,.47),(.18,.57),(.14,.70),(.02,.83)],'limestone',56)
    # Basin facing consists of separate radial stones with a narrow mortar gap.
    for j in range(28):
        aa=(j+.015)*math.tau/28;bb=(j+.985)*math.tau/28
        vs=[(cx+r*math.cos(t),cy+r*math.sin(t),base+z) for z in (.31,.64) for r,t in [(2.65,aa),(2.69,aa),(2.69,bb),(2.65,bb)]]
        stone.mesh(vs,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],'limestone' if j%5 else 'stone_light')
    # Fluting and petal carving attach to the turned supports.
    for j in range(12):
        aa=j*math.tau/12
        stone.tube([(cx+.45*math.cos(aa),cy+.45*math.sin(aa),base+.75),(cx+.36*math.cos(aa),cy+.36*math.sin(aa),base+1.18),(cx+.39*math.cos(aa),cy+.39*math.sin(aa),base+1.43)],[.022,.022,.01],'stone_light',8)
    for r,z in [(2.37,.60),(1.24,2.12),(.65,3.21)]:water.cylinder((cx,cy,base+z),r,.012,'water',80)
    # Thin widening sheets follow a gravity curve and terminate inside the bowl below.
    for r,z,tz,n in [(1.30,2.14,.61,18),(.73,3.22,2.13,10)]:
        for j in range(n):
            ang=j*math.tau/n;vs=[]
            for k in range(17):
                t=k/16;rad=r+.21*t;zz=z-(z-tz)*t*t;ww=.018+.020*t
                for sign in (-1,1):vs.append((cx+rad*math.cos(ang)-sign*ww*math.sin(ang),cy+rad*math.sin(ang)+sign*ww*math.cos(ang),base+zz))
            water.mesh(vs,[(2*k,2*k+1,2*k+3,2*k+2) for k in range(16)],'water' if j%4 else 'foam')
            if j%3==0:
                rr=r+.21
                water.tube([(cx+rr*math.cos(ang)+.09*math.cos(v*math.tau/24),cy+rr*math.sin(ang)+.09*math.sin(v*math.tau/24),base+tz+.013) for v in range(25)],[.004]*25,'foam',5)
    for j in range(4):
        ang=j*math.pi/2;drain=G();drain.box((0,0,0),(.15,.035,.075),'recess')
        stone.add(drain,(cx+2.385*math.cos(ang),cy+2.385*math.sin(ang),base+.69),math.degrees(ang)+90)
    _emit(stone,col,'tolbana_v5_fountain_turned_stone_and_joints');_emit(water,col,'tolbana_v5_fountain_pools_and_thin_streams')
    c=G();c.cylinder((cx,cy,base),2.83,.90,'limestone',64);_emit(c,col,'COL_tolbana_v5_fountain',True)
    return {'name':'tolbana_v5_fountain_turned_stone_and_joints','center':[cx,cy,base],'radius':2.84,'height':3.91,'collision':'COL_tolbana_v5_fountain'}

def _bench(col,name,x,y,yaw=0):
    g=G();w=2.35
    # Seat top is .46m above ground. Continuous side frames carry seat and arms.
    for j in range(4):g.box((0,-.205+j*.137,.425),(w,.125,.07),'wood_light' if j%3 else 'wood')
    for xx in (-.91,.91):
        for yy in (-.20,.20):g.box((xx,yy,.195),(.10,.10,.39),'wood_dark')
        g.box((xx,0,.365),(.14,.56,.09),'wood_dark')
        g.tube([(xx,-.20,.42),(xx,-.24,.66),(xx,-.15,.72),(xx,.22,.72),(xx,.25,.93)],[.028]*5,'iron',12)
        g.tube([(xx,.21,.42),(xx,.25,.93)],[.03,.027],'iron',12)
        g.box((xx,.035,.729),(.09,.45,.05),'wood')
    for zz in (.64,.78,.92):g.box((0,.25,zz),(w,.09,.105),'wood')
    g.box((0,.20,.22),(1.91,.07,.08),'wood_dark')
    for xx in (-.91,.91):
        for zz in (.64,.78,.92):g.rounded_sphere((xx,.198,zz),(.017,.008,.017),'brass',8,5)
    o=_emit(g,col,name);o.location=(x,y,.22);o.rotation_euler.z=math.radians(yaw)
    c=G();c.box((0,.025,.23),(w,.58,.46));c.box((0,.25,.73),(w,.14,.52))
    co=_emit(c,col,'COL_'+name,True);co.location=(x,y,.22);co.rotation_euler.z=math.radians(yaw)
    return {'name':name,'position':[x,y,.22],'width':w,'seat_height_above_ground_m':.46,'collision':'COL_'+name}

def _planter(col,name,x,y):
    # A low timber planter: slatted sides, corner posts and a recessed soil bed.
    w,d=1.75,.70; g=G()
    g.box((x,y,.285),(w-.12,d-.12,.13),'wood_dark')
    for xx in (x-w/2+.09,x+w/2-.09):
        for yy in (y-d/2+.09,y+d/2-.09):
            g.box((xx,yy,.57),(.12,.12,.70),'wood')
    for zz in (.34,.57,.78):
        g.box((x,y-d/2+.045,zz),(w-.10,.075,.09),'wood_light')
        g.box((x,y+d/2-.045,zz),(w-.10,.075,.09),'wood_light')
        g.box((x-w/2+.045,y,zz),(.075,d-.10,.09),'wood_dark')
        g.box((x+w/2-.045,y,zz),(.075,d-.10,.09),'wood_dark')
    g.box((x,y,.585),(w-.22,d-.18,.50),'soil')
    # Individual stems, leaves and a few restrained flowers with real volume.
    palette=['leaf','leaf_light','leaf_dark','leaf_mid']
    for j in range(8):
        xx=x-.60+j*.17; yy=y+.08*math.sin(j*1.7); top=.98+.10*(j%3)
        g.tube([(xx,yy,.86),(xx+.025*math.sin(j),yy+.02,top)], [.018,.012], 'leaf_dark', 6)
        g.rounded_sphere((xx-.075,yy,top-.04),(.13,.07,.18),palette[j%4],8,5)
        if j in (1,4,7):
            g.rounded_sphere((xx,yy,top+.08),(.07,.07,.07),['flower','fruit_yellow','fruit_red'][j%3],8,5)
    o=_emit(g,col,name); c=G();c.box((x,y,.47),(1.82,.78,.62));_emit(c,col,'COL_'+name,True)
    return {'name':name,'position':[x,y],'collision':'COL_'+name}

def build(col):
    """Build fountain, two benches and five optional low planters."""
    fountain=_fountain(col)
    benches=[_bench(col,'tolbana_v5_bench_west',-15,-8,0),_bench(col,'tolbana_v5_bench_east',16,12,180)]
    planters=[_planter(col,'tolbana_v5_planter_%02d'%i,x,y) for i,(x,y) in enumerate([(-18,-8),(-17,13),(-1,13),(18,13),(17,-8)])]
    return {'fountain':fountain,'benches':benches,'planters':planters,'stone_paving_radius':4.5,'walkway_clearance_m':1.0}
