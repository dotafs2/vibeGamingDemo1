"""Reference-led authored scene revision. Blender 5.2, metre scale.

The small scene scope is fixed; craftsmanship is not reduced to blockout level.
Geometry primitives are shared, but the visible facades have individual designs.
"""
import sys, os, math, json, random, hashlib
from pathlib import Path
import bpy, bmesh
from mathutils import Vector, Matrix

HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
import build_reference_scenes as base
PROJECT=HERE.parents[1]
EXPORT=PROJECT/'assets/reference_scenes'
R=random.Random(9142026)
STYLE='level0_anime_crafted_v2'
base.STYLE=STYLE
base.PALETTE.update({
 'limestone':'D7CAA8','stone_light':'DDD2B6','stone_shade':'CABB9D','mortar':'C3B69A',
 'paving':'C7C4AA','paving_light':'CECCB5','paving_warm':'CFC6AC','paving_cool':'BEC4B1',
 'plaster_cream':'E8DEBF','plaster_white':'EAE3CB','plaster_peach':'DFC5B3','plaster_sage':'C3D0B8',
 'roof':'9E6B61','roof_light':'A67569','roof_dark':'916158','roof_shadow':'755D56',
 'slate':'6C898E','wood':'796046','wood_light':'A4885E','wood_dark':'514638',
 'shutter':'557B62','shutter_light':'7C9980','glass':'5B7D79','glass_light':'8FA99A',
 'recess':'455848','iron':'53594F','brass':'B49A5A','canvas_cream':'EFE5CA',
 'canvas_green':'88AB87','canvas_gold':'DBBB7D','canvas_red':'B76A62',
 'leaf':'689247','leaf_light':'8CA952','leaf_dark':'476F3B','leaf_mid':'789D4A',
 'leaf_deep':'365D35','leaf_warm':'A6B962','grass':'8FAF61','grass_light':'A5BA71',
 'grass_dark':'7D9F55','water':'82B6B1','foam':'D6E8DD','bark':'77664D',
 'bark_light':'968365','bark_dark':'5E5641','soil':'877B59','flower':'DED28B',
 'fruit_red':'B65F45','fruit_green':'8A9D50','fruit_yellow':'DBBC64',
 'door_blue':'699FA9','door_blue_light':'9ABAC0','sign_red':'965056',
})

def mat(key):
    m=base.material(key)
    bs=m.node_tree.nodes.get('Principled BSDF')
    bs.inputs['Roughness'].default_value=.43 if key in ('water','glass','glass_light') else .66 if key.startswith('roof') else .78
    bs.inputs['Specular IOR Level'].default_value=.24
    if key in ('iron','brass'):bs.inputs['Metallic'].default_value=.45
    if key=='foam':bs.inputs['Base Color'].default_value=m.diffuse_color
    return m

class G(base.Geo):
    def __init__(self):
        super().__init__();self.smooth=set();self.smoothing=False
    def mesh(self,vs,fs,role):
        start=len(self.f)
        super().mesh(vs,fs,role)
        if self.smoothing:self.smooth.update(range(start,len(self.f)))
    def add(self,other,p=(0,0,0),yaw=0):
        start=len(self.f);super().add(other,p,yaw)
        self.smooth.update(start+i for i in getattr(other,'smooth',set()))
    def curved(self,p,profile,role,n=48):
        old=self.smoothing;self.smoothing=True;self.rings(p,profile,role,n);self.smoothing=old
    def tube(self,coords,radii,role,n=10):
        pts=[Vector(p) for p in coords];vs=[]
        for j,p in enumerate(pts):
            tangent=(pts[min(j+1,len(pts)-1)]-pts[max(0,j-1)]).normalized()
            ref=Vector((0,0,1)) if abs(tangent.z)<.94 else Vector((1,0,0))
            u=tangent.cross(ref).normalized();v=tangent.cross(u).normalized()
            for i in range(n):
                angle=i*math.tau/n
                vs.append(p+radii[j]*(math.cos(angle)*u+math.sin(angle)*v))
        fs=[tuple(range(n-1,-1,-1))]
        for j in range(len(pts)-1):
            for i in range(n):
                a=j*n+i;b=j*n+(i+1)%n;fs.append((a,b,b+n,a+n))
        fs.append(tuple((len(pts)-1)*n+i for i in range(n)))
        old=self.smoothing;self.smoothing=True;self.mesh(vs,fs,role);self.smoothing=old
    def rounded_sphere(self,p,s,role,n=12,rings=7):
        old=self.smoothing;self.smoothing=True;self.ellipsoid(p,s,role,n,rings);self.smoothing=old
    def obj(self,name,col,bevel=.012,collision=False):
        me=bpy.data.meshes.new(name);me.from_pydata(self.v,[],self.f);me.update()
        bm=bmesh.new();bm.from_mesh(me);bmesh.ops.recalc_face_normals(bm,faces=list(bm.faces));bm.to_mesh(me);bm.free()
        ob=bpy.data.objects.new(name,me);col.objects.link(ob)
        keys=list(dict.fromkeys(self.roles));lut={k:i for i,k in enumerate(keys)}
        for k in keys:me.materials.append(mat(k))
        for i,(poly,key) in enumerate(zip(me.polygons,self.roles)):
            poly.material_index=lut[key];poly.use_smooth=i in self.smooth
        uv=me.uv_layers.new(name='UV_Material_Metres')
        for poly in me.polygons:
            axis=max(range(3),key=lambda k:abs(poly.normal[k]));axes=[k for k in range(3) if k!=axis]
            for li in poly.loop_indices:
                v=me.vertices[me.loops[li].vertex_index].co;uv.data[li].uv=(v[axes[0]],v[axes[1]])
        if bevel:
            bpy.context.view_layer.objects.active=ob
            mod=ob.modifiers.new('Crafted edge radius','BEVEL');mod.width=bevel;mod.segments=2
            mod.limit_method='ANGLE';mod.angle_limit=.65;mod.use_clamp_overlap=True
            bpy.ops.object.modifier_apply(modifier=mod.name)
        ob['style_id']=STYLE;ob['collision_enabled']=collision
        ob['evidence']='anime_observed_shapes_project_unmeasured_dimensions'
        return ob

def rect_cut(x0,x1,z0,z1,holes):
    pieces=[(x0,x1,z0,z1)]
    for a,b,c,d in holes:
        next_parts=[]
        for l,r,bot,top in pieces:
            u,v=max(l,a),min(r,b);s,t=max(bot,c),min(top,d)
            if u>=v or s>=t:next_parts.append((l,r,bot,top));continue
            if l<u:next_parts.append((l,u,bot,top))
            if v<r:next_parts.append((v,r,bot,top))
            if bot<s:next_parts.append((u,v,bot,s))
            if t<top:next_parts.append((u,v,t,top))
        pieces=next_parts
    return pieces

