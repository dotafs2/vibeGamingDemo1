"""Reusable solid landmarks for the Starting Town reference scene.

Coordinates are local Blender metres: x east, y depth, z up.  These helpers
only create geometry in the supplied collection; they do not create scenes,
cameras, materials, or export files.
"""
import math, sys
from pathlib import Path
from mathutils import Vector

HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE.parent))
import build_reference_scenes_hq as hq

G=hq.G
_mesh=hq.mat

def _obj(g,col,name,bevel=.01,collision=False):
    o=g.obj(name,col,bevel,collision); o['style_id']='level0_anime_landmark_v1'; return o

def _arch_ring(g,cx,cy,cz,inner,thickness,depth,role='stone_light',segments=32):
    """Extruded semicircular voussoir ring, open below spring."""
    outer=inner+thickness; vs=[]
    # front/back vertices, ordered inner then outer along the upper semicircle
    for y in (cy-depth*.5,cy+depth*.5):
        for r in (inner,outer):
            for i in range(segments+1):
                a=math.pi*i/segments
                vs.append((cx+r*math.cos(a),y,cz+r*math.sin(a)))
    fs=[]; stride=(segments+1)
    # ring front/back strips and extruded inner/outer edges
    for side in (0,1):
        base=side*2*stride
        for i in range(segments): fs.append((base+i,base+i+1,base+stride+i+1,base+stride+i))
    for i in range(segments):
        a=i; b=i+1; c=2*stride+i; d=2*stride+i+1
        fs.extend([(a,b,d,c),(stride+a,2*stride+stride+a,2*stride+stride+b,stride+b)])
    g.mesh(vs,fs,role)

def _fill_above_arch(g,cx,cy,z0,z1,width,radius,role='limestone',segments=28):
    """Fill every interval above the true arch curve, leaving the void open."""
    for i in range(segments):
        x0=-width/2+i*width/segments; x1=-width/2+(i+1)*width/segments
        xm=(x0+x1)*.5
        arch=z0+math.sqrt(max(0,radius*radius-xm*xm))
        if arch<z1: g.box((cx+xm,cy,(arch+z1)*.5),(x1-x0-.012,z1-arch,.0+0.0+0.46),role)

