"""Build an original Cropout-inspired village cottage. Run using Blender --background --python.

Only the HOUSE collection is exported. Units: metres; ground/root: Z=0; front: -Y.
No external textures, services, add-ons or downloaded assets are required.
"""
import bpy
import bmesh
import json
import math
import random
import sys
from pathlib import Path
from mathutils import Vector

OUT = Path(__file__).resolve().parent
RNG = random.Random(17)
ARGS = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
RENDER = '--no-render' not in ARGS

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1.0
house = bpy.data.collections.new('HOUSE | Hearth Cottage')
stage = bpy.data.collections.new('PRESENTATION | not exported')
scene.collection.children.link(house)
scene.collection.children.link(stage)
root = bpy.data.objects.new('HearthCottage_ROOT', None)
house.objects.link(root)
root['asset_id'] = 'hearth_cottage_01'
root['front_axis'] = '-Y (Blender)'
root['units'] = 'metres'
root['purpose'] = 'Exterior village house; visual mesh, gameplay/collision assigned in UE.'
root['door_anchor'] = [0.0, -2.35, 0.0]

def linear(v):
    return v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4

def material(name, hex_color, roughness=.78, metallic=0.0):
    rgb = tuple(linear(int(hex_color[i:i+2], 16) / 255) for i in (0, 2, 4))
    m = bpy.data.materials.new(name)
    m.diffuse_color = (*rgb, 1)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (*rgb, 1)
    bsdf.inputs['Roughness'].default_value = roughness
    bsdf.inputs['Metallic'].default_value = metallic
    return m

M = {
    'plaster': material('01 | warm lime plaster', 'E8DCBA'),
    'gable': material('02 | gable cream', 'DACCAB'),
    'wood': material('03 | chestnut timber', '82583F'),
    'wood_light': material('04 | cut timber and door', 'B1885D'),
    'wood_dark': material('05 | timber recess', '503C30'),
    'stone': material('06 | soft blue-grey stone', '969EA3'),
    'stone_light': material('07 | stone edge', 'B8BBB2'),
    'stone_dark': material('08 | chimney opening', '515966'),
    'shutter': material('09 | sage-blue shutters', '799B98'),
    'glass': material('10 | opaque blue glass', '526D73', .3),
    'glass_light': material('11 | glass highlight', '9FBBB2', .4),
    'metal': material('12 | forged iron', '514D47', .63, .15),
    'leaf': material('13 | sage foliage', '7E974E'),
    'leaf_light': material('14 | sunlit foliage', 'A2B766'),
    'flower': material('15 | marigold', 'EAB967'),
    'soil': material('16 | planter earth', '64513C'),
}
TILES = [material('Roof | ' + str(i+1), c) for i,c in enumerate(['C98375', 'D69282', 'BB786C', 'D09A89'])]

def assign(obj, mat, collection=house):
    for c in list(obj.users_collection):
        c.objects.unlink(obj)
    collection.objects.link(obj)
    if collection == house:
        obj.parent = root
    if mat:
        obj.data.materials.append(mat)
    return obj

def mesh(name, verts, faces, mat, collection=house):
    data = bpy.data.meshes.new(name)
    data.from_pydata(verts, [], faces)
    data.update()
    bm = bmesh.new()
    bm.from_mesh(data)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(data)
    bm.free()
    obj = bpy.data.objects.new(name, data)
    collection.objects.link(obj)
    if collection == house:
        obj.parent = root
    if mat:
        data.materials.append(mat)
    return obj

def bevel(obj, width=.025):
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    mod = obj.modifiers.new('Small hand-cut edges', 'BEVEL')
    mod.width = width
    mod.segments = 1
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj

def box(name, center, dims, mat, edge=.02, collection=house):
    bpy.ops.mesh.primitive_cube_add(size=1, location=center)
    obj = bpy.context.object
    obj.name = name
    obj.dimensions = dims
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    assign(obj, mat, collection)
    if edge:
        bevel(obj, edge)
    return obj

