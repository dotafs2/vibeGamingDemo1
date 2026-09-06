"""Original, single-commodity carry props. Execute through local Blender MCP.

No stock, recipe, attachment, or UE state is created by this authoring script.
"""
from pathlib import Path
import importlib
import json
import math
import sys
import time
import bpy
from mathutils import Vector

OUT=Path(__file__).resolve().parent
if str(OUT) not in sys.path:sys.path.insert(0,str(OUT))
import goods_geometry as G
importlib.reload(G)
START=time.monotonic()
for folder in ('modules','previews','mcp'):(OUT/folder).mkdir(parents=True,exist_ok=True)
for obj in list(bpy.data.objects):bpy.data.objects.remove(obj,do_unlink=True)
for kind in ('collections','meshes','materials','armatures','actions','libraries'):
    for item in list(getattr(bpy.data,kind)):getattr(bpy.data,kind).remove(item)
scene=bpy.context.scene;scene.name='GoodsKit | eight original single-commodity carry props'
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
scene.render.threads_mode='FIXED';scene.render.threads=3
MODULES=bpy.data.collections.new('ORIGINAL_MODULES');scene.collection.children.link(MODULES)
M={
 'wood':G.mat('GK_Warm_Oak','BC9368',.43),'wood_honey':G.mat('GK_Honey_Plank','B2865C',.46),
 'wood_dark':G.mat('GK_Timber_Recess','594337',.55),'wood_edge':G.mat('GK_Cut_Edge','D0AC7A',.44),
 'cream':G.mat('GK_Cream_Label','E9DCB9',.61),'rope':G.mat('GK_Hemp_Rope','C8B085',.64),
 'wicker':G.mat('GK_Golden_Wicker','CAA771',.55),'wicker_dark':G.mat('GK_Wicker_Weave','AC895A',.57),
 'clay':G.mat('GK_Raw_Clay','A67D67',.64),'clay_light':G.mat('GK_Clay_Cut','BD9377',.61),
 'brick':G.mat('GK_Fired_Brick','B67450',.47),'brick_light':G.mat('GK_Fired_Brick_Light','C58761',.46),
 'terracotta':G.mat('GK_Terracotta_Tile','B96242',.30),'terra_light':G.mat('GK_Terracotta_Tile_Light','D47D55',.32),
 'slateblue':G.mat('GK_Slateblue_Tile','587C91',.31),'slate_light':G.mat('GK_Slateblue_Tile_Light','7397A4',.32),
 'lime':G.mat('GK_Lime_Powder','E2DCC5',.73),'ceramic':G.mat('GK_Cream_Glazed_Pail','D8D0B2',.32),
 'iron':G.mat('GK_Forged_Iron','535B5E',.34,.65),'iron_light':G.mat('GK_Iron_Edge','809091',.32,.65),
 'ochre':G.mat('GK_Ochre_Pigment','C59444',.65),'red':G.mat('GK_Red_Pigment','9F5B49',.65),
 'blue':G.mat('GK_Blue_Pigment','567F96',.65),'ink':G.mat('GK_Stamped_Charcoal','514D42',.62)
}
G.configure(MODULES,M)
box,beam,mesh=G.box,G.beam,G.mesh

def lathe(name,profile,material,center=(0,0,0),n=20,closed=False):
    cx,cy,cz=center
    verts=[(cx+r*math.cos(j*math.tau/n),cy+r*math.sin(j*math.tau/n),cz+z) for r,z in profile for j in range(n)]
    faces=[]
    for row in range(len(profile)-1):
        for j in range(n):
            a=row*n+j;b=row*n+(j+1)%n;faces.append((a,b,b+n,a+n))
    if closed:
        for j in range(n):faces.append(((len(profile)-1)*n+j,(len(profile)-1)*n+(j+1)%n,(j+1)%n,j))
    else:
        faces.append(tuple(reversed(range(n))));faces.append(tuple((len(profile)-1)*n+j for j in range(n)))
    return mesh(name,verts,faces,material,range(len(faces) if closed else len(faces)-2))