def wall(g,w,h,holes,stone=True,role='plaster_cream'):
    if stone:
        rh=.43;bw=1.0
        for row in range(math.ceil(h/rh)):
            z0=row*rh;z1=min(h,z0+rh)
            for col in range(math.ceil(w/bw)+2):
                x0=max(-w/2,-w/2+col*bw-(row%2)*bw*.5);x1=min(w/2,-w/2+(col+1)*bw-(row%2)*bw*.5)
                if x1<=x0:continue
                for l,r,b,t in rect_cut(x0,x1,z0,z1,holes):
                    if min(r-l,t-b)<.025:continue
                    color='limestone' if R.random()<.75 else 'stone_light' if R.random()<.5 else 'stone_shade'
                    g.box(((l+r)/2,.10,(b+t)/2),(r-l-.015,.25,t-b-.012),color)
    else:
        # One connected wall with real holes. Independent beveled rectangles caused
        # false vertical seams under directional shadows at every window boundary.
        xs=sorted(set([-w/2,w/2]+[v for a,b,c,d in holes for v in (max(-w/2,a),min(w/2,b))]))
        zs=sorted(set([0,h]+[v for a,b,c,d in holes for v in (max(0,c),min(h,d))]))
        filled=set()
        for i in range(len(xs)-1):
            for j in range(len(zs)-1):
                x=(xs[i]+xs[i+1])/2;z=(zs[j]+zs[j+1])/2
                if not any(a<x<b and c<z<d for a,b,c,d in holes):filled.add((i,j))
        vertices=[];faces=[];lookup={}
        def face(coords):
            ids=[]
            for p in coords:
                if p not in lookup:lookup[p]=len(vertices);vertices.append(p)
                ids.append(lookup[p])
            faces.append(ids)
        for i,j in sorted(filled):
            l,r=xs[i:i+2];b,t=zs[j:j+2]
            face([(l,-.03,b),(r,-.03,b),(r,-.03,t),(l,-.03,t)])
            face([(l,.27,t),(r,.27,t),(r,.27,b),(l,.27,b)])
            for adjacent,coords in [((i-1,j),[(l,-.03,b),(l,-.03,t),(l,.27,t),(l,.27,b)]),
              ((i+1,j),[(r,-.03,t),(r,-.03,b),(r,.27,b),(r,.27,t)]),
              ((i,j-1),[(r,-.03,b),(l,-.03,b),(l,.27,b),(r,.27,b)]),
              ((i,j+1),[(l,-.03,t),(r,-.03,t),(r,.27,t),(l,.27,t)])]:
                if adjacent not in filled:face(coords)
        g.mesh(vertices,faces,role)

def arc_panel(g,x,y,z,w,h,role):
    g.arch_fill(x,y,z,w,h,role)

def window(x,z,w=1.0,h=1.7,shutters=True,arched=True,style='stone'):
    g=G();r=w/2;spring=z+h-r
    # Actual inset opening, a separate stone reveal, glass and timber division.
    if arched:
        arc_panel(g,x,.20,z,w,h,'recess');arc_panel(g,x,.17,z+.045,w-.08,h-.08,'glass')
        g.arch(x,-.065,spring,r+.012,.13,.26,'stone_light',20)
        # Close the small rectangular corners around the arch without blocking the opening.
        for sign in (-1,1):
            for j in range(8):
                xx=(j+.5)*r/8;low=spring+math.sqrt(max(0,r*r-xx*xx))
                if z+h-low>.003:g.box((x+sign*xx,.12,(low+z+h)/2),(r/8+.005,.28,z+h-low),'limestone' if style=='stone' else style)
    else:
        g.box((x,.19,z+h/2),(w,.045,h),'recess');g.box((x,.16,z+h/2),(w-.10,.04,h-.1),'glass')
        g.box((x,-.065,z+h+.05),(w+.32,.26,.15),'stone_light')
    straight=h-r if arched else h
    for sign in (-1,1):
        g.box((x+sign*(r+.075),-.07,z+straight/2),(.13,.27,straight),'stone_light')
        g.box((x+sign*(r-.04),.11,z+straight/2),(.045,.06,straight),'wood_light')
    g.box((x,.095,z+h*.45),(.045,.065,h*.88),'wood_light')
    g.box((x,.09,z+h*.45),(w-.05,.065,.042),'wood_light')
    g.box((x,-.15,z-.05),(w+.4,.50,.13),'stone_light')
    g.box((x,-.09,z-.16),(w+.28,.31,.08),'limestone')
    if shutters:
        for sign in (-1,1):
            sh=G();sw=w*.43;shh=h*.87
            for j in range(5):sh.box((-sw/2+(j+.5)*sw/5,0,shh/2),(sw/5-.008,.065,shh),'shutter' if j%3 else 'shutter_light')
            for zz in (.17,shh-.17):
                sh.box((0,-.043,zz),(sw,.035,.06),'wood')
                for xx in (-sw*.36,sw*.36):sh.rounded_sphere((xx,-.07,zz),(.018,.012,.018),'iron',8,4)
            # Hinged opening produces a real side profile and shadow.
            g.add(sh,(x+sign*(r+.22+sw/2),-.11,z+.03),sign*R.choice([4,8,15]))
    return g

def arched_door(g,x,w=1.6,h=2.65,open_leaf=False):
    r=w/2;spring=h-r
    g.arch(x,-.08,spring,r,.23,.42,'stone_light',22)
    for sign in (-1,1):g.box((x+sign*(r+.115),-.06,spring/2),(.23,.38,spring),'stone_light')
    if open_leaf:
        g.box((x,2.35,1.35),(w,.18,2.7),'recess')
        for side in (-1,1):g.box((x+side*(r+.04),1.1,1.5),(.15,2.4,3),'wood_dark')
    else:arc_panel(g,x,.08,0,w,h,'wood_dark')
    door=G()
    for j in range(8):
        xx=-r+(j+.5)*w/8;top=spring+math.sqrt(max(0,r*r-xx*xx))
        door.box((xx,0,top/2),(w/8-.01,.09,top),'wood' if j%3 else 'wood_light')
    for zz in (.47,1.48):
        door.box((0,-.058,zz),(w*.86,.035,.055),'iron')
        for xx in (-w*.34,0,w*.34):door.rounded_sphere((xx,-.085,zz),(.023,.013,.023),'iron',8,4)
    door.rounded_sphere((w*.28,-.11,1.10),(.05,.035,.05),'brass',12,6)
    g.add(door,(x+r-.06,.75,0),76) if open_leaf else g.add(door,(x,.015,0))
    g.box((x,-.36,.04),(w+.45,.8,.16),'stone_light')

def blue_shop_door(g,x,w=1.7,h=2.68):
    g.box((x,.04,h/2),(w,.12,h),'door_blue')
    for xx in (-w/2,w/2,0):g.box((x+xx,-.065,h/2),(.10,.14,h+.12),'door_blue_light')
    for zz in (.14,.84,1.73,h):g.box((x,-.065,zz),(w+.10,.14,.10),'door_blue_light')
    for side in (-1,1):
        for zz,hh in [(.49,.53),(1.29,.70),(2.20,.74)]:
            g.box((x+side*w*.25,-.037,zz),(w*.35,.045,hh),'glass' if zz>2 else 'door_blue')
        g.rounded_sphere((x+side*.10,-.16,1.32),(.035,.026,.035),'brass',10,5)
    g.box((x,-.09,h+.13),(w+.42,.36,.17),'stone_light')
    for side in (-1,1):g.box((x+side*(w/2+.13),-.07,h/2),(.18,.30,h),'stone_light')
    g.box((x,-.39,.04),(w+.48,.80,.16),'stone_light')

def shop_sign(g,x,z,w=2.1,role='canvas_gold'):
    g.box((x,-.14,z),(w,.11,.46),'wood')
    g.box((x,-.208,z),(w-.09,.045,.38),role)
    for j in range(9):
        xx=x-w*.40+j*w*.10
        # Short carved strokes provide a plausible painted shop mark without a UI label.
        g.beam((xx,-.242,z-.09),(xx+.045,-.242,z+.08),.024,'canvas_cream')
        if j%2:g.beam((xx,-.243,z+.04),(xx+.09,-.243,z+.04),.023,'canvas_cream')

