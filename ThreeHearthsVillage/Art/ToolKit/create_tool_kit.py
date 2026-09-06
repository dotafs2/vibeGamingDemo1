"""Eight original warm-village tools. Execute via the local Blender MCP server."""
from pathlib import Path
import importlib
import json
import math
import sys
import time
import bpy
from mathutils import Vector
OUT=Path(__file__).resolve().parent
sys.dont_write_bytecode=True
if str(OUT) not in sys.path:sys.path.insert(0,str(OUT))
import tool_geometry as G
importlib.reload(G)
START=time.monotonic()
for name in ('modules','previews','mcp'):(OUT/name).mkdir(parents=True,exist_ok=True)
for obj in list(bpy.data.objects):bpy.data.objects.remove(obj,do_unlink=True)
for kind in ('collections','meshes','materials','armatures','actions','libraries'):
    for item in list(getattr(bpy.data,kind)):getattr(bpy.data,kind).remove(item)
scene=bpy.context.scene;scene.name='ToolKit | eight original work tools'
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
MODULES=bpy.data.collections.new('ORIGINAL_MODULES');scene.collection.children.link(MODULES)
M={'wood':G.mat('TK_Warm_Oak','BC9368',.43),'honey':G.mat('TK_Honey_Wood','B2865C',.46),
 'wood_dark':G.mat('TK_Timber_Recess','594337',.55),'cut':G.mat('TK_Cut_Edge','D0AC7A',.44),
 'linen':G.mat('TK_Cream_Grip_Wrap','E9DCB9',.61),'iron':G.mat('TK_Forged_Iron','535B5E',.34,.65),
 'edge':G.mat('TK_Sharpened_Edge','A1AFB0',.28,.72),'steel_dark':G.mat('TK_Iron_Recess','3C494C',.42,.65),
 'brass':G.mat('TK_Brass_Pin','B99550',.30,.70)}
G.configure(MODULES,M)
box,beam,mesh=G.box,G.beam,G.mesh

def lathe(name,profile,material,n=16,center=(0,0,0),bend=0):
    cx,cy,cz=center;top=max(z for r,z in profile)
    verts=[(cx+r*math.cos(j*math.tau/n)+bend*(z/top)**2,cy+r*math.sin(j*math.tau/n),cz+z) for r,z in profile for j in range(n)]
    faces=[]
    for row in range(len(profile)-1):
        for j in range(n):
            a=row*n+j;b=row*n+(j+1)%n;faces.append((a,b,b+n,a+n))
    faces.extend((tuple(reversed(range(n))),tuple((len(profile)-1)*n+j for j in range(n))))
    return mesh(name,verts,faces,material,range(len(faces)-2))

def grip(z0,z1,r=.029,bend=0):
    for j in range(5):
        z=z0+(j+.5)*(z1-z0)/5
        lathe('Cream handle wrap',[(r*.96,0),(r,.004),(r,(z1-z0)/5-.005),(r*.96,(z1-z0)/5-.002)],M['linen'],12,(bend,0,z))

def shaft(height,r=.023,wrap=True,bend=0):
    lathe('Tapered warm oak handle',[(r*.78,0),(r*1.16,.018),(r*1.05,.08),(r*.90,height*.55),(r,height-.014),(r*.84,height)],M['wood'],16,bend=bend)
    lathe('Dark protective handle heel',[(r*.76,0),(r*1.18,.01),(r*1.14,.04),(r*1.02,.047)],M['wood_dark'],16)
    if wrap:grip(.065,.165,r*1.12)

def slab(name,polygon,thickness,material,cy=0,bevel=.004):
    n=len(polygon);v=[(x,cy+sign*thickness/2,z) for sign in (-1,1) for x,z in polygon]
    f=[tuple(reversed(range(n))),tuple(n+j for j in range(n))]
    f += [(j,(j+1)%n,(j+1)%n+n,j+n) for j in range(n)]
    obj=mesh(name,v,f,material)
    bpy.context.view_layer.update()
    G.bevel(obj,bevel,1)
    return obj

def rivet(x,z,cy=-.025,r=.011):
    return G.cylinder('Brass assembly pin',(x,cy,z),r,.008,M['brass'],10,(0,1,0))

def collar(z,r=.033,h=.035):
    lathe('Forged handle collar',[(r*.94,0),(r,.004),(r,h-.004),(r*.94,h)],M['iron'],16,(0,0,z))

