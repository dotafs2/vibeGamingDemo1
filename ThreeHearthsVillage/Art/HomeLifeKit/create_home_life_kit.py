"""Generate HomeLifeKit originals via Blender MCP; generation does not render."""
from pathlib import Path
import importlib
import json
import math
import random
import sys
import time
import bpy
from mathutils import Vector

OUT=Path(__file__).resolve().parent
if str(OUT) not in sys.path:sys.path.insert(0,str(OUT))
import home_geometry as G
importlib.reload(G)
SPECS=json.loads((OUT/'module-specs.json').read_text(encoding='utf-8'))
START=time.monotonic()
for folder in ('modules','previews','examples'):(OUT/folder).mkdir(parents=True,exist_ok=True)
for obj in list(bpy.data.objects):bpy.data.objects.remove(obj,do_unlink=True)
for kind in ('collections','meshes','materials','armatures','actions','libraries'):
    collection=getattr(bpy.data,kind)
    for item in list(collection):collection.remove(item)
scene=bpy.context.scene;scene.name='HomeLifeKit | original living and social components'
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
MODULES=bpy.data.collections.new('ORIGINAL_MODULES');scene.collection.children.link(MODULES)
M={
 'wood':G.mat('HL_Warm_Oak','BC9368',.43),'wood_honey':G.mat('HL_Honey_Plank','B2865C',.46),
 'wood_dark':G.mat('HL_Timber_Recess','594337',.55),'wood_edge':G.mat('HL_Cut_Edge','D0AC7A',.44),
 'linen':G.mat('HL_Cream_Linen','E9DCB9',.61),'linen_edge':G.mat('HL_Linen_Stitch','D2C4A1',.60),
 'sage':G.mat('HL_Sage_Blanket','779F97',.56),'blue':G.mat('HL_Blue_Blanket','537F9A',.55),
 'iron':G.mat('HL_Forged_Iron','535B5E',.34,.65),'brass':G.mat('HL_Brass','B99550',.30,.70),
 'wicker':G.mat('HL_Golden_Wicker','CAA771',.55),'wicker_dark':G.mat('HL_Wicker_Weave','AC895A',.57),
 'ceramic':G.mat('HL_Glazed_Cream_Bowl','D8D0B2',.30),'soup':G.mat('HL_Stew','B9854C',.39),
 'bread':G.mat('HL_Bread_Crust','B88148',.52),'bread_cut':G.mat('HL_Bread_Score','DCB878',.58),
 'berry':G.mat('HL_Berry_Burgundy','79465D',.35),'berry_blue':G.mat('HL_Berry_Blue','617B91',.34),
 'leaf':G.mat('HL_Berry_Leaf','71835D',.51),'bark':G.mat('HL_Firewood_Bark','805C45',.57)
}
G.configure(MODULES,M)
box,beam,cylinder,mesh=G.box,G.beam,G.cylinder,G.mesh
ASSETS={}
REPORT={'schema_version':1,'kit_id':'home_life_kit_01','blender_version':bpy.app.version_string,
 'units':'metres','modules':[],'creates_inventory':False,'navigation_and_animation_verified':False}

def ellipsoid(name,center,scale,material):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2,radius=1,location=center)
    obj=G.adopt(bpy.context.view_layer.objects.active,material);obj.name=name;obj.scale=scale
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    G.uv_project(obj.data)
    for p in obj.data.polygons:p.use_smooth=True
    return obj

def lathe(name,profile,material,center=(0,0,0),n=32,closed_profile=False):
    # Positive-radius rings and explicit end caps; no coincident pole rings.
    cx,cy,cz=center
    verts=[(cx+r*math.cos(j*math.tau/n),cy+r*math.sin(j*math.tau/n),cz+z) for r,z in profile for j in range(n)]
    faces=[]
    for row in range(len(profile)-1):
        for j in range(n):
            a=row*n+j;b=row*n+(j+1)%n
            faces.append((a,b,b+n,a+n))
    if closed_profile:
        for j in range(n):faces.append(((len(profile)-1)*n+j,(len(profile)-1)*n+(j+1)%n,(j+1)%n,j))
    else:
        faces.append(tuple(reversed(range(n))));faces.append(tuple((len(profile)-1)*n+j for j in range(n)))
    return mesh(name,verts,faces,material,range(len(faces) if closed_profile else len(faces)-2))

