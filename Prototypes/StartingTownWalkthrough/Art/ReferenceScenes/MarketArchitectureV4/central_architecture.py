"""Root-authored reference geometry. No anime artwork is embedded in the asset."""
import math
import v4_geometry as geo
G=geo.G
def at(x,y,depth):return geo.project_point(x/2560,y/1440,depth)

def bridge(col,measurements):
    depth=23.; bottom=geo.screen_px((at(1380,737,depth).x,depth,.24))[1]
    opening=[(1280,bottom+5),(1480,bottom+5),(1480,737)]+[(1380+100*math.cos(a*math.pi/32),737-80*math.sin(a*math.pi/32)) for a in range(33)]
    def intrinsic(px):p=at(*px,depth);return(p.x,p.z)
    def xyz(p,d=0):return(p[0],depth-d,p[1])
    outer=[intrinsic(p) for p in [(930,567),(1530,540),(1530,bottom),(930,bottom)]];hole=[intrinsic(p) for p in opening]
    g=G();c=G();geo.extrude_shape(g,outer,[hole],xyz,0,-2.35,'limestone');geo.extrude_shape(c,outer,[hole],xyz,0,-2.35,'limestone')
    geo.emit(g,col,'V4_bridge_cut_arch');geo.emit(c,col,'COL_V4_bridge',0,True)
    det=G()
    # Shallow individual stone faces, clipped to the actual open arch.
    # This is geometry relief; no lighting/postprocess change is involved.
    for row in range(24):
        z=.24+row*.54
        for j in range(-15,16):
            x=(j+(row%2)*.5)*1.15
            tile=[(x+.018,z+.015),(x+1.132,z+.015),(x+1.132,z+.522),(x+.018,z+.522)]
            if all(geo.contains(outer,p) for p in tile) and not any(geo.contains(hole,p) for p in tile) and not any(geo.contains(tile,p) for p in hole):
                role='limestone' if (j*17+row*13)%9 else 'stone_light'
                geo.extrude_shape(det,tile,[],xyz,.012+((j+row)%3)*.004,-.025,role)
    for row in range(24):
        z=.24+row*.54
        for a,b in geo.cut_segment((-15,z),(15,z),outer,[hole]):det.tube([xyz(a,.009),xyz(b,.009)],[.009]*2,'mortar',4)
        for j in range(-15,16):
            x=(j+(row%2)*.5)*1.15
            for a,b in geo.cut_segment((x,z),(x,z+.54),outer,[hole]):det.tube([xyz(a,.009),xyz(b,.009)],[.009]*2,'mortar',4)
    for cx in (1120,1163,1207):
        w=[(cx-8,641),(cx-6,633),(cx+6,633),(cx+8,641),(cx+8,662),(cx-8,662)]
        det.mesh([at(*p,depth-.024) for p in w],[tuple(range(6))],'recess')
    for i in range(27):
        a=i*math.pi/27+.012;b=(i+1)*math.pi/27-.012
        px=[(1380+rx*math.cos(t),737-ry*math.sin(t)) for rx,ry,t in [(100,80,a),(100,80,b),(118,101,b),(118,101,a)]]
        vs=[at(*p,depth-.07) for p in px]+[at(*p,depth+.20) for p in px]
        det.mesh(vs,[(0,1,2,3),(0,4,5,1),(1,5,6,2),(2,6,7,3),(3,7,4,0)],'arch_stone' if i%3 else 'stone_light')
    for i in range(24):
        x=935+i*24;v=567-(x-930)*27/600;p=at(x,v,depth+.18);det.box((p.x,p.y,p.z+.075),(.14,.42,.15),'limestone')
    geo.emit(det,col,'V4_bridge_courses_voussoirs_and_slits',.003)
    measurements.append({'id':'bridge_opening','kind':'open_arch','reference_px':opening[2:],'points_blender_m':[list(at(*p,depth)) for p in opening[2:]]})
    return {'gate_opening_crown':list(at(1380,657,depth)),'gate_opening_left_spring':list(at(1280,737,depth)),'gate_opening_right_spring':list(at(1480,737,depth))}