def hammer():
    shaft(.425,.024)
    box('Forged hammer head',(-.015,0,.404),(.18,.067,.068),M['iron'],.010)
    G.cylinder('Round striking face',(.094,0,.404),.039,.052,M['edge'],16,(1,0,0))
    slab('Cross peen',[(-.099,.377),(-.156,.391),(-.156,.415),(-.099,.438)],.050,M['iron'])
    box('Oak head wedge',(-.005,0,.441),(.024,.037,.012),M['cut'],.003)
    collar(.350,.029,.032);rivet(-.010,.405,-.037)

def mallet():
    shaft(.421,.025)
    box('Heavy wooden mallet head',(0,0,.395),(.252,.111,.129),M['honey'],.018)
    for x in (-.122,.122):box('Mallet end grain',(x,0,.395),(.020,.108,.121),M['cut'],.009)
    for x in (-.096,.096):
        box('Mallet front cheek',(x,-.057,.395),(.012,.013,.087),M['wood_dark'],.003)
        box('Mallet rear cheek',(x,.057,.395),(.012,.013,.087),M['wood_dark'],.003)
    box('Mallet locking wedge',(0,0,.465),(.026,.039,.021),M['wood_dark'],.004)

def axe():
    shaft(.725,.026,bend=.014)
    collar(.588,.034,.067)
    body=[(-.044,.650),(.025,.638),(.176,.569),(.210,.585),(.228,.646),(.226,.723),(.199,.757),(.059,.709),(-.044,.710)]
    slab('Curved axe forged cheek',body,.045,M['iron'],bevel=.006)
    slab('Axe polished cutting bevel',[(.176,.569),(.210,.585),(.228,.646),(.226,.723),(.199,.757),(.182,.728),(.192,.650),(.183,.60)],.048,M['edge'],bevel=.002)
    box('Axe rear poll',(-.042,0,.681),(.055,.062,.067),M['steel_dark'],.007)
    rivet(.025,.68,-.028,.012)

def handle_ring(name,outer,inner,thickness,material):
    assert len(outer)==len(inner);n=len(outer)
    verts=[(x,sign*thickness/2,z) for sign in (-1,1) for poly in (outer,inner) for x,z in poly]
    faces=[]
    for j in range(n):
        k=(j+1)%n
        faces.extend(((j,k,n+k,n+j),(2*n+j,3*n+j,3*n+k,2*n+k),
          (j,2*n+j,2*n+k,k),(n+j,n+k,3*n+k,3*n+j)))
    o=mesh(name,verts,faces,material);bpy.context.view_layer.update();G.bevel(o,.006,2);return o

def saw():
    # Actual teeth in the closed blade silhouette; no opacity texture.
    poly=[(-.14,.081)]
    for j in range(13):poly.extend(((-.12+j*.035,.056),(-.105+j*.035,.082)))
    poly.extend(((.365,.106),(.350,.163),(-.13,.222)))
    slab('Saw blade with thirteen teeth',poly,.012,M['iron'],bevel=0)
    # Individual sharpened teeth preserve open valleys in the silhouette.
    for j in range(13):slab('Polished saw tooth',[(-.12+j*.035,.057),(-.105+j*.035,.081),(-.13+j*.035,.081)],.0128,M['edge'],bevel=0)
    outer=[(-.31,.073),(-.332,.112),(-.329,.208),(-.298,.252),(-.170,.250),(-.135,.218),(-.151,.104),(-.202,.071)]
    inner=[(-.277,.111),(-.287,.133),(-.286,.184),(-.270,.207),(-.210,.207),(-.189,.187),(-.199,.13),(-.228,.111)]
    handle_ring('Open oak saw grip',outer,inner,.044,M['honey'])
    for x,z in ((-.16,.18),(-.183,.108)):rivet(x,z,-.027,.010)
    box('Saw handle cream grip wrap',(-.309,0,.162),(.036,.052,.061),M['linen'],.008)

def pickaxe():
    shaft(.818,.026)
    slab('Curved double ended pick',[(-.282,.697),(-.221,.753),(-.096,.803),(-.039,.811),(.052,.808),(.151,.779),(.255,.718),(.218,.775),(.156,.824),(.044,.854),(-.036,.858),(-.141,.836),(-.241,.772)],.048,M['iron'],bevel=.004)
    slab('Left pick polished tip',[(-.282,.697),(-.221,.753),(-.235,.772),(-.257,.735)],.049,M['edge'],bevel=.001)
    slab('Right pick chisel edge',[(.203,.752),(.255,.718),(.235,.753),(.217,.774)],.049,M['edge'],bevel=.001)
    collar(.736,.038,.08);rivet(0,.823,-.031,.013)