def curved_roof(g,w,d,z,rise):
    half=w/2+.36;dep=d+.68;slope=math.atan2(rise,half);length=math.hypot(rise,half)
    rows=math.ceil(length/.46);cols=math.ceil(dep/.34);step=length/rows;tw=dep/cols
    for sign in (-1,1):
        down=Vector((sign*math.cos(slope),0,-math.sin(slope)));across=Vector((0,1,0));normal=Vector((sign*math.sin(slope),0,math.cos(slope)))
        origin=Vector((0,-.34,z+rise))
        vertices=[origin+down*u+across*v+normal*t for t in (-.12,0) for u,v in [(0,0),(length,0),(length,dep),(0,dep)]]
        g.mesh(vertices,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],'roof_shadow')
        for row in range(rows):
            offset=(row%2)*tw*.5
            for j in range(cols+1):
                lo=max(0,j*tw-offset);hi=min(dep,(j+1)*tw-offset-.012)
                if hi-lo<.06:continue
                tilelen=min(step*1.16,length-row*step+.02);vs=[]
                for layer in range(2):
                    for end in range(2):
                        for k in range(7):
                            t=k/6;bulge=math.sin(t*math.pi)*.044
                            vs.append(origin+down*(row*step+end*tilelen)+across*(lo+(hi-lo)*t)+normal*(.029+bulge-.022*(1-layer)+.008*end))
                fs=[]
                for k in range(6):
                    fs.extend([(k,k+1,k+8,k+7),(14+k,21+k,22+k,15+k),
                               (k,14+k,15+k,k+1),(7+k,8+k,22+k,21+k)])
                fs.extend([(0,7,21,14),(6,20,27,13)])
                role='roof' if R.random()<.84 else 'roof_light' if R.random()<.65 else 'roof_dark'
                # Smooth curved faces; the separately indexed ends retain crisp borders.
                g.mesh(vs,fs,role)
        for yy in (-.37,d+.37):g.beam((0,yy,z+rise),(sign*half,yy,z),.12,'wood')
        g.beam((sign*half,-.36,z),(sign*half,d+.36,z),.15,'wood')
    for j in range(math.ceil(dep/.38)):
        y=-.34+j*.38
        v=[(r*math.cos(k*math.pi/10),yy,z+rise+.025+r*math.sin(k*math.pi/10)) for r in (.10,.145) for yy in (y,min(d+.34,y+.40)) for k in range(11)]
        fs=[]
        for k in range(10):fs.extend([(k,k+1,k+12,k+11),(22+k,33+k,34+k,23+k),(k,22+k,23+k,k+1),(11+k,12+k,34+k,33+k)])
        fs.extend([(0,11,33,22),(10,32,43,21)]);g.mesh(v,fs,'roof_light')

def hip_roof(g,w,d,h,rise=4.4):
    # Four sloping triangular tile fields, clipped against the actual hip edges.
    corners=[Vector((-w/2-.38,-.38,h)),Vector((w/2+.38,-.38,h)),
             Vector((w/2+.38,d+.38,h)),Vector((-w/2-.38,d+.38,h))]
    apex=Vector((0,d/2,h+rise))
    for side in range(4):
        left=corners[side];right=corners[(side+1)%4];mid=(left+right)/2
        axis=(right-left).normalized();up=apex-mid;length=up.length;up.normalize()
        normal=axis.cross(up).normalized();width=(right-left).length
        g.mesh([left,right,apex],[(0,1,2)],'roof_shadow')
        row_count=math.ceil(length/.44)
        for row in range(row_count):
            v0=row*length/row_count;v1=min(length-.006,v0+length/row_count*1.12)
            half0=width*.5*(1-v0/length);half1=width*.5*(1-v1/length)
            if half0<.02:continue
            for j in range(-math.ceil(half0/.31)-1,math.ceil(half0/.31)+1):
                center=(j+.5*(row%2))*.31
                lo0=max(-half0,center-.15);hi0=min(half0,center+.15)
                lo1=max(-half1,center-.15);hi1=min(half1,center+.15)
                if hi0<=lo0 or hi1<=lo1:continue
                vs=[]
                for layer in range(2):
                    for u,lo,hi in ((v0,lo0,hi0),(v1,lo1,hi1)):
                        for k in range(7):
                            t=k/6;offset=.03+.04*math.sin(t*math.pi)-.024*(1-layer)
                            vs.append(mid+up*u+axis*(lo+(hi-lo)*t)+normal*offset)
                fs=[]
                for k in range(6):fs.extend([(k,k+1,k+8,k+7),(14+k,21+k,22+k,15+k),(k,14+k,15+k,k+1),(7+k,8+k,22+k,21+k)])
                fs.extend([(0,7,21,14),(6,20,27,13)])
                g.mesh(vs,fs,'roof' if R.random()<.87 else 'roof_light')
        g.beam(left,right,.13,'wood')
        # Rounded overlapping caps cover the diagonal hips.
        count=math.ceil((apex-left).length/.34)
        for j in range(count):
            a=left.lerp(apex,j/count)+normal*.06;b=left.lerp(apex,min(1,(j+1.09)/count))+normal*.06
            g.tube([a,b],[.085,.077],'roof_light',10)

def facade(name,col,w,d,h,openings,position,yaw=0,stone=True,plaster='plaster_cream',roof='flat',door_x=0,shop=False,blue_door=False):
    structure=G();details=G();solid=G()
    holes=[(x-ww/2,x+ww/2,z,z+hh) for x,z,ww,hh,arched,shutters in openings]
    holes.append((door_x-.85,door_x+.85,0,2.68))
    wall(structure,w,h,holes,stone,plaster)
    # Back and sides are modeled, so oblique and walking views remain coherent.
    for side in (-1,1):
        structure.box((side*(w/2-.14),d/2,h/2),(.28,d,h),'limestone' if stone else plaster)
    structure.box((0,d-.12,h/2),(w,.25,h),'limestone' if stone else plaster)
    structure.box((0,d/2,.01),(w,d,.15),'paving')
    for side in (-1,1):solid.box((side*(w/2-.2),d/2,h/2),(.4,d,h))
    solid.box((0,d,h/2),(w,.35,h));solid.box((0,d/2,-.02),(w,d,.20))
    for l,r,b,t in rect_cut(-w/2,w/2,0,h,[(door_x-.85,door_x+.85,0,2.68)]):solid.box(((l+r)/2,.1,(b+t)/2),(r-l,.35,t-b))
    for x,z,ww,hh,arched,shutters in openings:details.add(window(x,z,ww,hh,shutters,arched,'stone' if stone else plaster))
    if blue_door:blue_shop_door(details,door_x)
    else:arched_door(details,door_x,1.7,2.68,shop)
    if not stone:shop_sign(details,door_x,3.08,2.05,'sign_red' if roof=='hip' else 'canvas_gold')
    for zz,thick,projection in [(.34,.16,.10),(3.5,.17,.16),(h-.16,.24,.24)]:
        details.box((0,-projection/2,zz),(w+.18,projection+.23,thick),'stone_light')
    if not stone:
        # Sparse stone patches and properly wrapped quoins, not noise on every brick.
        for side in (-1,1):
            for j in range(math.ceil(h/.42)):
                width=.34 if j%2 else .53
                details.box((side*(w/2-width/2),-.04,(j+.5)*.42),(width,.12,.39),'limestone')
        for xx,zz in [(-w*.26,1.0),(w*.2,4.0),(-w*.4,h-1.7)]:
            details.box((xx,-.041,zz),(.65,.06,.28),'stone_light')
    if roof=='crossgable':
        rise=2.45
        rr=G();curved_roof(rr,d,w,h,rise)
        details.add(rr,(w/2,d/2,0),90)
        for xx in (-w/2,w/2):structure.mesh([(xx,0,h),(xx,d,h),(xx,d/2,h+rise)],[(0,1,2)],plaster)
        details.box((-w*.28,d*.57,h+rise+.22),(.55,.62,1.6),'limestone')
        details.box((-w*.28,d*.57,h+rise+1.04),(.70,.76,.16),'stone_light')
    elif roof=='gable':
        rise=min(4.2,w*.45)
        curved_roof(details,w,d,h,rise)
        for yy in (.0,d):structure.mesh([(-w/2,yy,h),(w/2,yy,h),(0,yy,h+rise)],[(0,1,2)],plaster)
        # The gable opening uses a recess in front of the thin gable infill.
        attic=window(0,h+.60,.65,1.15,False,True,plaster)
        details.add(attic,(0,-.23,0))
        details.box((w*.28,d*.55,h+1.1),(.54,.62,2.2),'limestone')
        for j in range(5):details.box((w*.28,d*.55,h+.3+j*.37),(.62,.70,.08),'stone_light')
        details.box((w*.28,d*.55,h+2.26),(.70,.77,.17),'stone_light')
        details.box((w*.28,d*.55,h+2.36),(.45,.5,.025),'recess')
    elif roof=='hip':
        hip_roof(details,w,d,h)
    elif roof=='terrace':
        structure.box((0,d/2,h),(w,d,.20),'stone_light')
        for yy in (.10,d-.1):details.box((0,yy,h+.28),(w,.25,.65),plaster)
        for xx in (-w/2+.1,w/2-.1):details.box((xx,d/2,h+.28),(.25,d,.65),plaster)
    else:
        structure.box((0,d/2,h),(w,d,.20),'stone_light')
        for j in range(math.ceil(w/1.2)):
            details.box((-w/2+(j+.5)*w/math.ceil(w/1.2),.12,h+.48),(.48,.60,.86),'stone_light')
        details.box((0,.06,h+.05),(w+.22,.66,.20),'stone_light')
    transform=Matrix.Translation(Vector(position))@Matrix.Rotation(math.radians(yaw),4,'Z')
    for ob in [structure.obj(name+'_masonry',col,.012),details.obj(name+'_crafted_details',col,.009),solid.obj('COL_'+name,col,0,True)]:ob.matrix_world=transform
    return details