def build_gate(col, center, width, height, opening_width, opening_height, depth):
    """Build a thick, walkable arched gate with solid wall above its curve."""
    cx,cy,base=center; g=G(); r=opening_width*.5; spring=base+opening_height-r
    wallw=(width-opening_width)*.5
    # Side piers continue to spring line; segmented infill follows arch curve.
    for sx in (-1,1): g.box((cx+sx*(opening_width/2+wallw/2),cy,base+height/2),(wallw,depth,height),'limestone')
    # A single closed extrusion fills the whole spandrel above the arch.
    seg=64; vs=[]
    for yy in (cy-depth*.5,cy+depth*.5):
        for i in range(seg+1):
            a=math.pi-math.pi*i/seg; vs.append((cx+r*math.cos(a),yy,spring+r*math.sin(a)))
        for i in range(seg+1):
            a=math.pi-math.pi*i/seg; vs.append((cx+r*math.cos(a),yy,base+height))
    fs=[]; stride=2*(seg+1)
    for i in range(seg):
        fs.extend([(i,i+1,seg+1+i+1,seg+1+i),(stride+i+1,stride+i,stride+seg+1+i,stride+seg+1+i+1),(i,stride+i,stride+i+1,i+1),(seg+1+i,seg+1+stride+i,seg+1+stride+i+1,seg+1+i+1)])
    fs.extend([(0,seg+1,stride+seg+1,stride),(seg,2*seg+1,stride+2*seg+1,stride+seg)])
    g.mesh(vs,fs,'limestone')
    # Full front masonry uses real .7m x .32m blocks.  Every block crossing
    # the opening gets a curved lower edge sampled at both ends, so the arch
    # edge is clipped exactly without long vertical strips.
    fy=cy-depth*.5-.018; brick_h=.32; brick_w=.70
    for row in range(math.ceil(height/brick_h)):
        z0=base+row*brick_h; z1=min(base+height,z0+brick_h-.018)
        offset=0 if row%2==0 else brick_w*.5
        for k in range(-16,17):
            x0=cx-width/2+offset+k*brick_w; x1=x0+brick_w*.94
            if x0<cx-width/2 or x1>cx+width/2 or z1<=z0: continue
            if z1<=spring: hole_half=r
            elif z1<spring+r: hole_half=math.sqrt(max(0,r*r-(z1-spring)**2))
            else: hole_half=0
            # Subdivide at <=7cm and reject only the opening interval at the
            # top edge, preventing a sloped brick from crossing the void.
            pieces=max(1,math.ceil((x1-x0)/.07))
            for q in range(pieces):
                xa=x0+(x1-x0)*q/pieces; xb=x0+(x1-x0)*(q+1)/pieces; xm=(xa+xb)*.5
                if abs(xm-cx)<hole_half: continue
                def curve_at(x):
                    d=x-cx
                    return spring+math.sqrt(max(0,r*r-d*d)) if abs(d)<r else base
                ba=max(z0,curve_at(xa)+.005); bb=max(z0,curve_at(xb)+.005)
                if min(ba,bb)>=z1-.005: continue
                vv=[(xa,fy,ba),(xb,fy,bb),(xb,fy,z1),(xa,fy,z1),(xa,fy+.045,ba),(xb,fy+.045,bb),(xb,fy+.045,z1),(xa,fy+.045,z1)]
                g.mesh(vv,[(0,1,2,3),(7,6,5,4),(0,4,5,1),(3,2,6,7),(0,3,7,4),(1,5,6,2)],'limestone')
    # Forty shallow independent front voussoir stones provide readable radial
    # seams while the closed extrusion remains the structural wall.
    fy_ring=cy-depth*.5-.12
    for i in range(40):
        a0=math.pi*i/40+.012; a1=math.pi*(i+1)/40-.012; ro=r+.28; y0=fy_ring; y1=fy_ring+.12
        vv=[]
        for yy in (y0,y1):
            vv += [(cx+r*math.cos(a0),yy,spring+r*math.sin(a0)),(cx+r*math.cos(a1),yy,spring+r*math.sin(a1)),(cx+ro*math.cos(a1),yy,spring+ro*math.sin(a1)),(cx+ro*math.cos(a0),yy,spring+ro*math.sin(a0))]
        g.mesh(vv,[(0,1,2,3),(7,6,5,4),(0,4,5,1),(1,5,6,2),(2,6,7,3),(3,7,4,0)],'stone_light')
    # Only thin exterior voussoir rings remain; the closed extrusion above
    # already supplies the complete inner curved soffit.
    _arch_ring(g,cx,cy-depth*.5-.06, spring,r,.28,.12,'stone_light',40)
    _arch_ring(g,cx,cy+depth*.5+.06, spring,r,.28,.12,'stone_light',40)
    # Three small defensive slit windows in the front masonry, each with a
    # dark recessed back and a projecting stone surround.
    fy=cy-depth*.5-.035
    for j,xoff in enumerate((-width*.38,-width*.29,-width*.20)):
        g.box((cx+xoff,fy,base+height*.73),(.30,.08,.60),'recess')
        g.box((cx+xoff-.17,fy-.035,base+height*.73),(.045,.12,.72),'stone_light')
        g.box((cx+xoff+.17,fy-.035,base+height*.73),(.045,.12,.72),'stone_light')
        g.box((cx+xoff,fy-.035,base+height*.73+.36),(.39,.12,.045),'stone_light')
        g.box((cx+xoff,fy-.035,base+height*.73-.36),(.39,.12,.045),'stone_light')
    # masonry courses and crenellation are attached to the solid parapet.
    for z in (base+.32,base+height-.28): g.box((cx,cy,z),(width+.18,depth+.18,.18),'stone_light')
    for i in range(15): g.box((cx-width/2+.5+i*(width-1)/14,cy,base+height+.42),(.42,depth,.75),'stone_light')
    _obj(g,col,'market_v3_solid_gate',.012)
    c=G();
    for sx in (-1,1): c.box((cx+sx*(opening_width/2+wallw/2),cy,base+height/2),(wallw,depth,height))
    c.box((cx,cy,(spring+r+base+height)/2),(width,depth,base+height-(spring+r)))
    _obj(c,col,'COL_market_v3_solid_gate',0,True)
    return {'name':'market_v3_solid_gate','opening_spring_z':spring,'opening_radius':r,'depth':depth}

