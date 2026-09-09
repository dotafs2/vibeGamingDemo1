"""Deterministic, engine-neutral town study. Python + Pillow + numpy; no remote assets.

Geometry is a project interpretation, not a measured official map.
Exports a metre-scale GLB, object ledger, top-down plan and geometric preview.
"""
from pathlib import Path
import json, math, struct, random
import numpy as np
from PIL import Image, ImageDraw, ImageFont

OUT = Path(__file__).resolve().parent
RNG = random.Random(20260909)
MATERIALS = {
    'ground': '#afbfa2', 'paving': '#dfd8c4', 'stone': '#d5cbb7',
    'observed': '#829aab', 'infill': '#c1b4a2', 'roof': '#938f89',
    'palace': '#566675', 'dark': '#5b5d5b', 'tree': '#71886e',
    'route': '#ce7950', 'clock': '#eee4c8', 'water': '#87aca6',
}
meshes, objects, roads = {}, [], []
def mesh(name, vertices, faces):
    meshes[name] = (np.array(vertices, dtype=float), faces)
mesh('box', [(-.5,-.5,0),(.5,-.5,0),(.5,.5,0),(-.5,.5,0),(-.5,-.5,1),(.5,-.5,1),(.5,.5,1),(-.5,.5,1)],
     [(0,2,1),(0,3,2),(4,5,6),(4,6,7),(0,1,5),(0,5,4),(1,2,6),(1,6,5),(2,3,7),(2,7,6),(3,0,4),(3,4,7)])
mesh('roof', [(-.5,-.5,0),(.5,-.5,0),(.5,.5,0),(-.5,.5,0),(0,-.5,1),(0,.5,1)],
     [(0,4,1),(3,2,5),(0,3,5),(0,5,4),(1,4,5),(1,5,2),(0,1,2),(0,2,3)])
def round_mesh(name, rings, count=24):
    vs = [(r*math.cos(i*math.tau/count), r*math.sin(i*math.tau/count), z) for r,z in rings for i in range(count)]
    fs=[]
    for k in range(len(rings)-1):
        for i in range(count):
            a=k*count+i; b=k*count+(i+1)%count; c=b+count; d=a+count
            fs.extend([(a,b,c),(a,c,d)])
    for i in range(1,count-1): fs.extend([(0,i+1,i),((len(rings)-1)*count,(len(rings)-1)*count+i,(len(rings)-1)*count+i+1)])
    mesh(name,vs,fs)
round_mesh('cylinder',[(1,0),(1,1)])
round_mesh('cone',[(1,0),(0,1)])
round_mesh('dome',[(math.cos(i*math.pi/20),math.sin(i*math.pi/20)) for i in range(11)])
# One reusable curved lintel, with a real opening. Span = 6m, outer span = 8.4m.
av=[]; af=[]
for i in range(13):
    a=i*math.pi/12
    av.extend([(r*math.cos(a),y,5+r*math.sin(a)) for y in (-1,1) for r in (3,4.2)])
for i in range(12):
    k=i*4; q=k+4
    for a,b,c,d in [(k,k+1,q+1,q),(k+2,q+2,q+3,k+3),(k,q,q+2,k+2),(k+1,k+3,q+3,q+1)]:
        af.extend([(a,b,c),(a,c,d)])
af.extend([(0,2,3),(0,3,1),(48,49,51),(48,51,50)])
mesh('arch',av,af)

def add(name, kind, pos, scale, material, yaw=0, evidence='project_infill'):
    objects.append(dict(id=name, mesh=kind, position_m=list(pos), scale=list(scale), yaw_degrees=yaw, material=material, evidence=evidence))
def box(name,x,y,w,d,h,mat='infill',z=0,yaw=0,evidence='project_infill'):
    add(name,'box',(x,y,z),(w,d,h),mat,yaw,evidence)
