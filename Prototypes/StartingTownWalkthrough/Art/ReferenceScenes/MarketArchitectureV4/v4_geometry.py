"""Real extruded facade geometry constrained by traced reference contours."""
import math, sys
from pathlib import Path
from mathutils import Vector
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import build_reference_scenes_hq as hq
G=hq.G
CAM=Vector((0,-19,1.8)); TARGET=Vector((-1.2,25,5.45)); HFOV=64.0
F=(TARGET-CAM).normalized(); R=F.cross(Vector((0,0,1))).normalized(); U=R.cross(F)
TH=math.tan(math.radians(HFOV*.5)); TV=TH*9/16

def ray(u,v):return F+R*((u-.5)*2*TH)+U*((.5-v)*2*TV)
def project_point(u,v,depth_y):
    direction=ray(u,v);return CAM+direction*((depth_y-CAM.y)/direction.y)
def point_px(px,plane_axis,plane_value):
    direction=ray(px[0]/2560,px[1]/1440)
    return CAM+direction*((plane_value-CAM[plane_axis])/direction[plane_axis])
def screen_px(p):
    d=Vector(p)-CAM;return ((.5+d.dot(R)/(2*TH*d.dot(F)))*2560,(.5-d.dot(U)/(2*TV*d.dot(F)))*1440)
def emit(g,col,name,bevel=0,collision=False):
    ob=g.obj(name,col,bevel,collision);ob['style_id']='level0_market_architecture_v4';return ob

def contains(poly,p):
    inside=False;x,z=p
    for a,b in zip(poly,poly[1:]+poly[:1]):
        if (a[1]>z)!=(b[1]>z) and x<(b[0]-a[0])*(z-a[1])/(b[1]-a[1])+a[0]:inside=not inside
    return inside
def cross_x(a,b,z):return a[0]+(b[0]-a[0])*(z-a[1])/(b[1]-a[1])
def fill_bands(outer,holes):
    """Trapezoidal decomposition preserves arbitrary traced window outlines.

    Every polygon vertex adds a height breakpoint. Segment order is constant
    inside each band, allowing exact holes without a triangulation dependency.
    """
    contours=[outer]+holes;levels=sorted(set(round(p[1],7) for poly in contours for p in poly))
    for bottom,top in zip(levels,levels[1:]):
        if top-bottom<1e-6:continue
        mid=(bottom+top)/2;edges=[]
        for poly in contours:
            for a,b in zip(poly,poly[1:]+poly[:1]):
                if min(a[1],b[1])<mid<max(a[1],b[1]):edges.append((cross_x(a,b,mid),a,b))
        edges.sort(key=lambda e:e[0])
        for left,right in zip(edges,edges[1:]):
            test=((left[0]+right[0])/2,mid)
            if right[0]-left[0]<1e-6 or not contains(outer,test) or any(contains(h,test) for h in holes):continue
            yield [(cross_x(left[1],left[2],bottom),bottom),(cross_x(right[1],right[2],bottom),bottom),(cross_x(right[1],right[2],top),top),(cross_x(left[1],left[2],top),top)]

def extrude_shape(g,outer,holes,xyz,front_depth,back_depth,role):
    for quad in fill_bands(outer,holes):
        g.mesh([xyz(p,front_depth) for p in quad],[(0,1,2,3)],role)
        g.mesh([xyz(p,back_depth) for p in quad],[(3,2,1,0)],role)
    for poly in [outer]+holes:
        for a,b in zip(poly,poly[1:]+poly[:1]):
            g.mesh([xyz(a,front_depth),xyz(b,front_depth),xyz(b,back_depth),xyz(a,back_depth)],[(0,1,2,3)],role)

def cut_segment(a,b,outer,holes):
    dx=b[0]-a[0];dz=b[1]-a[1];breaks=[0,1]
    for poly in [outer]+holes:
        for c,d in zip(poly,poly[1:]+poly[:1]):
            ex=d[0]-c[0];ez=d[1]-c[1];den=dx*ez-dz*ex
            if abs(den)<1e-10:continue
            t=((c[0]-a[0])*ez-(c[1]-a[1])*ex)/den
            q=((c[0]-a[0])*dz-(c[1]-a[1])*dx)/den
            if 0<t<1 and 0<=q<=1:breaks.append(t)
    breaks=sorted(set(breaks))
    for t0,t1 in zip(breaks,breaks[1:]):
        m=(t0+t1)/2;test=(a[0]+dx*m,a[1]+dz*m)
        if contains(outer,test) and not any(contains(h,test) for h in holes):
            yield (a[0]+dx*t0,a[1]+dz*t0),(a[0]+dx*t1,a[1]+dz*t1)

