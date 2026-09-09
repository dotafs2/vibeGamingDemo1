"""Normal metre-scale architecture. No image coordinates or camera projection."""
import math,sys,random
from pathlib import Path
from mathutils import Vector,Matrix
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import build_reference_scenes_hq as hq
G=hq.G
R=random.Random(915)

def emit(g,col,name,bevel=.015,collision=False):
    ob=g.obj(name,col,bevel,collision);ob['style_id']='level0_market_craft_v5';return ob

def curved_infill(g,x,b,w,h,rise,depth,role):
    """Solid spandrel above an elliptical opening; front y=0, back y=depth."""
    spring=b+h-rise;r=w/2;count=40;vs=[]
    for y in (0,depth):
        for i in range(count+1):
            dx=-r+w*i/count;z=spring+rise*math.sqrt(max(0,1-(dx/r)**2));vs.extend([(x+dx,y,z),(x+dx,y,b+h)])
    layer=2*(count+1);fs=[]
    for i in range(count):
        a=i*2;c=a+2;fs += [(a,c,c+1,a+1),(layer+a+1,layer+c+1,layer+c,layer+a),(a,layer+a,layer+c,c),(a+1,c+1,layer+c+1,layer+a+1)]
    fs += [(0,1,layer+1,layer),(2*count,layer+2*count,layer+2*count+1,2*count+1)];g.mesh(vs,fs,role)

def wall(col,name,w,h,openings,role='plaster_cream',base=0,stone=False):
    shell=G();sol=G();holes=[(o['x']-o['w']/2,o['x']+o['w']/2,o['b'],o['b']+o['h']) for o in openings]
    for l,r,b,t in hq.rect_cut(-w/2,w/2,base,base+h,holes):
        shell.box(((l+r)/2,.20,(b+t)/2),(r-l,.40,t-b),role)
        sol.box(((l+r)/2,.20,(b+t)/2),(r-l,.40,t-b),'limestone')
    for o in openings:
        rise=o.get('rise',o['w']/2)
        if rise>0:curved_infill(shell,o['x'],o['b'],o['w'],o['h'],rise,.40,role);curved_infill(sol,o['x'],o['b'],o['w'],o['h'],rise,.40,role)
    emit(shell,col,name+'_wall',.012);emit(sol,col,'COL_'+name,0,True)
    if stone:
        skin=G();rh=.32;bw=.78
        for row in range(math.ceil(h/rh)):
            z0=base+row*rh+.012;z1=min(base+h-.012,z0+rh-.025)
            for j in range(math.ceil(w/bw)+2):
                x0=max(-w/2+.01,-w/2+j*bw-(row%2)*bw/2);x1=min(w/2-.01,x0+bw-.022)
                if x1-x0<.07:continue
                for l,r,b,t in hq.rect_cut(x0,x1,z0,z1,holes):
                    if min(r-l,t-b)<.02:continue
                    skin.box(((l+r)/2,-.012-R.uniform(0,.012),(b+t)/2),(r-l,.045,t-b),R.choice(['stone_light','limestone','limestone','limestone','stone_shade']))
        emit(skin,col,name+'_dressed_stone_courses',.012)

def arch_frame(g,o,stone='stone_light',trim=.14):
    x,b,w,h=o['x'],o['b'],o['w'],o['h'];rise=o.get('rise',w/2);spring=b+h-rise
    for s in (-1,1):
        g.box((x+s*(w/2+trim/2),-.07,(b+spring)/2),(trim,.23,spring-b),stone)
        g.box((x+s*(w/2+trim/2),-.12,spring-.045),(trim+.08,.29,.13),stone)
    if rise:
        n=max(15,round(w*10))
        for i in range(n):
            a=i*math.pi/n+.006;c=(i+1)*math.pi/n-.006;vs=[]
            for y in (-.16,.09):
                vs += [(x+w/2*math.cos(a),y,spring+rise*math.sin(a)),(x+w/2*math.cos(c),y,spring+rise*math.sin(c)),(x+(w/2+trim)*math.cos(c),y,spring+(rise+trim)*math.sin(c)),(x+(w/2+trim)*math.cos(a),y,spring+(rise+trim)*math.sin(a))]
            g.mesh(vs,[(0,1,2,3),(4,7,6,5),(0,4,5,1),(1,5,6,2),(2,6,7,3),(3,7,4,0)],stone)
    else:g.box((x,-.07,b+h+.07),(w+trim*2,.24,.14),stone)
    g.box((x,-.13,b-.055),(w+trim*2+.14,.45,.12),stone)

