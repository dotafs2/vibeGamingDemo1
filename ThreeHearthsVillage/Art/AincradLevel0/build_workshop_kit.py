"""Original modular workshop fixtures. Run with Blender --background --python this_file.
Re-run after build_town_kit.py, then Tools/import_aincrad_workshop_kit.py in UE.
Does not create simulation inventory, contracts, animation, or resident actions.
"""
import bpy, json, math
from pathlib import Path
from mathutils import Vector, Matrix
OUT=Path(__file__).resolve().parent
STYLE=json.loads((OUT/'style.json').read_text(encoding='utf-8'))
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene
scene.unit_settings.system='METRIC'
MATS={}
def lin(v): return v/12.92 if v<=.04045 else ((v+.055)/1.055)**2.4
for role,code in STYLE['palette_srgb'].items():
    m=bpy.data.materials.new('AT_'+role); m.use_nodes=True
    rgb=[lin(int(code[i:i+2],16)/255) for i in (0,2,4)]
    m.diffuse_color=(*rgb,1)
    n=m.node_tree.nodes.get('Principled BSDF'); n.inputs['Base Color'].default_value=(*rgb,1)
    n.inputs['Roughness'].default_value=.62 if role in ('Iron','Brass') else .82
    n.inputs['Metallic'].default_value=.4 if role in ('Iron','Brass') else 0
    MATS[role]=m
PARTS=[]
def finish(o,name,role,bevel=.006):
    o.name=name; o.data.materials.append(MATS[role])
    bpy.context.view_layer.objects.active=o
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    if bevel:
        b=o.modifiers.new('Crafted edge','BEVEL'); b.width=bevel; b.segments=2
        bpy.ops.object.modifier_apply(modifier=b.name)
    if not o.data.uv_layers: o.data.uv_layers.new(name='UVMap')
    uv=o.data.uv_layers.active.data
    for poly in o.data.polygons:
        axes=sorted(range(3),key=lambda i:abs(poly.normal[i]))[:2]
        for li in poly.loop_indices:
            v=o.data.vertices[o.data.loops[li].vertex_index].co
            uv[li].uv=(v[axes[0]],v[axes[1]])
    PARTS.append(o); return o

def box(name,loc,size,role='Timber',bevel=.006):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc)
    o=bpy.context.object; o.dimensions=size
    return finish(o,name,role,bevel)
def cyl(name,loc,radius,depth,role='Iron',axis='Z',vertices=16):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices,radius=radius,depth=depth,location=loc)
    o=bpy.context.object
    if axis=='Y':o.rotation_euler.x=math.pi/2
    if axis=='X':o.rotation_euler.y=math.pi/2
    return finish(o,name,role,.002)
def beam(name,a,b,width,role='Timber'):
    mid=(Vector(a)+Vector(b))*.5; d=Vector(b)-Vector(a)
    o=box(name,mid,(width,width,d.length),role,min(.007,width*.12))
    o.rotation_euler=d.to_track_quat('Z','Y').to_euler();return o
def transform(items,loc=(0,0,0),yaw=0):
    mat=Matrix.Translation(Vector(loc))@Matrix.Rotation(math.radians(yaw),4,'Z')
    for o in items:o.matrix_world=mat@o.matrix_world

def vise():
    start=len(PARTS)
    box('Fixed jaw', (0,.02,.13),(.32,.09,.23),'TimberDark')
    box('Moving jaw',(0,-.14,.13),(.32,.07,.23),'Timber')
    for x in (-.11,.11):cyl('Guide screw',(x,-.07,.095),.015,.34,'Iron','Y')
    cyl('Wood screw knob',(0,-.245,.095),.047,.06,'TimberDark','Y')
    cyl('Sliding handle',(0,-.28,.095),.012,.28,'TimberLight','X')
    for x in (-.14,.14):cyl('Handle cap',(x,-.28,.095),.021,.018,'TimberDark','X')
    return PARTS[start:]
def plane():
    start=len(PARTS)
    box('Plane body',(0,0,.035),(.26,.09,.07),'TimberDark',.012)
    box('Sole',(0,0,.007),(.27,.093,.014),'Iron',.003)
    blade=box('Angled cutter',(-.01,0,.08),(.055,.065,.075),'Iron',.002);blade.rotation_euler.y=math.radians(-30)
    for x in (-.08,.09):
        cyl('Plane grip',(x,0,.095),.022,.075,'TimberLight')
    return PARTS[start:]