def tower(col,measurements):
    y=26.;apex=at(1554,105,y);cx=apex.x;r=(at(1630,475,y).x-at(1490,475,y).x)*.5
    balcony=(at(1665,403,y).x-at(1450,403,y).x)*.5
    lower=at(1554,471,y-r).z;floor=at(1554,439,y-balcony).z;railtop=at(1554,403,y-balcony).z;top=at(1554,290,y).z
    g=G();g.curved((cx,y,.24),[(r,0),(r,lower-.24)],'limestone',96)
    g.curved((cx,y,lower),[(r,0),(r+.15,.12),(balcony-.12,.32),(balcony,.43),(balcony,.56)],'stone_light',96)
    for j in range(40):
        a=j*math.tau/40;g.box((cx+(r+.018)*math.cos(a),y+(r+.018)*math.sin(a),lower-.10),(.10,.10,.16),'recess')
    g.curved((cx,y,floor),[(balcony,0),(balcony,.10),(balcony-.08,.17)],'stone_light',96)
    for zz in (floor+.18,railtop):g.curved((cx,y,zz),[(balcony-.09,0),(balcony-.09,.065)],'rail_red',96)
    for j in range(64):
        a=j*math.tau/64;b=(j+1)*math.tau/64
        for lo,hi in [(floor+.20,railtop),(railtop,floor+.20)]:g.tube([(cx+(balcony-.10)*math.cos(a),y+(balcony-.10)*math.sin(a),lo),(cx+(balcony-.10)*math.cos(b),y+(balcony-.10)*math.sin(b),hi)],[.021]*2,'rail_red',6)
    gallery=balcony-.13;spring=top-.95
    # The anime pavilion has dark inner panels behind the exposed arcade.
    g.curved((cx,y,floor+.18),[(gallery*.83,0),(gallery*.83,top-floor-.18)],'recess',64)
    for j in range(16):
        a=j*math.tau/16;rr=gallery*.835
        g.tube([(cx+rr*math.cos(a),y+rr*math.sin(a),railtop),(cx+rr*math.cos(a),y+rr*math.sin(a),top-.30)],[.025,.025],'wood_dark',8)
    for j in range(8):
        a=j*math.tau/8;g.tube([(cx+gallery*math.cos(a),y+gallery*math.sin(a),floor+.10),(cx+gallery*math.cos(a),y+gallery*math.sin(a),spring)],[.085,.085],'stone_light',12)
        g.curved((cx+gallery*math.cos(a),y+gallery*math.sin(a),spring-.08),[(.13,0),(.14,.15)],'stone_light',12)
        vs=[];ns=20
        for k in range(ns+1):
            s=k/ns;t=a+s*math.tau/8;z=spring+.82*math.sin(math.pi*s)
            for rad,zz in [(gallery-.12,z),(gallery-.12,top),(gallery+.10,z),(gallery+.10,top)]:vs.append((cx+rad*math.cos(t),y+rad*math.sin(t),zz))
        fs=[]
        for k in range(ns):
            q=k*4;n=q+4;fs.extend([(q,n,n+1,q+1),(q+2,q+3,n+3,n+2),(q,q+2,n+2,n),(q+1,n+1,n+3,q+3)])
        fs += [(0,1,3,2),(ns*4,ns*4+2,ns*4+3,ns*4+1)];g.mesh(vs,fs,'stone_light')
    roofbase=top+.06;roofr=balcony*1.15;roofh=apex.z-roofbase
    g.curved((cx,y,top),[(roofr-.05,0),(roofr,.10),(roofr,.18)],'roof_shadow',96)
    for row in range(28):
        t0=row/28;t1=(row+1)/28;r0=roofr*(1-t0)**1.10;r1=roofr*(1-t1)**1.10
        g.curved((cx,y,roofbase),[(r0+.025,roofh*t0),(r1,roofh*t1)],'roof' if row%4 else 'roof_light',96)
    for a in range(32):
        angle=a*math.tau/32;pts=[(cx+roofr*(1-t)**1.10*math.cos(angle),y+roofr*(1-t)**1.10*math.sin(angle),roofbase+roofh*t+.009) for t in [i/28 for i in range(28)]]
        g.tube(pts,[.012]*len(pts),'roof_dark',5)
    geo.emit(g,col,'V4_tower_pierced_drum_lattice_arched_pavilion',.002)
    c=G();c.curved((cx,y,.24),[(r,0),(r,lower-.24)],'limestone',32);geo.emit(c,col,'COL_V4_tower',0,True)
    measurements.append({'id':'tower_apex','kind':'geometry_vertex','reference_px':[(1554,105)],'points_blender_m':[list(apex)]})
    return {'tower_roof_apex':list(apex)}