def segment(name,a,b,width,mat='paving',z=.05,height=.18):
    x,y=(a[0]+b[0])/2,(a[1]+b[1])/2
    dx,dy=b[0]-a[0],b[1]-a[1]
    box(name,x,y,math.hypot(dx,dy)+.3,width,height,mat,z,math.degrees(math.atan2(dy,dx)))
def road(name,points,width=14):
    roads.append(dict(id=name,points_m=points,width_m=width,evidence='project_infill'))
    for i,(a,b) in enumerate(zip(points,points[1:])): segment(name+'_'+str(i),a,b,width)

# Metres, x=east, y=north, z=up. GLB root transforms this to the glTF Y-up basis.
verts=[(0,-200,0)]+[(500*math.cos(i*math.pi/80),-200+500*math.sin(i*math.pi/80),0) for i in range(81)]
mesh('town_ground',verts,[(0,i,i+1) for i in range(1,81)])
add('town_ground','town_ground',(0,0,-.3),(1,1,1),'ground',evidence='novel_context_project_orientation')
add('plaza_paving','cylinder',(0,0,0),(87,87,.3),'paving',evidence='anime_observed_project_scale')
road('west_market',[(0,0),(-105,0),(-165,18),(-235,30),(-310,50),(-405,62)],18)
road('east_street',[(0,0),(105,0),(190,15),(280,42),(403,70)],16)
road('south_street',[(0,0),(0,-185)],16)
road('palace_approach',[(0,0),(0,128)],22)
road('north_access_west',[(0,108),(-73,117),(-83,224),(0,276),(0,332)],14)
road('north_access_east',[(0,108),(73,117),(83,224),(0,276)],14)
road('west_lane',[(-90,-152),(-146,-85),(-176,10),(-195,145)],9)
road('east_lane',[(120,-150),(155,-62),(177,14),(212,132)],9)
road('southern_lane',[(-335,-118),(-205,-102),(-100,-126),(0,-140),(125,-128),(250,-92),(340,-103)],9)

# Semicircular perimeter is novel context; gates and exact wall heights are infill.
for i in range(100):
    a=(i+.5)*math.pi/100
    if abs(a-math.pi/2)<.042: continue
    x,y=500*math.cos(a),-200+500*math.sin(a)
    box(f'perimeter_{i}',x,y,16.2,4,13,'stone',yaw=math.degrees(a)+90,evidence='novel_context_project_geometry')
    if i%8==0:
        add(f'wall_tower_{i}','cylinder',(x,y,0),(5,5,18),'stone')
        add(f'wall_roof_{i}','cone',(x,y,18),(6,6,6),'roof')
box('north_gate_left',-15,299,10,14,24,'stone')
box('north_gate_right',15,299,10,14,24,'stone')
box('north_gate_lintel',0,299,20,14,6,'stone',z=18)

# Circular arcade: observed form. Exact bay count and size are deliberately inferred.
for i in range(72):
    a=i*math.tau/72
    if min(abs(math.sin(a)),abs(math.cos(a)))<.11: continue
    x,y=96*math.cos(a),96*math.sin(a); yaw=math.degrees(a)+90
    add(f'arcade_{i}','arch',(x,y,0),(1,1,1),'observed',yaw,'anime_observed_project_scale')
    box(f'arcade_cap_{i}',x,y,8.8,3,2,'stone',z=9.2,yaw=yaw,evidence='anime_observed_project_scale')
    for sign in (-1,1):
        xx=x+sign*3.65*math.cos(math.radians(yaw)); yy=y+sign*3.65*math.sin(math.radians(yaw))
        box(f'arcade_pier_{i}_{sign}',xx,yy,1.3,2.4,5.4,'stone',yaw=yaw,evidence='anime_observed_project_scale')
for i,a in enumerate([0,math.pi/2,math.pi,3*math.pi/2]):
    x,y=96*math.cos(a),96*math.sin(a)
    add(f'arcade_gateway_{i}','arch',(x,y,0),(2,2,2),'observed',math.degrees(a)+90,'anime_observed_project_scale')

