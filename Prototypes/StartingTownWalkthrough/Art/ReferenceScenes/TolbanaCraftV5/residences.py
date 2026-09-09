"""Complete Tolbana houses in metres; animation language, project-authored interiors."""
import math, random
from mathutils import Matrix, Vector
import architecture as a
import build_reference_scenes_hq as hq
import interiors
G=hq.G
R=random.Random(9529)

def lamp(g,x,y,z):
    g.tube([(x,y,z+.7),(x,y-.46,z+.7),(x,y-.46,z+.48)],[.023]*3,'iron',10)
    g.box((x,y-.46,z+.20),(.25,.25,.43),'glass_light')
    for dx in (-.14,.14):
        for dy in (-.14,.14):g.box((x+dx,y-.46+dy,z+.20),(.023,.023,.46),'iron')
    g.curved((x,y-.46,z+.43),[(.23,0),(0,.19)],'iron',4)
    g.box((x,y-.46,z-.04),(.33,.33,.06),'iron')

def chair(g,c,p,yaw=0):
    q=G();sol=G()
    for xx in (-.22,.22):
        for yy in (-.21,.21):q.box((xx,yy,.225),(.065,.065,.45),'wood_dark')
    for j in range(5):q.box((-.22+j*.11,0,.47),(.10,.52,.07),'wood')
    for xx in (-.22,.22):q.box((xx,.23,.73),(.065,.065,.57),'wood_dark')
    for zz in (.74,.93):q.box((0,.23,zz),(.50,.065,.12),'wood_light')
    for xx in (-.21,.21):q.box((xx,0,.20),(.04,.44,.05),'wood')
    sol.box((0,0,.29),(.50,.52,.58));g.add(q,p,yaw);c.add(sol,p,yaw)

def hall(col,name,w,d):
    g=G();c=G()
    # A wide entrance lane remains open through the dining room.
    for x,y in [(-2.20,3.2),(2.18,3.65),(-2.15,6.2)]:
        for j in range(7):g.box((x+(j-3)*.21,y,.79),(.20,1.05,.09),'wood' if j%3 else 'wood_light')
        for dx in (-.57,.57):
            for dy in (-.36,.36):g.box((x+dx,y+dy,.385),(.105,.105,.77),'wood_dark')
        g.box((x,y,.30),(1.15,.075,.08),'wood_dark');c.box((x,y,.43),(1.49,1.07,.86))
        chair(g,c,(x,y-.88,.01),180);chair(g,c,(x,y+.88,.01))
        for dx in (-.39,.38):
            g.curved((x+dx,y,.836),[(.14,0),(.20,.022),(.21,.042),(.17,.048)],'canvas_cream',32)
            interiors.loaf(g,(x+dx,y,.878),.24)
        interiors.jar(g,(x,y+.29,.84),.32)
    # Rear bar, shelf contents physically resting on boards.
    g.box((1.55,d-1.50,.52),(3.45,.73,1.04),'wood_dark')
    for j in range(18):g.box((-.11+j*.195,d-1.90,.51),(.185,.065,.95),'wood')
    g.box((1.55,d-1.5,1.09),(3.65,.93,.13),'wood_light');c.box((1.55,d-1.50,.565),(3.65,.93,1.13))
    for z in (.80,1.45,2.10):
        g.box((1.55,d-.58,z),(3.6,.45,.09),'wood')
        for j in range(7):interiors.jar(g,(-.02+j*.50,d-.58,z+.045),.28+(j%3)*.08)
    for xx in (-.3,3.4):g.box((xx,d-.56,1.25),(.10,.48,2.25),'wood_dark')
    for xx in (.6,2.1):interiors.jar(g,(xx,d-1.51,1.155),.44)
    # Hearth with an actual recessed opening and chimney above it.
    for xx in (-3.20,-1.60):g.box((xx,d-.85,.90),(.38,1.05,1.8),'limestone')
    g.box((-2.4,d-.85,1.73),(2.05,1.18,.28),'stone_light')
    g.box((-2.4,d-.20,1.0),(1.50,.30,2.0),'recess')
    g.box((-2.4,d-.85,2.38),(1.5,.9,1.12),'plaster_cream')
    g.box((-2.4,d-1.07,.07),(2.25,1.50,.14),'stone_light')
    for j in range(4):g.tube([(-2.82+j*.25,d-1.14,.18),(-2.78+j*.25,d-.50,.26)],[.075,.061],'bark',12)
    c.box((-2.4,d-.7,1.40),(2.20,1.45,2.80))
    for y in (2.0,5.0):
        # Ceiling joists and linked pendant lanterns, no floating objects.
        g.box((0,y,3.37),(w-.65,.18,.20),'wood_dark')
        g.tube([(0,y,3.28),(0,y,2.79)],[.009,.009],'iron',8)
        g.box((0,y,2.59),(.25,.25,.36),'canvas_gold')
        for dx in (-.14,.14):
            for dy in (-.14,.14):g.box((dx,y+dy,2.59),(.025,.025,.39),'iron')
    for j in range(math.ceil(w/.22)):
        x=-w/2+.41+j*.22
        if x<w/2-.3:g.box((x,d/2,.008),(.21,d-.8,.024),'wood' if j%4 else 'wood_light')
    a.emit(g,col,name+'_hall_tables_hearth_crockery',.007);a.emit(c,col,'COL_'+name+'_hall_furniture',0,True)