def shovel():
    shaft(.976,.022,wrap=False)
    # Blade rests above the handle in authoring coordinates; grip sits near heel.
    slab('Rounded shovel blade',[(-.079,.764),(-.129,.794),(-.125,.922),(-.085,1.026),(0,1.065),(.085,1.026),(.125,.922),(.129,.794),(.079,.764)],.025,M['iron'],bevel=.010)
    slab('Shovel sharpened rim',[(-.125,.91),(-.085,1.026),(0,1.065),(.085,1.026),(.125,.91),(.097,.925),(.067,1.003),(0,1.035),(-.067,1.003),(-.097,.925)],.026,M['edge'],bevel=.002)
    box('Shovel central rib',(0,-.020,.87),(.026,.020,.17),M['steel_dark'],.004)
    collar(.712,.031,.07)
    for x in (-.081,.081):box('Shovel shoulder step',(x,0,.770),(.112,.054,.030),M['steel_dark'],.005)
    grip(.076,.192,.026)
    rivet(0,.794,-.033,.010)

def hoe():
    shaft(.892,.023)
    collar(.797,.031,.067)
    # Broad hoe blade projects in front of the shaft, unlike axe/pick silhouettes.
    box('Hoe forged socket',(0,0,.85),(.066,.072,.073),M['iron'],.008)
    beam('Angled hoe neck',(0,-.02,.875),(0,-.19,.809),.036,.045,M['iron'])
    obj=slab('Hoe broad working blade',[(-.143,.742),(.143,.742),(.12,.818),(-.12,.818)],.025,M['iron'],cy=-.213,bevel=.005)
    box('Hoe polished scraping edge',(0,-.214,.744),(.282,.028,.021),M['edge'],.004)
    rivet(0,.849,-.040,.011)

def trowel():
    shaft(.184,.028,wrap=False)
    lathe('Trowel cream grip inlay',[(.03,0),(.031,.01),(.03,.057)],M['linen'],16,(0,0,.051))
    collar(.159,.032,.035)
    beam('Bent trowel tang',(0,0,.184),(0,-.016,.232),.019,.020,M['iron'])
    slab('Pointed mason trowel blade',[(-.087,.214),(-.078,.271),(0,.387),(.078,.271),(.087,.214)],.014,M['iron'],cy=-.020,bevel=.004)
    slab('Trowel polished perimeter',[(-.087,.214),(-.078,.271),(0,.387),(.078,.271),(.087,.214),(.072,.222),(.063,.267),(0,.361),(-.063,.267),(-.072,.222)],.015,M['edge'],cy=-.020,bevel=.001)
    rivet(0,.226,-.031,.009)

DATA=[('tool_hammer','hammer','Forged hammer',hammer,[0,0,.115],[.120,0,.404],[1,0,0],'one hand'),
 ('tool_mallet','mallet','Oak mallet',mallet,[0,0,.115],[.132,0,.395],[1,0,0],'one hand'),
 ('tool_axe','axe','Woodcutting axe',axe,[0,0,.125],[.226,0,.658],[1,0,0],'one or two hands'),
 ('tool_saw','saw','Carpenter hand saw',saw,[-.307,0,.158],[.13,0,.057],[1,0,0],'one hand'),
 ('tool_pickaxe','pickaxe','Forged pickaxe',pickaxe,[0,0,.125],[-.282,0,.697],[-1,0,0],'two hands'),
 ('tool_shovel','shovel','Garden shovel',shovel,[0,0,.135],[0,0,1.065],[0,0,1],'two hands'),
 ('tool_hoe','hoe','Field hoe',hoe,[0,0,.125],[0,-.214,.744],[0,-1,0],'two hands'),
 ('tool_trowel','trowel','Mason trowel',trowel,[0,0,.092],[0,-.020,.387],[0,0,1],'one hand')]
