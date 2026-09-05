"""Reproducible original modular Cropout-inspired village kit.

Run: blender --background --python create_village_kit.py -- [--no-render]
Can also be executed inside Blender MCP using exec(compile(...)).
Only local geometry and portable glTF PBR materials; no external services.
"""
from pathlib import Path
import argparse
import json
import math
import random
import sys
import time
import bpy
import bmesh
from mathutils import Vector

OUT = Path(__file__).resolve().parent
ARGS = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
RENDER = '--no-render' not in ARGS
RNG = random.Random(260906)
START = time.monotonic()
SPECS = json.loads((OUT / 'module-specs.json').read_text(encoding='utf-8-sig'))
for folder in ('modules', 'examples', 'previews'):
    (OUT / folder).mkdir(parents=True, exist_ok=True)
# Preserve the MCP timer's window/context; loading factory settings mid-command
# invalidates Blender's operator context until the following event-loop iteration.
for old in list(bpy.data.objects):
    bpy.data.objects.remove(old, do_unlink=True)
for old in list(bpy.data.collections):
    bpy.data.collections.remove(old)
for old in list(bpy.data.materials):
    bpy.data.materials.remove(old)
scene = bpy.context.scene
scene.name = 'VillageKit_ContactSheet'
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1.0
scene.render.engine = 'CYCLES'
scene.cycles.samples = 32
scene.cycles.use_denoising = True
scene.render.resolution_percentage = 100
scene.view_settings.view_transform = 'AgX'
scene.world = bpy.data.worlds.new('Village soft sky')
scene.world.use_nodes = True
scene.world.node_tree.nodes['Background'].inputs[0].default_value = (.50, .64, .72, 1)
scene.world.node_tree.nodes['Background'].inputs[1].default_value = .45
MODULES = bpy.data.collections.new('MODULE_SOURCES | local coordinates')
scene.collection.children.link(MODULES)
PARTS = []
MESHES = {}
REPORT = {'schema_version': 1, 'generator': 'create_village_kit.py',
          'blender_version': bpy.app.version_string, 'units': 'metres',
          'front_axis': '-Y', 'modules': [], 'examples': [],
          'material_design': 'Portable constant PBR with controlled variants; no hidden Blender-only procedural shader.'}


def linear(v):
    return v / 12.92 if v <= .04045 else ((v + .055) / 1.055) ** 2.4


def mat(name, color, roughness, metallic=0):
    rgba = tuple(linear(int(color[i:i+2], 16) / 255) for i in (0, 2, 4)) + (1,)
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    m.diffuse_color = rgba
    p = m.node_tree.nodes.get('Principled BSDF')
    p.inputs['Base Color'].default_value = rgba
    p.inputs['Roughness'].default_value = roughness
    p.inputs['Metallic'].default_value = metallic
    return m


M = {
    'wood': mat('VK_Chestnut', '916547', .48),
    'wood_light': mat('VK_Cut_Oak', 'BC9368', .43),
    'wood_dark': mat('VK_Timber_Recess', '594337', .56),
    'wood_honey': mat('VK_Honey_Plank', 'B2865C', .46),
    'plaster': mat('VK_Cream_Lime', 'E9DCB9', .57),
    'plaster_shadow': mat('VK_Warm_Lime', 'D9C89F', .59),
    'stone': mat('VK_BlueGrey_Stone', '98A5AC', .52),
    'stone_light': mat('VK_Stone_Edge', 'BAC1BA', .47),
    'stone_dark': mat('VK_Stone_Shadow', '7B8990', .57),
    'iron': mat('VK_Forged_Iron', '535B5E', .34, .65),
    'brass': mat('VK_Brass', 'B99550', .30, .70),
    'sage': mat('VK_Sage_Paint', '779F97', .41),
    'glass': mat('VK_Opaque_Window', '597F89', .24),
    'rope': mat('VK_Hemp_Rope', 'C5AE7E', .62),
}
TILES = {
    'terracotta': [mat('VK_Terracotta_' + str(i + 1), c, r) for i, (c, r) in enumerate([
        ('BB695F', .34), ('CA7C6F', .38), ('AF6059', .36), ('D28B79', .40)])],
    'slateblue': [mat('VK_SlateBlue_' + str(i + 1), c, r) for i, (c, r) in enumerate([
        ('597B8F', .34), ('7094A3', .38), ('4E6D84', .36), ('81A4AC', .40)])],
}


def adopt(obj, material):
    for c in list(obj.users_collection):
        c.objects.unlink(obj)
    MODULES.objects.link(obj)
    if material:
        obj.data.materials.append(material)
    PARTS.append(obj)
    return obj


def uv_project(mesh):
    """Repeatable local planar UVs, including custom geometry; never empty UVs."""
    layer = mesh.uv_layers.new(name='UVMap') if not mesh.uv_layers else mesh.uv_layers.active
    for p in mesh.polygons:
        axis = max(range(3), key=lambda i: abs(p.normal[i]))
        axes = [i for i in range(3) if i != axis]
        for li in p.loop_indices:
            v = mesh.vertices[mesh.loops[li].vertex_index].co
            layer.data[li].uv = (v[axes[0]], v[axes[1]])