def build(col,name,w,d,h,p,role,roof='cross',doorx=0,enterable=False,balcony=False):
    before=set(col.objects);g=G()
    door={'x':doorx,'b':0,'w':1.42 if enterable else 1.55,'h':2.72,'rise':.71 if enterable else .65}
    windows=[{'x':x,'b':.93,'w':.90,'h':1.55,'rise':.45} for x in (-w*.30,w*.30)]
    for opening in windows:
        side=-1 if opening['x']<doorx else 1
        clearance=door['w']/2+opening['w']/2+.49
        if abs(opening['x']-doorx)<clearance:opening['x']=doorx+side*clearance
    windows += [{'x':x,'b':4.08,'w':.96,'h':1.66,'rise':.48 if enterable else 0} for x in (-w*.30,0,w*.30)]
    if h>9:windows += [{'x':x,'b':7.45,'w':.82,'h':1.69,'rise':.41} for x in (-w*.27,w*.27)]
    a.wall(col,name+'_front',w,h,[door]+windows,role)
    a.arch_frame(g,door,trim=.20)
    for i,o in enumerate(windows):a.window(g,o,i%4!=1,i%3==0 and o['b']>3)
    # Wrap both side facades, including ground floor windows and real reveals.
    for side in (-1,1):
        fresh=set(col.objects);sidewins=[]
        for zz in [.95,4.08]+([7.45] if h>9 else []):
            sidewins += [{'x':x,'b':zz,'w':.85,'h':1.54,'rise':.425} for x in (-d*.28,d*.28)]
        a.wall(col,name+'_side_'+str(side),d,h,sidewins,role);sg=G()
        for i,o in enumerate(sidewins):a.window(sg,o,i%2==0)
        a.emit(sg,col,name+'_side_joinery_'+str(side),.009)
        trans=Matrix.Translation(Vector((side*w/2,d/2,0)))@Matrix.Rotation(math.radians(90 if side>0 else -90),4,'Z')
        for ob in set(col.objects)-fresh:ob.matrix_world=trans@ob.matrix_world
    fresh=set(col.objects);rearwins=[{'x':x,'b':4.08,'w':.88,'h':1.54,'rise':.44} for x in (-w*.29,w*.29)]
    a.wall(col,name+'_back',w,h,rearwins,role);rg=G()
    for o in rearwins:a.window(rg,o,True)
    a.emit(rg,col,name+'_rear_joinery',.009)
    for ob in set(col.objects)-fresh:ob.matrix_world=Matrix.Translation(Vector((0,d,0)))@Matrix.Rotation(math.pi,4,'Z')@ob.matrix_world
    # Ground stone socle is interrupted at the door; horizontal mouldings continue around corners.
    for l,r,b,t in hq.rect_cut(-w/2,w/2,0,.52,[(doorx-door['w']/2,doorx+door['w']/2,0,2.8)]):
        for j in range(max(1,math.ceil((r-l)/.72))):
            n=math.ceil((r-l)/.72);g.box((l+(j+.5)*(r-l)/n,-.05,.26),((r-l)/n-.015,.20,.50),'limestone')
    for zz,th,pr in [(3.52,.16,.17),(3.7,.08,.10),(h-.18,.22,.25)]+([(7.08,.14,.16)] if h>9 else []):
        for yy in (-.06,d+.06):g.box((0,yy,zz),(w+.3,.3+pr,th),'stone_light')
        for xx in (-w/2,w/2):g.box((xx,d/2,zz),(.3+pr,d+.32,th),'stone_light')
    for xx in (-w/2,w/2):
        for yy in (0,d):
            for j in range(math.floor(h/.42)):
                ww=.50 if j%2 else .32;g.box((xx-math.copysign(ww/2-.06,xx),yy,(j+.5)*.42),(ww,.48,.39),'limestone')
    # Eave brackets are attached under the wall plate, not decorative floating teeth.
    for j in range(math.floor(w/.65)):
        x=-w/2+.40+j*.65
        for yy in (-.16,d+.16):g.box((x,yy,h-.43),(.12,.40,.26),'wood')
    if roof=='gable':
        rise=w*.43;hq.curved_roof(g,w,d,h,rise)
        for yy in (0,d):
            vs=[(x,y,z) for y in (yy-.05,yy+.24) for x,z in [(-w/2,h),(w/2,h),(0,h+rise)]]
            g.mesh(vs,[(0,2,1),(3,4,5),(0,1,4,3),(1,2,5,4),(2,0,3,5)],role)
        o={'x':0,'b':h+.57,'w':.66,'h':1.15,'rise':.33};attic=G();a.window(attic,o,False);g.add(attic,(0,-.23,0))
    else:
        rise=2.15;rr=G();hq.curved_roof(rr,d,w,h,rise);g.add(rr,(w/2,d/2,0),90)
        for xx in (-w/2,w/2):
            vs=[(x,y,z) for x in (xx-.1,xx+.1) for y,z in [(0,h),(d,h),(d/2,h+rise)]]
            g.mesh(vs,[(0,2,1),(3,4,5),(0,1,4,3),(1,2,5,4),(2,0,3,5)],role)
    chimneyx=-2.4 if enterable else w*.24;chimneyy=d-.70 if enterable else d*.63
    # Open chimney cap, matching the inn hearth position.
    g.box((chimneyx,chimneyy,h+1.55),(.75,.80,3.10),'limestone')
    for j in range(10):g.box((chimneyx,chimneyy,h+.19+j*.30),(.80,.85,.045),'stone_light')
    g.box((chimneyx,chimneyy,h+3.17),(.96,1.0,.15),'stone_light');g.box((chimneyx,chimneyy,h+3.25),(.56,.61,.022),'recess')
    if enterable:
        interiors.open_doors(g,G(),doorx,door['w'])
        # Door leaf collision uses the same actual transformed panels as the reusable helper.
        dg=G();dc=G();interiors.open_doors(dg,dc,doorx,door['w']);a.emit(dc,col,'COL_'+name+'_open_doors',0,True)
        # An icon rather than fake pseudo-letter strokes.
        lamp(g,doorx-1.35,-.12,2.0)
        sign=G();sign.box((0,0,0),(1.8,.13,.58),'wood_dark');sign.box((0,-.08,0),(1.65,.04,.45),'canvas_gold')
        sign.box((0,-.115,-.02),(.80,.05,.08),'canvas_cream')
        for xx in (-.40,.40):sign.box((xx,-.115,-.04),(.06,.05,.29),'canvas_cream')
        sign.box((-.21,-.12,.08),(.28,.045,.14),'canvas_cream');g.add(sign,(doorx,-.14,3.13))
        hall(col,name,w,d)
    else:
        hq.arched_door(g,doorx,door['w'],door['h'],False)
        lamp(g,doorx+1.18,-.1,1.96)
        closed=G();closed.box((doorx,.12,1.34),(door['w'],.18,2.68));a.emit(closed,col,'COL_'+name+'_door_closed',0,True)
    if balcony:
        bx=0;by=-.64;bz=3.9;bw=3.0
        g.box((bx,by,bz),(bw,1.25,.17),'stone_light')
        for x in (-1.15,1.15):g.beam((x,-.07,3.0),(x,-1.15,3.82),.12,'wood_dark')
        for j in range(16):g.box((-1.44+j*.192,-1.22,4.34),(.028,.03,.80),'iron')
        for z in (3.96,4.77):g.box((0,-1.22,z),(3.04,.055,.06),'iron')
        for x in (-1.5,1.5):
            for j in range(6):g.box((x,-.1-j*.22,4.34),(.03,.028,.80),'iron')
            g.box((x,-.64,4.77),(.055,1.25,.06),'iron')
    # Small entrance canopy and real rafters on the low white house.
    if name.endswith('WhiteResidence'):
        rr=G();hq.curved_roof(rr,2.45,1.5,2.8,.68);g.add(rr,(doorx,-1.60,0))
        for dx in (-1.10,1.10):
            g.box((doorx+dx,-1.47,1.40),(.11,.11,2.8),'wood_dark');g.box((doorx+dx,-1.47,.12),(.25,.25,.24),'stone_light')
            g.beam((doorx+dx,-1.47,2.23),(doorx+dx,-.20,2.77),.07,'wood')
    # Ground slab and ceiling give closed and enterable houses true volume.
    floors=G();floors.box((0,d/2,-.06),(w,d,.12),'wood');floors.box((0,d/2,3.50),(w,d,.13),'wood_dark')
    a.emit(floors,col,name+'_floors',.006);a.emit(floors,col,'COL_'+name+'_floors',0,True)
    g.box((doorx,-.23,.025),(door['w']+.34,.55,.05),'stone_light')
    a.emit(g,col,name+'_crafted_roof_sash_stone_details',.010)
    trans=Matrix.Translation(Vector(p))
    for ob in set(col.objects)-before:ob.matrix_world=trans@ob.matrix_world
    return {'id':name,'dimensions_m':[w,d,h],'position':p,'enterable':enterable,'door_world':[p[0]+doorx,p[1],p[2]+.06],'inside_world':[p[0]+doorx,p[1]+3,p[2]+.06],'use':'project_infill_inn_dining_hall' if enterable else 'project_infill_residence'}