def bed(double=False):
    w=2 if double else 1.1
    for x in (-w/2+.045,w/2-.045):
        for y in (-.955,.955):
            h=.87 if y>0 else .57
            box('Bed corner post',(x,y,h/2),(.09,.09,h),M['wood_honey'],.022)
            ellipsoid('Rounded post cap',(x,y,h-.015),(.052,.052,.04),M['wood_edge'])
    for x in (-w/2+.05,w/2-.05):box('Bed side rail',(x,0,.30),(.10,1.88,.22),M['wood'],.018)
    for y,z,h in ((.953,.66,.32),(-.953,.435,.20)):
        box('Bed end board',(0,y,z),(w-.13,.08,h),M['wood'],.025)
        box('End board top rail',(0,y,z+h/2),(w-.11,.11,.075),M['wood_edge'],.018)
    for j in range(9):box('Mattress support slat',(0,-.80+j*.20,.31),(w-.18,.13,.055),M['wood_dark'],.007)
    box('Cream padded mattress',(0,0,.385),(w-.12,1.82,.17),M['linen'],.065)
    color=M['blue'] if double else M['sage']
    box('Soft quilt',(0,-.26,.48),(w-.10,1.27,.05),color,.020)
    box('Folded quilt edge',(0,.32,.506),(w-.10,.10,.025),M['linen_edge'],.006)
    for x in ((-.47,.47) if double else (0,)):
        box('Linen pillow',(x,.63,.535),(.63,.39,.13),M['linen'],.035)
        box('Pillow seam',(x,.63,.59),(.54,.29,.018),M['linen_edge'],.004)
    for x in (-w/2+.06,w/2-.06):
        for y in (-.80,.80):cylinder('Bed joinery peg',(x,y,.32),.018,.02,M['wood_dark'],12,(1,0,0))

def seat(bench=False):
    width=1.8 if bench else .9
    for x in (-width/2+.05,width/2-.05):
        for y in (-.27,.35):
            height=1.08 if y>0 else .42
            box('Seat leg and back post',(x,y,height/2),(.10,.10,height),M['wood_honey'],.020)
        beam('Side leg stretcher',(x,-.27,.17),(x,.35,.17),.06,.07,M['wood'])
    for y in (-.29,.30):box('Seat apron rail',(0,y,.36),(width,.07,.11),M['wood_dark'],.01)
    count=12 if bench else 6
    for j in range(count):
        box('Seat plank',(-width/2+(j+.5)*width/count,0,.41),(width/count-.007,.72,.08),M['wood'] if j%3 else M['wood_edge'],.011)
    for z in (.72,.94):box('Comfort back slat',(0,.385,z),(width+.05,.09,.15),M['wood'],.022)
    for x in (-width/2+.05,width/2-.05):
        for z in (.72,.94):cylinder('Back peg',(x,.332,z),.016,.016,M['wood_dark'],12,(0,1,0))
    if bench:box('Long bench stretcher',(0,.06,.18),(1.63,.10,.10),M['wood_honey'],.018)

def table(communal=False):
    width=2.6 if communal else 1.2;depth=1.1 if communal else .8
    for j in range(5):box('Table top plank',(0,-depth/2+(j+.5)*depth/5,.735),(width,depth/5-.008,.09),M['wood_edge'] if j%3==0 else M['wood'],.016)
    for x in (-width/2+.15,width/2-.15):
        if communal:
            box('Trestle foot',(x,0,.075),(.20,depth-.10,.15),M['wood_honey'],.025)
            for sign in (-1,1):beam('Trestle diagonal',(x,sign*(depth/2-.15),.12),(x,sign*.19,.69),.12,.13,M['wood'])
        else:
            for y in (-depth/2+.12,depth/2-.12):box('Dining table leg',(x,y,.345),(.095,.095,.69),M['wood_honey'],.018)
        box('Undertop rail',(x,0,.652),(.13,depth-.07,.08),M['wood_dark'],.012)
    box('Table cross brace',(0,0,.23),(width-.25,.10,.11),M['wood_honey'],.018)

