"""Root-authored domestic/workshop kit, metres, bottom origin, front -Y.

Each finished module is exported with independently replaceable semantic layers.
The composed studio is a source-art review, never evidence of paid construction.
"""
import hashlib
import json
import math
import sys
from pathlib import Path
import bpy
from mathutils import Vector, Matrix

BASE=Path(__file__).resolve().parent
OUT=BASE/'MarketLifeKit'; OUT.mkdir(exist_ok=True)
sys.path.insert(0,str(BASE))
from meshkit import Geo, COLORS
bpy.ops.wm.read_factory_settings(use_empty=True)
scene=bpy.context.scene; scene.unit_settings.system='METRIC'
lib=bpy.data.collections.new('Authored joinery and independent finish layers')
scene.collection.children.link(lib)
specs=[]

def module(mid,layers,notes,anchors):
    root=bpy.data.objects.new(mid,None);lib.objects.link(root)
    root['module_id']=mid
    for layer,g in layers.items():
        g.object(mid+'__'+layer,lib,root,layer,bevel=.004 if layer=='finish' else .008)
    root['anchors_json']=json.dumps(anchors)
    specs.append((root,notes,anchors))

def peg(g,x,y,z,axis='y'):
    a=(x,y-.009,z) if axis=='y' else (x,y,z-.009)
    b=(x,y+.009,z) if axis=='y' else (x,y,z+.009)
    g.tube(a,b,.016,role='endgrain',n=8)

# A broad two-plank bench with splayed trestles and a tusk-tenon stretcher.
g,f=Geo(),Geo()
for x in [-.56,.56]:
    for side in [-1,1]:
        g.beam((x,side*.245,.025),(x,side*.155,.405),.105,.105,'oak_dark')
    g.box((x,0,.385),(.125,.49,.085),'oak')
    g.box((x,0,.17),(.18,.07,.075),'oak_light')
    f.box((x+(-.10 if x<0 else .10),0,.17),(.026,.105,.145),'oak_dark')
g.box((0,0,.17),(1.37,.072,.08),'oak')
for y in [-.118,.118]:
    g.box((0,y,.455),(1.72,.227,.10),'oak_light' if y<0 else 'oak')
    for x in [-.56,.56]:peg(f,x,y,.51,'z')
    f.box((-.861,y,.454),(.004,.211,.085),'endgrain')
for x,y,length in [(-.38,-.18,.48),(.31,.16,.61)]:
    f.beam((x-length/2,y,.507),(x+length/2,y+.013,.507),.0024,.002,'oak_dark')
module('bench_low',{'structure':g,'finish':f},'Two-person porch bench; human seat height 0.505m. Empty seating, not a seated character.',{'seat_left':[-.43,0,.505],'seat_right':[.43,0,.505]})

# A heavy 1.9m work surface; nothing on the table implies manufactured stock.
g,f=Geo(),Geo()
for x in [-.73,.73]:
    for y in [-.33,.33]:
        g.beam((x*1.05,y*1.08,.025),(x,y,.82),.12,.12,'oak_dark')
        f.box((x*1.04,y*1.07,.16),(.133,.133,.09),'iron')
    g.beam((x,-.39,.25),(x,.39,.25),.10,.095,'oak')
for y in [-.33,.33]:
    g.beam((-.78,y,.735),(.78,y,.735),.095,.17,'oak')
    g.beam((-.70,y,.28),(.35,y,.68),.052,.055,'oak_dark')
for i in range(5):
    y=(i-2)*.179
    g.box((0,y,.875),(1.93,.169,.11),'oak_light' if i%2==0 else 'oak')
    for x in [-.73,.73]:peg(f,x,y,.935,'z')
    f.box((.966,y,.875),(.004,.16,.098),'endgrain')
g.box((-.42,0,.29),(.43,.58,.065),'oak_dark')
for y in [-.22,0,.22]:f.box((-.42,y,.325),(.41,.006,.009),'oak_light')
module('work_table',{'structure':g,'finish':f},'Empty freestanding craft table with braced legs. Counter and drying support share this same module.',{'surface':[0,0,.93],'front_work':[0,-.88,0]})

# Tool board and tools separate: an empty rack can be installed before tools.
g,f,a=Geo(),Geo(),Geo()
for x in [-.59,.59]:
    g.beam((x,-.035,.04),(x,.015,1.65),.095,.095,'oak_dark')
    g.beam((x,-.34,.055),(x,.26,.055),.12,.10,'oak')
    g.beam((x,.235,.09),(x,.015,.57),.062,.065,'oak_dark')
for z in [.70,1.01,1.34,1.55]:
    g.box((0,0,z),(1.35,.065,.17),'oak_light' if z==1.55 else 'oak')
    for x in [-.59,.59]:peg(f,x,-.04,z)