def window(g,o,shutters=True,flowerbox=False):
    x,b,w,h=o['x'],o['b'],o['w'],o['h'];rise=o.get('rise',w/2);spring=b+h-rise;r=w/2
    arch_frame(g,o)
    # Individual timber sash with a real inset, arched glass and transom.
    polygon=[(x-r,.14,b),(x+r,.14,b)]+[(x+r*math.cos(i*math.pi/24),.14,spring+rise*math.sin(i*math.pi/24)) for i in range(25)]
    g.mesh(polygon,[tuple(range(len(polygon)))],'glass')
    for dx in (-r,r):g.box((x+dx,.085,(b+spring)/2),(.055,.075,spring-b),'wood_dark')
    g.tube([(x+r*math.cos(i*math.pi/24),.085,spring+rise*math.sin(i*math.pi/24)) for i in range(25)],[.033]*25,'wood_dark',8)
    for z in (b+.05,b+h*.46,spring):g.box((x,.07,z),(w,.08,.055),'wood_light')
    g.box((x,.07,b+h/2),(.055,.08,h),'wood_light')
    if shutters:
        sw=w*.47;sh=h-rise-.1
        for side in (-1,1):
            panel=G();hingex=x+side*(r+.08)
            for j in range(7):panel.box((side*(sw*(j+.5)/7),0,sh/2),(sw/7-.009,.065,sh),'shutter' if j%3 else 'shutter_light')
            for z in (.09,sh-.09):panel.box((side*sw/2,-.043,z),(sw+.045,.05,.085),'wood_dark')
            for j in range(10):panel.box((side*sw/2,-.040,.18+j*(sh-.35)/10),(sw*.78,.055,.035),'shutter_light')
            for z in (.20,sh-.20):panel.box((side*sw*.25,-.075,z),(sw*.50,.035,.035),'iron')
            g.add(panel,(hingex,-.08,b+.06),side*22)
    if flowerbox:
        p=(x,-.43,b-.32);ww=w+.34
        for yy in (-.24,.24):
            for row in range(3):g.box((p[0],p[1]+yy,p[2]+row*.09),(ww,.06,.075),'wood')
        for side in (-1,1):g.box((x+side*ww/2,p[1],p[2]+.1),(.06,.51,.28),'wood_dark')
        g.box((x,p[1],p[2]+.21),(ww-.08,.42,.045),'soil')
        for j in range(9):
            xx=x-ww*.42+ww*.84*j/8;z=p[2]+.30+R.random()*.14
            for k in range(4):g.rounded_sphere((xx+R.uniform(-.1,.1),p[1]+R.uniform(-.18,.18),z),(.12,.08,.08),['leaf','leaf_light','leaf_dark'][k%3],8,5)
            if j%2==0:g.rounded_sphere((xx,p[1]-.06,z+.07),(.05,.05,.035),'flower',8,4)

def canopy(g,frame,x,width,attach=3.05,edge=2.55,projection=1.65,colors=('canvas_cream','canvas_green')):
    """Shared seams across physically connected roof and scalloped valance."""
    bays=6;nu=12;nv=18;top_y=-.10;edge_y=-projection
    def roof(s,t):
        xx=x-width/2+width*s;zz=attach*(1-t)+edge*t-.15*math.sin(t*math.pi)-.055*math.sin(s*bays*math.pi)**2*t
        return (xx,top_y+(edge_y-top_y)*t,zz)
    for bay in range(bays):
        s0=bay/bays;s1=(bay+1)/bays;vs=[]
        for j in range(nv+1):
            for i in range(nu+1):
                s=s0+(s1-s0)*i/nu;t=j/nv;vs.append(roof(s,t))
        g.mesh(vs,[(j*(nu+1)+i,j*(nu+1)+i+1,(j+1)*(nu+1)+i+1,(j+1)*(nu+1)+i) for j in range(nv) for i in range(nu)],colors[bay%len(colors)])
        vv=[]
        for j in range(7):
            t=j/6
            for i in range(nu+1):
                s=i/nu;p=Vector(roof(s0+(s1-s0)*s,1));p.z-=t*(.14+.16*math.sin(math.pi*s));p.y-=.015*math.sin(3*math.pi*s)*t;vv.append(p)
        g.mesh(vv,[(j*(nu+1)+i,j*(nu+1)+i+1,(j+1)*(nu+1)+i+1,(j+1)*(nu+1)+i) for j in range(6) for i in range(nu)],colors[bay%len(colors)])
        g.tube([roof(s0,t/24) for t in range(25)],[.007]*25,'canvas_cream',6)
    frame.beam((x-width/2,edge_y,edge-.07),(x+width/2,edge_y,edge-.07),.045,'wood_dark')
    for side in (-1,1):
        xx=x+side*(width/2-.05);frame.tube([(xx,edge_y,.05),(xx,edge_y,edge+.02)],[.038,.029],'wood_dark',12)
        frame.beam((xx,-.09,attach+.07),(xx,edge_y,edge+.02),.035,'iron')