def tray():
    box('Oak food tray',(0,0,.018),(.46,.32,.036),M['wood'],.012)
    for y in (-.153,.153):box('Tray long rim',(0,y,.043),(.46,.016,.05),M['wood_edge'],.003)
    for x in (-.223,.223):box('Tray short rim',(x,0,.043),(.016,.29,.05),M['wood_edge'],.003)
    lathe('Glazed serving bowl',[(.037,0),(.061,.014),(.092,.078),(.085,.084),(.055,.02),(.031,.014)],M['ceramic'],(-.095,.01,.04),24)
    cylinder('Existing prepared food visual',(-.095,.01,.101),.071,.009,M['soup'],24)
    for y in (-.085,.075):
        ellipsoid('Bread roll',(.11,y,.084),(.082,.054,.045),M['bread'])
        for j in (-1,0,1):beam('Bread score',(.09+j*.023,y-.025,.121),(.102+j*.023,y+.014,.122),.009,.007,M['bread_cut'])

def basket(full=False):
    lathe('Hollow woven basket',[(.156,.015),(.172,.035),(.23,.275),(.219,.282),(.205,.264),(.157,.05),(.145,.036)],M['wicker'],n=32)
    # Repeated low relief weave bands retain a physically open interior.
    for k in range(4):
        z=.075+k*.056;r=.171+(z-.03)*.235
        lathe('Wicker horizontal weave',[(r,z-.010),(r+.008,z),(r+.004,z+.012),(r-.004,z+.005)],M['wicker_dark'],n=32,closed_profile=True)
    for j in range(16):
        a=j*math.tau/16
        beam('Basket stave',(.168*math.cos(a),.168*math.sin(a),.045),(.23*math.cos(a),.23*math.sin(a),.269),.013,.013,M['wicker_dark'])
    for sign in (-1,1):
        x=sign*.231
        beam('Basket handle left',(x,-.075,.26),(x,-.075,.37),.024,.024,M['wicker'])
        beam('Basket handle top',(x,-.075,.37),(x,.075,.37),.026,.026,M['wicker'])
        beam('Basket handle right',(x,.075,.37),(x,.075,.26),.024,.024,M['wicker'])
    if full:
        rng=random.Random(4242)
        for row in range(3):
            count=12-row*3;r=.158-row*.04
            for j in range(count):
                a=j*math.tau/count+row*.6
                ellipsoid('Existing berry visual',(r*math.cos(a),r*math.sin(a),.247+row*.052),(.047,.045,.042),M['berry_blue'] if j%3==0 else M['berry'])
        for x,y in ((-.09,.03),(.12,-.06)):
            ellipsoid('Berry leaf',(x,y,.366),(.06,.022,.013),M['leaf'])
        ellipsoid('Berry at pile centre',(0,0,.365),(.055,.051,.045),M['berry_blue'])

def chest():
    for y in (-.245,.245):
        for j in range(3):box('Grain chest front back',(0,y,.14+j*.16),(.98,.07,.152),M['wood'] if j%2 else M['wood_honey'],.012)
    for x in (-.455,.455):box('Chest end panel',(x,0,.305),(.09,.44,.47),M['wood_honey'],.014)
    box('Chest floor',(0,0,.068),(.92,.50,.09),M['wood_dark'],.01)
    for x in (-.425,.425):
        for y in (-.21,.21):box('Chest foot',(x,y,.045),(.11,.11,.09),M['wood_dark'],.014)
    for j in range(4):box('Grain chest lid',(-.375+j*.25,0,.575),(.242,.58,.14),M['wood'] if j%2 else M['wood_edge'],.022)
    for x in (-.33,.33):
        box('Lid strap',(x,0,.65),(.052,.53,.028),M['iron'],.006)
        box('Front strap',(x,-.287,.38),(.052,.018,.43),M['iron'],.004)
    box('Storage latch',(0,-.294,.505),(.10,.026,.14),M['brass'],.006)
    beam('Wheat label stalk',(0,-.294,.17),(0,-.294,.37),.012,.012,M['wood_edge'])
    for j in range(3):
        for sign in (-1,1):ellipsoid('Wheat label grain',(sign*.03,-.303,.225+j*.045),(.029,.011,.019),M['wood_edge'])