# Stepped clock-tower mass taken from the visible silhouette, dimensions inferred.
for i,(w,h) in enumerate([(15,1),(12,1),(9,1)]): box('clock_step_'+str(i),0,0,w,w,h,'stone',z=i,evidence='anime_observed_project_scale')
box('clock_column',0,0,3.8,3.8,22,'observed',z=3,evidence='anime_observed_project_scale')
box('clock_head',0,0,5.5,5.5,5.5,'stone',z=25,evidence='anime_observed_project_scale')
box('clock_face',0,-2.78,3.2,.12,3.2,'clock',z=26,evidence='anime_observed_project_scale')
add('clock_cap','cone',(0,0,30.5),(3.4,3.4,5.5),'palace',evidence='anime_observed_project_scale')

# Palace silhouette/position remains provisional (secondary location source).
box('black_iron_palace_mass',0,180,92,74,28,'palace',evidence='secondary_reference_provisional')
add('palace_dome','dome',(0,180,28),(28,28,23),'palace',evidence='secondary_reference_provisional')
for x in (-37,37):
    add('palace_tower_'+str(x),'cylinder',(x,154,0),(8,8,35),'palace',evidence='secondary_reference_provisional')
    add('palace_cap_'+str(x),'dome',(x,154,35),(8,8,8),'palace',evidence='secondary_reference_provisional')

def dist_segment(p,a,b):
    d=np.array(b)-a; v=np.array(p)-a; t=np.clip(np.dot(v,d)/np.dot(d,d),0,1)
    return float(np.linalg.norm(v-t*d))
def dist_road(x,y):
    return min(dist_segment((x,y),a,b)-r['width_m']/2 for r in roads for a,b in zip(r['points_m'],r['points_m'][1:]))

# Dense, non-grid fabric is a reversible massing study, not invented canonical addresses.
footprints=[]
task_frontages=[(-127,-23),(-150,40),(-183,-6)]
for attempt in range(6000):
    if len(footprints)>=310: break
    x,y=RNG.uniform(-465,465),RNG.uniform(-182,270)
    if math.hypot(x,y+200)>468 or math.hypot(x,y)<116: continue
    if -108<x<108 and 97<y<269: continue
    w,d=RNG.uniform(10,19),RNG.uniform(13,24)
    radius=math.hypot(w,d)/2+2
    if dist_road(x,y)<radius: continue
    if any(math.hypot(x-u,y-v)<radius+13 for u,v in task_frontages): continue
    if any(math.hypot(x-u,y-v)<radius+r+1 for u,v,r in footprints): continue
    footprints.append((x,y,radius))
    yaw=RNG.choice([0,12,-12,24,-24,90])+RNG.uniform(-6,6)
    height=RNG.choice([7,10,13,16])
    j=len(footprints)
    box(f'fabric_{j}',x,y,w,d,height,'infill',yaw=yaw)
    add(f'roof_{j}','roof',(x,y,height),(w+1,d+1,RNG.uniform(3,6)),'roof',yaw)

# A short first-task frontage, with role assignment deliberately left uncommitted.
for j,(x,y) in enumerate(task_frontages):
    box(f'task_frontage_{j}',x,y,14,17,10,'route',evidence='project_mvp_site')
    add(f'task_roof_{j}','roof',(x,y,10),(15,18,4),'roof',evidence='project_mvp_site')
    box(f'task_awning_{j}',x,y+(10 if y<0 else -10),13,4,.3,'route',z=3,evidence='project_mvp_site')

for j in range(36):
    a=RNG.random()*math.tau; r=RNG.uniform(117,390); x,y=r*math.cos(a),r*math.sin(a)
    if y< -180 or math.hypot(x,y+200)>465 or dist_road(x,y)<8: continue
    if -110<x<110 and 95<y<270: continue
    if any(math.hypot(x-u,y-v)<r0+5 for u,v,r0 in footprints): continue
    if any(math.hypot(x-u,y-v)<18 for u,v in task_frontages): continue
    add('tree_trunk_'+str(j),'cylinder',(x,y,0),(.7,.7,5),'dark')
    add('tree_crown_'+str(j),'dome',(x,y,4),(5,5,7),'tree')