SPECS={'schema_version':1,'kit_id':'tool_kit_01','units':'metres','authoring_axes':{'up':'+Z','front':'-Y'},
 'glb_axes':{'up':'+Y','front':'+Z','convert_to_authoring':'(x,-z,y)'},'origin':'exact bottom centre of mesh envelope',
 'metadata_coordinates':'authoring local metres; glTF conversion is not applied to custom extras',
 'character_height_reference_m':1.557142,'tool_inventory_created':False,'runtime_attachment_verified':False,
 'grip_contract':'Align grip_anchor.position_m to a hand/socket target. Axes and working tips are authoring suggestions; no live skeleton, IK, recipe, collision or work animation has been bound.',
 'modules':[]}
REPORT={'kit_id':'tool_kit_01','blender_version':bpy.app.version_string,'built_via':'Blender MCP execute_blender_code',
 'original_meshes_only':True,'licensed_reference_included':False,'modules':[]}
ASSETS={}
for mid,kind,title,builder,grip_at,tip,axis,hands in DATA:
    G.PARTS.clear();builder()
    bpy.ops.object.select_all(action='DESELECT')
    for o in G.PARTS:o.select_set(True)
    bpy.context.view_layer.objects.active=G.PARTS[0];bpy.ops.object.join()
    obj=bpy.context.view_layer.objects.active;obj.name=mid;obj.data.name=mid
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    pts=[v.co.copy() for v in obj.data.vertices];lo=Vector([min(p[i] for p in pts) for i in range(3)]);hi=Vector([max(p[i] for p in pts) for i in range(3)])
    shift=Vector(((lo.x+hi.x)/2,(lo.y+hi.y)/2,lo.z))
    for v in obj.data.vertices:v.co-=shift
    grip_at=list(Vector(grip_at)-shift);tip=list(Vector(tip)-shift)
    scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    obj.data.update();G.uv_project(obj.data);obj.data.calc_loop_triangles();tris=len(obj.data.loop_triangles)
    assert tris<=3000,(mid,tris)
    obj['asset_id']=mid;obj['tool_id']=kind;obj['grip_anchor_m']=grip_at;obj['working_tip_m']=tip
    obj['tool_inventory_created']=False;obj['runtime_attachment_verified']=False
    path=OUT/'modules'/(mid+'.glb')
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,export_yup=True,export_apply=True,
      export_texcoords=True,export_normals=True,export_materials='EXPORT',export_extras=True,
      export_animations=False,export_cameras=False,export_lights=False)
    spec={'id':mid,'tool_id':kind,'label':title,'asset_glb':'modules/'+path.name,'nominal_size_m':[round(x,6) for x in hi-lo],
      'grip_anchor':{'position_m':grip_at,'handle_axis_local':[0,0,1]},
      'working_tip_m':tip,'suggested_working_axis_local':axis,'hand_count_suggestion':hands,
      'work_kind_suggestion':kind,'tool_inventory_created':False,'runtime_attachment_verified':False,
      'collision_suggestion':'Disable per-part collision while equipped; design one simple work-volume separately.'}
    if hands=='two hands' or kind=='axe':spec['secondary_grip_anchor_m']=list(Vector((0,0,.39 if kind!='axe' else .34))-shift)
    SPECS['modules'].append(spec)
    REPORT['modules'].append({'id':mid,'triangles':tris,'size_m':spec['nominal_size_m'],'materials':[m.name for m in obj.data.materials],
      'uv_layers':len(obj.data.uv_layers),'bytes':path.stat().st_size})
    ASSETS[mid]=obj
REPORT['total_triangles']=sum(m['triangles'] for m in REPORT['modules']);REPORT['generation_seconds']=round(time.monotonic()-START,3)
(OUT/'module-specs.json').write_text(json.dumps(SPECS,indent=2)+'\n',encoding='utf-8')
(OUT/'model-report.json').write_text(json.dumps(REPORT,indent=2)+'\n',encoding='utf-8')
DISPLAY=bpy.data.collections.new('DISPLAY | original tools');scene.collection.children.link(DISPLAY)
for i,(mid,obj) in enumerate(ASSETS.items()):
    clone=bpy.data.objects.new(mid+' | overview',obj.data);DISPLAY.objects.link(clone);clone.location=((i%4-1.5)*.8,(.5-i//4)*.8,0)
MODULES.hide_viewport=True;MODULES.hide_render=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'ToolKit.blend'))
print('TOOL_KIT_COMPLETE',len(ASSETS),REPORT['total_triangles'],flush=True)