def draped_awning(name,col,p,yaw,w,projection=2.6,z=3.45,color='canvas_green',sag=.28):
    g=G();frame=G();stripes=max(5,round(w/.62));n=18
    for j in range(stripes):
        xa=-w/2+j*w/stripes;xb=xa+w/stripes
        verts=[]
        for i in range(n+1):
            t=i/n;yy=-projection*t
            for u in range(4):
                x=xa+(xb-xa)*u/3
                wave=.07*math.sin((x+w/2)*math.tau/.62)*math.sin(t*math.pi/2)
                zz=z-.56*t-sag*math.sin(t*math.pi)+wave-.12*math.sin((x+w/2)/w*math.pi)
                verts.append((x,yy,zz))
        fs=[(i*4+k,i*4+k+1,(i+1)*4+k+1,(i+1)*4+k) for i in range(n) for k in range(3)]
        g.mesh(verts,fs,color if j%2==0 else 'canvas_cream')
        # Hem has a continuous shallow scallop, rather than a sharp triangle.
        vv=[]
        for k in range(9):
            t=k/8;x=xa+(xb-xa)*t;top=z-.56-.12*math.sin((x+w/2)/w*math.pi)
            vv.extend([(x,-projection,top),(x,-projection-.02,top-.17-.10*math.sin(t*math.pi))])
        g.mesh(vv,[(k*2,k*2+1,k*2+3,k*2+2) for k in range(8)],color if j%2==0 else 'canvas_cream')
    for side in (-1,1):
        x=side*(w/2-.08)
        frame.tube([(x,-projection,0),(x,-projection,z-.42)], [.045,.045],'wood',12)
        frame.beam((x,0,z-.7),(x,-projection,z-.58),.045,'wood_dark')
        frame.tube([(x,.1,z+.2),(x,-projection*.5,z-.14),(x,-projection,z-.52)],[.012,.012,.012],'wood_dark',6)
    frame.beam((-w/2,-projection,z-.56),(w/2,-projection,z-.56),.065,'wood')
    trans=Matrix.Translation(Vector(p))@Matrix.Rotation(math.radians(yaw),4,'Z')
    for ob in [g.obj(name+'_canvas',col,0),frame.obj(name+'_supports',col,.008)]:ob.matrix_world=trans

def crate(g,p,size=(.72,.60,.55),produce=None):
    c=G();w,d,h=size
    for x in (-w/2+.035,w/2-.035):
        for y in (-d/2+.035,d/2-.035):c.box((x,y,h/2),(.06,.06,h),'wood')
    for j in range(3):
        z=(j+.5)*h/3
        for side in (-1,1):
            c.box((0,side*d/2,z),(w,.045,h/3-.025),'wood_light')
            c.box((side*w/2,0,z),(.045,d,h/3-.025),'wood')
    c.box((0,0,.025),(w,d,.05),'wood')
    if produce:
        for i in range(14):
            x=R.uniform(-w*.38,w*.38);y=R.uniform(-d*.36,d*.36);z=h-.035+R.uniform(-.015,.04)
            c.rounded_sphere((x,y,z),(.065,.068,.063),produce,10,5)
            c.beam((x,y,z+.05),(x+.008,y,z+.085),.008,'wood_dark')
    g.add(c,p)

def barrel(g,p,scale=1,open_top=False):
    a=G();n=18
    for j in range(n):
        t=j*math.tau/n;dt=math.tau/n*.465
        vs=[(r*math.cos(t+da),r*math.sin(t+da),z) for r,z in [(.33,0),(.40,.22),(.43,.55),(.39,.90),(.34,1.08)] for da in (-dt,dt)]
        a.mesh(vs,[(k*2,k*2+1,k*2+3,k*2+2) for k in range(4)],'wood' if j%4 else 'wood_light')
    for z,r in [(.12,.377),(.34,.426),(.78,.419),(.98,.375)]:a.curved((0,0,z),[(r,0),(r,.055)],'iron',36)
    a.cylinder((0,0,1.04),.335,.025,'recess' if open_top else 'wood_light',32)
    a.v=[tuple(Vector(v)*scale) for v in a.v];g.add(a,p)

def shop_stall(name,col,p,yaw,width=2.6):
    g=G()
    for side in (-1,1):
        for yy in (-.38,.38):g.box((side*(width/2-.16),yy,.47),(.12,.12,.94),'wood')
        g.beam((-width/2+.15,side*.37,.22),(width/2-.15,side*.37,.70),.06,'wood_dark')
    for j in range(6):g.box((0,-.48+(j+.5)*.96/6,.99),(width,.96/6-.009,.065),'wood_light')
    for j,color in enumerate(['fruit_green','fruit_yellow','fruit_red']):crate(g,(-width*.32+j*width*.32,0,1.04),(.7,.72,.23),color)
    trans=Matrix.Translation(Vector(p))@Matrix.Rotation(math.radians(yaw),4,'Z')
    g.obj(name,col,.009).matrix_world=trans

def stone_path(name,col,bounds,z=.22,step=.64):
    g=G();x0,x1,y0,y1=bounds
    for row in range(math.ceil((y1-y0)/step)):
        yy=y0+row*step;off=(row%2)*step*.72
        for j in range(math.ceil((x1-x0)/(step*1.45))+1):
            xx=x0+j*step*1.45-off;l=max(x0,xx);r=min(x1,xx+step*1.45)
            if r-l<.07:continue
            top=min(y1,yy+step)
            role='paving' if R.random()<.67 else R.choice(['paving_light','paving_warm','paving_cool'])
            g.box(((l+r)/2,(yy+top)/2,z),(r-l-.011,top-yy-.011,.08),role)
    g.obj(name,col,.008)
    c=G();c.box(((x0+x1)/2,(y0+y1)/2,z-.06),(x1-x0,y1-y0,.16));c.obj('COL_'+name,col,0,True)

