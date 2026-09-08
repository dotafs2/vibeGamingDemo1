"""Render the actual catalog GLBs in an isolated Blender process."""
import argparse
import json
import math
import pathlib
import sys
import time

import bpy
from mathutils import Vector

HERE = pathlib.Path(__file__).resolve().parent
PROJECT = HERE.parents[1]
REPO = PROJECT.parent
args = argparse.ArgumentParser()
args.add_argument('--limit', type=int, default=0)
args.add_argument('--engine', default='BLENDER_EEVEE')
options = args.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
inventory = json.loads((PROJECT / 'Docs/Art_Asset_Inventory.json').read_text(encoding='utf-8'))
models = [r for r in inventory['records'] if r['kind'] in ('module', 'assembly', 'whole_model_version')]
if options.limit:
    models = models[:options.limit]
out = HERE / 'thumbnails'
out.mkdir(parents=True, exist_ok=True)
report_path = HERE / 'render-report.json'
report = json.loads(report_path.read_text()) if report_path.exists() else {}


def area(name, position, center, energy, size):
    data = bpy.data.lights.new(name, 'AREA')
    data.energy = energy
    data.shape = 'DISK'
    data.size = size
    obj = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(obj)
    obj.location = position
    obj.rotation_euler = (center - obj.location).to_track_quat('-Z', 'Y').to_euler()


for i, item in enumerate(models):
    key = item['group'] + '__' + item['id']
    target = out / (key + '.png')
    if target.exists() and report.get(key, {}).get('source_sha256') == item['sha256']:
        print('ATLAS_CACHED', i + 1, key, flush=True)
        continue
    started = time.monotonic()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.engine = options.engine
    scene.render.resolution_x = 512
    scene.render.resolution_y = 400
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGBA'
    scene.render.film_transparent = True
    scene.render.threads_mode = 'FIXED'
    scene.render.threads = 4
    if options.engine == 'CYCLES':
        scene.cycles.samples = 16
        scene.cycles.use_denoising = True
    if hasattr(scene, 'eevee') and hasattr(scene.eevee, 'taa_render_samples'):
        scene.eevee.taa_render_samples = 24
    scene.view_settings.view_transform = 'AgX'
    scene.view_settings.exposure = 0
    world = bpy.data.worlds.new('AtlasWorld')
    scene.world = world
    world.use_nodes = True
    world.node_tree.nodes['Background'].inputs['Color'].default_value = (.72, .77, .82, 1)
    world.node_tree.nodes['Background'].inputs['Strength'].default_value = .7
    bpy.ops.import_scene.gltf(filepath=str(REPO / item['path']))
    meshes = [obj for obj in scene.objects if obj.type == 'MESH']
    corners = [obj.matrix_world @ Vector(c) for obj in meshes for c in obj.bound_box]
    if not corners:
        raise RuntimeError('No mesh in ' + item['path'])
    lo = Vector(tuple(min(c[axis] for c in corners) for axis in range(3)))
    hi = Vector(tuple(max(c[axis] for c in corners) for axis in range(3)))
    center = (lo + hi) * .5
    span = max((hi - lo).length, .1)
    cam_data = bpy.data.cameras.new('AtlasCamera')
    cam = bpy.data.objects.new('AtlasCamera', cam_data)
    scene.collection.objects.link(cam)
    cam.location = center + Vector((6, -9, 7)).normalized() * span * 3
    cam.rotation_euler = (center - cam.location).to_track_quat('-Z', 'Y').to_euler()
    cam_data.type = 'ORTHO'
    cam_data.clip_start = .001
    cam_data.clip_end = span * 20 + 100
    inverse = cam.rotation_euler.to_quaternion().inverted()
    projected = [inverse @ (c - center) for c in corners]
    width = max(c.x for c in projected) - min(c.x for c in projected)
    height = max(c.y for c in projected) - min(c.y for c in projected)
    # Blender's orthographic scale is horizontal for a landscape camera.
    cam_data.ortho_scale = max(width, height * 512 / 400) * 1.20
    scene.camera = cam
    area('Key', center + Vector((-3, -4, 6)) * span, center, 700 * span * span, 4 * span)
    area('Fill', center + Vector((4, -1, 3)) * span, center, 350 * span * span, 5 * span)
    area('Rim', center + Vector((1, 4, 5)) * span, center, 500 * span * span, 3 * span)
    scene.render.filepath = str(target)
    bpy.ops.render.render(write_still=True)
    report[key] = {'source': item['path'], 'source_sha256': item['sha256'],
                   'output': target.relative_to(HERE).as_posix(), 'engine': options.engine,
                   'mesh_objects': len(meshes), 'size_m': list(hi - lo),
                   'seconds': round(time.monotonic() - started, 3)}
    report_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print('ATLAS_RENDERED', i + 1, len(models), key, report[key]['seconds'], flush=True)
print('ATLAS_THUMBNAILS_COMPLETE', len(report), flush=True)