for x in [-.46,-.08,.34]:
    f.tube((x,-.035,1.40),(x,-.13,1.40),.017,role='iron',n=8)
    f.tube((x,-.13,1.40),(x,-.13,1.43),.017,role='iron',n=8)
# Smith's hammer, closed tongs and a small toothed saw have legible silhouettes.
a.beam((-.46,-.10,.88),(-.46,-.10,1.37),.035,.030,'oak_light')
a.box((-.46,-.10,1.29),(.23,.105,.085),'iron')
a.box((-.46,-.157,1.29),(.05,.012,.06),'steel')
for side in [-1,1]:
    a.beam((-.08+side*.09,-.10,.91),(-.08-side*.022,-.10,1.24),.020,.017,'iron')
    a.beam((-.08-side*.022,-.10,1.24),(-.08+side*.048,-.10,1.36),.022,.02,'iron')
a.tube((-.08,-.12,1.22),(-.08,-.082,1.22),.026,role='steel',n=10)
a.ring((.34,-.10,1.35),.078,.044,.030,'oak_dark',12,axis=(0,1,0))
a.mesh([(.29,-.09,1.31),(.39,-.09,1.31),(.44,-.09,.85),(.30,-.09,.85),(.29,-.075,1.31),(.39,-.075,1.31),(.44,-.075,.85),(.30,-.075,.85)],[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],'steel')
for k in range(9):
    z=.87+k*.043;x=.44-(z-.85)*.11
    a.mesh([(x,-.09,z),(x+.022,-.09,z+.019),(x,-.09,z+.032)],[(0,1,2)],'iron')
module('tool_rack',{'structure':g,'finish':f,'equipment':a},'Craft equipment board; equipment layer can be omitted. Tools are workshop equipment, never tradeable stock grants.',{'front_work':[0,-.65,0]})

# Pegged four-post frame plus separately replaceable sagged linen skin.
g,f,c=Geo(),Geo(),Geo()
for x in [-1.10,1.10]:
    for y,z in [(-.69,2.17),(.69,2.53)]:
        g.box((x,y,.06),(.25,.23,.12),'stone_dark')
        g.beam((x,y,.12),(x,y,z),.10,.10,'oak_dark')
        g.beam((x,y,z-.45),(x*.69,y,z-.02),.065,.06,'oak')
    g.beam((x,-.81,2.17),(x,.81,2.57),.095,.11,'oak')
for y,z in [(-.69,2.18),(.69,2.54)]:
    g.beam((-1.19,y,z),(1.19,y,z),.095,.10,'oak_light')
    for x in [-1.10,1.10]:peg(f,x,y-.055,z)
# Long cloth strips with gently curved sag and a turned-over front edge.
for stripe in range(8):
    left=-1.21+stripe*.3025;right=left+.304
    verts=[]
    for k in range(13):
        t=k/12;y=.84-t*1.76;z=2.61-t*.43-.08*math.sin(math.pi*t)
        for x in [left,right]:verts.append((x,y,z-.024*math.sin(math.pi*(x+1.21)/2.42)))
    faces=[(k*2,k*2+1,k*2+3,k*2+2) for k in range(12)]
    c.mesh(verts,faces,'linen' if stripe%3 else 'blue_light')
    x=(left+right)/2
    c.mesh([(left,-.92,2.18),(right,-.92,2.18),(right,-.92,2.03),(x,-.925,1.99),(left,-.92,2.03)],[(0,1,2,3,4)],'linen' if stripe%3 else 'blue_light')
for x in [-1.18,1.18]:
    for y,z in [(-.70,2.20),(.70,2.56)]:
        f.ring((x,y,z),.031,.020,.012,'leather',10,axis=(1,0,0))
module('linen_canopy',{'structure':g,'cover':c,'finish':f},'Standalone 2.42m craft canopy. Duplicate bays at 2.4m intervals; table, bench and stock stay independent.',{'table':[0,0,0],'next_bay':[2.4,0,0],'front_work':[0,-1.35,0]})

# Hollow, thick-rimmed earthenware with actual inner wall and a wiped band.
g,f=Geo(),Geo()
profile=[(.00,.115),(.035,.16),(.14,.225),(.29,.217),(.37,.124),(.415,.126),(.424,.112),(.415,.096),(.37,.097),(.29,.188),(.14,.194),(.050,.127)]
g.loft([((0,0,z),r,r) for z,r in profile],'red',n=24)
f.ring((0,0,.405),.128,.100,.024,'linen',24,axis=(0,0,1))
for side in [-1,1]:
    points=[(side*.16,0,.32),(side*.265,0,.32),(side*.29,0,.23),(side*.225,0,.18)]
    for p,q in zip(points,points[1:]):g.tube(p,q,.022,role='red',n=8)