def leafy_tree(col,p,height=13.5):
    trunk=G();leaves=G();origin=Vector(p)
    main=[(0,0,0),(.23,.07,1.4),(.69,.12,3),(1.03,.05,4.8),(.70,.05,6.4),(.85,.3,8.2)]
    trunk.tube([origin+Vector(v) for v in main],[.65,.54,.47,.39,.31,.14],'bark',20)
    for i in range(7):
        t=i*math.tau/7
        trunk.tube([origin+Vector((0,0,.6)),origin+Vector((math.cos(t)*.7,math.sin(t)*.7,.15)),origin+Vector((math.cos(t)*1.5,math.sin(t)*1.3,.04))],[.19,.16,.025],'bark',10)
    crowns=[]
    limbs=[[(.75,.05,4.2),(1.7,-.1,6.9),(3.1,-.6,9.7),(5.4,-1,12.1)],
           [(.98,.05,4.8),(-.4,.4,7.0),(-2.1,.7,9.8),(-5.2,1.1,12.5)],
           [(.72,.08,5.8),(1.0,2.0,8.1),(2.1,3.3,11),(2.5,4,14)],
           [(.89,.2,7.3),(.2,-1.4,9.3),(-1.1,-3.2,12.1),(-2,-4,14.1)],
           [(.8,.1,6.3),(.4,.1,10.1),(1.0,.4,13.4),(.3,.7,15.2)]]
    for j,limb in enumerate(limbs):
        points=[origin+Vector(v) for v in limb]
        trunk.tube(points,[.30-j*.022,.20-j*.012,.11,.04],'bark',16)
        for k in range(5):
            angle=(j*1.45+k*1.65);r=1.5+k*.42
            end=points[-1]+Vector((math.cos(angle)*r,math.sin(angle)*r*.85,R.uniform(-.7,1.1)))
            start=points[2] if k%2 else points[1].lerp(points[2],.70)
            mid=start.lerp(end,.57)+Vector((0,0,.4))
            trunk.tube([start,mid,end],[.09,.052,.012],'bark',10)
            crowns.append((end,Vector((R.uniform(1.8,2.55),R.uniform(1.65,2.3),R.uniform(1.35,2.1)))))
            for a in (-1,1):
                tip=end+Vector((math.cos(angle+a)*.95,math.sin(angle+a)*.8,.55))
                trunk.tube([mid,end,tip],[.04,.023,.006],'bark',7)
    for j in range(18):
        angle=j*math.tau/18;coords=[]
        for k,v in enumerate(main[:5]):
            radius=[.65,.54,.47,.39,.31][k]+.003
            coords.append(origin+Vector(v)+Vector((radius*math.cos(angle+.025*math.sin(k+j)),radius*math.sin(angle+.025*math.sin(k+j)),0)))
        trunk.tube(coords,[.017,.013,.011,.010,.002],'bark_dark' if j%3 else 'bark_light',5)
    # Dense individually shaped leaves, with coherent light/dark crown regions.
    for center,size in crowns:
        for j in range(840):
            v=Vector((R.gauss(0,.55),R.gauss(0,.55),R.gauss(0,.52)))
            if v.length>1.15:v.normalize()
            q=center+Vector((v.x*size.x,v.y*size.y,v.z*size.z))
            length=R.uniform(.14,.25);width=length*R.uniform(.40,.64)
            angle=R.uniform(0,math.tau);u=Vector((math.cos(angle),math.sin(angle),R.uniform(-.95,.95))).normalized()*length
            vaxis=Vector((-math.sin(angle),math.cos(angle),R.uniform(-.75,.75))).normalized()*width
            vv=[q+Vector((0,0,.045)),q-u,q-u*.55+vaxis*.75,q+u*.5+vaxis,q+u,q+u*.5-vaxis,q-u*.55-vaxis*.75]
            value=(q.z-p[2])/height
            role=R.choice(['leaf','leaf_mid','leaf_light']) if value>.76 else R.choice(['leaf_dark','leaf','leaf_deep'])
            leaves.mesh(vv,[(0,i,1 if i==6 else i+1) for i in range(1,7)],role)
    trunk.obj('tolbana_tree_branch_structure',col,.005)
    leaves.obj('tolbana_tree_individual_leaf_clusters',col,0)
    c=G();c.cylinder(p,.52,5,'limestone',16);c.obj('COL_tolbana_tree',col,0,True)

def grass_patch(col,bounds,exclude):
    g=G();x0,x1,y0,y1=bounds
    for j in range(6500):
        x=R.uniform(x0,x1);y=R.uniform(y0,y1)
        if exclude(x,y):continue
        for blade in range(R.randint(4,7)):
            xx=x+R.uniform(-.16,.16);yy=y+R.uniform(-.16,.16);h=R.uniform(.11,.26)
            angle=R.uniform(0,math.tau);wide=R.uniform(.008,.022)
            direction=Vector((math.cos(angle),math.sin(angle),0));side=Vector((-math.sin(angle),math.cos(angle),0))
            vv=[]
            for k in range(5):
                t=k/4;q=Vector((xx,yy,.22))+direction*(t*t*h*.55)+Vector((0,0,h*t))
                vv.extend([q-side*wide*(1-t),q+side*wide*(1-t)])
            g.mesh(vv,[(k*2,k*2+1,k*2+3,k*2+2) for k in range(4)],R.choice(['grass','grass','grass_light','grass_dark']))
    g.obj('tolbana_curved_grass_clumps',col,0)

def fountain(col,p):
    g=G();water=G()
    # Turned, molded profiles preserve a continuous silhouette at close range.
    g.curved((0,0,0),[(2.62,0),(2.64,.10),(2.60,.19),(2.43,.23),(2.42,.59),(2.53,.64),(2.57,.72),(2.56,.83),(2.29,.85),(2.22,.73),(2.20,.35),(.58,.35)],'stone_light',64)
    for j in range(16):
        t=j*math.tau/16
        g.tube([(2.435*math.cos(t),2.435*math.sin(t),.24),(2.44*math.cos(t),2.44*math.sin(t),.60)],[.009,.009],'stone_shade',6)
    g.curved((0,0,.35),[(.60,0),(.65,.10),(.55,.22),(.43,.35),(.32,.68),(.30,1.08),(.36,1.30),(.55,1.40),(1.25,1.55),(1.37,1.64),(1.38,1.73),(1.33,1.81),(1.19,1.84),(.28,1.61)],'limestone',64)
    g.curved((0,0,2.01),[(.28,0),(.33,.14),(.23,.31),(.20,.62),(.29,.79),(.70,.93),(.80,1.02),(.80,1.09),(.71,1.17),(.19,1.04)],'stone_light',56)
    g.curved((0,0,3.10),[(.19,0),(.23,.1),(.17,.24),(.10,.38),(.04,.44)],'limestone',40)
    water.cylinder((0,0,.46),2.20,.025,'water',64)
    water.cylinder((0,0,2.10),1.2,.02,'water',56)
    water.cylinder((0,0,3.18),.70,.02,'water',48)
    for tier,r,z,targetz in [(0,.68,3.18,2.11),(1,1.21,2.12,.47)]:
        for j in range(18 if tier else 10):
            t=j*math.tau/(18 if tier else 10);coords=[]
            for k in range(9):
                u=k/8;rr=r+.11*u+.14*math.sin(u*math.pi/2)
                coords.append((rr*math.cos(t),rr*math.sin(t),z-(z-targetz)*u*u))
            water.tube(coords,[.014+.008*k/8 for k in range(9)],'foam' if j%4==0 else 'water',6)
    for r in (.75,1.3,1.7):
        water.tube([(r*math.cos(j*math.tau/64),r*math.sin(j*math.tau/64),.493) for j in range(65)],[.009]*65,'foam',5)
    for ob in [g.obj('tolbana_fountain_molded_stone',col,.005),water.obj('tolbana_fountain_water_and_streams',col,0)]:ob.location=p;ob.scale=(1.15,1.15,1.15)
    c=G();c.cylinder(p,2.55*1.15,.84*1.15,'limestone',32);c.obj('COL_tolbana_fountain',col,0,True)

def camera(sc,name,p,target,fov):
    data=bpy.data.cameras.new(name);ob=bpy.data.objects.new(name,data);sc.collection.objects.link(ob)
    ob.location=p;ob.rotation_euler=(Vector(target)-ob.location).to_track_quat('-Z','Y').to_euler()
    data.lens=36/(2*math.tan(math.radians(fov)/2));data.clip_end=20000;sc.camera=ob
    sc['reference_camera_position']=list(p);sc['reference_camera_target']=list(target);sc['reference_horizontal_fov']=fov

def scene(name):
    sc=bpy.data.scenes.new(name);bpy.context.window.scene=sc;sc.unit_settings.system='METRIC'
    sc['style_id']=STYLE;sc['canonical_exact_coordinates']=False
    world=bpy.data.worlds.new(name+'_sky');world.use_nodes=True
    world.node_tree.nodes['Background'].inputs[0].default_value=(.52,.66,.79,1)
    world.node_tree.nodes['Background'].inputs[1].default_value=.5;sc.world=world
    ld=bpy.data.lights.new(name+'_sun','SUN');ld.energy=2.0;ld.angle=.035
    lo=bpy.data.objects.new(name+'_sun',ld);sc.collection.objects.link(lo)
    lo.rotation_euler=(math.radians(25),math.radians(-28),math.radians(-25))
    sc.render.engine='CYCLES';sc.cycles.samples=32;sc.cycles.use_denoising=True
    sc.render.threads_mode='FIXED';sc.render.threads=4
    sc.render.resolution_x=1600;sc.render.resolution_y=900;sc.render.resolution_percentage=100
    sc.view_settings.view_transform='AgX'
    return sc