def domes(col,measurements):
    g=G();y=39.;apex=at(963,311,y);eave=at(963,426,y).z;cx=apex.x;r=(at(1080,426,y).x-at(865,426,y).x)/2
    g.box((cx,y,(eave+.24)/2),(r*2,r*2,eave-.24),'plaster_blue')
    g.curved((cx,y,eave),[(r*math.cos(i/32*math.pi/2),(apex.z-eave)*math.sin(i/32*math.pi/2)) for i in range(33)],'slate',96)
    g.curved((cx,y,eave-.16),[(r+.06,0),(r+.06,.18)],'stone_light',96)
    for j in range(5):
        xx=cx-r+(j+.5)*2*r/5;g.box((xx,y-r-.015,eave-1.20),(.60,.08,1.30),'glass')
        for dx in (-.36,.36):g.box((xx+dx,y-r-.05,eave-1.20),(.08,.09,1.44),'stone_light')
    y2=37.;a2=at(1096,344,y2);bottom=at(1096,485,y2).z;e2=at(1096,413,y2).z;r2=(at(1150,413,y2).x-at(1045,413,y2).x)/2;x2=a2.x
    g.curved((x2,y2,.24),[(r2*.82,0),(r2*.82,bottom-.24),(r2*1.05,bottom-.10),(r2*1.05,bottom+.05)],'plaster_blue',64)
    g.curved((x2,y2,bottom+.1),[(r2*.90,0),(r2*.90,e2-bottom-.1)],'recess',64)
    for j in range(10):
        a=j*math.tau/10;xx=x2+r2*math.cos(a);yy=y2+r2*math.sin(a);g.tube([(xx,yy,bottom+.12),(xx,yy,e2-.28)],[.045,.045],'stone_light',8)
        pts=[(x2+r2*math.cos(a+k/12*math.tau/10),y2+r2*math.sin(a+k/12*math.tau/10),e2-.3+.25*math.sin(math.pi*k/12)) for k in range(13)]
        g.tube(pts,[.042]*len(pts),'stone_light',8)
    g.curved((x2,y2,e2),[(r2*math.cos(i/24*math.pi/2),(a2.z-e2)*math.sin(i/24*math.pi/2)) for i in range(25)],'slate',64)
    for j in range(14):
        a=j*math.tau/14;g.box((x2+(r2+.04)*math.cos(a),y2+(r2+.04)*math.sin(a),bottom-.02),(.10,.10,.25),'stone_light')
    for px,py,depth,rad in [(865,265,43,.45),(1210,334,44,.34)]:
        tip=at(px,py,depth);g.curved((tip.x,tip.y,.24),[(rad*.58,0),(rad*.58,tip.z-3.2),(rad,tip.z-3.2),(0,tip.z-.24)],'slate',40)
    geo.emit(g,col,'V4_broad_dome_building_and_small_arcade_tower',.002)
    for name,p,source in [('large_dome_apex',apex,(963,311)),('small_dome_apex',a2,(1096,344))]:measurements.append({'id':name,'kind':'geometry_vertex','reference_px':[source],'points_blender_m':[list(p)]})

def build(col,measurements):
    markers=bridge(col,measurements);markers.update(tower(col,measurements));domes(col,measurements)
    geo.facade(col,{'id':'right_bridge_connection','plane_axis':1,'plane_value':24,'outward_sign':-1,'contour_px':[(1604,536),(1765,530),(1765,915),(1604,915)],'windows':[],'material':'limestone','building_depth':2.35,'course_height':.54},measurements)
    return markers