def beam(name, a, b, width, depth, mat=None, edge=.025):
    a,b = Vector(a),Vector(b)
    obj = box(name, (a+b)*.5, (width,depth,(b-a).length), mat or M['wood'], edge)
    obj.rotation_euler = (b-a).to_track_quat('Z','Y').to_euler()
    return obj

def cylinder(name, center, radius, depth, mat, vertices=10, direction=None):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=center)
    obj = assign(bpy.context.object, mat)
    obj.name = name
    if direction:
        obj.rotation_euler = Vector(direction).to_track_quat('Z','Y').to_euler()
    return obj

def ico(name, center, scale, mat):
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=1, location=center)
    obj = assign(bpy.context.object, mat)
    obj.name = name
    obj.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return obj

# Broad, compact proportions and chunky edges read well at the existing game camera distance.
box('Foundation | solid plinth', (0,0,.13), (3.64,3.03,.26), M['stone'], .07)
box('Foundation | top course', (0,0,.28), (3.52,2.93,.15), M['stone_light'], .035)
box('Walls | lime plaster', (0,0,1.25), (3.26,2.64,1.90), M['plaster'], .045)
for y in (-1.325,1.325):
    # A shallow prism instead of an open/zero-thickness triangle.
    verts = [(x,yy,z) for yy in (y-.055,y+.055) for x,z in [(-1.63,2.13),(1.63,2.13),(0,3.28)]]
    mesh('Gable | plaster', verts, [(0,2,1),(3,4,5),(0,1,4,3),(1,2,5,4),(2,0,3,5)], M['gable'])
for x in (-1.65,1.65):
    for y in (-1.34,1.34):
        box('Frame | corner post', (x,y,1.24), (.18,.18,1.91), M['wood'], .035)
        box('Frame | stone post shoe', (x,y,.39), (.24,.24,.20), M['stone'], .028)
for y in (-1.38,1.38):
    box('Frame | horizontal lintel', (0,y,2.09), (3.48,.17,.17), M['wood'], .025)
    box('Frame | lower sill', (0,y,.46), (3.43,.12,.14), M['wood'], .023)
    beam('Frame | gable king post', (0,y,2.12),(0,y,3.30),.15,.17)
    for sign in (-1,1):
        beam('Frame | gable rafter', (0,y,3.32),(sign*1.74,y,2.10),.17,.18)
        beam('Frame | gable brace', (sign*.17,y,2.15),(sign*.85,y,2.69),.11,.12)
for x in (-1.68,1.68):
    box('Frame | side sill', (x,0,.46), (.12,2.8,.14), M['wood'], .02)
    box('Frame | side top plate', (x,0,2.08), (.17,2.8,.17), M['wood'], .025)
    box('Frame | side middle post', (x,.16,1.24), (.15,.16,1.76), M['wood'], .02)