def export_glb():
    doc={'asset':{'version':'2.0','generator':'StartingTown evidence-aware blockout study'},'scene':0,'scenes':[{'nodes':[0]}],
         'nodes':[{'name':'TownOfBeginnings_Study_Metres','rotation':[-math.sqrt(.5),0,0,math.sqrt(.5)],'children':[]}],
         'meshes':[],'materials':[],'buffers':[],'bufferViews':[],'accessors':[]}
    data=bytearray(); cache={}; material_ids={}
    for name,color in MATERIALS.items():
        rgb=[int(color[i:i+2],16)/255 for i in (1,3,5)]
        material_ids[name]=len(doc['materials'])
        doc['materials'].append({'name':name,'doubleSided':True,'pbrMetallicRoughness':{'baseColorFactor':rgb+[1],'metallicFactor':0,'roughnessFactor':1}})
    def accessor(arr):
        arr=np.asarray(arr,dtype='<f4'); offset=len(data); data.extend(arr.tobytes())
        idx=len(doc['bufferViews']); doc['bufferViews'].append({'buffer':0,'byteOffset':offset,'byteLength':arr.nbytes,'target':34962})
        doc['accessors'].append({'bufferView':idx,'componentType':5126,'count':len(arr),'type':'VEC3','min':arr.min(axis=0).tolist(),'max':arr.max(axis=0).tolist()})
        return len(doc['accessors'])-1
    for o in objects:
        key=o['mesh'],o['material']
        if key not in cache:
            vs,fs=meshes[o['mesh']]; positions=[];normals=[]
            for face in fs:
                tri=vs[list(face)]; normal=np.cross(tri[1]-tri[0],tri[2]-tri[0]); length=np.linalg.norm(normal)
                if length<1e-10:continue
                positions.extend(tri); normals.extend([normal/length]*3)
            p=accessor(positions); n=accessor(normals); cache[key]=len(doc['meshes'])
            doc['meshes'].append({'name':'_'.join(key),'primitives':[{'attributes':{'POSITION':p,'NORMAL':n},'material':material_ids[o['material']]}]})
        a=math.radians(o['yaw_degrees'])/2
        doc['nodes'][0]['children'].append(len(doc['nodes']))
        doc['nodes'].append({'name':o['id'],'mesh':cache[key],'translation':o['position_m'],'scale':o['scale'],
                             'rotation':[0,0,math.sin(a),math.cos(a)],'extras':{'evidence':o['evidence'],'canonical_coordinates':False}})
    doc['buffers']=[{'byteLength':len(data)}]
    jb=json.dumps(doc,separators=(',',':')).encode(); jb+=b' '*((-len(jb))%4); data+=b'\0'*((-len(data))%4)
    glb=struct.pack('<III',0x46546c67,2,12+8+len(jb)+8+len(data))+struct.pack('<II',len(jb),0x4e4f534a)+jb+struct.pack('<II',len(data),0x004e4942)+data
    (OUT/'StartingTown_Outline.glb').write_bytes(glb)
    # Read back chunk lengths, transforms, vertex accessors and evidence metadata.
    magic,version,length=struct.unpack_from('<III',glb)
    assert magic==0x46546c67 and version==2 and length==len(glb)
    assert all(len(n.get('translation',[0,0,0]))==3 for n in doc['nodes'])
    assert all(n.get('extras',{}).get('canonical_coordinates') is False for n in doc['nodes'][1:])
    return len(glb),len(doc['meshes'])

def font(size,bold=False):
    p=Path('C:/Windows/Fonts')/('msyhbd.ttc' if bold else 'msyh.ttc')
    return ImageFont.truetype(str(p),size)
def world_vertices(o):
    vs,fs=meshes[o['mesh']]; a=math.radians(o['yaw_degrees']); c,s=math.cos(a),math.sin(a)
    rot=np.array([[c,-s,0],[s,c,0],[0,0,1]])
    return (vs*np.array(o['scale']))@rot.T+o['position_m'],fs
