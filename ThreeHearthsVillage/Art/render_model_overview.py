"""Render deterministic catalog thumbnails from actual inventoried GLB files.

Run with Blender --background --factory-startup --threads 4 --python this_file
and optionally append -- --limit 4 for a rendering smoke check.
"""
from pathlib import Path
import argparse
import hashlib
import json
import math
import sys
import time
import bpy
from mathutils import Vector

PROJECT = Path(__file__).resolve().parents[1]
REPO = PROJECT.parent
OUTPUT = REPO / '.codex-ue58-diagnostics/model-overview-20260907'
THUMBS = OUTPUT / 'thumbs'
THUMBS.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser()
parser.add_argument('--limit', type=int, default=0)
args = parser.parse_args(sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else [])
inventory = json.loads((PROJECT/'Docs/Art_Asset_Inventory.json').read_text(encoding='utf-8'))
entries = [r for r in inventory['records'] if r['kind'] in ('module', 'assembly', 'whole_model_version')]
assert len(entries) == 138
assert len({r['path'] for r in entries}) == 138
# Large complete compositions first; module images then use the same studio.
entries.sort(key=lambda r: (0 if r['kind'] != 'module' else 1, r['group'], r['id']))
if args.limit:
    entries = entries[:args.limit]

report = []
started = time.monotonic()
for index, item in enumerate(entries):
    source = REPO/item['path']
    assert source.resolve().is_relative_to((PROJECT/'Art').resolve())
    assert hashlib.sha256(source.read_bytes()).hexdigest() == item['sha256'], item['path']
    key = item['group']+'__'+item['id']
    target = THUMBS/(key+'.png')
    metadata = THUMBS/(key+'.json')
    if target.exists() and metadata.exists():
        cached = json.loads(metadata.read_text(encoding='utf-8'))
        if cached['source_sha256'] == item['sha256'] and cached.get('render_version') == 1:
            report.append(cached)
            continue
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.render.resolution_x = 720 if item['kind'] != 'module' else 480
    scene.render.resolution_y = 480 if item['kind'] != 'module' else 320
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGBA'
    scene.render.film_transparent = True
    scene.display.render_aa = '32'
    shading = scene.display.shading
    shading.light = 'STUDIO'
    shading.studio_light = 'paint.sl'
    shading.color_type = 'MATERIAL'
    shading.show_shadows = True
    shading.show_cavity = True
    shading.cavity_type = 'BOTH'
    shading.curvature_ridge_factor = 1.2
    shading.curvature_valley_factor = 0.7
    shading.cavity_ridge_factor = 1.0
    shading.cavity_valley_factor = 0.7
    shading.show_specular_highlight = True
    shading.show_object_outline = False
    scene.view_settings.view_transform = 'Standard'
    bpy.ops.import_scene.gltf(filepath=str(source))
    meshes = [o for o in scene.objects if o.type == 'MESH']
    assert meshes, item['path']
    points = [o.matrix_world @ Vector(corner) for o in meshes for corner in o.bound_box]
    minimum = Vector(tuple(min(p[k] for p in points) for k in range(3)))
    maximum = Vector(tuple(max(p[k] for p in points) for k in range(3)))
    center = (minimum+maximum)*0.5
    span = max(maximum-minimum)
    assert span > 0.00001, item['path']
    # Front is Blender -Y; view from the front-right, high enough to see flat parts.
    direction = Vector((6.4, -9.2, 6.6)).normalized()
    camera_data = bpy.data.cameras.new('Catalog orthographic')
    camera = bpy.data.objects.new('Catalog camera', camera_data)
    scene.collection.objects.link(camera)
    camera.location = center+direction*span*5
    camera.rotation_euler = (center-camera.location).to_track_quat('-Z', 'Y').to_euler()
    camera_data.type = 'ORTHO'
    camera_data.clip_start = max(span*0.0001, 0.000001)
    camera_data.clip_end = span*20
    camera_data.sensor_fit = 'HORIZONTAL'
    rotation = camera.rotation_euler.to_matrix().transposed()
    projected = [rotation @ (p-center) for p in points]
    width = max(p.x for p in projected)-min(p.x for p in projected)
    height = max(p.y for p in projected)-min(p.y for p in projected)
    aspect = scene.render.resolution_x/scene.render.resolution_y
    camera_data.ortho_scale = max(width, height*aspect)*1.16
    scene.camera = camera
    scene.render.filepath = str(target)
    bpy.ops.render.render(write_still=True)
    result = {'source': item['path'], 'source_sha256': item['sha256'], 'thumbnail': str(target),
              'id': item['id'], 'group': item['group'], 'name_zh': item['name_zh'],
              'dimensions_m': list(maximum-minimum), 'mesh_objects': len(meshes),
              'render_version': 1, 'renderer': 'Blender Workbench material-color studio',
              'resolution': [scene.render.resolution_x, scene.render.resolution_y]}
    metadata.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    report.append(result)
    print('MODEL_OVERVIEW '+json.dumps({'done': index+1, 'total': len(entries), 'id': item['id'],
                                      'seconds': round(time.monotonic()-started, 1)}), flush=True)
(OUTPUT/'render-report.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
print('MODEL_OVERVIEW_COMPLETE '+str(len(report)), flush=True)