def make_roof(label, half_width, half_depth, ridge, eave, center_y, rows, columns):
    slope = math.atan2(ridge-eave, half_width)
    length = math.hypot(half_width, ridge-eave)
    for sign in (-1,1):
        down = Vector((sign*math.cos(slope),0,-math.sin(slope)))
        cross = Vector((0,1,0))
        normal = Vector((sign*math.sin(slope),0,math.cos(slope)))
        origin = Vector((0,center_y,ridge))
        # Solid dark roof substrate, never a paper-thin plane.
        panel_verts = [origin + down*u + cross*v + normal*w
                       for w in (-.10,0) for u,v in [(0,-half_depth),(length,-half_depth),(length,half_depth),(0,half_depth)]]
        mesh(label+' | timber roof bed', panel_verts,
             [(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)], M['wood_dark'])
        step = length/rows
        tile_width = half_depth*2/columns
        for row in range(rows):
            # Slightly staggered joints; clipped end tiles maintain a clean silhouette.
            offset = tile_width*.5 if row%2 else 0
            starts = [-half_depth + j*tile_width-offset for j in range(columns+1)]
            for start in starts:
                lo,hi=max(start,-half_depth),min(start+tile_width-.013,half_depth)
                if hi-lo < .07:
                    continue
                tile_len = min(step*1.14,length-row*step+.05)
                verts=[]
                for layer in range(2):
                    for end in range(2):
                        for k in range(5):
                            t=k/4
                            v=lo+(hi-lo)*t
                            bulge=math.sin(t*math.pi)*.048
                            lift=.037+bulge-(.026 if layer==0 else 0)+(.012 if end else 0)
                            verts.append(origin+down*(row*step+end*tile_len)+cross*v+normal*lift)
                faces=[]
                for layer in range(2):
                    b=layer*10
                    for k in range(4):
                        faces.append((b+k,b+k+1,b+k+6,b+k+5))
                for end in range(2):
                    b=end*5
                    for k in range(4):
                        faces.append((b+k,b+k+1,b+k+11,b+k+10))
                faces.extend([(0,5,15,10),(4,9,19,14)])
                mesh(label+' | curved tile', verts, faces, RNG.choice(TILES))
        for y in (center_y-half_depth-.015,center_y+half_depth+.015):
            beam(label+' | bargeboard', (0,y,ridge-.045),(sign*(half_width+.04),y,eave-.045),.14,.15,M['wood_light'],.018)
        beam(label+' | eave', (sign*half_width,center_y-half_depth,eave-.02),
             (sign*half_width,center_y+half_depth,eave-.02),.14,.14,M['wood'],.02)
    count=math.ceil(half_depth*2/.34)
    for j in range(count):
        ya=center_y-half_depth+j*half_depth*2/count
        yb=ya+half_depth*2/count+.02
        verts=[]
        for radius in (.11,.15):
            for y in (ya,yb):
                for k in range(7):
                    angle=math.pi*k/6
                    verts.append((math.cos(angle)*radius,y,ridge+.012+math.sin(angle)*radius))
        faces=[]
        for k in range(6):
            faces += [(k,k+1,k+8,k+7),(14+k,21+k,22+k,15+k),
                      (k,14+k,15+k,k+1),(7+k,8+k,22+k,21+k)]
        faces += [(0,7,21,14),(6,20,27,13)]
        mesh(label+' | ridge cap',verts,faces,TILES[j%len(TILES)])

make_roof('Main roof',2.00,1.65,3.34,2.02,0,7,10)

# A shaded door recess represented by dark backing, with an inset planked leaf.
box('Door | deep surround', (0,-1.362,1.11), (1.04,.10,1.63), M['wood_dark'],.04)
for i in range(5):
    box('Door | vertical oak plank',(-.36+i*.18,-1.431,1.10),(.17,.065,1.48), M['wood_light'],.014)
for x in (-.52,.52):
    box('Door | jamb',(x,-1.47,1.11),(.15,.15,1.77),M['wood'],.026)
box('Door | lintel',(0,-1.47,1.94),(1.19,.16,.17),M['wood'],.025)
for z in (.63,1.45):
    box('Door | iron strap',(0,-1.472,z),(.78,.025,.065),M['metal'],.012)
    for x in (-.30,.30):
        cylinder('Door | rivet',(x,-1.49,z),.023,.025,M['metal'],8,(0,1,0))
cylinder('Door | handle plate',(.28,-1.497,1.1),.065,.025,M['metal'],10,(0,1,0))
ico('Door | round handle',(.28,-1.536,1.1),(.045,.037,.045),M['wood_light'])

# Porch: posts, thick stone landing, and a smaller matching tiled gable.
box('Porch | landing',(0,-1.64,.23),(1.67,.73,.46),M['stone'],.06)
box('Porch | lower step',(0,-2.23,.065),(1.64,.44,.13),M['stone_light'],.035)
box('Porch | middle step',(0,-2.025,.14),(1.66,.43,.28),M['stone'],.035)
for x in (-.73,.73):
    box('Porch | post shoe',(x,-1.90,.49),(.27,.26,.15),M['stone_light'],.025)
    box('Porch | oak post',(x,-1.90,1.25),(.16,.16,1.52),M['wood'],.025)
    beam('Porch | diagonal brace',(x,-1.90,1.66),(x*.53,-1.90,2.0),.10,.12)
box('Porch | front header',(0,-1.90,1.98),(1.72,.17,.15),M['wood_light'],.024)
make_roof('Porch roof',.96,.50,2.62,2.10,-1.69,4,4)