def mesh(name, verts, faces, material, smooth_faces=()):
    data = bpy.data.meshes.new(name)
    data.from_pydata(verts, [], faces)
    data.update()
    bm = bmesh.new()
    bm.from_mesh(data)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(data)
    bm.free()
    uv_project(data)
    for i in smooth_faces:
        data.polygons[i].use_smooth = True
    return adopt(bpy.data.objects.new(name, data), material)


def bevel(obj, width=.018, segments=2):
    if width <= 0:
        return obj
    bpy.ops.object.select_all(action='DESELECT')
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    mod = obj.modifiers.new('Rounded hand-cut edges', 'BEVEL')
    # Keep a safety margin below half the thinnest dimension. Letting Blender's
    # overlap clamp collapse a narrow bevel to its centre creates zero-area faces.
    mod.width = min(width, min(obj.dimensions)*.24)
    mod.segments = segments
    mod.limit_method = 'ANGLE'
    mod.angle_limit = math.radians(45)
    bpy.ops.object.modifier_apply(modifier=mod.name)
    return obj


def box(name, center, dims, material, edge=.018):
    bpy.ops.mesh.primitive_cube_add(size=1, location=center)
    obj = bpy.context.view_layer.objects.active
    obj.name = name
    obj.dimensions = dims
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    adopt(obj, material)
    bevel(obj, edge)
    return obj


def beam(name, a, b, width=.12, depth=.12, material=None):
    a, b = Vector(a), Vector(b)
    obj = box(name, (a+b)/2, (width, depth, (b-a).length), material or M['wood'])
    obj.rotation_euler = (b-a).to_track_quat('Z', 'Y').to_euler()
    return obj


def cylinder(name, center, radius, depth, material, vertices=12, direction=None):
    bpy.ops.mesh.primitive_cylinder_add(vertices=vertices, radius=radius, depth=depth, location=center)
    obj = adopt(bpy.context.view_layer.objects.active, material)
    obj.name = name
    if direction:
        obj.rotation_euler = Vector(direction).to_track_quat('Z', 'Y').to_euler()
    bevel(obj, .008)
    return obj