def facade(col,spec,measurements):
    axis=spec.get('plane_axis',0);fixed=spec['plane_value'];normal=spec['outward_sign']
    coordinate=1 if axis==0 else 0
    def intrinsic(px):
        p=point_px(px,axis,fixed);return (p[coordinate],p.z)
    def xyz(p,depth=0):
        q=[0.,0.,p[1]];q[axis]=fixed+depth*normal;q[coordinate]=p[0];return tuple(q)
    outer=[intrinsic(p) for p in spec['contour_px']]
    openings=spec.get('windows',[]);holes=[[intrinsic(p) for p in w['outline_px']] for w in openings]
    role=spec.get('material','limestone');stone=G();detail=G();collision=G()
    extrude_shape(stone,outer,holes,xyz,0,-.36,role)
    extrude_shape(collision,outer,holes,xyz,0,-.36,role)
    emit(stone,col,spec['id']+'_traced_wall',.003)
    emit(collision,col,'COL_'+spec['id']+'_traced_wall',0,True)
    # Each window has a real hole/reveal, an inset pane and an individual frame.
    for wi,(window,poly) in enumerate(zip(openings,holes)):
        back=G()
        # Shallow recesses remain visible from the grazing street camera.
        # Closed volume avoids dependence on a single polygon's backface.
        extrude_shape(back,poly,[],xyz,-.045,-.07,window.get('material','recess'))
        emit(back,col,spec['id']+'_'+window['id']+'_inset')
        for a,b in zip(poly,poly[1:]+poly[:1]):detail.tube([xyz(a,.005),xyz(b,.005)],[.009,.009],role,6)
        xs=[p[0] for p in poly];zs=[p[1] for p in poly]
        if window.get('bars',False):
            midx=(min(xs)+max(xs))/2
            for x in [midx]:
                for a,b in cut_segment((x,min(zs)),(x,max(zs)),poly,[]):detail.tube([xyz(a,-.037),xyz(b,-.037)],[.010,.010],'shutter_light',6)
            for z in [min(zs)+(max(zs)-min(zs))*t for t in (.04,.30,.60,.91)]:
                for a,b in cut_segment((min(xs),z),(max(xs),z),poly,[]):detail.tube([xyz(a,-.036),xyz(b,-.036)],[.012,.012],'shutter_light',6)
            if window['id'].startswith('shop_') and len(poly)>6:
                # The two near shop lancets have actual diamond lattice.
                span=max(xs)-min(xs)
                for direction in (-1,1):
                    for k in range(-12,40):
                        z=min(zs)+k*.14
                        for a,b in cut_segment((min(xs),z),(max(xs),z+direction*span*.9),poly,[]):detail.tube([xyz(a,-.030),xyz(b,-.030)],[.007,.007],'stone_light',5)
        measurements.append({'id':spec['id']+'/'+window['id'],'kind':'window_outline','reference_px':window['outline_px'],'points_blender_m':[list(xyz(p)) for p in poly]})
    # Joints use the traced wall and opening boundaries, with modest scale.
    minx,maxx=min(p[0] for p in outer),max(p[0] for p in outer);minz,maxz=min(p[1] for p in outer),max(p[1] for p in outer)
    rh=spec.get('course_height',.48);bw=spec.get('block_width',1.1)
    for row in range(math.floor(minz/rh),math.ceil(maxz/rh)):
        z=row*rh
        for a,b in cut_segment((minx,z),(maxx,z),outer,holes):detail.tube([xyz(a,.007),xyz(b,.007)],[.006,.006],'mortar',4)
        for j in range(math.floor(minx/bw)-1,math.ceil(maxx/bw)+1):
            x=(j+(row%2)*.5)*bw
            for a,b in cut_segment((x,z),(x,z+rh),outer,holes):detail.tube([xyz(a,.007),xyz(b,.007)],[.006,.006],'mortar',4)
    for feature in spec.get('bands',[]):
        points=[point_px(p,axis,fixed) for p in feature['line_px']]
        for a,b in zip(points,points[1:]):detail.beam(a,b,feature.get('width_m',.14),feature.get('material',role))
    # A full depth return and rear wall make each traced facade an actual building.
    body=G();rear_depth=spec.get('building_depth',5.5)
    extrude_shape(body,outer,[],xyz,-rear_depth,-rear_depth-.25,role)
    for edge,(a,b) in enumerate(zip(outer,outer[1:]+outer[:1])):
        if edge not in spec.get('omit_return_edges',[]):body.mesh([xyz(a,-.36),xyz(b,-.36),xyz(b,-rear_depth),xyz(a,-rear_depth)],[(0,1,2,3)],role)
    emit(body,col,spec['id']+'_building_returns',.003)
    emit(detail,col,spec['id']+'_frames_and_courses',.001)
    measurements.append({'id':spec['id'],'kind':'facade_outline','reference_px':spec['contour_px'],'points_blender_m':[list(xyz(p)) for p in outer]})
    return outer

def window_shape(cx,top,bottom,width,kind='cinched'):
    h=bottom-top;r=width/2
    if kind=='rect':return [(cx-r,top),(cx+r,top),(cx+r,bottom),(cx-r,bottom)]
    if kind=='lancet':
        crown=min(h*.22,width*1.08)
        return [(cx,top),(cx+r*.65,top+crown*.40),(cx+r*.93,top+crown*.76),(cx+r,top+crown),(cx+r,bottom),(cx-r,bottom),(cx-r,top+crown),(cx-r*.93,top+crown*.76),(cx-r*.65,top+crown*.40)]
    if kind=='arch':
        return [(cx-r,bottom),(cx+r,bottom)]+[(cx+r*math.cos(i*math.pi/12),top+r-r*math.sin(i*math.pi/12)) for i in range(13)]
    return [(cx,top),(cx+r*.65,top+h*.025),(cx+r*.90,top+h*.08),(cx+r*.90,top+h*.44),(cx+r*.78,top+h*.47),(cx+r,top+h*.52),(cx+r,bottom),(cx-r,bottom),(cx-r,top+h*.52),(cx-r*.78,top+h*.47),(cx-r*.9,top+h*.44),(cx-r*.9,top+h*.08),(cx-r*.65,top+h*.025)]