def window(name, center_x, center_y, z, side=False, planter=False):
    before=set(house.objects)
    # Build a front-facing window at the origin, then rotate/translate as a group.
    box(name+' | recess',(0,0,0),(.77,.07,.88),M['wood_dark'],.018)
    box(name+' | blue glass',(0,-.045,0),(.60,.026,.69),M['glass'],.01)
    for x in (-.36,.36):
        box(name+' | jamb',(x,-.08,0),(.09,.12,.92),M['wood_light'],.015)
    for zz in (-.41,.41):
        box(name+' | head and sill',(0,-.08,zz),(.84,.13,.09),M['wood_light'],.014)
    box(name+' | mullion',(0,-.092,0),(.045,.04,.75),M['wood_light'],.008)
    box(name+' | transom',(0,-.092,0),(.67,.04,.045),M['wood_light'],.008)
    for sign in (-1,1):
        for i in range(3):
            box(name+' | shutter plank',(sign*(.47+i*.09),-.036,0),(.08,.07,.76),M['shutter'],.011)
        for zz in (-.25,.25):
            box(name+' | shutter batten',(sign*.56,-.083,zz),(.28,.045,.045),M['wood_light'],.008)
    if planter:
        box(name+' | flower box',(0,-.23,-.62),(.91,.33,.27),M['wood'],.025)
        box(name+' | soil',(0,-.23,-.48),(.78,.23,.035),M['soil'],.009)
        for x in (-.34,.34):
            box(name+' | planter strap',(x,-.408,-.62),(.045,.025,.26),M['metal'],.005)
        for i in range(7):
            x=-.33+i*.11
            ico(name+' | leaves',(x,-.24,-.40+RNG.uniform(-.02,.07)),(.17,.15,.17),M['leaf' if i%2 else 'leaf_light'])
            if i%2==0:
                ico(name+' | flowers',(x,-.25,-.23),(.063,.058,.053),M['flower'])
    angle=math.pi/2 if side else 0
    for obj in set(house.objects)-before:
        p=obj.location.copy()
        obj.location=(center_x+math.cos(angle)*p.x-math.sin(angle)*p.y,
                      center_y+math.sin(angle)*p.x+math.cos(angle)*p.y,z+p.z)
        obj.rotation_euler.z += angle

window('Front left window',-1.09,-1.385,1.35,planter=True)
window('Front right window',1.10,-1.385,1.35)
window('East window',1.685,-.55,1.30,side=True,planter=True)
window('East rear window',1.685,.82,1.30,side=True)
# Back elevation has a door-sized window pair so the model works from all game angles.
before=set(house.objects)
window('Rear window',0,-1.385,1.32,planter=True)
for obj in set(house.objects)-before:
    obj.location.x *= -1
    obj.location.y *= -1
    obj.rotation_euler.z += math.pi

# Stone chimney, individual masonry courses and a genuinely hollow dark top.
cx,cy=1.14,.58
box('Chimney | core',(cx,cy,2.99),(.52,.57,1.31),M['stone'],.035)
for row in range(5):
    z=2.40+row*.255
    box('Chimney | front course',(cx,cy-.295,z),(.53,.055,.21),M['stone_light'] if row%3==0 else M['stone'],.026)
    box('Chimney | side course',(cx+.272,cy,z),(.055,.58,.21),M['stone_light'] if row%3==1 else M['stone'],.026)
    if row>1:
        box('Chimney | mortar seam',(cx+(-.09 if row%2 else .11),cy-.325,z),(.024,.012,.16),M['stone_dark'],.003)
for x in (cx-.29,cx+.29):
    box('Chimney | rim',(x,cy,3.71),(.14,.73,.17),M['stone_light'],.025)
for y in (cy-.295,cy+.295):
    box('Chimney | rim',(cx,y,3.71),(.47,.14,.17),M['stone_light'],.025)
box('Chimney | soot recess',(cx,cy,3.645),(.43,.43,.025),M['stone_dark'],.004)