def sign(g,x,z,kind):
    bracket=[(x,-.12,z+.6),(x,-.72,z+.6),(x,-1.3,z+.56)];g.tube(bracket,[.025]*3,'iron',10)
    g.tube([(x,-.18,z+.58),(x,-.37,z+.32),(x,-.85,z+.35),(x,-1.05,z+.58)],[.018]*4,'iron',10)
    # A small side-facing solid wood sign, with a modelled trade emblem.
    g.box((x,-.95,z),(.10,1.10,.58),'wood_dark')
    g.box((x+.059,-.95,z),(.025,.94,.44),'canvas_gold' if kind=='bakery' else 'canvas_green')
    for y in (-1.3,-.6):g.tube([(x,y,z+.3),(x,y,z+.55)],[.011]*2,'iron',8)
    if kind=='bakery':
        g.rounded_sphere((x+.09,-.95,z),(.04,.33,.14),'bread',20,8)
        g.box((x-.059,-.95,z),(.025,.94,.44),'canvas_gold')
        g.rounded_sphere((x-.09,-.95,z),(.04,.33,.14),'bread',20,8)
        for j in (-1,0,1):g.tube([(x+.133,-.95+j*.15-.04,z-.06),(x+.14,-.95+j*.15+.04,z+.065)],[.012,.012],'canvas_cream',6)
        for j in (-1,0,1):g.tube([(x-.133,-.95+j*.15-.04,z-.06),(x-.14,-.95+j*.15+.04,z+.065)],[.012,.012],'canvas_cream',6)
    else:
        g.box((x+.092,-.95,z),(.04,.34,.31),'canvas_cream');g.tube([(x+.12,-1.08,z+.11),(x+.12,-1.09,z+.22),(x+.12,-.82,z+.22),(x+.12,-.82,z+.11)],[.018]*4,'wood_dark',8)