def tube(name,points,radius,material,sides=6):
    points=[Vector(p) for p in points];verts=[]
    for i,p in enumerate(points):
        tangent=(points[min(i+1,len(points)-1)]-points[max(i-1,0)]).normalized()
        helper=Vector((0,0,1)) if abs(tangent.z)<.9 else Vector((0,1,0))
        u=tangent.cross(helper).normalized();v=tangent.cross(u).normalized()
        verts.extend(p+radius*(u*math.cos(j*math.tau/sides)+v*math.sin(j*math.tau/sides)) for j in range(sides))
    faces=[]
    for i in range(len(points)-1):
        for j in range(sides):
            a=i*sides+j;b=i*sides+(j+1)%sides;faces.append((a,b,b+sides,a+sides))
    faces.append(tuple(reversed(range(sides))));faces.append(tuple((len(points)-1)*sides+j for j in range(sides)))
    return mesh(name,verts,faces,material,range(len(faces)-2))

def lump(name,center,scale,material,subdivisions=1):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=subdivisions,radius=1,location=center)
    obj=G.adopt(bpy.context.view_layer.objects.active,material);obj.name=name;obj.scale=scale
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    G.uv_project(obj.data)
    for p in obj.data.polygons:p.use_smooth=True
    return obj

def crate(width=.62,depth=.39,height=.30,slats=2):
    for j in range(3):box('Crate floor plank',((j-1)*width/3,0,.035),(width/3-.005,depth-.035,.05),M['wood_dark'],.006)
    for x in (-width/2+.055,width/2-.055):
        box('Crate support runner',(x,0,.0125),(.065,depth,.025),M['wood_honey'],.005)
        for y in (-depth/2+.022,depth/2-.022):box('Crate corner post',(x,y,height/2),(.048,.045,height),M['wood_edge'],.007)
    for y in (-depth/2+.015,depth/2-.015):
        for j in range(slats):box('Crate visible horizontal slat',(0,y,.102+j*.088),(width,.031,.080),M['wood'] if j else M['wood_honey'],.007)
    for x in (-width/2+.017,width/2-.017):
        box('Crate end wall',(x,0,.115),(.034,depth-.025,.12),M['wood_honey'],.007)
        for y in (-depth/2+.034,depth/2-.034):box('Hand opening side',(x,y,.226),(.042,.055,.15),M['wood'],.009)
        box('Rounded crate grip',(x,0,height-.020),(.048,depth-.038,.042),M['wood_edge'],.010)
    # Legible blank label and distinct commodity icon are geometry, not text/font dependencies.
    box('Cream commodity label',(0,-depth/2-.005,.147),(.132,.014,.075),M['cream'],.008)

def brick_icon(depth=.39):
    box('Brick stock stamp',(0,-depth/2-.014,.147),(.064,.006,.027),M['brick'],.002)
    box('Brick stamp mortar',(0,-depth/2-.018,.147),(.004,.005,.027),M['cream'],.001)

def clay_basket():
    lathe('Open clay basket',[(.15,0),(.166,.025),(.218,.245),(.210,.261),(.197,.245),(.146,.045)],M['wicker'],n=24)
    for k in range(3):
        z=.070+k*.068;r=.166+(z-.025)*.236
        lathe('Raised woven band',[(r,z-.009),(r+.008,z),(r+.003,z+.009),(r-.005,z)],M['wicker_dark'],n=24,closed=True)
    for j in range(12):
        a=j*math.tau/12
        tube('Basket upright weave',[(.162*math.cos(a),.162*math.sin(a),.032),(.219*math.cos(a),.219*math.sin(a),.245)],.007,M['wicker_dark'])
    for sign in (-1,1):
        x=sign*.215
        tube('Rope basket side grip',[(x,-.07,.23),(x+sign*.025,-.075,.29),(x+sign*.035,0,.32),(x+sign*.025,.075,.29),(x,.07,.23)],.014,M['rope'])
    for j,(x,y,z,scale) in enumerate(((-.09,-.055,.24,(.112,.10,.075)),(.086,-.07,.249,(.10,.09,.07)),(-.057,.10,.252,(.105,.085,.075)),(.075,.078,.30,(.113,.087,.063)))):
        lump('Raw clay lump',(x,y,z),scale,M['clay'] if j%2 else M['clay_light'])
    tube('Clay packing cord',[(-.1,-.07,.303),(-.04,-.062,.30),(.017,-.052,.285)],.006,M['clay'])

