"""Market reference assembly: camera-relative measured anchors and crafted solids.

Root owns scene assembly. craft_landmarks.py is independently authored/reviewed.
The imported anime is never used as geometry, texture, or background.
"""
import bpy, sys, math, json, os, hashlib, random
from pathlib import Path
from mathutils import Vector, Matrix
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE));sys.path.insert(0,str(HERE.parent))
import build_reference_scenes_hq as hq
import build_reference_scenes as base
import craft_landmarks as craft
G=hq.G
PROJECT=HERE.parents[2]
EXPORT=PROJECT/'assets/reference_scenes'
CAM=Vector((0,-19,1.80)); TARGET=Vector((-1.2,25,5.45)); HFOV=64.0
FORWARD=(TARGET-CAM).normalized(); RIGHT=FORWARD.cross(Vector((0,0,1))).normalized(); UP=RIGHT.cross(FORWARD)
TAN_H=math.tan(math.radians(HFOV/2)); TAN_V=TAN_H*9/16
R=random.Random(91429)
MARKERS={}
base.PALETTE.update({'canvas_blue':'75AEB7','canvas_rust':'A96C4C','canvas_leaf':'657F47','tile_red':'A97873'})

def ray_at(u,v,depth_y):
    ray=FORWARD+RIGHT*((u-.5)*2*TAN_H)+UP*((.5-v)*2*TAN_V)
    return CAM+ray*((depth_y-CAM.y)/ray.y)

def emit(g,col,name,bevel=.008,collision=False):
    ob=g.obj(name,col,bevel,collision)
    ob['style_id']='level0_anime_market_demo_v3'
    return ob

def marker(name,position): MARKERS[name]=list(position)

def arch_window(g,cx,z,w,h,shutters=False,bars=False):
    # Actual inset arch and masonry reveal; thickness is depth, never height.
    rad=w*.5; spring=z+h-rad
    g.arch_fill(cx,.16,z,w,h,'recess')
    g.arch(cx,-.045,spring,rad,.075,.19,'stone_light',24)
    for side in (-1,1):
        g.box((cx+side*(rad+.037),-.035,z+(h-rad)/2),(.075,.21,h-rad),'stone_light')
        for k in range(10):
            xx=(k+.5)*rad/10; curve=spring+math.sqrt(max(0,rad*rad-xx*xx))
            if z+h>curve:g.box((cx+side*xx,.075,(curve+z+h)/2),(rad/10+.003,.24,z+h-curve),'limestone')
    g.box((cx,-.09,z-.06),(w+.19,.32,.10),'stone_light')
    if bars:
        for xx in (-w*.26,0,w*.26):g.box((cx+xx,.105,z+h*.42),(.018,.045,h*.8),'iron')
        for zz in (.35,h*.57):g.box((cx,.10,z+zz),(w*.90,.045,.024),'iron')
    if shutters:
        for side in (-1,1):
            sw=w*.39; panel=G()
            for j in range(5):panel.box(((j-2)*sw/5,0,h*.40),(sw/5-.006,.055,h*.80),'shutter')
            for zz in (.18,h*.68):panel.box((0,-.038,zz),(sw,.035,.05),'wood_dark')
            g.add(panel,(cx+side*(rad+sw*.55+.08),-.12,z+.04),side*8)

