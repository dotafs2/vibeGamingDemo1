"""Central market landmarks V4, assembled from measured image projections.

The caller owns the camera and supplies project_point(u, v, depth_y), returning
Blender east/north/up coordinates.  No reference image is loaded or embedded.
"""
import math
from mathutils import Vector
import sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT))
import build_reference_scenes_hq as hq
G=hq.G
# Measurements are in the original 2560x1440 frame.  The supplied crop is
# offset by (770,55); keep these anchors explicit so the root assembler can
# overlay the same points without treating them as surveyed world coordinates.
REFERENCE_CROP_OFFSET_PX=(770,55)
ACTIVE_SILHOUETTES_PX={
    'gate_arch_crown':(1380,657), 'gate_wall_left':(1000,550), 'gate_wall_right':(1515,550),
    'gate_slit_windows':[(1120,649),(1163,649),(1207,649)],
    'tower_roof_apex':(1554,105), 'tower_gallery_left':(1450,403), 'tower_gallery_right':(1665,403),
    'tower_shaft_left':(1490,475), 'tower_shaft_right':(1630,475),
    'large_dome_apex':(963,311), 'large_dome_shoulders':[(865,480),(1080,480)],
    'small_tower_apex':(1096,344), 'small_tower_shoulders':[(1045,500),(1150,500)]
}

def _mat(names,key,fallback):
    return names.get(key,names.get(fallback,fallback)) if isinstance(names,dict) else key
def _emit(g,col,name,materials):
    # hq.G.obj resolves the project palette; aliases let the caller retain its
    # own material naming without forcing a global style decision.
    o=g.obj(name,col,.006); o['style_id']='level0_market_architecture_v4'; o['material_aliases']=str(materials); return o
def _box(g,p,size,role): g.box(tuple(p),tuple(size),role)

def _cyl(g,cx,cy,z,r,h,role): g.curved((cx,cy,z),[(r,0),(r,.12),(r,h)],role,64)

def _gate(col,project,materials):
    y=23.0; base=project(1380/2560,735/1440,y); top=project(1257/2560,550/1440,y)
    cx=base.x; z0=base.z; height=max(5.5,top.z-z0); left=project(.45,.60,y).x; right=project(.70,.60,y).x; width=abs(right-left)
    ow=width*.30; r=ow*.5; spring=z0+height*.52; depth=2.8; g=G(); wallw=(width-ow)*.5
    for s in (-1,1): _box(g,(cx+s*(ow/2+wallw/2),y,z0+height/2),(wallw,depth,height),'limestone')
    # Closed curved spandrel: the face above the arch is one solid band.
    ns=48; vs=[]
    for yy in (y-depth/2,y+depth/2):
        for i in range(ns+1):
            a=math.pi-math.pi*i/ns; vs.append((cx+r*math.cos(a),yy,spring+r*math.sin(a)))
        for i in range(ns+1):
            a=math.pi-math.pi*i/ns; vs.append((cx+r*math.cos(a),yy,z0+height))
    fs=[]; stride=2*(ns+1)
    for i in range(ns): fs += [(i,i+1,ns+1+i+1,ns+1+i),(stride+i+1,stride+i,stride+ns+1+i,stride+ns+1+i+1),(i,stride+i,stride+i+1,i+1),(ns+1+i,ns+1+stride+i,ns+1+stride+i+1,ns+1+i+1)]
    fs += [(0,ns+1,stride+ns+1,stride),(ns,2*ns+1,stride+2*ns+1,stride+ns)]
    g.mesh(vs,fs,'limestone')
    # Three small dark slits on the left upper face, positioned from the
    # observed x≈1110/1150/1200 region rather than centered over the arch.
    fy=y-depth/2-.04
    for i in range(3):
        x=project((1120+i*43)/2560,649/1440,y).x; _box(g,(x,fy,project((1120+i*43)/2560,649/1440,y).z),(.30,.05,.55),'recess')
    for z in (z0+.28,z0+height-.25): _box(g,(cx,y,z),(width+.18,depth+.16,.18),'stone_light')
    return {'object':_emit(g,col,'V4_gate_closed_masonry',materials),'center':(cx,y,z0),'width':width,'height':height,'opening_width':ow,'opening_radius':r,'depth':depth}