def market():
    sc=scene('StartingTown_Market');col=sc.collection
    stone_path('market_fitted_paving',col,(-6.8,6.8,-25,67),.20,.61)
    ground=G();ground.box((0,20,-.15),(56,98,.5),'paving');ground.obj('market_ground_base',col,.01)
    c=G();c.box((0,20,-.14),(56,98,.5));c.obj('COL_market_ground',col,0,True)
    # Distinct street facades: tall foreground, stepped cornice, paired windows, low arcade shop.
    designs=[
      ('left_linen',-1,-17,11,8,14.8,[(-3.7,4.4,1.1,1.9,True,True),(-.9,4.4,1.1,1.9,True,True),(2.2,4.4,1.2,1.9,True,True),(-3.7,8.1,.85,2.2,True,False),(-.9,8.1,.85,2.2,True,False),(2.2,8.1,.85,2.2,True,False),(-3.7,11.7,.7,1.7,True,False),(0,11.7,.7,1.7,True,False),(3.4,11.7,.7,1.7,True,False)],.9),
      ('left_arched_shop',-1,-5.2,12,8,10.7,[(-4.2,4.1,.9,1.75,True,True),(-1.6,4.1,.9,1.75,True,False),(1.2,4.1,1.1,1.75,True,True),(4.1,4.1,.9,1.75,True,False),(-3.4,7.3,.8,1.95,True,False),(-.5,7.3,.8,1.95,True,False),(2.8,7.3,.8,1.95,True,False)],-2.1),
      ('left_corner',-1,7.0,11.7,9,13.5,[(-3.7,4.5,1.0,1.8,False,True),(0,4.5,1.0,1.8,False,True),(3.7,4.5,1.0,1.8,False,True),(-3.7,8.5,.9,2,True,False),(-.3,8.5,.9,2,True,False),(3.1,8.5,.9,2,True,False)],1.7),
      ('right_foreground',1,-15,14,9,16.0,[(-5,4.3,.9,1.9,True,True),(-2,4.3,.9,1.9,True,True),(1.4,4.3,1.1,1.9,True,True),(4.7,4.3,.9,1.9,True,True),(-4.7,8.5,.85,2.0,True,False),(-1.1,8.5,.85,2,True,False),(2.7,8.5,.85,2,True,False),(-4.7,12,.75,2,True,False),(-1.1,12,.75,2,True,False),(2.7,12,.75,2,True,False)],-1.5),
      ('right_green_shop',1,-1.0,12,8,12.2,[(-4,4.6,1.0,1.7,True,True),(-.8,4.6,1.0,1.7,True,True),(3.0,4.6,1.0,1.7,True,True),(-3.8,8.1,.85,2,True,False),(-.5,8.1,.85,2,True,False),(2.8,8.1,.85,2,True,False)],2.1),
      ('right_archway_house',1,11,12,8,14.4,[(-4,4.3,1,1.8,True,False),(-1,4.3,1,1.8,True,True),(2,4.3,1,1.8,True,False),(-3.2,8.2,.85,2.1,True,False),(0,8.2,.85,2.1,True,False),(3.2,8.2,.85,2.1,True,False)],-.3),
    ]
    for name,side,y,w,d,h,openings,door_x in designs:
        yaw=90 if side<0 else -90;x=side*6.65
        facade('market_'+name,col,w,d,h,openings,(x,y,.24),yaw,True,door_x=door_x,shop=True)
        color='canvas_gold' if name=='left_linen' else 'canvas_cream' if name=='left_arched_shop' else 'canvas_green'
        draped_awning(name+'_awning',col,(x,y,.24),yaw,w-.5,2.15,3.45,color,.25)
        if name=='left_linen':draped_awning('left_high_gold_canopy',col,(x,y+4,.24),yaw,9,3.1,6.1,'canvas_gold',.43)
        shop_stall(name+'_produce',col,(side*4.6,y+1.5,.24),yaw,2.65)
    props=G()
    for x,y,scale in [(-4.8,-11,.95),(4.8,-8,1.05),(-5.1,2,.85),(5.0,5,.9),(-4.6,13,.8)]:
        barrel(props,(x,y,.24),scale,True);crate(props,(x+.65,y+.55,.24))
    # Bridge and turret are authored as a landmark, not another house template.
    gate=G();gy=25.8
    for sign in (-1,1):gate.box((sign*5.35,gy,4.0),(4.4,2.7,8.0),'limestone')
    gate.arch(.3,gy,3.8,3.1,.52,2.9,'stone_light',32)
    for j in range(40):
        xx=-3.3+(j+.5)*6.6/40;low=3.8+math.sqrt(max(0,3.62**2-(xx-.3)**2))
        gate.box((xx,gy,(low+8.55)/2),(.18,2.65,max(.05,8.55-low)),'limestone')
    gate.box((0,gy,9.1),(15,2.8,1.2),'limestone')
    for zz in (7.85,9.75):gate.box((0,gy-.03,zz),(15.3,3,.19),'stone_light')
    for j in range(15):gate.box((-7.1+j,gy,10.3),(.47,2.8,.95),'stone_light')
    for x in (-5.6,-4.4,-3.2):gate.add(window(x,8.15,.38,.85,False,True),(0,gy-1.5,0))
    bridge_stones=G()
    for row in range(22):
        z0=row*.43;z1=z0+.418
        for j in range(20):
            x=-7.5+j*.78-(row%2)*.39
            if x< -7.5 or x+.765>7.5:continue
            center=x+.3825
            # Leave the arch unobstructed; thin fitted facing adds depth to the landmark.
            arch_top=3.8+math.sqrt(max(0,3.66**2-(center-.3)**2)) if abs(center-.3)<3.66 else 0
            if abs(center-.3)<3.66 and z0<arch_top:continue
            bridge_stones.box((center,gy-1.40,(z0+z1)/2),(.765,.14,z1-z0),'limestone' if R.random()<.85 else 'stone_light')
    gate.add(bridge_stones)
    tx=6.5;ty=gy+1.6
    gate.curved((tx,ty,0),[(2,0),(2.05,.3),(1.8,.5),(1.78,11.4),(2.08,11.65),(2.17,11.95),(2.20,12.22),(2.42,12.4),(2.45,12.7)],'limestone',64)
    gate.curved((tx,ty,12.7),[(2.12,0),(2.12,2.65)],'recess',56)
    for j in range(14):
        t=j*math.tau/14;xx=tx+2.08*math.cos(t);yy=ty+2.08*math.sin(t)
        gate.curved((xx,yy,12.7),[(.13,0),(.09,.18),(.09,2.15),(.15,2.35)],'stone_light',12)
    gate.curved((tx,ty,15.15),[(2.62,0),(2.63,.22),(2.30,.33),(.08,5.05)],'roof',64)
    for j in range(24):
        t=j*math.tau/24
        gate.tube([(tx+2.34*math.cos(t),ty+2.34*math.sin(t),15.49),(tx+.12*math.cos(t),ty+.12*math.sin(t),20.1)],[.022,.012],'roof_light',5)
    gate.tube([(tx,ty,20.15),(tx,ty,21.05)],[.065,.025],'iron',12)
    gate.obj('market_bridge_and_colonnaded_turret',col,.014)
    cg=G()
    for sign in (-1,1):cg.box((sign*5.35,gy,4.0),(4.4,2.7,8.0))
    cg.box((0,gy,8.8),(15,2.8,2));cg.obj('COL_market_bridge',col,0,True)
    rear=G()
    for x,y,r,h in [(-4.2,47,4.7,16.5),(-12,56,3.4,17.0)]:
        rear.curved((x,y,0),[(r,0),(r,h),(r+.18,h+.2)],'plaster_cream',48)
        rear.curved((x,y,h+.2),[(r*math.cos(j*math.pi/40),r*.78*math.sin(j*math.pi/40)) for j in range(21)],'slate',64)
        for j in range(16):
            t=j*math.tau/16
            a=window(0,h-3.0,.46,1.45,False,True,'plaster_cream')
            rear.add(a,(x+math.cos(t)*(r+.02),y+math.sin(t)*(r+.02),0),math.degrees(t)+90)
    rear.obj('market_distant_domed_silhouettes',col,.008)
    for yy,zz in [(-2.5,11.5),(18,12.0)]:
        coords=[(-6.7+i*13.4/20,yy,zz-.6*math.sin(i/20*math.pi)) for i in range(21)]
        props.tube(coords,[.012]*21,'wood_dark',6)
        for j in range(12):
            x=-6.2+j*1.05;z=zz-.6*math.sin((x+6.7)/13.4*math.pi)
            props.mesh([(x,yy,z),(x+.54,yy,z-.02),(x+.55,yy+.025,z-1.1),(x,yy+.03,z-1.12)],[(0,3,2,1)],['canvas_red','canvas_cream','canvas_green'][j%3])
    props.obj('market_crates_barrels_banners',col,.008)
    camera(sc,'Anime_Market_Camera',(.0,-19,2.25),(.3,25,6.1),70)
    return sc