def firewood():
    for layer in range(3):
        count=4-layer
        for j in range(count):
            x=(j-(count-1)/2)*.19;z=.099+layer*.153
            cylinder('Existing split log visual',(x,0,z),.099,.50,M['bark'],10,(0,1,0))
            for sign in (-1,1):cylinder('Cut log end',(x,sign*.253,z),.087,.006,M['wood_edge'],10,(0,1,0))
    for y in (-.22,.22):box('Woodpile support',(0,y,.028),(.84,.055,.05),M['wood_dark'],.008)

def fence():
    # Both end posts are inset by half their width. Adjacent sections meet at a
    # grid plane without coincident post volumes; their paired posts read as a joint.
    for x in (-.945,.945):box('Inset end post',(x,0,.38),(.11,.16,.76),M['wood_honey'],.020)
    for z in (.27,.61):box('Low fence rail',(.025,0,z),(1.89,.08,.095),M['wood'],.018)
    for x in (-.61,-.23,.15,.53,.91):
        box('Rounded fence paling',(x,-.045,.38),(.13,.055,.65),M['wood_edge'] if x<0 else M['wood'],.012)

BUILDERS={'bed_single_1_1x2m':bed,'bed_double_2x2m':lambda:bed(True),
 'chair_oak_wide':seat,'bench_backed_1_8m':lambda:seat(True),
 'table_dining_1_2m':table,'table_communal_2_6m':lambda:table(True),
 'food_tray_bread':tray,'basket_empty':basket,'basket_berries':lambda:basket(True),
 'grain_chest':chest,'firewood_stack':firewood,'fence_low_2m':fence}

for spec in SPECS['modules']:
    G.PARTS.clear();BUILDERS[spec['id']]()
    bpy.ops.object.select_all(action='DESELECT')
    for obj in G.PARTS:obj.select_set(True)
    bpy.context.view_layer.objects.active=G.PARTS[0]
    if len(G.PARTS)>1:bpy.ops.object.join()
    obj=bpy.context.view_layer.objects.active;obj.name=spec['id'];obj.data.name=spec['id']
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    obj['asset_id']=spec['id'];obj['creates_inventory']=False;obj['category']=spec['category']
    path=OUT/'modules'/(spec['id']+'.glb')
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,export_yup=True,
        export_apply=True,export_texcoords=True,export_normals=True,export_materials='EXPORT',
        export_extras=True,export_animations=False,export_cameras=False,export_lights=False)
    obj.data.calc_loop_triangles();bounds=[Vector(v) for v in obj.bound_box]
    REPORT['modules'].append({'id':spec['id'],'file':'modules/'+path.name,'triangles':len(obj.data.loop_triangles),
        'bounds_min_m':[round(min(p[i] for p in bounds),6) for i in range(3)],
        'bounds_max_m':[round(max(p[i] for p in bounds),6) for i in range(3)],
        'materials':[m.name for m in obj.data.materials],'uv_layers':len(obj.data.uv_layers),'bytes':path.stat().st_size})
    ASSETS[spec['id']]=obj
REPORT['total_triangles']=sum(x['triangles'] for x in REPORT['modules'])
REPORT['generation_seconds']=round(time.monotonic()-START,3)
REPORT['source_meshes_are_original_only']=True
(OUT/'model-report.json').write_text(json.dumps(REPORT,indent=2)+'\n',encoding='utf-8')
DISPLAY=bpy.data.collections.new('DISPLAY | originals only');scene.collection.children.link(DISPLAY)
for i,spec in enumerate(SPECS['modules']):
    obj=bpy.data.objects.new(spec['id'],ASSETS[spec['id']].data);DISPLAY.objects.link(obj)
    obj.location=((i%4-1.5)*3.4,(1-i//4)*3.4,0)
MODULES.hide_render=True;MODULES.hide_viewport=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'HomeLifeKit.blend'))
print('HOME_LIFE_KIT_COMPLETE',len(ASSETS),REPORT['total_triangles'],flush=True)