def _tower(col,project,materials):
    y=26.0; p=project(1554/2560,735/1440,y); cx=p.x; base=.24
    shaft_r=abs(project(1630/2560,475/1440,y).x-project(1490/2560,475/1440,y).x)*.5
    balcony_r=abs(project(1665/2560,403/1440,y).x-project(1450/2560,403/1440,y).x)*.5
    gallery_r=balcony_r*.98; shaft_h=project(1554/2560,461/1440,y).z-base; gallery_bottom=base+shaft_h; gallery_h=project(1554/2560,295/1440,y).z-project(1554/2560,461/1440,y).z
    roof_apex=project(1554/2560,105/1440,y); roof_h=roof_apex.z-(gallery_bottom+gallery_h+.25); g=G(); _cyl(g,cx,y,base,shaft_r,shaft_h,'limestone')
    for z,r in [(gallery_bottom-.12,balcony_r),(gallery_bottom+.66,balcony_r*.98)]: g.curved((cx,y,z),[(r,0),(r+.10,.12),(r,.28)],'stone_light',64)
    cols=8; bay=math.tau/cols; spring=gallery_bottom+1.15; top=gallery_bottom+gallery_h
    for j in range(cols):
        a=j*bay; px=cx+gallery_r*math.cos(a); py=y+gallery_r*math.sin(a); g.tube([(px,py,gallery_bottom+.18),(px,py,spring)],[.14,.14],'stone_light',10)
        mid=a+bay*.5; inner=gallery_r-.15; outer=gallery_r+.08; ns=14; vv=[]
        for i in range(ns+1):
            t=-1+2*i/ns; aa=mid+t*bay*.5; zb=spring+1.15*math.sqrt(max(0,1-t*t)); vv.extend([(cx+inner*math.cos(aa),y+inner*math.sin(aa),zb),(cx+inner*math.cos(aa),y+inner*math.sin(aa),top),(cx+outer*math.cos(aa),y+outer*math.sin(aa),zb),(cx+outer*math.cos(aa),y+outer*math.sin(aa),top)])
        ff=[]
        for i in range(ns):
            q=4*i;n=q+4;ff += [(q,n,n+1,q+1),(q+2,q+3,n+3,n+2),(q,q+2,n+2,n),(q+1,n+1,n+3,q+3)]
        ff += [(0,1,3,2),(4*ns,4*ns+2,4*ns+3,4*ns+1)]; g.mesh(vv,ff,'stone_light')
    g.curved((cx,y,top),[(gallery_r+.30,0),(gallery_r+.34,.22),(gallery_r+.27,.36)],'stone_light',64)
    g.curved((cx,y,top+.25),[(gallery_r+.36,0),(gallery_r*.80,roof_h*.35),(gallery_r*.52,roof_h*.68),(.06,roof_h)],'roof',64)
    return {'object':_emit(g,col,'V4_round_tower_shaft_gallery_roof',materials),'center':(cx,y,base),'shaft_radius':shaft_r,'balcony_radius':balcony_r,'gallery_radius':gallery_r,'roof_height':roof_h}

def _large_dome(col,project,materials):
    ground=project(963/2560,735/1440,39.0); apex=project(963/2560,311/1440,39.0); eave=project(963/2560,426/1440,39.0); g=G(); r=abs(project(1080/2560,426/1440,39.0).x-project(865/2560,426/1440,39.0).x)*.5; drum=eave.z-ground.z; cap=apex.z-eave.z; _cyl(g,ground.x,39,ground.z,r,drum,'plaster_white'); g.curved((ground.x,39,eave.z),[(r,0),(r*.98,.25),(r*.82,cap*.55),(r*.52,cap*.84),(.06,cap)],'slate',64); return {'object':_emit(g,col,'V4_large_dome_wide_wall',materials),'center':tuple(ground),'radius':r,'drum_height':drum,'apex_projection':tuple(apex)}

def _small_window_tower(col,project,materials):
    ground=project(1096/2560,485/1440,37.0); apex=project(1096/2560,344/1440,37.0); eave=project(1096/2560,413/1440,37.0); g=G(); r=abs(project(1150/2560,413/1440,37.0).x-project(1045/2560,413/1440,37.0).x)*.5; drum=eave.z-ground.z; cap=apex.z-eave.z; _cyl(g,ground.x,37,ground.z,r,drum,'plaster_white'); top=ground.z+drum
    for j in range(6):
        a=j*math.tau/6; g.tube([(p.x+(r+.04)*math.cos(a),37+(r+.04)*math.sin(a),p.z+.4),(p.x+(r+.04)*math.cos(a),37+(r+.04)*math.sin(a),top-.2)],[.045,.045],'stone_light',8)
    g.curved((ground.x,37,top),[(r+.05,0),(r*.72,cap*.62),(.05,cap)],'slate',48); return {'object':_emit(g,col,'V4_small_window_tower',materials),'center':tuple(ground),'radius':r,'apex_projection':tuple(apex)}

def build(col, project_point, material_names):
    """Create V4 central landmarks at projection-derived positions."""
    gate=_gate(col,project_point,material_names); tower=_tower(col,project_point,material_names); large=_large_dome(col,project_point,material_names); small=_small_window_tower(col,project_point,material_names)
    return {'objects':[gate['object'].name,tower['object'].name,large['object'].name,small['object'].name],'gate':gate,'tower':tower,'large_dome':large,'small_window_tower':small,'projection':'image_uv_at_depth_y','source_reference_offset_px':[770,55]}