def frontage(col,name,side,start_y,end_y,height,upper_windows,shop_bays,parapet=True,street_x=None):
    # The left and right facades are individual street-facing surfaces with depth.
    x=side*5.65 if street_x is None else street_x; length=end_y-start_y; center_y=(start_y+end_y)/2
    yaw=90 if side<0 else -90
    sign_local=1 if side<0 else -1
    holes=[]; detail=G(); shell=G()
    for abs_y,z,w,h,shutters in upper_windows:
        local_x=(abs_y-center_y)*sign_local
        holes.append((local_x-w/2,local_x+w/2,z,z+h))
        arch_window(detail,local_x,z,w,h,shutters,not shutters)
    for abs_y,w,h in shop_bays:
        local_x=(abs_y-center_y)*sign_local
        holes.append((local_x-w/2,local_x+w/2,0,h))
        arch_window(detail,local_x,0,w,h,False,False)
        # Recess extends behind the street face, with a real floor and return walls.
        detail.box((local_x,1.4,h/2),(w, .10,h),'wood_dark')
        for dx in (-w/2,w/2): detail.box((local_x+dx,.72,h/2),(.13,1.55,h),'stone_shade')
        detail.box((local_x,.7,.02),(w,1.55,.1),'wood')
        detail.box((local_x-w*.35,.55,h*.46),(.11,.09,h*.87),'wood')
    hq.wall(shell,length,height,holes,True,'limestone')
    depth=7.0
    for end in (-length/2,length/2):shell.box((end,depth/2,height/2),(.26,depth,height),'limestone')
    shell.box((0,depth-.1,height/2),(length,.22,height),'stone_shade')
    shell.box((0,depth/2,height),(length+.16,depth,.20),'stone_light')
    # Only the reference's restrained string courses; no uniform storey slabs.
    detail.box((0,-.035,height-.16),(length,.24,.14),'stone_light')
    if parapet:
        count=max(2,round(length/.95))
        for j in range(count):
            xx=-length/2+(j+.5)*length/count
            detail.box((xx,.06,height+.39),(.34,.46,.72),'stone_light')
            detail.box((xx,.06,height+.77),(.39,.51,.075),'tile_red' if name=='left_mid' else 'stone_light')
    transform=Matrix.Translation(Vector((x,center_y,.24)))@Matrix.Rotation(math.radians(yaw),4,'Z')
    emit(shell,col,name+'_stone_wall',.009).matrix_world=transform
    emit(detail,col,name+'_openings_parapet',.005).matrix_world=transform
    collision=G()
    for l,r,b,t in hq.rect_cut(-length/2,length/2,0,height,holes):
        collision.box(((l+r)/2,.1,(b+t)/2),(r-l,.28,t-b))
    for end in (-length/2,length/2):collision.box((end,depth/2,height/2),(.26,depth,height))
    emit(collision,col,'COL_'+name,0,True).matrix_world=transform

def cloth(col,name,side,y0,y1,attach_z,edge_z,projection,colors,wall_position=None):
    # Shared-edge cloth surface, thickness, seams and a hanging scalloped valance.
    g=G();g.smoothing=True;frame=G();wall_x=side*5.78 if wall_position is None else wall_position;edge_x=wall_x-side*projection
    count=max(3,round((y1-y0)/1.05)); boundaries=[y0]
    weights=[1+.13*math.sin(j*2.7) for j in range(count)]
    for weight in weights:boundaries.append(boundaries[-1]+(y1-y0)*weight/sum(weights))
    for j in range(count):
        a,b=boundaries[j:j+2]; verts=[];faces=[];nu=10;nv=8
        def height(s,t):
            yy=a+(b-a)*s
            broad=.16*math.sin((yy-y0)/(y1-y0)*math.pi)
            fold=.14*math.sin(s*math.pi)*(.3+.7*t)
            return attach_z*(1-t)+edge_z*t-.25*math.sin(t*math.pi)-fold-broad*t
        for back in (0,1):
            for iu in range(nu+1):
                t=iu/nu
                for iv in range(nv+1):
                    s=iv/nv;verts.append((wall_x+(edge_x-wall_x)*t,a+(b-a)*s,height(s,t)-back*.018))
        stride=(nu+1)*(nv+1)
        for offset in (0,stride):
            for iu in range(nu):
                for iv in range(nv):
                    n=offset+iu*(nv+1)+iv;faces.append((n,n+1,n+nv+2,n+nv+1))
        for iv in range(nv):
            n=nu*(nv+1)+iv;faces.append((n,n+stride,n+stride+1,n+1))
        role=colors[j%len(colors)];g.mesh(verts,faces,role)
        val=[]
        for iv in range(17):
            s=iv/16;zz=height(s,1)
            val.extend([(edge_x,a+(b-a)*s,zz),(edge_x-side*.012,a+(b-a)*s,zz-.10-.17*math.sin(s*math.pi))])
        g.mesh(val,[(k*2,k*2+1,k*2+3,k*2+2) for k in range(16)],role)
        g.tube([(wall_x+(edge_x-wall_x)*k/16,a,height(0,k/16)+.008) for k in range(17)],[.008]*17,'canvas_cream',5)
    for yy in (y0+.12,y1-.12):
        frame.tube([(edge_x,yy,.25),(edge_x,yy,edge_z-.09)],[.035,.028],'wood_dark',10)
        frame.tube([(wall_x,yy,attach_z+.15),(edge_x,yy,edge_z+.025)],[.014,.014],'wood_dark',8)
    frame.beam((edge_x,y0,edge_z-.08),(edge_x,y1,edge_z-.08),.043,'wood')
    emit(g,col,name+'_fabric',.0015);emit(frame,col,name+'_supports',.003)
    return (edge_x,y1,edge_z-.23)