def bricks():
    crate();brick_icon()
    for row in range(3):
        for x in (-.137,.137):
            for y in (-.079,.079):
                z=.090+row*.083
                box('Single fired brick',(x,y,z),(.257,.143,.074),M['brick'] if (row+int(x>0)+int(y>0))%2 else M['brick_light'],.011)
                if row==2:
                    for dx in (-.064,.064):box('Pressed brick frog',(x+dx,y,z+.037),(.044,.042,.004),M['brick'],.003)

def tiles(slate=False):
    crate(.64,.39,.30)
    material=M['slateblue'] if slate else M['terracotta'];accent=M['slate_light'] if slate else M['terra_light']
    for x in (-.14,.14):
        for layer in range(5):
            G.arch_tile('Curved slateblue roof tile' if slate else 'Curved fired clay roof tile',
                (x,-.145,.075+layer*.042),(0,1,0),(1,0,0),(0,0,1),.29,.235,
                accent if layer%3==1 else material,.047)
    # Miniature tile silhouette is separate from the brick crate label.
    tube('Tile commodity stamp',[(-.045,-.211,.14),(-.022,-.211,.164),(0,-.211,.17),(.022,-.211,.164),(.045,-.211,.14)],.007,material)

def lime():
    lathe('Cream ceramic lime pail',[(.143,0),(.155,.016),(.186,.267),(.183,.282),(.165,.282),(.156,.045),(.132,.028)],M['ceramic'],n=24)
    lathe('Pail rolled rim',[(.182,.266),(.19,.278),(.186,.290),(.169,.290),(.165,.279)],M['iron'],n=24,closed=True)
    lathe('Lime powder level',[(.151,.240),(.155,.253),(.134,.272),(.046,.286)],M['lime'],n=24)
    for x,y,z in ((-.058,-.04,.275),(.045,.05,.28),(.068,-.06,.271)):lump('Lime powder soft clump',(x,y,z),(.050,.043,.020),M['lime'])
    tube('Lowered carry bail',[(-.18,0,.24),(-.21,-.055,.30),(-.20,-.108,.37),(-.11,-.137,.43),(.11,-.137,.43),(.20,-.108,.37),(.21,-.055,.30),(.18,0,.24)],.012,M['iron'])
    tube('Wood pail hand grip',[(-.083,-.137,.43),(.083,-.137,.43)],.022,M['wood_edge'],8)
    box('Lime stock cream badge',(0,-.18,.149),(.112,.018,.075),M['wood'],.008)
    lump('Lime badge powder diamond',(0,-.192,.15),(.023,.009,.026),M['lime'])

def pigments():
    crate(.63,.34,.30,slats=1)
    for i,(x,key) in enumerate(((-.193,'ochre'),(0,'red'),(.193,'blue'))):
        lathe('Pigment pot '+key,[(.064,.0),(.077,.014),(.087,.164),(.079,.211),(.080,.223),(.066,.223),(.057,.035)],M['ceramic'],(x,0,.057),20)
        lathe('Open pigment powder '+key,[(.061,.0),(.065,.006),(.039,.015)],M[key],(x,0,.26),20)
        lathe('Painted pigment identification band '+key,[(.079,.126),(.080,.128),(.085,.167),(.084,.169)],M[key],(x,0,.057),20,True)
        box('Pigment pot packing divider',(x+.091,0,.127),(.012,.24,.12),M['wood_dark'],.002) if i<2 else None
    for x,key in ((-.035,'ochre'),(0,'red'),(.035,'blue')):lump('Pigment commodity stamp',(x,-.182,.145),(.010,.004,.018),M[key])

def ingot(name,center,material):
    x,y,z=center
    verts=[(x+sx*a,y+sy*b,z+h) for a,b,h in ((.15,.06,0),(.137,.052,.06)) for sx,sy in ((-1,-1),(1,-1),(1,1),(-1,1))]
    return mesh(name,verts,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],material)