def plan_png():
    im=Image.new('RGB',(1680,1080),'#f6f3eb'); d=ImageDraw.Draw(im)
    d.text((56,35),'起始之城 · 轮廓研究 01',font=font(38,True),fill='#233f49')
    d.text((58,91),'动画镜头重建 + 小说外轮廓参考｜非官方完整地图',font=font(21),fill='#526361')
    d.text((96,178),'本轮落点',font=font(22,True),fill='#254855')
    d.text((96,220),'中央广场  →  市场街段  →  首次真实交易与使用',font=font(24),fill='#526361')
    d.text((96,265),'先校准空间，再接居民；外围街区保留为可修改体量。',font=font(18),fill='#66716e')
    def P(x,y): return (620+x*1.03,720-y*1.03)
    coords=[P(0,-200)]+[P(500*math.cos(i*math.pi/120),-200+500*math.sin(i*math.pi/120)) for i in range(121)]
    d.polygon(coords,fill='#e2e7d8')
    for r in roads:
        d.line([P(*p) for p in r['points_m']],fill='#faf7ee',width=max(2,int(r['width_m']*1.03)),joint='curve')
    for o in objects:
        if o['id'].startswith(('fabric_','task_frontage','black_iron_palace_mass','perimeter_','north_gate_')):
            v,_=world_vertices(o); pts=[P(*v[i,:2]) for i in range(4)]
            d.polygon(pts,fill=MATERIALS[o['material']],outline='#a49e90')
    for radius,col,width in [(96,'#829aab',9),(87,'#829aab',2)]:
        x1,y1=P(-radius,radius);x2,y2=P(radius,-radius); d.ellipse((x1,y1,x2,y2),fill='#f4eee0' if radius==96 else None,outline=col,width=width)
    # Mark actual gate openings on arcade ring.
    for x,y in [(96,0),(-96,0),(0,96),(0,-96)]:
        xx,yy=P(x,y);d.rectangle((xx-7,yy-7,xx+7,yy+7),fill='#f4eee0')
    xx,yy=P(0,0); d.rectangle((xx-5,yy-5,xx+5,yy+5),fill='#456c83')
    for rad in (27,53,76):
        xx,yy=P(-rad,rad);aa,bb=P(rad,-rad);d.ellipse((xx,yy,aa,bb),outline='#e0d5bf',width=1)
    # First task corridor: geometric site selection only, no live agents.
    route=[P(0,0),P(-105,0),P(-165,18),P(-235,30)]
    d.line(route,fill='#c67950',width=5,joint='curve')
    labels=[('01',(0,0)),('02',(0,180)),('03',(-163,19)),('04',(0,303)),('05',(-276,-85))]
    for text,(x,y) in labels:
        xx,yy=P(x,y); d.ellipse((xx-16,yy-16,xx+16,yy+16),fill='#254855');d.text((xx,yy-1),text,font=font(16,True),fill='white',anchor='mm')
    # Overall study is rotated north-up using novel context, not a compass inferred from stills.
    d.line((1090,357,1090,295),fill='#254855',width=3);d.polygon([(1090,285),(1083,301),(1097,301)],fill='#254855')
    d.text((1074,252),'N*',font=font(22,True),fill='#254855')
    d.text((440,958),'南侧边界待校准 · 直线切边仅为原型简化',font=font(19),fill='#66716e')
    d.line((95,990,198,990),fill='#254855',width=4); d.text((95,1000),'100 m 研究尺度',font=font(16),fill='#66716e')
    d.line((1198,185,1198,1005),fill='#d5d7ce',width=2)
    sidebar=[('01  中央广场 / 钟塔','圆形广场与连续拱廊有镜头依据。','直径 174 m 为研究选值。'),
             ('02  黑铁宫预留体量','地标位置参考二手原作索引。','体量、穹顶及坐标仍待镜头校准。'),
             ('03  首批任务街段','从广场接入约 140 m 市场街。','店铺位置与用途待核实/补全。'),
             ('04  出城通路','预留通往城外草原的路径。','城门数量、方向与尺寸为补全。'),
             ('05  后续街区','灰色建筑只表示城市密度。','未赋予居民、商店或原作地址。')]
    y=194
    for title,a,b in sidebar:
        d.text((1230,y),title,font=font(22,True),fill='#254855');
        d.text((1230,y+38),a,font=font(17),fill='#5e6868'); d.text((1230,y+64),b,font=font(17),fill='#5e6868');y+=131
    d.rectangle((1230,883,1248,901),fill='#829aab');d.text((1261,879),'有镜头依据的形体',font=font(18),fill='#526361')
    d.rectangle((1230,918,1248,936),fill='#c1b4a2');d.text((1261,914),'项目街区补全',font=font(18),fill='#526361')
    d.text((1230,960),'* 朝向结合小说；并非动画测绘。',font=font(16),fill='#66716e')
    im.save(OUT/'StartingTown_Plan.png')

