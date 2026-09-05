"""Independently compare saved Blender UV0 with GLB TEXCOORD_0 triangle data."""
import bpy
from collections import Counter
import json
import math
import struct
from pathlib import Path

out=Path(__file__).resolve().parent
stem='HearthCottage_SharedUV'
bpy.ops.wm.open_mainfile(filepath=str(out/(stem+'.blend')))
if bpy.context.object and bpy.context.object.mode!='OBJECT':
    bpy.ops.object.mode_set(mode='OBJECT')

def tri_key(points):
    return tuple(sorted((round(p[0],5),round(p[1],5)) for p in points))

source={}
for obj in bpy.data.collections['HOUSE | Hearth Cottage'].objects:
    if obj.type!='MESH' or not obj.name.startswith('Roof |'): continue
    assert len(obj.data.uv_layers)==1 and obj.data.uv_layers.active.name=='UV_Material'
    uv=obj.data.uv_layers.active.data
    obj.data.calc_loop_triangles()
    source[obj.name]=Counter(tri_key([uv[i].uv for i in tri.loops]) for tri in obj.data.loop_triangles)
raw=(out/(stem+'.glb')).read_bytes()
json_size=struct.unpack_from('<I',raw,12)[0]
gltf=json.loads(raw[20:20+json_size])
bin_header=20+json_size
bin_size,bin_type=struct.unpack_from('<II',raw,bin_header)
assert bin_type==0x004E4942
binary=raw[bin_header+8:bin_header+8+bin_size]

def accessor(index):
    a=gltf['accessors'][index]
    v=gltf['bufferViews'][a['bufferView']]
    assert not a.get('sparse')
    components={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
    kind={5121:'B',5123:'H',5125:'I',5126:'f'}[a['componentType']]
    fmt='<'+kind*components
    stride=v.get('byteStride',struct.calcsize(fmt))
    offset=v.get('byteOffset',0)+a.get('byteOffset',0)
    return [struct.unpack_from(fmt,binary,offset+i*stride) for i in range(a['count'])]

result={}
for node in gltf['nodes']:
    name=node.get('name')
    if name not in source: continue
    actual=Counter()
    for primitive in gltf['meshes'][node['mesh']]['primitives']:
        assert primitive.get('mode',4)==4
        assert 'TEXCOORD_0' in primitive['attributes']
        assert 'TEXCOORD_1' not in primitive['attributes']
        # glTF's V axis is the inverse of Blender's UV V axis.
        uv=[(u,1-v) for u,v in accessor(primitive['attributes']['TEXCOORD_0'])]
        indices=[i[0] for i in accessor(primitive['indices'])]
        for i in range(0,len(indices),3):
            actual[tri_key([uv[j] for j in indices[i:i+3]])]+=1
    assert actual==source[name], f'UV0 differs after GLB export: {name}'
    assert max(actual.values())>1, f'No shared texture coordinates: {name}'
    result[name]={'uv_triangles':sum(actual.values()),'distinct_uv_triangles':len(actual),
                  'max_identical_uv_triangle_copies':max(actual.values()),'matches_saved_blend':True}
assert len(result)==4
report={'status':'passed','saved_blend_matches_glb_texture_uv0':True,
        'intentional_overlap_preserved':True,'roof_meshes':result}
(out/(stem+'_uv-validation.json')).write_text(json.dumps(report,indent=2),encoding='utf-8')
print('SHARED_UV_VALIDATION '+json.dumps(report),flush=True)