def iron():
    for x in (-.23,.23):box('Iron transport wood runner',(x,0,.025),(.095,.35,.05),M['wood'],.008)
    box('Iron bundle wood base',(0,0,.071),(.64,.37,.058),M['wood_honey'],.009)
    for row in range(3):
        for x in (-.157,.157):
            for y in (-.069,.069):ingot('Stacked single iron ingot',(x,y,.10+row*.063),M['iron_light'] if row==2 else M['iron'])
    for x in (-.22,.22):
        tube('Hemp binding around ingots',[(x,-.187,.082),(x,-.179,.29),(x,-.15,.306),(x,.15,.306),(x,.18,.288),(x,.187,.083)],.015,M['rope'])
    tube('Front iron transport grip',[(-.12,-.187,.10),(-.09,-.217,.182),(.09,-.217,.182),(.12,-.187,.10)],.017,M['rope'])

def nail(center,angle,tilt=0):
    x,y,z=center
    d=Vector((math.sin(angle)*math.cos(tilt),math.cos(angle)*math.cos(tilt),math.sin(tilt)))
    a=Vector(center);b=a+d*.115
    tube('Individual forged nail shank',[a,b],.007,M['iron'],4)
    obj=G.cylinder('Square forged nail head',b,.016,.010,M['iron_light'],4,d)

def nails():
    crate(.47,.35,.28,slats=2)
    box('Existing nail stock underlayer',(0,0,.113),(.35,.24,.073),M['iron'],.008)
    for row in range(3):
        for col in range(4):
            x=-.135+col*.084;y=-.088+row*.069;z=.16+(row%2)*.017
            nail((x,y,z),.35 if (row+col)%2 else -.36,.10 if col%2 else -.05)
    nail((-.034,-.028,.212),1.25,.16)
    tube('Nail stock label shank',[(-.035,-.197,.13),(.028,-.197,.165)],.005,M['iron'],4)
    tube('Nail stock label head',[(.020,-.197,.18),(.037,-.197,.151)],.007,M['iron'],4)

DATA=[
 ('goods_raw_clay_basket','raw_clay','Raw clay basket',clay_basket,4,'visible clay lumps',[0,0,.16],[-.245,0,.29],[.245,0,.29]),
 ('goods_bricks_crate','bricks','Fired brick crate',bricks,12,'visible fired bricks',[0,0,.16],[-.292,0,.28],[.292,0,.28]),
 ('goods_tiles_terracotta_crate','tiles_terracotta','Terracotta tile crate',lambda:tiles(False),10,'visible terracotta tiles',[0,0,.16],[-.302,0,.28],[.302,0,.28]),
 ('goods_tiles_slateblue_crate','tiles_slateblue','Slate blue tile crate',lambda:tiles(True),10,'visible slate blue tiles',[0,0,.16],[-.302,0,.28],[.302,0,.28]),
 ('goods_lime_pail','lime','Lime pail',lime,1,'pail of lime; no physical mass specified',[0,0,.17],[-.075,-.137,.43],[.075,-.137,.43]),
 ('goods_pigment_pots','pigment','Pigment pot carrier',pigments,3,'pigment pots; colorways are visual only',[0,0,.16],[-.297,0,.28],[.297,0,.28]),
 ('goods_iron_ingots_bundle','iron_ingots','Bound iron ingots',iron,12,'visible iron ingots',[0,0,.16],[-.22,0,.305],[.22,0,.305]),
 ('goods_nails_box','nails','Forged nail box',nails,13,'individually modeled nails plus visual underlayer',[0,0,.15],[-.217,0,.26],[.217,0,.26])
]
SPECS={'schema_version':1,'kit_id':'goods_kit_01','units':'metres','authoring_axes':{'up':'+Z','front':'-Y'},
 'glb_axes':{'up':'+Y','front':'+Z','convert_back_to_authoring':'(x,y,z) = (gltf.x,-gltf.z,gltf.y)'},
 'origin':'bottom centre of each exported mesh envelope','character_height_reference_m':1.557142,
 'carry_contract':'Anchor and grip positions are design proposals in authoring local metres. Align carry_anchor to an abdomen-front carry target; preserve authored scale. No socket, animation or IK is bound.',
 'suggested_resident_target_m':[0,-.60,.86], 'creates_inventory':False,'runtime_integration_verified':False,'modules':[]}