def anvil():
    start=len(PARTS)
    box('Anvil foot',(0,0,.045),(.45,.25,.09),'Iron',.015)
    box('Anvil waist',(-.025,0,.16),(.22,.16,.21),'Iron',.012)
    box('Anvil face',(-.04,0,.29),(.45,.24,.08),'Iron',.006)
    # An asymmetric tapered horn, with a readable tip and flat heel.
    verts=[]
    for x,ry,rz,z in ((.18,.12,.04,.29),(.33,.09,.045,.285),(.49,.025,.018,.285),(.53,.004,.004,.29)):
        for k in range(12):
            a=2*math.pi*k/12;verts.append((x,math.cos(a)*ry,z+math.sin(a)*rz))
    faces=[tuple(reversed(range(12)))]
    for ring in range(3):
        for k in range(12): faces.append((ring*12+k,ring*12+(k+1)%12,(ring+1)*12+(k+1)%12,(ring+1)*12+k))
    faces.append(tuple(range(36,48)))
    mesh=bpy.data.meshes.new('Forged horn');mesh.from_pydata(verts,[],faces);mesh.update()
    o=bpy.data.objects.new('Tapered horn',mesh);scene.collection.objects.link(o);finish(o,'Tapered horn','Iron',.002)
    for x in (-.15,.15):
        for y in (-.085,.085):cyl('Mounting pin',(x,y,.097),.018,.027,'Brass')
    return PARTS[start:]
def smith_tools():
    start=len(PARTS)
    beam('Hammer haft',(-.2,-.055,.025),(.13,-.055,.025),.026,'TimberLight')
    box('Forged hammer head',(.15,-.055,.037),(.075,.145,.07),'Iron',.008)
    for side in (-1,1):
        beam('Tongs handle',(-.18,.065+side*.03,.025),(.06,.07,.025),.012,'Iron')
        beam('Tongs jaw',(.06,.07,.025),(.15,.07+side*.022,.025),.016,'Iron')
    cyl('Tongs pivot',(.045,.07,.029),.018,.025,'Brass')
    return PARTS[start:]
def bench(smith=False):
    start=len(PARTS)
    # Common adult-height trestle proportions; diagonal bracing and joinery.
    for x in (-.70,.70):
        for y in (-.29,.29):
            o=box('Splayed trestle leg',(x,y,.40),(.115,.13,.80),'TimberDark',.01)
            o.rotation_euler.y=math.radians(-3 if x<0 else 3)
            box('Iron foot collar',(x,y,.065),(.122,.137,.10),'Iron',.004)
        box('Trestle lower tie',(x,0,.18),(.12,.72,.10),'Timber',.008)
        beam('Diagonal brace',(x,-.28,.20),(x,.28,.66),.07,'Timber')
    box('Long lower stretcher',(0,0,.24),(1.52,.105,.13),'TimberDark',.008)
    for x in (-.81,.81):box('Wedged through tenon',(x,0,.24),(.065,.16,.075),'TimberLight',.005)
    for y in (-.325,.325):box('Deep apron',(0,y,.70),(1.66,.09,.17),'Timber',.008)
    for i in range(5):
        box('Thick top plank',(0,(i-2)*.165,.82),(1.8,.158,.12),'TimberLight' if i in (1,4) else 'Timber',.006)
    for x in (-.73,.73):
        for y in (-.32,.32):cyl('Recessed top peg',(x,y,.881),.012,.006,'TimberDark')
    if smith:
        box('Protective forge plate',(.18,.02,.891),(1.04,.65,.022),'Iron',.005)
        for x in (-.27,.62):
            for y in (-.24,.28):cyl('Plate rivet',(x,y,.906),.012,.012,'Brass')
        transform(anvil(),(.12,.035,.908),180)
        transform(smith_tools(),(-.57,-.025,.88),82)
        box('Tool rail',(0,.34,.53),(1.24,.065,.07),'Iron',.004)
        for x in (-.37,.0,.37):
            beam('Rail hanger',(x,.37,.53),(x,.40,.40),.022,'Iron')
    else:
        transform(vise(),(-.47,-.345,.71))
        transform(plane(),(.36,-.12,.885),-12)
        # Bench dogs and a planing stop are fixed workshop features, not tradable lumber.
        for x in (-.08,.14,.36,.58):
            cyl('Bench dog socket',(x,.24,.882),.016,.008,'Iron')
        box('Planing stop',(.66,.12,.909),(.055,.23,.055),'TimberDark',.003)
        for x in (-.59,-.30,.02,.31,.60):box('Lower slatted shelf',(x,0,.325),(.23,.53,.055),'Timber',.004)
    return PARTS[start:]