def iso_png():
    im=Image.new('RGB',(1680,1120),'#eaece5'); d=ImageDraw.Draw(im)
    # Orthographic camera faces north; height exaggerated neither here nor in GLB.
    right=np.array([.92,.392,0]); forward=np.array([-.392,.92,0]); up=np.array([-.392*.62,.92*.62,.785])
    faces=[];light=np.array([-.4,-.5,1]);light/=np.linalg.norm(light)
    for o in objects:
        vs,fs=world_vertices(o)
        for f in fs:
            tri=vs[list(f)];norm=np.cross(tri[1]-tri[0],tri[2]-tri[0]); ln=np.linalg.norm(norm)
            if ln<1e-8: continue
            shade=.67+.3*abs(float(np.dot(norm/ln,light)))
            c=MATERIALS[o['material']]; rgb=tuple(int(int(c[i:i+2],16)*shade) for i in (1,3,5))
            p=[(810+np.dot(v,right)*1.4,610-np.dot(v,up)*1.4) for v in tri]
            depth=float(np.dot(tri.mean(axis=0),forward)-tri.mean(axis=0)[2]*.78)
            layer=0 if o['mesh']=='town_ground' else 1 if o['material']=='paving' else 2
            faces.append((layer,depth,p,rgb))
    for _,_,p,c in sorted(faces,key=lambda f:(f[0],-f[1])):d.polygon(p,fill=c)
    d.text((50,40),'起始之城 · 几何轮廓预览',font=font(37,True),fill='#233f49')
    d.text((52,95),'圆形广场 · 钟塔 · 连续拱廊 · 街区体量 · 半圆外围城墙',font=font(22),fill='#526361')
    d.text((52,1010),'由同一份 GLB 几何投影生成；不是游戏实机，也不是官方复原图。',font=font(21),fill='#526361')
    d.text((52,1051),'蓝灰：有镜头依据的形体 / 深色：待校准地标 / 暖灰：可修改街区补全',font=font(19),fill='#657471')
    im.save(OUT/'StartingTown_Massing.png')

if __name__=='__main__':
    size,mesh_count=export_glb();plan_png();iso_png()
    manifest={'schema':1,'id':'starting_town_anime_outline_study_01','units':'metres','axes':'x east, y north, z up; GLB root converts to Y-up',
              'canonical_complete_map':False,'live_world_modified':False,'status':'geometry_study_only',
              'perimeter_basis':'Progressive chapter 3, 1km semicircle; orientation and exact fit are project choices',
              'objects':objects,'roads':roads,'first_task_site':{'id':'west_market','role_assignment':'not_committed','npc_execution':'not_implemented'}}
    (OUT/'StartingTown_Layout.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    result={'objects':len(objects),'housing_masses':len(footprints),'mesh_material_pairs':mesh_count,'glb_bytes':size,'glb_container_checks':'passed','engine_import':'not_tested','owned_background_processes':0}
    (OUT/'validation.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps(result))
