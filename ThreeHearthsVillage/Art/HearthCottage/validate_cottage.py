"""Reimport the deliverable GLB in an empty Blender scene and verify geometry/materials."""
import bpy
import json
import math
import struct
import sys
from pathlib import Path

out = Path(__file__).resolve().parent
args=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
stem=args[0] if args else 'HearthCottage'
report_file='model-report.json' if stem=='HearthCottage' else stem+'_model-report.json'
expected = json.loads((out/report_file).read_text(encoding='utf-8'))
raw = (out/(stem+'.glb')).read_bytes()
magic, version, length = struct.unpack_from('<4sII', raw)
assert (magic, version, length) == (b'glTF', 2, len(raw))
json_length, chunk_type = struct.unpack_from('<II', raw, 12)
assert chunk_type == 0x4E4F534A
gltf = json.loads(raw[20:20+json_length])
assert not gltf.get('images') and not gltf.get('textures')
assert not gltf.get('cameras')
assert all('uri' not in buffer for buffer in gltf['buffers'])
assert not any('Stage |' in n.get('name','') or 'Camera |' in n.get('name','') for n in gltf['nodes'])
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(out/(stem+'.glb')))
bpy.context.view_layer.update()
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
points = [o.matrix_world@v.co for o in meshes for v in o.data.vertices]
assert all(math.isfinite(v) for p in points for v in p)
mins = [min(v[i] for v in points) for i in range(3)]
maxs = [max(v[i] for v in points) for i in range(3)]
for i in range(3):
    assert abs(mins[i]-expected['bounds_metres']['min'][i]) < 0.0001
    assert abs(maxs[i]-expected['bounds_metres']['max'][i]) < 0.0001
triangle_count = 0
degenerate_count = 0
for obj in meshes:
    obj.data.calc_loop_triangles()
    triangle_count += len(obj.data.loop_triangles)
    for tri in obj.data.loop_triangles:
        a,b,c = [obj.data.vertices[i].co for i in tri.vertices]
        degenerate_count += (b-a).cross(c-a).length < 1e-10
assert triangle_count == expected['triangles']
assert degenerate_count == 0
assert len(meshes) == expected['mesh_objects']
materials = {m.name for o in meshes for m in o.data.materials}
assert len(materials) == expected['materials']
result = {'status':'passed','glb_bytes':len(raw),'reimported_triangles':triangle_count,
          'reimported_mesh_objects':len(meshes),'reimported_materials':len(materials),
          'degenerate_triangles':degenerate_count,'bounding_box_matches_metres':True,
          'no_external_dependencies':True,'presentation_excluded':True,
          'ue_import_verified':False}
validation_file='validation.json' if stem=='HearthCottage' else stem+'_validation.json'
(out/validation_file).write_text(json.dumps(result,indent=2),encoding='utf-8')
print('VALIDATION '+json.dumps(result),flush=True)