# Discrete props: low-poly firewood and a rain barrel along the west wall.
for row in range(2):
    for j in range(3-row):
        y=-.27+j*.23+row*.115
        z=.42+row*.20
        cylinder('Firewood | bark',(-1.92,y,z),.13,.68,M['wood'],9,(1,0,0))
        cylinder('Firewood | cut end',(-2.265,y,z),.105,.013,M['wood_light'],9,(1,0,0))

def barrel(center):
    x,y,z=center
    rings=[(0,.23),(.09,.27),(.35,.30),(.64,.26),(.69,.23)]
    verts=[(x+r*math.cos(i*math.tau/12),y+r*math.sin(i*math.tau/12),z+h) for h,r in rings for i in range(12)]
    faces=[tuple(range(11,-1,-1)),tuple(range(48,60))]
    for row in range(4):
        for i in range(12):
            faces.append((row*12+i,row*12+(i+1)%12,(row+1)*12+(i+1)%12,(row+1)*12+i))
    mesh('Rain barrel | staves',verts,faces,M['wood_light'])
    for h,r in ((.13,.282),(.54,.283)):
        verts=[(x+r*math.cos(i*math.tau/12),y+r*math.sin(i*math.tau/12),z+h+hh) for hh in (-.025,.025) for i in range(12)]
        mesh('Rain barrel | iron hoop',verts,[(i,(i+1)%12,(i+1)%12+12,i+12) for i in range(12)],M['metal'])
    cylinder('Rain barrel | lid',(x,y,z+.697),.226,.023,M['wood'],12)

barrel((-1.99,.96,.27))

# Consolidate repeated construction pieces into editable logical meshes/material slots.
def join_prefix(prefix, name):
    objects=[o for o in house.objects if o.type=='MESH' and o.name.startswith(prefix)]
    if not objects:
        return
    bpy.ops.object.select_all(action='DESELECT')
    for o in objects:
        o.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
    bpy.ops.object.join()
    objects[0].name=name

for prefix,name in [('Main roof | curved tile','Roof | main curved tiles'),('Main roof | ridge cap','Roof | main ridge caps'),
                    ('Porch roof | curved tile','Roof | porch curved tiles'),('Porch roof | ridge cap','Roof | porch ridge caps'),
                    ('Frame |','Structure | timber frame'),('Chimney |','Chimney | masonry'),('Door |','Door | oak and iron'),
                    ('Firewood |','Props | firewood'),('Rain barrel |','Props | rain barrel')]:
    join_prefix(prefix,name)
for prefix in ['Front left window','Front right window','East window','East rear window','Rear window']:
    join_prefix(prefix,prefix)

# Repeated roof pieces reuse two prototype unwraps, including on future rebuilds.
sys.path.insert(0, str(OUT))
from shared_roof_uv import apply_shared_roof_uv
uv_report = apply_shared_roof_uv(house)
(OUT/'uv-report.json').write_text(json.dumps(uv_report, indent=2), encoding='utf-8')

# Export BEFORE adding presentation geometry/lights. No texture dependencies.
bpy.ops.object.select_all(action='DESELECT')
for obj in house.objects:
    obj.select_set(True)
bpy.context.view_layer.objects.active=root
bpy.ops.export_scene.gltf(filepath=str(OUT/'HearthCottage.glb'),export_format='GLB',use_selection=True,
    export_yup=True,export_apply=True,export_animations=False,export_cameras=False,export_lights=False,
    export_extras=True,export_materials='EXPORT')

verts=[]
triangles=0
for obj in house.objects:
    if obj.type!='MESH':
        continue
    obj.data.calc_loop_triangles()
    triangles+=len(obj.data.loop_triangles)
    verts.extend(obj.matrix_world@v.co for v in obj.data.vertices)
mins=[min(v[i] for v in verts) for i in range(3)]
maxs=[max(v[i] for v in verts) for i in range(3)]
report={
    'asset':'HearthCottage','blender_version':bpy.app.version_string,'triangles':triangles,'uv':uv_report,
    'mesh_objects':sum(o.type=='MESH' for o in house.objects),
    'materials':len({m.name for o in house.objects if o.type=='MESH' for m in o.data.materials}),
    'bounds_metres':{'min':mins,'max':maxs,'size':[maxs[i]-mins[i] for i in range(3)]},
    'root_at_ground':True,'front_axis_blender':'-Y','glb_uses_gltf_y_up':True,
    'external_textures':False,'ue_import_verified':False,
    'notes':['Exterior visual only; door does not open.','UE assigns collision, navigation and interaction.',
             'Presentation floor, cameras and lights excluded from GLB.']}