REPORT={'kit_id':'goods_kit_01','blender_version':bpy.app.version_string,'built_via':'Blender MCP execute_blender_code',
 'source_meshes_are_original_only':True,'licensed_reference_mesh_included':False,'modules':[]}
ASSETS={}
for mid,cid,title,build,quantity,capacity_note,anchor,left,right in DATA:
    G.PARTS.clear();build()
    bpy.ops.object.select_all(action='DESELECT')
    for obj in G.PARTS:obj.select_set(True)
    bpy.context.view_layer.objects.active=G.PARTS[0];bpy.ops.object.join()
    obj=bpy.context.view_layer.objects.active;obj.name=mid;obj.data.name=mid
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    # Exact envelope centre in X/Y and supporting plane at Z=0.
    bounds=[v.co.copy() for v in obj.data.vertices]
    lo=Vector([min(p[i] for p in bounds) for i in range(3)]);hi=Vector([max(p[i] for p in bounds) for i in range(3)])
    shift=Vector(((lo.x+hi.x)/2,(lo.y+hi.y)/2,lo.z))
    for v in obj.data.vertices:v.co-=shift
    anchor,left,right=[list(Vector(p)-shift) for p in (anchor,left,right)]
    scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    obj['asset_id']=mid;obj['commodity_id']=cid;obj['creates_inventory']=False
    obj['carry_anchor_m']=anchor;obj['grip_left_m']=left;obj['grip_right_m']=right
    obj['runtime_integration_verified']=False;obj['visual_capacity_suggestion']=quantity
    # UV project after all modifiers/join, so every exported loop has explicit UVs.
    obj.data.update();G.uv_project(obj.data)
    obj.data.calc_loop_triangles();tris=len(obj.data.loop_triangles)
    assert tris<=3000,(mid,tris)
    path=OUT/'modules'/(mid+'.glb')
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,export_yup=True,
        export_apply=True,export_texcoords=True,export_normals=True,export_materials='EXPORT',
        export_extras=True,export_animations=False,export_cameras=False,export_lights=False)
    dims=list(hi-lo)
    SPECS['modules'].append({'id':mid,'commodity_id':cid,'label':title,'category':'carry_goods','asset_glb':'modules/'+path.name,
        'nominal_size_m':[round(x,6) for x in dims],'carry_anchor':{'position_m':anchor,'orientation_euler_degrees':[0,0,0]},
        'grip_points':{'left_m':left,'right_m':right},'visual_capacity_suggestion':{'quantity':quantity,'unit':'visual units','note':capacity_note,'is_gameplay_capacity':False},
        'creates_inventory':False,'recipe_bound':False,'holding_animation_verified':False,
        'pickup_clearance_suggestion_m':.12,'collision_suggestion':'One simple convex hull; detailed visual parts have no individual gameplay collision.'})
    REPORT['modules'].append({'id':mid,'commodity_id':cid,'triangles':tris,'size_m':[round(x,6) for x in dims],
        'bounds_min_m':[round(x,6) for x in lo-shift],'bounds_max_m':[round(x,6) for x in hi-shift],
        'uv_layers':len(obj.data.uv_layers),'materials':[m.name for m in obj.data.materials],'bytes':path.stat().st_size})
    ASSETS[mid]=obj
REPORT['total_triangles']=sum(m['triangles'] for m in REPORT['modules']);REPORT['generation_seconds']=round(time.monotonic()-START,3)
(OUT/'module-specs.json').write_text(json.dumps(SPECS,indent=2)+'\n',encoding='utf-8')
(OUT/'model-report.json').write_text(json.dumps(REPORT,indent=2)+'\n',encoding='utf-8')
DISPLAY=bpy.data.collections.new('DISPLAY | originals only');scene.collection.children.link(DISPLAY)
for i,(mid,obj) in enumerate(ASSETS.items()):
    clone=bpy.data.objects.new(mid+' | overview',obj.data);DISPLAY.objects.link(clone)
    clone.location=((i%4-1.5)*.95,(.5-i//4)*.95,0)
MODULES.hide_render=True;MODULES.hide_viewport=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'GoodsKit.blend'))
print('GOODS_KIT_COMPLETE',len(ASSETS),REPORT['total_triangles'],flush=True)