def props(col):
    # Stall shapes and goods are distinct foreground objects with actual volume.
    for index,(x,y,yaw,width) in enumerate([(-3.95,-13.6,90,2.6),(-3.8,-8.8,90,2.2),(-3.65,-3.6,90,2.4),(-3.8,2.4,90,2.0),(-3.9,9.2,90,1.9),(4.4,-1.7,-90,2.4),(4.25,5.8,-90,2.0),(4.3,13,-90,1.9)]):
        hq.shop_stall('market_v3_stall_%02d'%index,col,(x,y,.24),yaw,width)
    g=G()
    for x,y,s in [(-3.1,-11.5,.7),(-3.0,-5.0,.65),(-4.2,6,.7),(3.65,-8.8,.85),(3.9,-4.5,.8),(3.1,11,.7)]:
        hq.barrel(g,(x,y,.24),s,True)
        hq.crate(g,(x+.55,y+.5,.24),(.65,.60,.45))
    # Reference right foreground basket carries long shafts, angled at varying slopes.
    for j in range(11):
        x=3.7+R.uniform(-.2,.2);y=-8.8+R.uniform(-.25,.25)
        g.tube([(x,y,.5),(x+R.uniform(.15,.5),y+.25,2.3+R.uniform(0,.6))],[.022,.015],'wood_dark',8)
    # Upright wooden baskets, clay jugs and hanging wares break regular table rhythms.
    for x,y in [(-3.2,-12.9),(-2.8,-7.0),(-4.2,3.0),(4.1,-5.5)]:
        g.curved((x,y,.24),[(.24,0),(.30,.12),(.34,.55),(.22,.78),(.14,.83),(.14,.98),(.17,1.01)],'stone_shade',24)
        g.curved((x+.42,y+.17,.24),[(.18,0),(.27,.25),(.20,.50),(.11,.60)],'canvas_rust',20)
    emit(g,col,'market_v3_baskets_jugs_and_equipment',.005)