def build_tower(col, center, radius, base_height, gallery_height, roof_height):
    """Build the reference tower: slim drum, balcony, open arched gallery, roof."""
    cx,cy,base=center; g=G(); drum_top=base+base_height; gallery_top=drum_top+gallery_height
    g.curved((cx,cy,base),[(radius,0),(radius,.22),(radius,base_height-.18),(radius+.12,base_height)],'limestone',72)
    for z in (base+.4,drum_top-.25): g.curved((cx,cy,z),[(radius+.06,0),(radius+.10,.14),(radius+.06,.30)],'stone_light',56)
    cols=8; ring=radius*1.07; bay=math.pi*2/cols; half_chord=ring*math.sin(bay*.5)
    for j in range(cols):
        a=j*bay; p=(cx+ring*math.cos(a),cy+ring*math.sin(a),drum_top)
        rise=1.25; spring=gallery_top-rise-.22
        g.tube([p,(p[0],p[1],spring)],[.16,.13],'stone_light',12)
        # Continuous cylindrical arch wall.  Each bay is bounded by the
        # columns; its lower edge is a true semicircle in angular space and
        # the inner/outer radial surfaces are closed, avoiding rotated planar
        # arches landing on the columns.
        mid=a+bay*.5; inner=ring-.18; outer=ring+.08; ns=20; av=[]
        for i in range(ns+1):
            t=-1+2*i/ns; aa=mid+t*bay*.5; zb=spring+rise*math.sqrt(max(0,1-t*t))
            av.extend([(cx+inner*math.cos(aa),cy+inner*math.sin(aa),zb),(cx+inner*math.cos(aa),cy+inner*math.sin(aa),gallery_top),(cx+outer*math.cos(aa),cy+outer*math.sin(aa),zb),(cx+outer*math.cos(aa),cy+outer*math.sin(aa),gallery_top)])
        af=[]
        for i in range(ns):
            q=4*i; n=q+4
            af.extend([(q,n,n+1,q+1),(q+2,q+3,n+3,n+2),(q,q+2,n+2,n),(q+1,n+1,n+3,q+3)])
        af.extend([(0,1,3,2),(4*ns,4*ns+2,4*ns+3,4*ns+1)])
        ag=G(); ag.mesh(av,af,'stone_light'); ao=ag.obj('tower_cylindrical_arch_wall_%02d'%j,col,.004)
        # square capital directly above each column
        g.box((p[0],p[1],spring-.08),(.42,.42,.20),'stone_light')
    # A projecting balcony has a lower moulding, parapet rail and dark red
    # infill visible between the front columns.
    g.curved((cx,cy,drum_top+.05),[(ring+.28,0),(ring+.34,.18),(ring+.28,.36)],'stone_light',64)
    g.curved((cx,cy,drum_top+.68),[(ring+.05,0),(ring+.11,.11),(ring+.05,.22)],'roof_dark',64)
    for j in range(20):
        a=j*math.tau/20; g.tube([(cx+(ring+.04)*math.cos(a),cy+(ring+.04)*math.sin(a),drum_top+.08),(cx+(ring+.04)*math.cos(a),cy+(ring+.04)*math.sin(a),drum_top+.65)],[.035,.035],'roof_dark',8)
    g.curved((cx,cy,drum_top+.68),[(ring+.08,0),(ring+.13,.10),(ring+.08,.20)],'stone_light',64)
    g.curved((cx,cy,gallery_top),[(ring+.25,0),(ring+.31,.18),(ring+.25,.35)],'stone_light',64)
    # Back half of gallery is a recessed shadow wall, preserving depth while
    # preventing a see-through skybox behind the open front arcade.
    vs=[]; fs=[]; seg=24
    for i in range(seg+1):
        a=math.pi*i/seg
        for z in (drum_top+.48,gallery_top-.55): vs.append((cx+ring*.72*math.cos(a),cy+ring*.72*math.sin(a),z))
    for i in range(seg): fs.append((2*i,2*i+1,2*i+3,2*i+2))
    g.mesh(vs,fs,'recess')
    # Roof starts directly above the gallery, with no enclosed neck drum.
    roof_base=gallery_top+.25
    g.curved((cx,cy,roof_base),[(ring+.35,0),(ring+.38,.18),(ring*.82,roof_height*.30),(ring*.62,roof_height*.55),(ring*.38,roof_height*.78),(.08,roof_height)],'roof',72)
    for j in range(18):
        a=j*math.tau/18; g.tube([(cx+(ring+.30)*math.cos(a),cy+(ring+.30)*math.sin(a),roof_base+.10),(cx+.08*math.cos(a),cy+.08*math.sin(a),roof_base+roof_height)],[.018,.010],'roof_dark',5)
    g.tube([(cx,cy,roof_base+roof_height),(cx,cy,roof_base+roof_height+.62)],[.07,.025],'iron',12)
    _obj(g,col,'market_v3_round_tower_arch_gallery',.01)
    c=G();c.cylinder((cx,cy,base+base_height/2),radius,base_height,'limestone',40);_obj(c,col,'COL_market_v3_round_tower',0,True)
    return {'name':'market_v3_round_tower_arch_gallery','gallery_columns':cols,'gallery_top_z':gallery_top,'roof_starts_directly_at_gallery':True}

