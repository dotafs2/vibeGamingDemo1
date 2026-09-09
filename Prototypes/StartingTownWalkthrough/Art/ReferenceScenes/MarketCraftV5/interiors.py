"""Supported goods, real joinery and clear circulation for the two shops."""
import math
from mathutils import Vector
import architecture as arch
G=arch.G

def loaf(g,p,scale=1,kind='loaf'):
    x,y,z=p;s=scale;length=.31*s if kind=='loaf' else .18*s;rise=.14*s
    g.rounded_sphere((x,y,z+rise),(length,.16*s,rise),'bread',24,12)
    # Scored crust follows the actual domed surface, not floating decoration.
    for offset in ([-.15,0,.15] if kind=='loaf' else [-.05,.05]):
        pts=[]
        for i in range(13):
            xx=(offset+(i/12-.5)*.055)*s;yy=(i/12-.5)*.20*s
            zz=z+rise+rise*math.sqrt(max(0,1-(xx/length)**2-(yy/(.16*s))**2))+.002
            pts.append((x+xx,y+yy,zz))
        g.tube(pts,[.006*s]*len(pts),'bread_light',5)

def jar(g,p,scale=1):
    x,y,z=p;s=scale;anchors=[(.14,0),(.22,.09),(.25,.34),(.20,.52),(.11,.60),(.11,.67),(.15,.70),(.15,.73),(.10,.73),(.10,.66)];profile=[]
    for (r0,z0),(r1,z1) in zip(anchors,anchors[1:]):
        for j in range(6):
            t=j/6;blend=t*t*(3-2*t);profile.append(((r0*(1-blend)+r1*blend)*s,(z0*(1-t)+z1*t)*s))
    profile.append((anchors[-1][0]*s,anchors[-1][1]*s));g.curved(p,profile,'pottery',48)
    g.curved((x,y,z+.742*s),[(.102*s,0),(.13*s,.03*s),(.115*s,.07*s)],'wood',24)
    for side in (-1,1):g.tube([(x+side*(.20+.12*math.sin(math.pi*j/20))*s,y,z+(.50-.24*j/20)*s) for j in range(21)],[.027*s]*21,'pottery',10)

def sack(g,p,s=.8):
    x,y,z=p;g.rounded_sphere((x,y,z+.34*s),(.28*s,.24*s,.34*s),'canvas_cream',20,12)
    g.curved((x,y,z+.55*s),[(.16*s,0),(.095*s,.09*s),(.13*s,.15*s)],'canvas_cream',24)
    for i in range(3):g.curved((x,y,z+.63*s+i*.012),[(.11*s,0),(.11*s,.012)],'wood',24)

def open_doors(g,c,x,width=1.25):
    leaf=width*.49;height=1.94
    for side in (-1,1):
        panel=G();center=-side*leaf/2
        for j in range(6):panel.box((-side*(j+.5)*leaf/6,0,height/2),(leaf/6-.005,.082,height),'wood')
        for zz in (.14,height-.14):panel.box((center,-.055,zz),(leaf+.035,.052,.13),'wood_dark')
        panel.beam((-side*.06,-.053,.2),(-side*(leaf-.06),-.053,height-.2),.065,'wood_light',.04)
        for zz in (.28,1.55):
            panel.box((-side*leaf*.3,-.094,zz),(leaf*.6,.032,.045),'iron')
            for xx in (leaf*.12,leaf*.36):panel.rounded_sphere((-side*xx,-.116,zz),(.013,.008,.013),'brass',8,5)
            panel.tube([(0,0,zz-.07),(0,0,zz+.07)],[.022,.022],'iron',10)
        panel.tube([(-side*leaf*.82,-.08,.9),(-side*leaf*.82,-.16,.96),(-side*leaf*.82,-.16,1.10),(-side*leaf*.82,-.08,1.15)],[.014]*4,'iron',10)
        g.add(panel,(x+side*width/2,-.04,.02),side*95)
        slab=G();slab.box((center,0,height/2),(leaf,.11,height));c.add(slab,(x+side*width/2,-.04,.02),side*95)
    # Stationary arched fanlight above the two timber leaves.
    g.box((x,.10,2.0),(width,.10,.09),'wood_dark')
    for j in range(1,8):
        a=j*math.pi/8;g.tube([(x,.11,2.0),(x+width*.49*math.cos(a),.11,1.975+.61*math.sin(a))],[.017,.017],'wood_dark',8)

def shelf(g,c,x,y,width=4.6,depth=.55):
    for xx in (-width/2+.065,width/2-.065):
        g.box((x+xx,y,1.16),(.13,depth,2.32),'wood_dark')
        for z in (.35,1.04,1.73):g.box((x+xx,y-.015,z),(.19,depth+.07,.08),'wood_light')
    levels=[.41,1.10,1.79]
    for z in levels:
        for j in range(3):g.box((x,y-depth/2+(j+.5)*depth/3,z-.055),(width+.08,depth/3-.008,.11),'wood_light')
        g.box((x,y-depth/2-.02,z-.065),(width,.04,.15),'wood')
    for i in range(11):g.box((x-width/2+(i+.5)*width/11,y+depth/2,1.13),(width/11-.012,.065,2.25),'wood')
    c.box((x,y,1.13),(width+.15,depth+.12,2.26))
    return levels