module('clay_jar',{'structure':g,'finish':f},'One empty hollow vessel, not a stack of manufactured pottery. Separate painted rim.',{'bottom':[0,0,0]})

manifest={'schema_version':1,'units':'meters','front':'-Y','up':'+Z','materials':COLORS,'modules':[]}
for root,notes,anchors in specs:
    bpy.ops.object.select_all(action='DESELECT')
    members=[root]+list(root.children_recursive)
    for ob in members:ob.select_set(True)
    bpy.context.view_layer.objects.active=root
    fbx=OUT/(root.name+'.fbx')
    bpy.ops.export_scene.fbx(filepath=str(fbx),use_selection=True,object_types={'MESH','EMPTY'},apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',axis_forward='-Y',axis_up='Z',bake_anim=False,use_mesh_modifiers=True,mesh_smooth_type='FACE',add_leaf_bones=False)
    bpy.ops.export_scene.gltf(filepath=str(OUT/(root.name+'.glb')),export_format='GLB',use_selection=True,export_apply=True,export_extras=True,export_cameras=False,export_lights=False)
    bpy.context.view_layer.update();dg=bpy.context.evaluated_depsgraph_get();points=[];tris=0;layers=[]
    for ob in members:
        if ob.type!='MESH':continue
        assert ob.data.uv_layers and len(ob.data.materials)
        ev=ob.evaluated_get(dg);mesh=ev.to_mesh();mesh.calc_loop_triangles()
        points.extend(ob.matrix_world@v.co for v in mesh.vertices);tris+=len(mesh.loop_triangles);ev.to_mesh_clear()
        layers.append({'layer':ob['layer'],'object_name':ob.name})
    manifest['modules'].append({'id':root.name,'fbx':fbx.name,'sha256':hashlib.sha256(fbx.read_bytes()).hexdigest(),'glb':root.name+'.glb','layers':layers,'triangles':tris,'notes':notes,'anchors_m':anchors,'bounds_m':{'min':[min(v[i] for v in points) for i in range(3)],'max':[max(v[i] for v in points) for i in range(3)]}})
(OUT/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')

# A joined workshop vignette and an uncluttered bench/ceramics foreground.
placements=[(-.8,-1.75,0),(0,.20,0),(-2,.65,0),(0,.25,0),(2,-1.1,0)]
for (root,_,_),pos in zip(specs,placements):root.location=pos
# Extra jar instances are presentation only, excluded from all module exports.
jar=specs[-1][0]
for pos in [(-.55,.14,.94),(-.11,.14,.94),(2.5,-.70,0)]:
    inst=bpy.data.objects.new('Preview ceramic instance',None);lib.objects.link(inst);inst.location=pos
    for child in jar.children:
        cp=child.copy();lib.objects.link(cp);cp.parent=inst;cp.matrix_basis=child.matrix_basis.copy()
stage=bpy.data.collections.new('Studio only - excluded from FBX and GLB');scene.collection.children.link(stage)
g=Geo();g.box((0,0,-.14),(200,200,.25),'stone_light');g.object('Studio ground',stage,bevel=0)
world=bpy.data.worlds.new('Soft overcast');world.use_nodes=True
world.node_tree.nodes['Background'].inputs['Color'].default_value=(.75,.84,.95,1)
world.node_tree.nodes['Background'].inputs['Strength'].default_value=.55;scene.world=world
for name,p,power,color in [('Key',(-4,-6,9),1900,(1,.91,.8)),('Fill',(6,1,8),1800,(.83,.91,1))]:
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.shape='DISK';data.size=7;data.color=color
    ob=bpy.data.objects.new(name,data);stage.objects.link(ob);ob.location=p
    ob.rotation_euler=(Vector((0,0,1))-ob.location).to_track_quat('-Z','Y').to_euler()
data=bpy.data.cameras.new('Authored kit review');data.type='ORTHO';data.ortho_scale=7.25
cam=bpy.data.objects.new('Authored kit review',data);stage.objects.link(cam);scene.camera=cam
cam.location=(7,-11,7);cam.rotation_euler=(Vector((.55,-.2,1.12))-cam.location).to_track_quat('-Z','Y').to_euler()
scene.render.engine='CYCLES';scene.cycles.samples=40;scene.cycles.use_denoising=True
scene.render.threads_mode='FIXED';scene.render.threads=6
scene.render.resolution_x=1800;scene.render.resolution_y=1350;scene.render.resolution_percentage=100
scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast'
scene.render.image_settings.file_format='PNG';scene.render.filepath=str(OUT/'MarketLifeKit_Source_Preview.png')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Medieval_Market_Life_Kit.blend'))
bpy.ops.render.render(write_still=True)
print('MARKET_LIFE_KIT_COMPLETE',len(specs),sum(len(m['layers']) for m in manifest['modules']),flush=True)