def tolbana():
    sc=scene('Tolbana_Plaza');col=sc.collection
    terrain=G();n=36;extent=260
    vs=[]
    for iy in range(n+1):
        y=-extent+iy*extent*2/n
        for ix in range(n+1):
            x=-extent+ix*extent*2/n
            z=max(0,(abs(x)-60)/7)+max(0,(y-90)/11)
            if abs(x)<60 and -45<y<85:z=0
            vs.append((x,y,z-.10))
    terrain.mesh(vs,[(j*(n+1)+i,j*(n+1)+i+1,(j+1)*(n+1)+i+1,(j+1)*(n+1)+i) for j in range(n) for i in range(n)],'grass')
    terrain.obj('tolbana_valley_ground',col,0)
    lawn=G();lawn.box((0,15,-.06),(120,130,.5),'grass');lawn.obj('tolbana_grass_soil_surface',col,0)
    c=G();c.box((0,15,-.06),(120,130,.5));c.obj('COL_tolbana_ground',col,0,True)
    stone_path('tolbana_front_walk',col,(-25,25,-18,-13),.22,.72)
    stone_path('tolbana_west_walk',col,(-25,-21,-13,23),.22,.72)
    stone_path('tolbana_east_walk',col,(20,25,-13,23),.22,.72)
    stone_path('tolbana_rear_walk',col,(-25,25,18,23),.22,.72)
    stone_path('tolbana_fountain_approach',col,(8,11,-2,18),.22,.65)
    edge=G()
    for j in range(46):
        for y in (-12.83,17.84):edge.box((-20.5+j*.88,y,.24),(.87,.17,.20),'stone_light')
    edge.obj('tolbana_lawn_curbstones',col,.014)
    # The reference has low houses between two taller bookends, not a uniform wall.
    buildings=[
      ('west_tall',-22,26,0,8.1,9,10.8,'plaster_cream','gable',[(-2.3,3.65,.85,1.6,True,True),(1.4,3.65,.85,1.6,True,True),(-2.3,7.3,.70,1.45,True,False),(1.4,7.3,.70,1.45,True,False)],0),
      ('west_low',-13.8,25,0,8,8,6.5,'plaster_white','crossgable',[(-2.4,3.95,.70,1.2,False,False),(1.8,3.95,.70,1.2,False,False),(-2.65,.55,1,1.85,True,False),(2.1,.55,1,1.85,True,False)],-.3),
      ('peach_bread_front',-5.2,25.7,0,8.9,8,6.6,'plaster_peach','crossgable',[(-2.5,3.95,.80,1.5,False,True),(0,3.95,.80,1.5,False,True),(2.5,3.95,.80,1.5,False,True),(-2.5,.72,.65,1.15,False,False)],1.3),
      ('sage_front',4.7,25.2,0,10.5,8.6,7.8,'plaster_cream','crossgable',[(-3.4,4.65,1.05,1.55,False,True),(-.1,4.65,1.05,1.55,False,True),(3.2,4.65,1.05,1.55,False,True),(-3.65,.85,.62,1.12,False,False),(1.7,.85,.62,1.12,False,False)],-1.6),
      ('east_tower_base',15.9,26.2,0,10.8,9,6.6,'plaster_sage','gable',[(-3.6,4.0,.8,1.45,False,False),(-.5,4,.8,1.45,False,False),(2.7,4,.8,1.45,False,False)],0),
      ('upper_left',-16,40,3.3,8.4,9,8,'plaster_white','gable',[(-2.5,4.2,.8,1.5,True,True),(1.4,4.2,.8,1.5,True,True)],0),
      ('upper_peach',-6,42,4.6,9.1,9,8.4,'plaster_peach','terrace',[(-2.6,4.3,.8,1.7,False,False),(1.6,4.3,.8,1.7,False,False)],0),
      ('upper_cream',5,41,5.8,10,9,8.5,'plaster_cream','terrace',[(-3,4.3,.8,1.7,False,False),(0,4.3,.8,1.7,False,False),(3,4.3,.8,1.7,False,False)],0),
      ('upper_sage',16,42,6.5,9,8,8,'plaster_sage','crossgable',[(-2.4,4.2,.8,1.7,True,False),(1.4,4.2,.8,1.7,True,False)],0),
      ('skyline_center',-1,54,9,10,8,7.8,'plaster_white','terrace',[(-2.5,4,.75,1.45,False,False),(1.2,4,.75,1.45,False,False)],0),
    ]
    for name,x,y,z,w,d,h,plaster,roof,openings,doorx in buildings:
        facade('tolbana_'+name,col,w,d,h,openings,(x,y,z+.22),0,False,plaster,roof,doorx,blue_door=name in ('west_tall','sage_front'))
        if z:
            podium=G();podium.box((x,y+d/2,z/2),(w+.3,d+.3,z),'stone_shade');podium.obj(name+'_retaining_stone',col,.012)
    facade('tolbana_left_street_return',col,12,8,7.3,[(-3.9,4.4,.8,1.4,False,False),(0,4.4,.8,1.4,False,False),(3.9,4.4,.8,1.4,False,False)],(-27,13,.22),90,False,'plaster_cream','crossgable',1.0)
    facade('tolbana_right_roof_wing',col,9,8,7.1,[(-2.6,4.1,.8,1.4,False,False),(.5,4.1,.8,1.4,False,False)],(31,25,.22),0,False,'plaster_cream','crossgable',-.5)
    facade('tolbana_right_square_tower',col,5.7,6.6,12.5,[(0,6.2,.85,1.55,False,False),(0,9.2,.75,1.35,False,False)],(23,25,.22),0,False,'plaster_white','hip',0,blue_door=True)
    leafy_tree(col,(-2.5,4,.22),18)
    fountain(col,(8.9,-5,.22))
    grass_patch(col,(-20.6,19.6,-12.6,17.5),lambda x,y:math.hypot(x-8.9,y+5)<3.2 or (7.6<x<11.4 and y>-2) or math.hypot(x+2.5,y-4)<1.3)
    props=G()
    for x,y in [(-17,-8),(-17,13),(-1,13),(17,13),(17,-8)]:
        # Small shrubs use individual leaves as well, with a rounded silhouette.
        for j in range(120):
            t=R.uniform(0,math.tau);r=R.uniform(0,.48);zz=R.uniform(.25,1.0)
            q=Vector((x+r*math.cos(t),y+r*math.sin(t),zz));u=Vector((.12,0,.03));v=Vector((0,.07,0))
            props.mesh([q-u,q+v,q+u,q-v,q+Vector((0,0,.035))],[(0,1,4),(1,2,4),(2,3,4),(3,0,4)],R.choice(['leaf','leaf_mid','leaf_dark']))
    for x,y in [(-15,-8),(16,12)]:
        for j in range(4):props.box((x,y-.28+j*.16,.65),(2.4,.145,.075),'wood_light')
        for side in (-1,1):props.box((x+side*.90,y,.35),(.12,.50,.60),'wood')
    props.obj('tolbana_shrubs_and_benches',col,.007)
    shop_stall('tolbana_foreground_market_table',col,(0,-14.5,.25),-9,3.0)
    cloth=G();cloth.box((0,-14.5,1.28),(3.05,1.01,.025),'canvas_red');cloth.obj('tolbana_market_red_cloth',col,.003)
    camera(sc,'Anime_Tolbana_Camera',(10,-24,4.7),(-2,16,2.7),66)
    return sc