def build_house(col,name,kind,w,d,h,position,yaw=0):
    before=set(col.objects)
    doorx=-2.9 if kind=='bakery' else 2.15;shopx=1.35 if kind=='bakery' else -1.2;shopw=3.8 if kind=='bakery' else 3.6
    door={'x':doorx,'b':0,'w':1.25,'h':2.6,'rise':.625};serving={'x':shopx,'b':.85,'w':shopw,'h':1.78,'rise':.44}
    windows=[{'x':x,'b':4.02,'w':1.05,'h':1.92,'rise':.525} for x in ([-3.1,0,3.1] if kind=='bakery' else [-2.4,0,2.4])]
    wall(col,name+'_ground',w,3.5,[door,serving],'limestone',stone=True)
    wall(col,name+'_upper',w,h-3.5,windows,'plaster_peach' if kind=='bakery' else 'plaster_cream',3.5)
    # Closed side/rear walls are full architectural surfaces, not image-aligned returns.
    for side in (-1,1):
        before_side=set(col.objects);sidewins=[{'x':x,'b':4.02,'w':.92,'h':1.8,'rise':.46} for x in (-d*.27,d*.27)]
        wall(col,name+'_side_'+str(side),d,h,sidewins,'plaster_peach' if kind=='bakery' else 'plaster_cream')
        trim=G()
        for o in sidewins:window(trim,o,True)
        emit(trim,col,name+'_side_windows_'+str(side),.012)
        transform=Matrix.Translation(Vector((side*w/2,d/2,0)))@Matrix.Rotation(math.radians(90 if side>0 else -90),4,'Z')
        for ob in set(col.objects)-before_side:ob.matrix_world=transform@ob.matrix_world
    before_back=set(col.objects);backwins=[{'x':x,'b':4.02,'w':1,'h':1.8,'rise':.5} for x in (-w*.3,w*.3)]
    wall(col,name+'_rear',w,h,backwins,'plaster_cream');bg=G()
    for o in backwins:window(bg,o,False)
    emit(bg,col,name+'_rear_windows',.01)
    for ob in set(col.objects)-before_back:ob.matrix_world=Matrix.Translation(Vector((0,d,0)))@Matrix.Rotation(math.pi,4,'Z')@ob.matrix_world
    g=G();arch_frame(g,door,trim=.18);arch_frame(g,serving,trim=.18)
    for i,o in enumerate(windows):window(g,o,True,i!=1)
    # Ground floor spring capitals, wraparound mouldings, alternating corner quoins.
    for z,width,thick in [(.20,.17,.24),(3.46,.22,.19),(3.67,.12,.10),(h-.13,.26,.23)]:
        for yy in (-.06,d+.06):g.box((0,yy,z),(w+.23,width+.27,thick),'stone_light')
        for xx in (-w/2,w/2):g.box((xx,d/2,z),(width+.27,d+.30,thick),'stone_light')
    for side in (-1,1):
        for j in range(math.ceil(h/.40)):
            ww=.52 if j%2 else .34;zz=(j+.5)*.40
            for yy in (0,d):g.box((side*(w/2-ww/2),yy,zz),(ww,.53,.37),'stone_light')
    if kind=='bakery':
        # Habitable roof terrace with coping and a small tiled stair enclosure.
        g.box((0,d/2,h),(w+.16,d+.16,.20),'stone_light')
        for yy in (.12,d-.12):g.box((0,yy,h+.38),(w,.25,.76),'plaster_peach');g.box((0,yy,h+.80),(w+.13,.37,.12),'tile_red')
        for xx in (-w/2+.12,w/2-.12):g.box((xx,d/2,h+.38),(.25,d,.76),'plaster_peach');g.box((xx,d/2,h+.80),(.37,d+.10,.12),'tile_red')
        rr=G();rr.box((0,1.30,h+.80),(3.2,2.6,1.6),'plaster_cream');hq.curved_roof(rr,3.2,2.6,h+1.6,.78);g.add(rr,(-w*.22,d-3.15,0))
        g.box((-2.5,d-1.10,h+1.65),(.58,.60,3.30),'limestone')
        for j in range(10):g.box((-2.5,d-1.10,h+.25+j*.30),(.63,.65,.05),'stone_light')
        g.box((-2.5,d-1.10,h+3.32),(.75,.78,.15),'stone_light');g.box((-2.5,d-1.10,h+3.405),(.41,.43,.025),'recess')
        # Long shallow pitched tiled cover over the front moulding.
        r=G();hq.curved_roof(r,1.5,w+.55,h+.18,.35);g.add(r,(w/2+.27,-.16,0),90)
    else:
        hq.hip_roof(g,w,d,h,2.25)
        g.box((w*.25,d*.62,h+1.8),(.70,.76,2.5),'limestone')
        for j in range(8):g.box((w*.25,d*.62,h+.8+j*.28),(.78,.84,.05),'stone_light')
        g.box((w*.25,d*.62,h+3.06),(.93,.98,.16),'stone_light');g.box((w*.25,d*.62,h+3.15),(.53,.56,.018),'recess')
    # Ground threshold is flush enough to enter without a jump.
    g.box((doorx,-.24,.035),(1.66,.55,.07),'stone_light')
    sign(g,-w/2+.58,3.13,kind)
    emit(g,col,name+'_stonework_windows_roof_joinery',.013)
    fabric=G();fabric.smoothing=True;frame=G();canopy(fabric,frame,shopx,shopw+.6,3.04,2.55,1.58,('canvas_cream','canvas_green') if kind=='bakery' else ('canvas_gold','canvas_cream'))
    cloth=emit(fabric,col,name+'_woven_awning',0);sol=cloth.modifiers.new('Woven cloth thickness','SOLIDIFY');sol.thickness=.018
    emit(frame,col,name+'_awning_frame',.006)
    floors=G();floors.box((0,d/2,-.03),(w,d,.1),'wood');floors.box((0,d/2,3.46),(w,d,.12),'wood_dark');emit(floors,col,name+'_floor_and_ceiling',.004)
    emit(floors,col,'COL_'+name+'_floor_ceiling',0,True)
    import interiors
    result=interiors.build(col,name,kind)
    transform=Matrix.Translation(Vector(position))@Matrix.Rotation(math.radians(yaw),4,'Z')
    for ob in set(col.objects)-before:ob.matrix_world=transform@ob.matrix_world
    return {'id':name,'kind':kind,'position':list(position),'yaw':yaw,'dimensions_m':[w,d,h], 'door_world':list(transform@Vector((doorx,0,.1))), 'inside_world':list(transform@Vector((doorx,2.8,.1))), 'counter_world':list(transform@Vector((shopx,-.8,1.12))), 'fittings':result}