def draped_shop_canopy(col,name,wall_x,edge_x,y0,y1,attach_z,edge_z,drop,colors,near_lowering=0.0):
    """The canopy recedes along the street, not across it as a front-facing banner.

    Separate cloth bays hang from seam supports. Sag and narrow gathered folds
    change across each bay; both the sloped roof and hanging hem are real meshes.
    """
    g=G();g.smoothing=True;seams=G();hem_points={}
    n=len(colors);nu=40;nv=24
    weights=[1.0+.17*math.sin(j*2.3) for j in range(n)];cuts=[y0]
    for weight in weights:cuts.append(cuts[-1]+(y1-y0)*weight/sum(weights))
    for bay,role in enumerate(colors):
        a,b=cuts[bay:bay+2]
        def roof(s,t):
            progress=(a+(b-a)*s-y0)/(y1-y0)
            z=attach_z*(1-t)+edge_z*t-.27*math.sin(math.pi*t)-near_lowering*(1-progress)
            z-=.065*math.sin(math.pi*s)**2*t
            z+=.025*math.cos(5*math.pi*s+2*t)*math.sin(math.pi*t)*math.sin(math.pi*s)
            return (wall_x+(edge_x-wall_x)*t,a+(b-a)*s,z)
        vs=[roof(i/nu,j/nv) for j in range(nv+1) for i in range(nu+1)]
        fs=[(j*(nu+1)+i,j*(nu+1)+i+1,(j+1)*(nu+1)+i+1,(j+1)*(nu+1)+i) for j in range(nv) for i in range(nu)]
        g.mesh(vs,fs,role)
        vs=[];rows=20
        for j in range(rows+1):
            t=j/rows
            for i in range(nu+1):
                s=i/nu;yy=a+(b-a)*s;progress=(yy-y0)/(y1-y0)
                sag=drop*(1-.22*progress)+.08*math.sin(math.pi*s)**2+.13*(1-progress)**2-near_lowering*(1-progress)
                # Folds tighten near seam supports and relax towards the hem.
                gathering=.038*math.sin(5*math.pi*s+.6*t)*math.sin(math.pi*s)*(.3+.7*t)
                billow=.03*math.sin(math.pi*t)*math.sin(math.pi*s)
                pos=(edge_x+billow+gathering,yy,roof(s,1)[2]-sag*t)
                vs.append(pos)
                if j==rows:hem_points.setdefault(role,[]).append(pos)
        g.mesh(vs,[(j*(nu+1)+i,j*(nu+1)+i+1,(j+1)*(nu+1)+i+1,(j+1)*(nu+1)+i) for j in range(rows) for i in range(nu)],role)
        seams.tube([roof(0,k/20) for k in range(21)],[.009]*21,'canvas_cream',6)
        seams.tube([vs[rows*(nu+1)+i] for i in range(nu+1)],[.006]*(nu+1),role,6)
    ob=emit(g,col,name+'_draped_fabric',0)
    solid=ob.modifiers.new('Woven cloth thickness','SOLIDIFY');solid.thickness=.012;solid.offset=0
    for yy in (y0+.12,y1-.12):
        seams.tube([(edge_x,yy,.24),(edge_x,yy,edge_z-.1)],[.028,.023],'wood_dark',10)
        seams.tube([(wall_x,yy,attach_z+.03),(edge_x,yy,edge_z+.01)],[.015,.015],'wood_dark',8)
    emit(seams,col,name+'_seams_and_supports',.001)
    return hem_points

def people(col):
    # Actor roots are normally scaled. Proximity, never body enlargement, sets crop.
    # The Godot scene replaces these small mesh markers with the existing rigged Kirito.
    entries=[(.84,-16.8,12),(-.30,-15.5,100),(-.05,-14.5,185),(.20,-12.2,250),
        (-.52,-10.0,85),(-.21,-10.0,35),(.11,-8.7,200),(.35,-7.0,20)]
    roots=[]
    for u_offset,y,yaw in entries:
        # First close silhouettes have explicit screen x targets.
        u=.835 if len(roots)==0 else [.12,.18,.265,.325,.46,.565,.645][len(roots)-1]
        p=ray_at(u,.8,y);roots.append((p.x,y,yaw))
    for u,y,yaw in [(.335,-5.5,40),(.375,-6.2,250),(.405,-4.8,100),(.52,-3.4,315),(.615,-2.6,110),(.665,-.8,200)]:
        p=ray_at(u,.8,y);roots.append((p.x,y,yaw))
    for row,(y,count) in enumerate([(1,3),(6,4),(12,5),(19,6),(28,4)]):
        for j in range(count):
            x=-2.5+(j+.5)*5/count+R.uniform(-.20,.20)
            roots.append((x,y+R.uniform(-2.7,2.7),R.choice([0,25,75,130,180,215,270])))
    for idx,(x,y,yaw) in enumerate(roots):
        g=G();g.box((0,0,.4),(.15,.15,.7),'wood')
        ob=emit(g,col,'StartingTown_Market_visual_person_%02d'%idx,0)
        ob.location=(x,y,.24);ob.rotation_euler.z=math.radians(yaw)
        if idx in (1,2):ob.scale=(.90,.90,.90)
        ob['visual_prop_only']=True
    marker('foreground_character_head_top',(roots[0][0],roots[0][1],.24+1.72))
    return roots