def add_people(scenes):
    source=PROJECT.parents[1]/'ThreeHearthsVillage/Art/AincradCharacters/V2/AincradCharacters.blend'
    # Reuse visual meshes only. No resident identity, memory, rig, or state is installed.
    with bpy.data.libraries.load(str(source),link=False) as (src,dst):
        dst.objects=[n for n in src.objects if n in ('SK_Aileen','SK_Takuma','SK_Kashiwagi')]
    meshes=[o for o in dst.objects if o and o.type=='MESH']
    if len(meshes)!=3:raise RuntimeError('Existing character visual meshes missing')
    pose_scene=bpy.data.scenes.new('TemporaryVisualPoseBake');bpy.context.window.scene=pose_scene
    for ob in meshes:
        pose_scene.collection.objects.link(ob)
        rig=ob.parent
        if rig and rig.name not in pose_scene.objects:pose_scene.collection.objects.link(rig)
        if rig:rig.animation_data_clear();rig.matrix_world=Matrix.Identity(4)
        ob.hide_render=False;ob.hide_set(False)
    positions=[[(4.0,-12,20),(-3.1,-10,-15),(-1.5,2,110),(2.4,6,-30),(-3.5,12,30),(.7,16,175),(1.1,28,150),(-1.2,31,-40),
                (-.7,-4,15),(1.1,-2,165),(-3.2,6,-70),(3.7,14,100),(-.8,20,-30),(2.3,23,60),(-2.5,25,190)],
               [(11.7,-7,20),(5.5,-5.3,-35),(-1,9,120),(-1.6,-14,12),(17,12,45)]]
    baked={}
    for sc,poses in zip(scenes,positions):
        for i,(x,y,yaw) in enumerate(poses):
            proto=meshes[i%len(meshes)]
            variant=i%4;key=(proto.name,variant)
            if key not in baked:
                rig=proto.parent
                if not rig:raise RuntimeError('Character visual rig unavailable for relaxed pose bake')
                for bone in rig.pose.bones:
                    bone.rotation_mode='XYZ';bone.rotation_euler=(0,0,0);bone.location=(0,0,0)
                for side,label in [(-1,'L'),(1,'R')]:
                    bone=rig.pose.bones['upperarm.'+label];rest=bone.bone.matrix_local.to_3x3()
                    bone.rotation_euler=(rest.inverted()@Matrix.Rotation(side*.44,3,'Y')@rest).to_euler()
                    if variant in (1,3):
                        rig.pose.bones['thigh.'+label].rotation_euler.x=side*.15
                        rig.pose.bones['forearm.'+label].rotation_euler.x=-.12
                    if variant==2 and label=='R':rig.pose.bones['forearm.'+label].rotation_euler.x=-.75
                rig.pose.bones['head'].rotation_euler.z=[.06,-.10,.13,0][variant]
                bpy.context.view_layer.update()
                baked[key]=bpy.data.meshes.new_from_object(proto.evaluated_get(bpy.context.evaluated_depsgraph_get()))
            ob=bpy.data.objects.new(sc.name+'_visual_person_%02d'%i,baked[key]);sc.collection.objects.link(ob)
            ob.location=(x,y,.25);ob.rotation_euler.z=math.radians(yaw)
            ob['visual_prop_only']=True;ob['collision_enabled']=False;ob['style_id']=STYLE
    for ob in meshes:bpy.data.objects.remove(ob,do_unlink=True)
    bpy.data.scenes.remove(pose_scene)
    bpy.context.window.scene=scenes[0]

def main():
    print(json.dumps({'pid':os.getpid(),'stage':'crafted_model_build'}),flush=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scs=[market(),tolbana()];add_people(scs)
    unused=bpy.data.scenes.get('Scene')
    if unused:bpy.data.scenes.remove(unused)
    locations=[('beginnings_plaza',[-163,-4752],[-163,0,-18],78),('tolbana',[0,2900],[0,0,-7670],0)]
    manifest={'schema':2,'style_id':STYLE,'blender_version':bpy.app.version_string,'palette_srgb':base.PALETTE,
      'canonical_exact_coordinates':False,'npc_behavior_implemented':False,'prototype_origin_east_north_m':[0,-4770],
      'source_axes':'Blender east/north/up metres; glTF Y up','scenes':[]}
    for sc,(region,xy,xyz,yaw) in zip(scs,locations):
        bpy.context.window.scene=sc
        for ob in sc.objects:ob.select_set(ob.type=='MESH')
        path=EXPORT/(sc.name+'.glb')
        if 'use_active_scene' not in bpy.ops.export_scene.gltf.get_rna_type().properties:raise RuntimeError('Active-scene-only export unavailable')
        bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,use_active_scene=True,
          export_extras=True,export_cameras=False,export_lights=False,export_yup=True,export_apply=True)
        render_tris=0;coll_tris=0
        for ob in sc.objects:
            if ob.type!='MESH':continue
            ob.data.calc_loop_triangles()
            if ob.name.startswith('COL_'):coll_tris+=len(ob.data.loop_triangles)
            else:render_tris+=len(ob.data.loop_triangles)
        sc['region_id']=region;sc['floor_east_north_m']=xy
        manifest['scenes'].append({'id':sc.name,'region_id':region,'floor_east_north_m':xy,'godot_position_m':xyz,'godot_yaw_degrees':yaw,
          'glb':path.name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'triangles':render_tris,'collision_triangles':coll_tris,
          'collision_mesh_names':[o.name for o in sc.objects if o.name.startswith('COL_')],
          'reference_camera_blender_m':list(sc['reference_camera_position']),'reference_target_blender_m':list(sc['reference_camera_target']),
          'reference_horizontal_fov':sc['reference_horizontal_fov'],'mesh_objects':sum(o.type=='MESH' for o in sc.objects)})
        for ob in sc.objects:
            if ob.name.startswith('COL_'):ob.hide_render=True;ob.hide_set(True)
    geo=scene('Level0_Geographic_Placement')
    for sc,(region,xy,xyz,yaw) in zip(scs,locations):
        anchor=bpy.data.objects.new(region+'_anchor',None);geo.collection.objects.link(anchor)
        anchor.location=(*xy,0);anchor.rotation_euler.z=math.radians(yaw)
        for ob in sc.objects:
            if ob.type!='MESH' or ob.name.startswith('COL_'):continue
            linked=ob.copy();linked.data=ob.data;geo.collection.objects.link(linked);linked.parent=anchor;linked.matrix_parent_inverse=Matrix.Identity(4)
    camera(geo,'Floor_Geography',(6600,-5500,8800),(0,-700,0),58)
    bpy.context.window.scene=scs[0]
    for screen in bpy.data.screens:
        for area in screen.areas:
            if area.type=='VIEW_3D':
                area.spaces.active.region_3d.view_perspective='CAMERA';area.spaces.active.shading.color_type='MATERIAL';area.spaces.active.clip_end=20000
    target=HERE/'Level0_Anime_Reference_Scenes_HQ.blend'
    bpy.ops.wm.save_as_mainfile(filepath=str(target),compress=True)
    for dest in (HERE/'scene_manifest.json',EXPORT/'scene_manifest.json'):dest.write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps({'stage':'complete','blend':str(target),'scenes':manifest['scenes']}),flush=True)

if __name__=='__main__':main()
