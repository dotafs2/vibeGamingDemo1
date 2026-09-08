"""Root-authored freight components; one cargo mesh represents one real unit.

blender -b -t 6 --python-exit-code 1 --python create_freight_kit.py
"""
import hashlib
import json
import math
import sys
from pathlib import Path
import bpy
from mathutils import Vector

BASE = Path(__file__).resolve().parent
OUT = BASE / 'FreightKit'
OUT.mkdir(exist_ok=True)
sys.path.insert(0, str(BASE))
from meshkit import Geo, COLORS

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
coll = bpy.data.collections.new('Authored freight modules')
scene.collection.children.link(coll)
specs = []

def module(mid, layers, notes):
    root = bpy.data.objects.new(mid, None)
    coll.objects.link(root)
    root['module_id'] = mid
    for layer, geo in layers.items():
        geo.object(mid + '__' + layer, coll, root, layer, bevel=.005 if layer == 'finish' else .008)
    specs.append((root, notes))

# A single sawn plank. The slightly chipped end, two narrow grain lines and
# exposed end grain describe timber without distorting the stacking surfaces.
g, f = Geo(), Geo()
g.box((0, 0, .035), (.205, 2.08, .07), 'oak_light')
f.box((0, -1.041, .035), (.195, .004, .060), 'endgrain')
f.box((0, 1.041, .035), (.195, .004, .060), 'endgrain')
for x, begin, end in [(-.051, -.91, -.36), (.037, .13, .96)]:
    f.beam((x, begin, .071), (x + .009, end, .071), .0025, .002, 'oak')
module('cargo_planks', {'structure': g, 'finish': f}, 'Exactly one sawn plank per instance. Bottom origin; stack without scaling.')

g, f = Geo(), Geo()
g.box((0, 0, .095), (.215, 2.20, .19), 'oak')
f.box((0, -1.101, .095), (.204, .004, .179), 'endgrain')
f.box((0, 1.101, .095), (.204, .004, .179), 'endgrain')
for x in [-.063, .029]:
    f.beam((x, -1.104, .04), (x * .84, -1.104, .15), .004, .002, 'oak_dark')
for y in [-.70, .52]:
    f.box((.108, y, .11), (.003, .16, .026), 'oak_light')
module('cargo_beams', {'structure': g, 'finish': f}, 'Exactly one squared beam per instance. Bottom origin; stack without scaling.')

# Traces join the shaft grips to the actual authored horse harness. The
# existing master cart shafts remain structural; these are separate leather.
g, f = Geo(), Geo()
for side in [-1, 1]:
    points = [(side * .52, -3.78, .80), (side * .48, -3.70, 1.08), (side * .44, -3.53, 1.38)]
    for a, b in zip(points, points[1:]):
        g.beam(a, b, .050, .016, 'leather')
    f.ring((side * .48, -3.70, 1.08), .041, .027, .025, 'iron', 12)
    f.tube((side * .52 - .035, -3.78, .80), (side * .52 + .035, -3.78, .80), .024, role='iron', n=10)
module('cart_traces', {'structure': g, 'attachments': f}, 'Cart axle ground origin; links shafts to horse located at (0,-2.90,0).')

# Empty public loading rack, with joinery, diagonal bracing and a painted
# timber emblem. Its geometry never implies that inventory is stocked.
g, f, a = Geo(), Geo(), Geo()
for x in [-1.24, 1.24]:
    for y in [-.47, .47]:
        g.box((x, y, .045), (.36, .34, .09), 'stone_dark')
        g.beam((x, y, .09), (x * .92, y * .88, 1.60), .135, .135, 'oak_dark')
        f.box((x * .985, y * .975, .29), (.15, .15, .14), 'iron')
    for z in [.24, 1.48]:
        g.beam((x, -.59, z), (x, .59, z), .13, .16, 'oak')
for y in [-.43, .43]:
    g.beam((-1.30, y, .32), (1.30, y, .32), .17, .16, 'oak')
    g.beam((-1.20, y, 1.52), (1.20, y, 1.52), .11, .14, 'oak_light')
g.beam((-1.1, .49, .42), (.98, .49, 1.42), .077, .075, 'oak_dark')
g.box((0, -.525, 1.39), (.55, .055, .34), 'oak_light')
for x in [-.17, 0, .17]:
    f.box((x, -.557, 1.39), (.08, .012, .22), 'blue')
for x in [-1.20, 1.20]:
    for y in [-.505, .505]:
        for z in [.35, 1.50]:
            a.tube((x, y, z), (x, y + (-.018 if y < 0 else .018), z), .021, role='iron', n=8)
module('freight_depot_rack', {'structure': g, 'finish': f, 'attachments': a}, 'Empty modular loading rack. Stored cargo is a separate inventory-driven layer.')