def arch_tile(name, origin, down, cross, normal, length, width, material, bulge=.055):
    """Shared tile UVs; smooth ONLY the curved surfaces, keep end/side edges hard."""
    origin, down, cross, normal = map(Vector, (origin, down, cross, normal))
    n = 8
    verts = []
    for layer in range(2):
        for end in range(2):
            for k in range(n+1):
                t = k/n
                h = math.sin(t*math.pi)*bulge + layer*.027 + (end*.012)
                verts.append(origin + down*(end*length) + cross*((t-.5)*width) + normal*h)
    stride = n+1
    faces = []
    smooth = []
    for layer in range(2):
        a = layer*2*stride
        for k in range(n):
            smooth.append(len(faces))
            faces.append((a+k, a+k+1, a+stride+k+1, a+stride+k))
    for end in range(2):
        a = end*stride
        for k in range(n):
            faces.append((a+k, a+k+1, a+2*stride+k+1, a+2*stride+k))
    faces += [(0, stride, stride*3, stride*2), (n, stride+n, stride*3+n, stride*2+n)]
    obj = mesh(name, verts, faces, material, smooth)
    # Identical local tile UV parameterization reused independent of placement.
    uv = obj.data.uv_layers.active
    for p in obj.data.polygons:
        vertex_ids=[obj.data.loops[li].vertex_index for li in p.loop_indices]
        same_end=len({(vi//stride)%2 for vi in vertex_ids})==1
        same_cross=len({vi%stride for vi in vertex_ids})==1
        for li in p.loop_indices:
            vi = obj.data.loops[li].vertex_index
            uv.data[li].uv = (((vi//stride)%2) if same_cross else (vi%stride)/n,
                              (vi//(2*stride)) if same_end or same_cross else ((vi//stride)%2))
    return obj


def plank_panel(x0, x1, z0, z1, y=0, family='plaster'):
    if x1 <= x0 or z1 <= z0:
        return
    if family == 'plaster':
        box('Warm plaster infill', ((x0+x1)/2,y,(z0+z1)/2), (x1-x0,.16,z1-z0), M['plaster'], .024)
    elif family == 'timber':
        count = max(1, round((x1-x0)/.23))
        for j in range(count):
            w = (x1-x0)/count
            box('Vertical oak plank', (x0+(j+.5)*w,y,(z0+z1)/2), (w-.008,.16,z1-z0),
                M['wood_light'] if j % 3 == 0 else M['wood_honey'], .010)
    else:
        rows = max(1, round((z1-z0)/.31))
        for row in range(rows):
            h = (z1-z0)/rows
            w = .45
            shift = .225*(row%2)
            left = x0-shift
            while left < x1:
                lo, hi = max(x0, left), min(x1, left+w)
                if hi-lo > .025:
                    box('Soft stone block', ((lo+hi)/2,y,z0+(row+.5)*h), (hi-lo-.012,.16,h-.012),
                        [M['stone'], M['stone_light'], M['stone_dark']][(row+round(left*9))%3], .025)
                left += w


def wall(family, opening=None):
    if not opening:
        plank_panel(-.91,.91,.16,2.24,family=family)
    else:
        half = .47 if opening == 'door' else .45
        bottom, top = (.16,2.06) if opening == 'door' else (.96,1.86)
        plank_panel(-.91,-half,.16,2.24,family=family)
        plank_panel(half,.91,.16,2.24,family=family)
        plank_panel(-half,half,top,2.24,family=family)
        if bottom > .16:
            plank_panel(-half,half,.16,bottom,family=family)
        for x in (-half-.035,half+.035):
            box('Opening jamb', (x,-.018,(top+bottom)/2), (.07,.21,top-bottom+.07), M['wood_light'])
        box('Opening lintel', (0,-.018,top+.035), (half*2+.14,.22,.09), M['wood_light'])
        if opening == 'window':
            box('Deep window sill', (0,-.10,bottom-.045), (1.10,.34,.12), M['stone_light'])
            box('Opaque painted glass', (0,.035,(bottom+top)/2), (.83,.035,.83), M['glass'], .01)
            box('Window vertical muntin', (0,-.025,(bottom+top)/2), (.045,.08,.84), M['wood_light'], .007)
            box('Window crossbar', (0,-.025,(bottom+top)/2), (.84,.08,.045), M['wood_light'], .007)


def foundation():
    box('Stone core', (0,0,-.13), (1.98,1.98,.22), M['stone_dark'], .02)
    for side in (-1,1):
        for k in range(4):
            x = -.75+k*.5
            box('Dressed plinth front', (x,side*.89,-.115), (.487,.22,.23), M['stone_light'] if k%2 else M['stone'], .026)
            box('Dressed plinth side', (side*.89,x,-.115), (.22,.487,.23), M['stone_light'] if k%2 else M['stone'], .026)


def floor(opening=False):
    if opening:
        for x in (-.95,.95):
            box('Stair opening rim', (x,0,.08), (.10,2,.16), M['wood'], .012)
    else:
        for j in range(8):
            box('Floor plank', (-.875+j*.25,0,.095), (.242,2,.13), M['wood_light'] if j%3 else M['wood_honey'], .009)
        for y in (-.78,.78):
            box('Underfloor joist', (0,y,.025), (2,.16,.05), M['wood'], .008)


def frame():
    for x in (-1,1):
        for y in (-1,1):
            box('Square frame post', (x,y,1.2), (.18,.18,2.4), M['wood'], .025)
            box('Peg detail', (x,y-.095,2.18), (.042,.014,.042), M['wood_light'], .004)
    for side in (-1,1):
        box('Frame top plate', (0,side,2.30), (1.82,.18,.20), M['wood'], .024)
        box('Frame cross plate', (side,0,2.30), (.18,1.82,.20), M['wood'], .024)
    # Braces stay in edge planes and preserve a comfortable central walk-through.
    for x in (-1,1):
        beam('Corner knee brace', (x,-1,1.85), (x,-.58,2.30), .10,.10)


def gable(family):
    verts = [(x,y,z) for y in (-.08,.08) for x,z in ((-2,0),(2,0),(0,1.2))]
    obj = mesh('Solid gable infill', verts, [(0,2,1),(3,4,5),(0,1,4,3),(1,2,5,4),(2,0,3,5)],
               M['plaster'] if family == 'plaster' else (M['stone'] if family == 'stone' else M['wood_honey']))
    bevel(obj,.018)
    beam('Gable king post',(0,-.10,0),(0,-.10,1.18),.13,.12)
    for sign in (-1,1):
        beam('Gable bargeboard',(sign*2,-.11,0),(0,-.11,1.2),.13,.13, M['wood_light'])
        beam('Gable brace',(sign*.28,-.10,.05),(sign*.92,-.10,.61),.09,.10)
    if family == 'timber':
        for j in range(-7,8):
            if j:
                x = j*.24
                h = 1.2*(1-abs(x)/2)
                box('Gable board joint',(x,-.083,h/2),(.013,.012,h-.025),M['wood_dark'],0)
    if family == 'stone':
        for row in range(3):
            z=.27+row*.28
            half=2*(1-z/1.2)
            box('Gable stone course',(0,-.083,z),(half*2-.10,.012,.014),M['stone_dark'],0)


def roof(family):
    width, rise, depth = 2.18, 1.2, 2.0
    slope = math.atan2(rise,width)
    down = Vector((math.cos(slope),0,-math.sin(slope)))
    cross = Vector((0,1,0))
    normal = Vector((math.sin(slope),0,math.cos(slope)))
    origin = Vector((0,0,rise))
    length = math.hypot(width,rise)
    verts = [origin+down*u+cross*v+normal*w for w in (-.09,-.005)
             for u,v in ((0,-1),(length,-1),(length,1),(0,1))]
    mesh('Solid timber roof bed',verts,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],M['wood_dark'])
    rows, columns = 7, 7
    step, tw = length/rows, depth/columns
    for row in range(rows):
        for col in range(columns):
            tile_origin = origin + down*(row*step) + cross*(-1+(col+.5)*tw) + normal*(.014+row*.001)
            arch_tile('Smooth ceramic tile',tile_origin,down,cross,normal,
                      min(step*1.1,length-row*step),tw-.012,TILES[family][(row*3+col)%4])
    beam('Rounded eave board',(width,-1,-.06),(width,1,-.06),.14,.14,M['wood_light'])


def ridge(family):
    for j in range(6):
        arch_tile('Smooth half-round ridge cap',(0,-1+(j/6)*2,1.22),(0,1,0),(1,0,0),(0,0,1),
                  min(.36,2-j/6*2),.30,TILES[family][j%4],.15)


def steps():
    for j in range(3):
        box('Rounded stone step',(0,-.30+j*.30,(j+1)*.40/6),(2,.30,(j+1)*.40/3),M['stone_light'],.028)


def railing():
    for x in (-.93,0,.93):
        box('Railing post',(x,0,.45),(.12,.12,.90),M['wood'],.022)
        box('Post cap',(x,0,.92),(.17,.17,.06),M['wood_light'],.014)
    for z in (.28,.79):
        box('Railing rail',(0,0,z),(2,.10,.11),M['wood_light'])
    for x in (-.62,-.31,.31,.62):
        box('Railing spindle',(x,0,.54),(.065,.065,.46),M['wood'],.012)


def canopy(family='terracotta'):
    # Front is -Y; wall connection at Y=0, eave projects towards -Y.
    down = Vector((0,-1.25,-.45)).normalized()
    cross = Vector((1,0,0))
    normal = cross.cross(down).normalized()*-1
    length = math.hypot(1.25,.45)
    verts = [Vector((0,0,.50))+down*u+cross*v+normal*w for w in (-.075,0)
             for u,v in ((0,-1.1),(length,-1.1),(length,1.1),(0,1.1))]
    mesh('Porch roof bed',verts,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],M['wood_dark'])
    for row in range(4):
        for col in range(7):
            arch_tile('Canopy ceramic',Vector((0,0,.51))+down*(row*length/4)+cross*(-1.1+(col+.5)*2.2/7),
                      down,cross,normal,min(length/4*1.08,length-row*length/4),2.2/7-.013,TILES[family][(row+col)%4])
    beam('Porch front fascia',(-1.1,-1.25,.01),(1.1,-1.25,.01),.12,.13,M['wood_light'])


def switchback():
    # Two 6-riser flights in a two-bay well. Generous turning platforms, no
    # transverse floor trimmer/beam through the ascent or the headroom envelope.
    run = .24
    front, rear = -1.02, .42
    for j in range(6):
        y = front+(j+.5)*run
        top = .16+(j+1)*.2
        box('Lower flight tread',(-.48,y,top-.045),(.84,run+.016,.09),M['wood_light'],.012)
        box('Lower flight riser',(-.48,y-run/2+.015,top-.145),(.84,.03,.20),M['wood_honey'],.006)
        top2 = 1.36+(j+1)*.2
        y2 = rear-(j+.5)*run
        box('Upper flight tread',(.48,y2,top2-.045),(.84,run+.016,.09),M['wood_light'],.012)
        box('Upper flight riser',(.48,y2+run/2-.015,top2-.145),(.84,.03,.20),M['wood_honey'],.006)
    box('Turn landing',(0,rear+.45,1.315),(1.80,.90,.09),M['wood_light'],.012)
    box('Upper exit landing',(.48,front-.45,2.515),(.84,.90,.09),M['wood_light'],.012)
    for x in (-.87,-.09):
        beam('Lower stringer',(x,front,.19),(x,rear,1.34),.075,.13,M['wood'])
        beam('Lower handrail',(x,front,.97),(x,rear,2.12),.065,.065,M['wood_light'])
        for y,z in ((front+.05,.35),(rear-.06,1.40)):
            box('Lower baluster',(x,y,z+.32),(.055,.055,.70),M['wood'],.009)
    for x in (.09,.87):
        beam('Upper stringer',(x,rear,1.39),(x,front,2.54),.075,.13,M['wood'])
        beam('Upper handrail',(x,rear,2.17),(x,front,3.32),.065,.065,M['wood_light'])
        for y,z in ((rear-.06,1.55),(front+.05,2.58)):
            box('Upper baluster',(x,y,z+.32),(.055,.055,.70),M['wood'],.009)
    beam('Rear landing rail',(-.90,rear+.90,2.15),(.90,rear+.90,2.15),.065,.065,M['wood_light'])
    for x in (-.90,.90):
        box('Landing rail post',(x,rear+.90,1.75),(.07,.07,.80),M['wood'],.012)


def carry_logs():
    for j,(y,z) in enumerate(((-.10,.105),(.10,.105),(0,.275))):
        cylinder('Chestnut log', (0,y,z),.102,.75,M['wood'],12,(1,0,0))
        for x in (-.379,.379):
            cylinder('End grain',(x,y,z),.088,.012,M['wood_light'],12,(1,0,0))
            cylinder('Heartwood',(x*1.002,y,z),.044,.014,M['wood_honey'],10,(1,0,0))
    for x in (-.23,.23):
        box('Bundle tie',(x,0,.18),(.035,.40,.03),M['rope'],.008)
        for y in (-.18,.18):
            box('Bundle tie side',(x,y,.18),(.035,.03,.27),M['rope'],.008)
        box('Bundle tie top',(x,0,.32),(.035,.36,.03),M['rope'],.008)


def carry_planks():
    for level in range(3):
        for row in (-1,1):
            box('Portable plank',(0,row*.085,.04+level*.07),(.78,.16,.06),M['wood_light'] if level%2 else M['wood_honey'],.01)
    for x in (-.25,.25):
        box('Plank bundle strap',(x,0,.238),(.035,.36,.026),M['rope'],.007)
        for y in (-.173,.173):
            box('Plank bundle strap',(x,y,.125),(.035,.025,.24),M['rope'],.007)


def crate(width=.58, depth=.40, height=.30):
    box('Crate floor',(0,0,.04),(width,depth,.08),M['wood'],.012)
    for x in (-width/2+.03,width/2-.03):
        for y in (-depth/2+.025,depth/2-.025):
            box('Crate corner',(x,y,height/2),(.055,.055,height),M['wood_light'],.009)
    for z in (.105,.215):
        for y in (-depth/2+.022,depth/2-.022):
            box('Crate side',(0,y,z),(width,.044,.082),M['wood_honey'],.01)
        for x in (-width/2+.022,width/2-.022):
            box('Crate end',(x,0,z),(.044,depth,.082),M['wood_honey'],.01)


def carry_stones():
    crate(.6,.42,.30)
    for i in range(6):
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1,radius=1,location=((i%3-1)*.16,(-.075 if i<3 else .075),.23+(i%2)*.045))
        obj=adopt(bpy.context.view_layer.objects.active,[M['stone'],M['stone_light'],M['stone_dark']][i%3])
        obj.name='Rounded carry stone'
        obj.scale=(.13,.11,.115)
        bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
        bevel(obj,.014)
        uv_project(obj.data)


def carry_tiles():
    crate()
    for row in range(4):
        arch_tile('Stacked portable tile',(0,-.13+row*.078,.27),(1,0,0),(0,1,0),(0,0,1),.40,.17,
                  TILES['terracotta'][row%4],.045)
    # Recenter ceramic stack within crate.
    for obj in PARTS[-4:]:
        obj.location.x -= .20


def carry_plaster():
    cylinder('Bucket staves',(0,0,.205),.16,.34,M['wood_light'],14)
    for z in (.07,.33):
        cylinder('Iron hoop',(0,0,z),.165,.035,M['iron'],14)
    cylinder('Wet lime surface',(0,0,.38),.145,.026,M['plaster'],24)
    # Simple rigid bail, unmistakable at game scale.
    for x in (-.15,.15):
        beam('Bucket handle',(x,0,.28),(x,0,.51),.027,.027,M['iron'])
    beam('Bucket grip',(-.15,0,.51),(.15,0,.51),.032,.032,M['wood_dark'])


def table():
    for y in (-.30,-.10,.10,.30):
        box('Tabletop plank',(0,y,.755),(2,.192,.09),M['wood_light'],.023)
    for x in (-.65,.65):
        for sign in (-1,1):
            beam('Splayed table leg',(x,sign*.32,.04),(x,sign*.20,.71),.13,.13,M['wood'])
        box('Trestle',(x,0,.63),(.16,.85,.14),M['wood'],.02)
    box('Table stretcher',(0,0,.31),(1.45,.10,.12),M['wood_honey'])


def bench():
    for y in (-.10,.10):
        box('Bench seat',(0,y,.425),(1.8,.193,.07),M['wood_light'],.022)
    for x in (-.64,.64):
        box('Bench leg',(x,0,.20),(.15,.32,.40),M['wood'],.022)
    box('Bench stretcher',(0,0,.16),(1.4,.09,.10),M['wood_honey'])


def workbench():
    for y in (-.24,0,.24):
        box('Workbench top',(0,y,.80),(1.6,.232,.12),M['wood_light'],.025)
    for x in (-.64,.64):
        for y in (-.24,.24):
            box('Workbench leg',(x,y,.37),(.15,.15,.74),M['wood'],.023)
    box('Workbench shelf',(0,0,.22),(1.4,.54,.08),M['wood_honey'])
    box('Vise jaw',(.52,-.41,.74),(.34,.12,.27),M['wood_dark'])
    cylinder('Vise screw',(.52,-.50,.70),.035,.20,M['iron'],12,(0,1,0))
    beam('Vise handle',(.52,-.60,.60),(.52,-.60,.86),.025,.025,M['wood_light'])
    box('Cutting board',(-.30,.05,.875),(.62,.26,.045),M['wood_honey'],.013)
    cylinder('Mallet head',(.20,.12,.92),.06,.20,M['wood_dark'],10,(1,0,0))
    beam('Mallet shaft',(.20,-.17,.89),(.20,.12,.92),.033,.033,M['wood_light'])


def door():
    for j in range(4):
        box('Door oak board',(-.3225+j*.215,0,.915),(.208,.085,1.83),M['wood_light'] if j%2 else M['wood_honey'],.012)
    for z in (.34,1.49):
        box('Door forged strap',(0,-.055,z),(.76,.028,.055),M['iron'],.009)
        for x in (-.32,.32):
            cylinder('Door rivet',(x,-.075,z),.019,.018,M['brass'],10,(0,1,0))
    cylinder('Door handle',(.27,-.085,.88),.045,.05,M['brass'],12,(0,1,0))


def shutter():
    for x in (-.1425,-.0475,.0475,.1425):
        box('Sage shutter board',(x,0,.40),(.088,.055,.80),M['sage'],.011)
    for z in (.16,.64):
        box('Shutter rail',(0,-.04,z),(.35,.035,.07),M['wood_light'],.009)
    for z in (.12,.68):
        cylinder('Shutter hinge',(-.18,0,z),.021,.07,M['iron'],10)


BUILDERS = {
    'foundation_stone_2m': foundation, 'floor_timber_2m': floor, 'frame_timber_2m': frame,
    'post_timber_2_4m': lambda: box('Frame post',(0,0,1.2),(.18,.18,2.4),M['wood'],.025),
    'beam_timber_2m': lambda: box('Frame beam',(0,0,.10),(1.82,.18,.20),M['wood'],.024),
    'wall_plaster_2m': lambda: wall('plaster'), 'wall_timber_2m': lambda: wall('timber'),
    'wall_stone_2m': lambda: wall('stone'),
    'wall_door_plaster_2m': lambda: wall('plaster','door'),
    'wall_door_timber_2m': lambda: wall('timber','door'),
    'wall_window_plaster_2m': lambda: wall('plaster','window'),
    'wall_window_timber_2m': lambda: wall('timber','window'),
    'wall_door_stone_2m': lambda: wall('stone','door'),
    'wall_window_stone_2m': lambda: wall('stone','window'),
    'gable_plaster_4m': lambda: gable('plaster'), 'gable_timber_4m': lambda: gable('timber'),
    'gable_stone_4m': lambda: gable('stone'),
    'roof_slope_terracotta_2m': lambda: roof('terracotta'),
    'roof_slope_slateblue_2m': lambda: roof('slateblue'),
    'roof_ridge_terracotta_2m': lambda: ridge('terracotta'),
    'roof_ridge_slateblue_2m': lambda: ridge('slateblue'),
    'porch_post_timber': lambda: (box('Porch post',(0,0,1.2),(.18,.18,2.4),M['wood'],.026),
        box('Stone post shoe',(0,0,.10),(.28,.28,.20),M['stone_light'],.03),
        box('Post capital',(0,0,2.32),(.26,.26,.16),M['wood_light'],.024)),
    'porch_steps_stone_2m': steps, 'railing_timber_2m': railing, 'canopy_terracotta_2m': canopy,
    'canopy_slateblue_2m': lambda: canopy('slateblue'),
    'floor_opening_2m': lambda: floor(True), 'stairs_switchback_2x4m': switchback,
    'carry_logs': carry_logs, 'carry_planks': carry_planks, 'carry_stones': carry_stones,
    'carry_tiles': carry_tiles, 'carry_plaster': carry_plaster,
    'table_communal': table, 'bench_timber': bench, 'workbench_carpenter': workbench,
    'door_oak': door, 'shutter_sage': shutter,
}


def mesh_stats(obj):
    data = obj.data
    data.calc_loop_triangles()
    coords = [obj.matrix_world @ Vector(v) for v in obj.bound_box]
    low = [round(min(v[i] for v in coords),5) for i in range(3)]
    high = [round(max(v[i] for v in coords),5) for i in range(3)]
    return {'triangles': len(data.loop_triangles), 'vertices': len(data.vertices),
            'bounds_min_m': low, 'bounds_max_m': high,
            'dimensions_m': [round(high[i]-low[i],5) for i in range(3)],
            'materials': [m.name for m in data.materials if m],
            'uv_layers': len(data.uv_layers),
            'smooth_polygons': sum(p.use_smooth for p in data.polygons)}


def select_only(objects):
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:
        obj.select_set(True)
    if objects:
        bpy.context.view_layer.objects.active = objects[0]


def export(path, objects):
    select_only(objects)
    bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',use_selection=True,
                              export_yup=True,export_apply=True,export_texcoords=True,
                              export_normals=True,export_materials='EXPORT',export_extras=True,
                              export_animations=False,export_cameras=False,export_lights=False)


for spec in SPECS['modules']:
    asset_id = spec['id']
    PARTS.clear()
    BUILDERS[asset_id]()
    select_only(PARTS)
    if len(PARTS)>1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    obj.name = asset_id
    # Apply world transforms before resetting origin: all exported roots are identity.
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    scene.cursor.location = (0,0,0)
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    obj['asset_id'] = asset_id
    obj['category'] = spec['category']
    obj['front_axis'] = '-Y'
    obj['npc_portable'] = spec['category'] == 'carry'
    obj['nominal_size_m'] = spec['nominal_size_m']
    path = OUT / 'modules' / (asset_id + '.glb')
    export(path,[obj])
    MESHES[asset_id] = obj
    REPORT['modules'].append({'id':asset_id,'file':str(path.relative_to(OUT)).replace('\\','/'),
                              'bytes':path.stat().st_size,**mesh_stats(obj)})
    print('VILLAGE_KIT_MODULE',asset_id,REPORT['modules'][-1]['triangles'],flush=True)


EXAMPLE_COLLECTIONS = {}
EXAMPLE_ASSEMBLIES = {}


def instance(collection, asset_id, at=(0,0,0), angle=0):
    source = MESHES[asset_id]
    obj = bpy.data.objects.new(asset_id,source.data)
    collection.objects.link(obj)
    obj.location = at
    obj.rotation_euler.z = math.radians(angle)
    obj['module_id'] = asset_id
    return obj


def make_house(example):
    collection = bpy.data.collections.new('EXAMPLE | ' + example['id'])
    scene.collection.children.link(collection)
    EXAMPLE_COLLECTIONS[example['id']] = collection
    width, depth = example['footprint_m']
    wall_family, roof_family = example['wall'], example['roof']
    placements = []
    occupied_posts, occupied_plates = set(), set()
    def add(asset_id, at, angle=0):
        obj = instance(collection,asset_id,at,angle)
        placements.append({'instance_id':example['id']+'_'+str(len(placements)+1).zfill(3),
                           'module_id':asset_id,'position_m':[round(v,6) for v in at],
                           'yaw_degrees':angle,'level':max(0,int((at[2]+.001)//2.4))})
        return obj
    ys = [(-depth/2)+1+i*2 for i in range(int(depth/2))]
    for x in (-1,1):
        for y in ys:
            add('foundation_stone_2m',(x,y,0))
    for level in range(example['storeys']):
        z = level*2.4
        for x in (-1,1):
            for y in ys:
                stair_hole = level==1 and x==-1
                add('floor_opening_2m' if stair_hole else 'floor_timber_2m',(x,y,z))
        # One post per grid vertex and one beam per edge: no coincident shared frames.
        edge_ys=[-depth/2+i*2 for i in range(int(depth/2)+1)]
        for x in (-2,0,2):
            for y in edge_ys:
                add('post_timber_2_4m',(x,y,z))
        for x in (-1,1):
            for y in edge_ys:
                if example['storeys']==2 and level==0 and x==-1 and y==0:
                    continue
                add('beam_timber_2m',(x,y,z+2.2))
        for x in (-2,0,2):
            for y in ys:
                add('beam_timber_2m',(x,y,z+2.2),90)
        for side in (-1,1):
            for i,y in enumerate(ys):
                kind = 'wall_window_' if i%2==0 else 'wall_'
                add(kind+wall_family+'_2m',(side*2,y,z),side*90)
            for x in (-1,1):
                front_door = side==-1 and x==-1 and level==0
                kind = 'wall_door_' if front_door else 'wall_window_'
                add(kind+wall_family+'_2m',(x,side*depth/2,z),0 if side==-1 else 180)
                if front_door:
                    # Open 95 degrees around its left hinge to show a genuinely open doorway.
                    a=math.radians(-95)
                    hinge=Vector((x-.43,-depth/2-.04,z+.16))
                    center=hinge+Vector((math.cos(a)*.43,math.sin(a)*.43,0))
                    add('door_oak',tuple(center),-95)
                else:
                    for sign in (-1,1):
                        add('shutter_sage',(x+sign*.70,side*(depth/2+.13),z+1.01),0 if side==-1 else 180)
    top = example['storeys']*2.4
    for y in ys:
        add('roof_slope_'+roof_family+'_2m',(0,y,top))
        add('roof_slope_'+roof_family+'_2m',(0,y,top),180)
        add('roof_ridge_'+roof_family+'_2m',(0,y,top))
    for sign in (-1,1):
        add('gable_'+wall_family+'_4m',(0,sign*depth/2,top),0 if sign<0 else 180)
    if example['storeys']==2:
        add('stairs_switchback_2x4m',(-1,0,0))
        add('canopy_terracotta_2m',(-1,-depth/2,2.10))
        for x in (-1.95,-.05):
            add('porch_post_timber',(x,-depth/2-1.15,-.25))
    else:
        add('table_communal',(0,0,.16))
        add('bench_timber',(0,-.8,.16))
    # A broad approach landing; stairs top is at finished first-floor height.
    add('porch_steps_stone_2m',(-1,-depth/2-.48,-.24))
    path = OUT/'examples'/(example['id']+'.glb')
    export(path,list(collection.objects))
    EXAMPLE_ASSEMBLIES[example['id']] = placements
    REPORT['examples'].append({'id':example['id'],'file':str(path.relative_to(OUT)).replace('\\','/'),
                                'bytes':path.stat().st_size,'module_instances':len(placements),
                                'has_stairs':example['storeys']==2,
                                'status':example.get('status','visual_assembly_candidate')})
    return collection


for example in SPECS['examples']:
    make_house(example)
(OUT/'example-assemblies.json').write_text(json.dumps(EXAMPLE_ASSEMBLIES,indent=2)+'\n',encoding='utf-8')
(OUT/'example-layouts.json').write_text(json.dumps({'schema_version':1,'units':'metres','up_axis':'+Z',
    'examples':[{'id':key,'placements':value} for key,value in EXAMPLE_ASSEMBLIES.items()]},indent=2)+'\n',encoding='utf-8')


def area(name, at, energy, size, color, target=(0,0,0)):
    data=bpy.data.lights.new(name,'AREA')
    data.energy=energy
    data.shape='DISK'
    data.size=size
    data.color=color
    obj=bpy.data.objects.new(name,data)
    scene.collection.objects.link(obj)
    obj.location=at
    obj.rotation_euler=(Vector(target)-obj.location).to_track_quat('-Z','Y').to_euler()
    return obj


def camera(at,target,scale):
    data=bpy.data.cameras.new('Presentation camera')
    data.type='ORTHO'
    data.ortho_scale=scale
    obj=bpy.data.objects.new('Presentation camera',data)
    scene.collection.objects.link(obj)
    obj.location=at
    obj.rotation_euler=(Vector(target)-obj.location).to_track_quat('-Z','Y').to_euler()
    scene.camera=obj
    return obj


def text_label(body,at,size=.18):
    data=bpy.data.curves.new('Caption','FONT')
    data.body=body
    data.size=size
    data.align_x='CENTER'
    data.extrude=0
    obj=bpy.data.objects.new('Caption | '+body,data)
    PRESENT.objects.link(obj)
    obj.location=at
    return obj


PRESENT=bpy.data.collections.new('PRESENTATION | never exported')
scene.collection.children.link(PRESENT)
ground=mat('Presentation_Sage','9CAC91',.65)
caption=mat('Presentation_Ink','263C36',.6)


def stage_box(name,at,dims,material):
    PARTS.clear()
    obj=box(name,at,dims,material,.04)
    MODULES.objects.unlink(obj)
    PRESENT.objects.link(obj)
    return obj


MODULES.hide_render=True
MODULES.hide_viewport=True
for collection in EXAMPLE_COLLECTIONS.values():
    collection.hide_render=True
    collection.hide_viewport=True

# Contact sheet: category rows, legible forms, lower row devoted to carrying packs.
contact_instances=[]
for i,spec in enumerate(SPECS['modules']):
    col,row=i%8,i//8
    x,y=(col-3.5)*4.5,(2.0-row)*4.3
    obj=instance(PRESENT,spec['id'],(x,y,.28))
    contact_instances.append(obj)
    stage_box('Display plinth',(x,y,.02),(3.9,3.65,.16),ground)
    label=text_label(spec['id'].replace('_2m','').replace('_4m','').replace('_',' '),(x,y-1.66,.11),.18)
    label.data.materials.append(caption)
cam=camera((8,-44,60),(0,0,.7),46)
lights=[area('Large warm key',(-10,-16,25),4500,15,(1,.86,.68)),
        area('Cool soft fill',(15,8,20),2700,13,(.72,.88,1))]
scene.render.resolution_x=2400
scene.render.resolution_y=1500
if RENDER:
    scene.render.filepath=str(OUT/'previews'/'VillageKit_Modules.png')
    bpy.ops.render.render(write_still=True)

# Save master with all modules + recipes + material library, contact sheet active.
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'VillageKit.blend'))

# Three complete houses, same light/camera exposure; nothing added to export.
for obj in list(PRESENT.objects):
    obj.hide_render=True
    obj.hide_viewport=True
example_offsets=[(-5.8,0,0),(0,1.0,0),(5.8,0,0)]
for example,offset in zip(SPECS['examples'],example_offsets):
    collection=EXAMPLE_COLLECTIONS[example['id']]
    collection.hide_render=False
    collection.hide_viewport=False
    for obj in collection.objects:
        obj.location += Vector(offset)
    stage_box('House display terrain',(offset[0],offset[1],-.38),(5.2,example['footprint_m'][1]+2.2,.22),ground)
cam.location=(17,-25,20)
cam.rotation_euler=(Vector((0,0,1.7))-cam.location).to_track_quat('-Z','Y').to_euler()
cam.data.ortho_scale=22
scene.render.resolution_x=2000
scene.render.resolution_y=1200
if RENDER:
    scene.render.filepath=str(OUT/'previews'/'VillageKit_HouseChoices.png')
    bpy.ops.render.render(write_still=True)

# Save an assembly source with chosen examples laid out for review.
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'VillageKit_Examples.blend'))

# Isolated roof closeup: direct evidence of smooth ceramic highlights.
for collection in EXAMPLE_COLLECTIONS.values():
    collection.hide_render=True
for obj in PRESENT.objects:
    obj.hide_render=True
for index,family in enumerate(('terracotta','slateblue')):
    instance(PRESENT,'roof_slope_'+family+'_2m',(-2.6+index*3.0,0,0))
    instance(PRESENT,'roof_ridge_'+family+'_2m',(-2.6+index*3.0,0,0))
stage_box('Roof comparison ground',(0,0,-.22),(7,3.2,.20),ground)
cam.location=(7,-9,8)
cam.rotation_euler=(Vector((0,0,.5))-cam.location).to_track_quat('-Z','Y').to_euler()
cam.data.ortho_scale=8.2
lights[0].data.energy=1900
lights[0].data.size=5
lights[0].location=(-3,-5,8)
lights[1].data.energy=1100
lights[1].data.size=4
lights[1].location=(5,4,7)
scene.render.resolution_x=1600
scene.render.resolution_y=1000
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'VillageKit_RoofStudy.blend'))
if RENDER:
    scene.render.filepath=str(OUT/'previews'/'VillageKit_RoofMaterials.png')
    bpy.ops.render.render(write_still=True)
REPORT['materials']=[{'name':m.name,'roughness':round(m.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value,4),
                     'metallic':round(m.node_tree.nodes['Principled BSDF'].inputs['Metallic'].default_value,4)}
                     for m in list(M.values())+TILES['terracotta']+TILES['slateblue']]
REPORT['total_module_triangles']=sum(m['triangles'] for m in REPORT['modules'])
REPORT['elapsed_seconds']=round(time.monotonic()-START,3)
REPORT['rendered']=RENDER
REPORT['notes']=['UVs exist on every module; curved tile surfaces smooth, tile ends remain hard.',
                 'Frame module is a standalone prototype; examples use unique post and beam modules at each grid vertex/edge.',
                 'Switchback stairs and floor opening are geometric candidates. No claim of UE collision/navigation acceptance.',
                 'Portable materials are hand-sized packs. Walls/floors/frames are on-site construction outputs.']
(OUT/'model-report.json').write_text(json.dumps(REPORT,indent=2)+'\n',encoding='utf-8')
print('VILLAGE_KIT_COMPLETE',len(REPORT['modules']),REPORT['total_module_triangles'],REPORT['elapsed_seconds'],flush=True)