def build(col,prefix,kind='bakery'):
    g=G();c=G();floor=G();bakery=kind=='bakery';width=9.8 if bakery else 7.8;depth=7.3 if bakery else 6.5
    door=-2.9 if bakery else 2.15;cx=1.35 if bakery else -1.2;cw=3.8 if bakery else 3.6
    open_doors(g,c,door)
    # 1.14m counter with boards, cross rails and a supported display tray.
    for j in range(3):g.box((cx,-.38+j*.20,1.075),(cw+.1,.191,.13),'wood_light')
    for xx in (-cw/2+.14,cw/2-.14):
        for yy in (-.40,.16):g.box((cx+xx,yy,.55),(.12,.12,1.10),'wood_dark')
    for zz in (.26,.82):g.box((cx,-.39,zz),(cw,.065,.095),'wood')
    c.box((cx,-.20,.54),(cw,.7,1.08))
    for xx in (-cw/2,cw/2):g.box((cx+xx,-.15,1.19),(.055,.63,.12),'wood_dark')
    g.box((cx,.17,1.19),(cw,.045,.12),'wood_dark')
    # Every product's input z is the support surface, not its centre.
    if bakery:
        for j in range(6):loaf(g,(cx-cw*.38+j*cw*.145,-.17,1.14),.72)
    else:
        for j,color in enumerate(['canvas_green','canvas_gold','canvas_red','canvas_cream']):
            xx=cx-.95+j*.62;g.curved((xx,-.12,1.14),[(.15,0),(.15,.44)],color,32)
            g.curved((xx,-.12,1.585),[(.10,0),(.10,.004)],'canvas_cream',24)
    sx=1.40 if bakery else -1.05;sy=depth-.57;sw=4.9 if bakery else 4.6
    levels=shelf(g,c,sx,sy,sw)
    for row,z in enumerate(levels):
        for j in range(7):
            x=sx-sw*.40+j*sw*.13;y=sy-.06
            if bakery:
                if row<2:loaf(g,(x,y,z),.70,'round' if j%3 else 'loaf')
                else:jar(g,(x,y,z),.60)
            elif row==2:jar(g,(x,y,z),.55)
            else:
                s=.38;g.box((x,y,z+.22),(.32,.26,.44),'canvas_green' if j%2 else 'canvas_gold')
                for side in (-1,1):g.tube([(x+side*.1,y-.15,z+.08),(x+side*.1,y-.19,z+.34),(x+side*.1,y-.13,z+.46)],[.016]*3,'wood_dark',8)
                g.box((x,y-.147,z+.28),(.22,.025,.12),'wood');g.box((x,y-.165,z+.28),(.04,.014,.045),'brass')
    # Side preparation table with braces; bags rest on floor beside it.
    tx=3.65 if bakery else -2.75;ty=3.35
    g.box((tx,ty,.95),(1.55,2.6,.13),'wood_light');c.box((tx,ty,.46),(1.55,2.6,.92))
    for dx in (-.66,.66):
        for dy in (-1.18,1.18):g.box((tx+dx,ty+dy,.45),(.13,.13,.90),'wood_dark')
    g.box((tx,ty,.29),(1.35,2.45,.11),'wood')
    for j in range(3):
        if bakery:jar(g,(tx,ty-.7+j*.65,1.015),.68)
        else:
            for k in range(5):
                radius=.18+.018*k;g.tube([(tx+radius*math.cos(t*math.tau/48),ty-.7+j*.68+radius*math.sin(t*math.tau/48),1.027+k*.005) for t in range(49)],[.012]*49,'wood_light',6)
    for j in range(3):sack(g,(sx-1.0+j*.58,depth-1.5,.072),.85)
    if bakery:
        # A compact masonry oven and preparation tools give the interior purpose.
        ox=-2.5;oy=depth-1.35
        for side in (-1,1):g.box((ox+side*.64,oy,.48),(.32,1.45,.96),'limestone')
        g.box((ox,oy,.98),(1.95,1.7,.16),'stone_light');g.rounded_sphere((ox,oy,1.28),(.93,.75,.65),'pottery',40,20)
        g.arch_fill(ox,oy-.755,1.07,.82,.57,'recess');g.arch(ox,oy-.81,1.23,.41,.12,.18,'stone_light',24)
        for side in (-1,1):g.box((ox+side*.47,oy-.80,1.15),(.12,.18,.23),'stone_light')
        g.curved((ox,oy+.25,1.78),[(.16,0),(.16,1.68)],'iron',24);c.box((ox,oy,.90),(1.95,1.7,1.80))
        g.box((tx,ty-.78,1.04),(1.08,.63,.05),'wood');loaf(g,(tx-.20,ty-.8,1.065),.78)
        g.tube([(tx+.22,ty-.97,1.09),(tx+.22,ty-.55,1.09)],[.06,.06],'wood_light',16)
        for j in range(7):g.tube([(ox-.4+j*.12,oy-.52,.18),(ox-.4+j*.12,oy+.47,.18)],[.055,.055],'wood',12)
    else:
        for row in range(3):
            for j in range(3):g.box((tx,ty-.7+j*.65,1.035+row*.043),(.52,.43,.04),['canvas_green','canvas_cream','canvas_red'][row])
    for i in range(math.ceil(depth/.22)):
        y=.40+(i+.5)*.22
        if y>depth-.2:break
        for j in range(3):floor.box((-width*.475+(j+.5)*width*.95/3,y,.025),(width*.95/3-.01,.211,.05),'wood_light' if (i+j)%4 else 'wood')
    arch.emit(floor,col,prefix+'_interior_floorboards',.004);arch.emit(g,col,prefix+'_joinery_and_supported_goods',.008);arch.emit(c,col,'COL_'+prefix+'_shop_fittings',0,True)
    return {'kind':kind,'door_clear_width_m':1.14,'counter_top_m':1.14,'shelf_top_heights_m':levels,'goods_positions_use_support_surface':True,'source':'root interiors.py; prior shop_fittings.py not used'}