ASSETS=[];GROUPS={}
def export(name,items,role):
    bpy.ops.object.select_all(action='DESELECT')
    for o in items:o.select_set(True)
    bpy.context.view_layer.objects.active=items[0]
    dest=OUT/'Modules'/(name+'.glb');dest.parent.mkdir(exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(dest),export_format='GLB',use_selection=True,export_apply=True,export_yup=True,export_materials='EXPORT')
    points=[o.matrix_world@v.co for o in items for v in o.data.vertices]
    ASSETS.append({'id':name,'glb':dest.relative_to(OUT).as_posix(),'role':role,'vertices':sum(len(o.data.vertices) for o in items),'triangles':sum(sum(len(f.vertices)-2 for f in o.data.polygons) for o in items),'objects':len(items),'bounds_min_m':[min(v[i] for v in points) for i in range(3)],'bounds_max_m':[max(v[i] for v in points) for i in range(3)],'uv':'UV0 metric projected; shared material palette','materials':sorted({m.name for o in items for m in o.data.materials})})
    col=bpy.data.collections.new(name);scene.collection.children.link(col)
    for o in items:
        for old in list(o.users_collection):old.objects.unlink(o)
        col.objects.link(o)
    GROUPS[name]=items
    for o in items:o.hide_render=True;o.hide_set(True)
for name,fn,role in [('bench_vise',vise,'reusable carpentry vise'),('hand_plane',plane,'reusable hand plane'),('bench_anvil',anvil,'reusable mounted anvil'),('smith_hand_tools',smith_tools,'reusable hammer and tongs'),('carpentry_workbench',lambda:bench(False),'existing carpenter work-table visual'),('smithy_workbench',lambda:bench(True),'existing smith work-table visual')]:export(name,fn(),role)
manifest={'style_id':STYLE['id'],'revision':1,'source_axis':STYLE['source_axis'],'scope':'Original static fixtures. Two existing work-table replacements; four reusable components. No new stock, wealth, skills, actions, repair animations or forced NPC choice.','assets':ASSETS}
(OUT/'workshop_kit_manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
for base,module in [('SM_Carpentry','carpentry_workbench'),('SM_Smithy','smithy_workbench')]:
    path=OUT/'Recipes'/(base+'.json');recipe=json.loads(path.read_text(encoding='utf-8'))
    rows=[p for p in recipe['parts'] if p['id']=='working_table'];assert len(rows)==1
    rows[0]['module']=module
    recipe['workshop_fixture_revision']=1
    path.write_text(json.dumps(recipe,ensure_ascii=False,indent=2),encoding='utf-8')
# Save a legible editable presentation with the reusable components retained in hidden collections.
for name,offset in [('carpentry_workbench',(-1.18,0,0)),('smithy_workbench',(1.18,.22,0))]:
    for o in GROUPS[name]:o.hide_render=False;o.hide_set(False)
    transform(GROUPS[name],offset)
box('Studio plinth',(0,0,-.07),(5.5,3.6,.12),'StoneLight',.025)
world=bpy.data.worlds.new('Town daylight');world.use_nodes=True
world.node_tree.nodes['Background'].inputs['Color'].default_value=(.63,.72,.84,1)
world.node_tree.nodes['Background'].inputs['Strength'].default_value=.55;scene.world=world
for name,loc,power,size in [('Key',(-3,-4,7),900,5),('Fill',(4,-1,4),650,4),('Rim',(1,4,6),1000,3)]:
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.shape='DISK';data.size=size
    obj=bpy.data.objects.new(name,data);scene.collection.objects.link(obj);obj.location=loc;obj.rotation_euler=(Vector((0,0,.5))-obj.location).to_track_quat('-Z','Y').to_euler()
cam=bpy.data.objects.new('Workshop kit camera',bpy.data.cameras.new('Workshop kit camera'));scene.collection.objects.link(cam);scene.camera=cam
cam.location=(4.2,-6.2,4.5);cam.rotation_euler=(Vector((0,0,.52))-cam.location).to_track_quat('-Z','Y').to_euler();cam.data.type='ORTHO';cam.data.ortho_scale=5.8
scene.render.engine='CYCLES';scene.cycles.samples=32;scene.cycles.use_denoising=True
scene.render.threads_mode='FIXED';scene.render.threads=6
scene.render.resolution_x=1400;scene.render.resolution_y=1000;scene.render.resolution_percentage=100
scene.view_settings.view_transform='AgX'
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'AincradWorkshopKit.blend'))
scene.render.filepath=str(OUT/'Previews/workshop_source.png');bpy.ops.render.render(write_still=True)
print('AINCRAD_WORKSHOP_KIT_EXPORTED',len(ASSETS),flush=True)