manifest = {'schema_version': 1, 'units': 'meters', 'front': '-Y', 'up': '+Z', 'materials': COLORS, 'modules': []}
for root, notes in specs:
    bpy.ops.object.select_all(action='DESELECT')
    members = [root] + list(root.children_recursive)
    for ob in members: ob.select_set(True)
    bpy.context.view_layer.objects.active = root
    fbx = OUT / (root.name + '.fbx')
    bpy.ops.export_scene.fbx(filepath=str(fbx), use_selection=True, object_types={'MESH', 'EMPTY'},
        apply_unit_scale=True, apply_scale_options='FBX_SCALE_UNITS', axis_forward='-Y', axis_up='Z',
        bake_anim=False, use_mesh_modifiers=True, mesh_smooth_type='FACE', add_leaf_bones=False)
    bpy.ops.export_scene.gltf(filepath=str(OUT / (root.name + '.glb')), export_format='GLB', use_selection=True,
        export_apply=True, export_extras=True, export_cameras=False, export_lights=False)
    bpy.context.view_layer.update()
    dg = bpy.context.evaluated_depsgraph_get()
    points, tris, layers = [], 0, []
    for ob in members:
        if ob.type != 'MESH': continue
        assert ob.data.uv_layers and len(ob.data.materials)
        ev = ob.evaluated_get(dg); mesh = ev.to_mesh(); mesh.calc_loop_triangles()
        points.extend(ob.matrix_world @ v.co for v in mesh.vertices)
        tris += len(mesh.loop_triangles); ev.to_mesh_clear()
        layers.append({'layer': ob['layer'], 'object_name': ob.name})
    manifest['modules'].append({'id': root.name, 'fbx': fbx.name, 'sha256': hashlib.sha256(fbx.read_bytes()).hexdigest(),
        'glb': root.name + '.glb', 'layers': layers, 'triangles': tris, 'notes': notes,
        'bounds_m': {'min': [min(v[i] for v in points) for i in range(3)], 'max': [max(v[i] for v in points) for i in range(3)]}})
(OUT / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding='utf-8')

# Show the new joinery and per-unit cargo in a clean source-art preview.
for (root, _), pos in zip(specs, [(-1.65, -1.30, .01), (-1.15, -1.30, .01), (3.8, 2.8, 0), (0, 2.15, 0)]):
    root.location = pos
stage = bpy.data.collections.new('Preview only'); scene.collection.children.link(stage)
g = Geo(); g.box((0, 0, -.11), (200, 200, .20), 'earth'); g.object('Ground', stage, bevel=0)
for mid, pos in [('cart_body', (3.8, 2.8, 0)), ('cart_wheel', (2.76, 2.8, .638)), ('cart_wheel', (4.84, 2.8, .638))]:
    bpy.ops.object.select_all(action='DESELECT')
    bpy.ops.import_scene.gltf(filepath=str(BASE / 'Modules' / (mid + '.glb')))
    roots = [o for o in bpy.context.selected_objects if o.parent is None]
    for ob in roots: ob.location += Vector(pos)
world = bpy.data.worlds.new('Soft daylight'); world.use_nodes=True
world.node_tree.nodes['Background'].inputs['Color'].default_value=(.73,.81,.92,1)
world.node_tree.nodes['Background'].inputs['Strength'].default_value=.5;scene.world=world
for name, p, power, color in [('Key',(-5,-8,12),1900,(1,.9,.77)),('Fill',(9,1,10),1600,(.79,.89,1))]:
    data=bpy.data.lights.new(name,'AREA');data.energy=power;data.shape='DISK';data.size=8;data.color=color
    ob=bpy.data.objects.new(name,data);stage.objects.link(ob);ob.location=p
    ob.rotation_euler=(Vector((1,1,.8))-ob.location).to_track_quat('-Z','Y').to_euler()
data=bpy.data.cameras.new('Review camera');data.type='ORTHO';data.ortho_scale=10
cam=bpy.data.objects.new('Review camera',data);stage.objects.link(cam);scene.camera=cam
cam.location=(11,-14,11);cam.rotation_euler=(Vector((1.5,.8,.7))-cam.location).to_track_quat('-Z','Y').to_euler()
scene.render.engine='CYCLES';scene.cycles.samples=24;scene.cycles.use_denoising=True
scene.render.threads_mode='FIXED';scene.render.threads=6
scene.render.resolution_x=1600;scene.render.resolution_y=1100;scene.render.resolution_percentage=100
scene.view_settings.view_transform='AgX';scene.view_settings.look='AgX - Medium High Contrast'
scene.render.image_settings.file_format='PNG';scene.render.filepath=str(OUT/'FreightKit_Source_Preview.png')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Medieval_Freight_Kit.blend'))
bpy.ops.render.render(write_still=True)
print('FREIGHT_KIT_COMPLETE', len(specs), sum(len(m['layers']) for m in manifest['modules']), flush=True)