def banners(col):
    g=G()
    # Off-centre hanging pennants, avoiding a large new band over the landmark tower.
    for line,(y,z,x0,x1,n) in enumerate([(-7.5,8.8,-6.0,5.8,9),(26,11.15,-6,5.8,10)]):
        g.tube([(x0+(x1-x0)*j/28,y,z-.28*math.sin(j/28*math.pi)) for j in range(29)],[.009]*29,'wood_dark',6)
        for j in range(n):
            x=x0+.4+j*(x1-x0)/(n+1);zz=z-.28*math.sin((x-x0)/(x1-x0)*math.pi)
            g.mesh([(x,y,zz),(x+.43,y,zz),(x+.45,y+.035,zz-.82),(x,y+.03,zz-.86)],[(0,3,2,1)],['canvas_red','canvas_cream','canvas_green'][j%3])
    emit(g,col,'market_v3_pennants',.001)

def main():
    print(json.dumps({'pid':os.getpid(),'stage':'market_reference_assembly'}),flush=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc=hq.scene('StartingTown_Market_V3');col=sc.collection
    hq.camera(sc,'Anime_Market_V3_Camera',CAM,TARGET,HFOV)
    ground=G();ground.box((0,25,-.08),(45,120,.5),'paving');emit(ground,col,'market_v3_ground_base',.002)
    c=G();c.box((0,25,-.08),(45,120,.5));emit(c,col,'COL_market_v3_ground',0,True)
    hq.stone_path('market_v3_fitted_paving',col,(-6.0,6.0,-22,65),.20,.65)
    frontage(col,'left_near',-1,-17,-5.9,12.0,[(-14,5.9,.72,1.85,False),(-10.5,6.0,.8,1.85,False),(-6.8,6,.75,2.0,False),(-14,9.5,.56,1.85,False),(-10,9.5,.58,1.85,False),(-6.8,9.5,.6,1.9,False)],[(-12.6,2.3,2.8),(-7,1.9,2.8)])
    frontage(col,'left_mid',-1,-5.9,9,5.15,[(y,3.0,.60,1.65,False) for y in (-4.6,-2.4,-.2,2,4.2,6.4,8.1)],[(0,1.4,2.6),(4,1.5,2.6),(7.7,1.2,2.6)],street_x=-4.5)
    frontage(col,'left_far',-1,9,23,4.65,[(y,3.25,.50,1.05,False) for y in (11,13,15,17,19,21)],[(11.5,1.7,2.6),(15.5,1.8,2.6),(20,1.7,2.6)],street_x=-4.5)
    frontage(col,'right_near',1,-2.8,13.4,13.5,[(y,3.45,.68,2.65,True) for y in (-1,3,7,11)]+[(y,10.9,.56,2.1,False) for y in (-1,3,7,11)],[(-1,1.4,2.8),(7,1.6,2.7),(12,1.7,2.7)],street_x=7.4)
    frontage(col,'right_far',1,13.4,24,11.1,[(y,3.45,.68,2.65,True) for y in (15,19,22)]+[(y,8.3,.56,1.8,False) for y in (15,19,22)],[(18.5,1.5,2.7)],street_x=7.4)
    # The close right corner faces the camera. Its street-facing plane recedes
    # from the visible corner near u=.88 instead of ending in the sky at u=.71.
    corner=G();hq.wall(corner,7.0,13.5,[],True)
    corner_ob=emit(corner,col,'right_foreground_corner_masonry',.009)
    corner_ob.location=(10.9,-2.96,.24)
    canopy_hems=draped_shop_canopy(col,'left_broad_cream',-5.8,-2.6,-17,-7.5,3.4,2.8,.64,['canvas_cream']*3+['canvas_green'],near_lowering=.34)
    draped_shop_canopy(col,'left_broad_gold',-5.8,-3.6,-17,-9.2,4.6,4.0,.50,['canvas_gold']*4,near_lowering=.30)
    cloth(col,'left_middle_green',-1,-6,6,3.65,2.8,1.37,['canvas_cream','canvas_leaf','canvas_green'],wall_position=-4.5)
    cloth(col,'left_distant_awning',-1,6,18,3.5,2.85,1.50,['canvas_cream','canvas_green'],wall_position=-4.5)
    cloth(col,'right_blue_canopy',1,2,10,4.0,3.45,3.87,['canvas_blue','canvas_blue','canvas_cream'],wall_position=7.4)
    cloth(col,'right_distant_canopy',1,11,20,3.7,3.2,3.62,['canvas_cream','canvas_blue'],wall_position=7.4)
    def projected_u(pos):
        rel=Vector(pos)-CAM
        return .5+rel.dot(RIGHT)/(2*TAN_H*rel.dot(FORWARD))
    # These are actual hem vertices, selected by horizontal screen coordinate;
    # their vertical errors remain independent measured outputs in Godot.
    marker('left_white_canopy_lower_corner',min(canopy_hems['canvas_cream'],key=lambda p:abs(projected_u(p)-.015)))
    marker('left_green_canopy_lower_corner',min(canopy_hems['canvas_green'],key=lambda p:abs(projected_u(p)-.335)))
    # Measured visible arch crown and tower silhouette drive independent landmark geometry.
    gate_y=23.0;apex=ray_at(.537,.455,gate_y)
    opening_w=ray_at(.583,.535,gate_y).x-ray_at(.491,.535,gate_y).x
    gate_top=ray_at(.537,.348,gate_y).z
    gate=craft.build_gate(col,(apex.x,gate_y,.24),8.0,gate_top-.24,opening_w,apex.z-.24,2.35)
    # Asymmetric bridge connects to the tower side without hiding its entire drum.
    wing=G();wing_x=apex.x-6.4
    hq.wall(wing,4.8,gate_top-.24,[],True)
    wing_ob=emit(wing,col,'market_v3_left_bridge_wing',.006);wing_ob.location=(wing_x,gate_y-1.2,.24)
    rear=G();rear.box((wing_x,gate_y+.5,gate_top/2),(4.8,1.4,gate_top),'limestone')
    for j in range(6):rear.box((wing_x-2.0+j*.8,gate_y,gate_top+.42),(.40,2.3,.75),'stone_light')
    emit(rear,col,'market_v3_bridge_wing_depth',.006)
    wing_collision=G();wing_collision.box((wing_x,gate_y,gate_top/2),(4.8,2.3,gate_top))
    emit(wing_collision,col,'COL_market_v3_bridge_left_wing',0,True)
    marker('gate_opening_crown',apex)
    marker('gate_opening_left_spring',(apex.x-opening_w/2,gate_y-1.18,gate['opening_spring_z']))
    marker('gate_opening_right_spring',(apex.x+opening_w/2,gate_y-1.18,gate['opening_spring_z']))
    # Front face is the actual visible crown surface, correct the marker depth.
    MARKERS['gate_opening_crown'][1]=gate_y-1.18
    tower_y=25.8; tower_apex=ray_at(.609,.075,tower_y);gallery_base=ray_at(.609,.330,tower_y).z;gallery_top=ray_at(.609,.205,tower_y).z
    tower_radius=(ray_at(.659,.215,tower_y).x-ray_at(.559,.215,tower_y).x)/2/1.22
    craft.build_tower(col,(tower_apex.x,tower_y,.24),tower_radius,gallery_base-.24,gallery_top-gallery_base,tower_apex.z-gallery_top-.87)
    tower_mesh=bpy.data.objects['market_v3_round_tower_arch_gallery']
    actual_tip=max((tower_mesh.matrix_world@v.co for v in tower_mesh.data.vertices),key=lambda v:v.z)
    marker('tower_roof_apex',actual_tip)
    for u,y,r,top_v,dome_h in [(.354,39,4.1,.201,2.85),(.412,43,3.4,.222,2.40),(.459,48,2.2,.265,1.80)]:
        top=ray_at(u,top_v,y)
        craft.build_dome_cluster(col,[(top.x,y,.24)],r,top.z-dome_h-.24,dome_h)
    distant=G()
    for u,y,topv,rad in [(.318,46,.155,.43),(.489,51,.220,.33),(.377,49,.180,.3)]:
        tip=ray_at(u,topv,y)
        distant.curved((tip.x,y,0),[(rad*.72,0),(rad*.72,tip.z-3.0),(rad,tip.z-3.0),(.018,tip.z)],'slate',24)
    emit(distant,col,'market_v3_distant_spires',.002)
    # Buildings visible through the arch establish a continuing street, not a closed wall.
    for side in (-1,1):
        frontage(col,'beyond_gate_'+str(side),side,31,62,7.5,[(y,4.3,.62,1.5,True) for y in (34,39,44,49,54,59)],[(y,1.6,2.7) for y in (35,43,51,59)],False)
    for j,(x,y,height) in enumerate([(-4.1,66,8.7),(2.2,72,7.8),(8.3,68,9.7)]):
        hq.facade('market_v3_beyond_gate_front_'+str(j),col,6.1,7,height,[(-1.6,3.8,.7,1.4,False,True),(1.1,3.8,.7,1.4,False,True),(-1.6,6.1,.65,1.25,False,False),(1.1,6.1,.65,1.25,False,False)],(x,y,.24),0,False,'plaster_white','terrace',0,False,True)
    props(col); roots=people(col);banners(col)
    # Small wall-mounted iron bracket and stacked timber stock at the right edge.
    iron=G()
    for y in (-6,9):
        iron.tube([(5.62,y,3.8),(5.05,y,3.8),(4.91,y,3.65)],[.02]*3,'iron',8)
        iron.curved((4.95,y,3.11),[(.12,0),(.19,.12),(.19,.43),(.09,.56)],'iron',8)
    emit(iron,col,'market_v3_hanging_brackets',.003)
    for ob in sc.objects:ob.select_set(ob.type=='MESH')
    path=EXPORT/'StartingTown_Market_V3.glb'
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,use_active_scene=True,export_extras=True,export_cameras=False,export_lights=False,export_yup=True,export_apply=True)
    visual=collision=0
    for ob in sc.objects:
        if ob.type=='MESH':
            ob.data.calc_loop_triangles();count=len(ob.data.loop_triangles)
            if ob.name.startswith('COL_'):collision+=count;ob.hide_render=True;ob.hide_set(True)
            else:visual+=count
    entry={'id':'StartingTown_Market','region_id':'beginnings_plaza','floor_east_north_m':[-163,-4752],'godot_position_m':[-163,0,-18],'godot_yaw_degrees':78,'glb':path.name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'triangles':visual,'collision_triangles':collision,'collision_mesh_names':[o.name for o in sc.objects if o.name.startswith('COL_')],'reference_camera_blender_m':list(CAM),'reference_target_blender_m':list(TARGET),'reference_horizontal_fov':HFOV,'mesh_objects':sum(o.type=='MESH' for o in sc.objects),'reference_landmarks_3d':MARKERS,'visual_actor_count':len(roots),'walkable_gate':True}
    manifest={'schema':2,'style_id':'level0_anime_market_demo_v3','scenes':[entry],'composition_method':'measured_uv_depth_projection_plus_independent_geometry_review','visual_approval':False,
        'source_files_sha256':{name:hashlib.sha256((HERE/name).read_bytes()).hexdigest() for name in ['assemble_market_demo.py','craft_landmarks.py']}}
    for folder in (HERE,EXPORT):(folder/'market_v3_manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    bpy.ops.wm.save_as_mainfile(filepath=str(HERE/'StartingTown_Market_V3.blend'),compress=True)
    print(json.dumps({'stage':'assembled','entry':entry}),flush=True)

if __name__=='__main__':main()