(OUT/'model-report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('ASSET_REPORT '+json.dumps(report),flush=True)

# An understated orthographic studio, matching the village camera and soft daylight.
ground_mat=material('STAGE | muted meadow', '829579')
box('Stage | ground',(0,0,-.145),(200,200,.20),ground_mat,.0,stage)
world=bpy.data.worlds.new('Soft daylight')
world.use_nodes=True
world.node_tree.nodes['Background'].inputs['Color'].default_value=(.72,.79,.88,1)
world.node_tree.nodes['Background'].inputs['Strength'].default_value=.38
scene.world=world

def area(name,loc,energy,size,color):
    data=bpy.data.lights.new(name,'AREA')
    data.energy=energy
    data.shape='DISK'
    data.size=size
    data.color=color
    obj=bpy.data.objects.new(name,data)
    stage.objects.link(obj)
    obj.location=loc
    obj.rotation_euler=(Vector((0,0,1.4))-obj.location).to_track_quat('-Z','Y').to_euler()

area('Key | warm morning',(-3,-4,7),550,4.5,(1.0,.88,.73))
area('Fill | soft sky',(4,-1,5),260,5,(.78,.87,1.0))
area('Rim | afternoon',(-1,5,6),450,3.5,(1.0,.93,.78))
camera_data=bpy.data.cameras.new('Cottage portrait')
camera=bpy.data.objects.new('Camera | three quarter',camera_data)
stage.objects.link(camera)
camera_data.type='ORTHO'
camera_data.ortho_scale=7.1
camera.location=(6.5,-8.5,6.0)
target=Vector((0,-.10,1.55))
camera.rotation_euler=(target-camera.location).to_track_quat('-Z','Y').to_euler()
scene.camera=camera
scene.render.engine='CYCLES'
scene.cycles.device='CPU'
scene.cycles.samples=40
scene.cycles.use_denoising=True
scene.render.threads_mode='FIXED'
scene.render.threads=4
scene.render.resolution_x=1280
scene.render.resolution_y=1152
scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'
scene.render.film_transparent=False
scene.view_settings.view_transform='AgX'
scene.view_settings.look='AgX - Medium High Contrast'
scene.render.filepath=str(OUT/'HearthCottage_Preview.png')

# Open the file in an immediately useful, clean material-coloured modelling view.
bpy.ops.object.select_all(action='DESELECT')
for obj in stage.objects:
    obj.hide_set(True)
for screen in bpy.data.screens:
    for area_ui in screen.areas:
        if area_ui.type=='VIEW_3D':
            space=area_ui.spaces.active
            space.clip_end=500
            space.shading.type='SOLID'
            space.shading.color_type='MATERIAL'
            space.shading.light='STUDIO'
            space.shading.show_shadows=True
            space.shading.show_cavity=True
            space.shading.cavity_type='BOTH'
            space.overlay.show_floor=False
            space.overlay.show_axis_x=False
            space.overlay.show_axis_y=False
            space.region_3d.view_rotation=camera.rotation_euler.to_quaternion()
            space.region_3d.view_distance=8.5
            space.region_3d.view_location=target
            space.region_3d.view_perspective='ORTHO'
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'HearthCottage.blend'))
if RENDER:
    bpy.ops.render.render(write_still=True)
    camera.location=(-6.8,7.5,5.6)
    camera.rotation_euler=(Vector((0,0,1.5))-camera.location).to_track_quat('-Z','Y').to_euler()
    scene.render.resolution_x=1000
    scene.render.resolution_y=1000
    scene.cycles.samples=28
    scene.render.filepath=str(OUT/'HearthCottage_Rear.png')
    bpy.ops.render.render(write_still=True)
print('COTTAGE_COMPLETE',flush=True)