def build_dome_cluster(col, centers, radius, drum_height, dome_height):
    """Build offset domes with cylindrical drums, neck windows and true caps."""
    g=G()
    for j,(cx,cy,base) in enumerate(centers):
        top=base+drum_height
        g.curved((cx,cy,base),[(radius,0),(radius,.22),(radius,drum_height)],'plaster_white',56)
        # Elliptic half-cap; dome_height controls the vertical extent.
        profile=[(radius*math.cos((math.pi*.5)*i/16),dome_height*math.sin((math.pi*.5)*i/16)) for i in range(17)]
        g.curved((cx,cy,top),[(r,z) for r,z in profile],'slate',64)
        # Tall narrow arched windows and thin mullions on the drum, oriented
        # radially so the cluster reads correctly from oblique viewpoints.
        win_ring=radius*1.04; win_r=radius*.13; win_bottom=top-2.0; win_top=top-.30; win_spring=win_top-win_r
        for k in range(8):
            a=k*math.tau/8; mid=a
            ag=G(); ag.arch(0,0,win_spring,win_r,.065,.10,'stone_light',16)
            ao=ag.obj('dome_arch_window_%02d'%k,col,.004); ao.location=(cx+win_ring*math.cos(mid),cy+win_ring*math.sin(mid),0); ao.rotation_euler.z=mid+math.pi*.5
            bg=G(); bg.arch_fill(0,.02,win_bottom,win_r*2.0,win_top-win_bottom,'recess')
            bo=bg.obj('dome_arch_recess_%02d'%k,col,0); bo.location=(cx+win_ring*.995*math.cos(mid),cy+win_ring*.995*math.sin(mid),0); bo.rotation_euler.z=mid+math.pi*.5
            fg=G(); fg.tube([(-win_r*1.05,0,win_bottom),(-win_r*1.05,0,win_spring)],[.045,.045],'stone_light',8); fg.tube([(win_r*1.05,0,win_bottom),(win_r*1.05,0,win_spring)],[.045,.045],'stone_light',8)
            fo=fg.obj('dome_arch_window_side_frames_%02d'%k,col,.003); fo.location=(cx+win_ring*math.cos(mid),cy+win_ring*math.sin(mid),0); fo.rotation_euler.z=mid+math.pi*.5
        g.curved((cx,cy,top-.04),[(radius+.07,0),(radius+.11,.16),(radius+.07,.30)],'stone_light',48)
    _obj(g,col,'market_v3_offset_true_dome_cluster',.008)
    return {'name':'market_v3_offset_true_dome_cluster','count':len(centers),'dome_profile':'rounded_multi_ring'}
