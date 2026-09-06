"""Independent GLB byte-stream comparison; no Blender dependency."""
from pathlib import Path
import collections
import hashlib
import importlib.util
import json
import math
import struct
import sys
sys.dont_write_bytecode=True
OUT=Path(__file__).resolve().parent
sp=importlib.util.spec_from_file_location('roof_glb_reader',OUT.parent/'ToolKit/validate_tool_kit.py')
V=importlib.util.module_from_spec(sp);sp.loader.exec_module(V)
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def corners(path):
    raw=path.read_bytes();offset=12;chunks={}
    while offset<len(raw):
        size,kind=struct.unpack_from('<II',raw,offset);chunks[kind]=raw[offset+8:offset+8+size];offset+=size+8
    d=json.loads(chunks[0x4E4F534A]);blob=chunks[0x004E4942]
    def acc(i):
        a=d['accessors'][i];v=d['bufferViews'][a['bufferView']]
        n={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
        fmt='<'+{5121:'B',5123:'H',5125:'I',5126:'f'}[a['componentType']]*n
        start=v.get('byteOffset',0)+a.get('byteOffset',0);stride=v.get('byteStride',struct.calcsize(fmt))
        return [struct.unpack_from(fmt,blob,start+j*stride) for j in range(a['count'])]
    result=[]
    for p in d['meshes'][0]['primitives']:
        data=[acc(p['attributes'][attr]) for attr in ('POSITION','NORMAL','TEXCOORD_0')]
        ids=[i[0] for i in acc(p['indices'])]
        for j in range(0,len(ids),3):
            tri=[tuple(v for stream in data for v in stream[i]) for i in ids[j:j+3]]
            result.append(tri)
    return result
def mapped_triangles(tris):
    # Position + UV are bit-exact through this roundtrip; retain winding while
    # accepting cyclic corner order and primitive-buffer reordering.
    result={}
    for tri in tris:
        rows=min((tri[i:]+tri[:i] for i in range(3)),key=lambda rows:tuple(v for r in rows for v in r[:3]+r[6:]))
        key=tuple(v for r in rows for v in r[:3]+r[6:])
        assert key not in result,'Unexpected duplicate textured triangle'
        result[key]=rows
    return result
def validate():
    specs=json.loads((OUT/'timber-roof-specs.json').read_text());rows=[]
    assert sha(OUT/specs['material_reference_asset'])==specs['material_reference_sha256']
    base=specs['base_wood_pbr']['baseColorFactor']
    for m in specs['modules']:
        path=OUT/m['asset_glb'];src=OUT.parent/'VillageKit/modules'/(m['source_geometry_asset_id']+'.glb')
        assert sha(src)==m['source_geometry_sha256']
        a=V.inspect_asset(path);original=V.inspect_asset(src)
        d,bounds,_=V.decode(path);old,old_bounds,_=V.decode(src)
        assert len(d['nodes'])==1 and len(d['meshes'])==1
        assert not any(k in d['nodes'][0] for k in ('matrix','translation','rotation','scale'))
        assert d['nodes'][0]['name']==m['id'] and d['nodes'][0]['extras']['asset_id']==m['id']
        assert a['triangles']==original['triangles']
        assert all(math.dist(x,y)<1e-6 for x,y in zip(bounds,old_bounds))
        before,after=corners(src),corners(path)
        before,after=mapped_triangles(before),mapped_triangles(after)
        assert before.keys()==after.keys(),(m['id'],'winding/positions/UV changed')
        max_normal_delta=max(math.dist(before[k][i][3:6],after[k][i][3:6]) for k in before for i in range(3))
        # Blender packs imported split normals internally. Permit <0.00015
        # unit-vector error (under 0.009 degrees); never claim byte-exact normals.
        assert max_normal_delta<.00015,(m['id'],'normal orientation changed')
        assert not d.get('images') and not d.get('textures')
        wood=[]
        for mat in d['materials']:
            pbr=mat['pbrMetallicRoughness'];assert pbr.get('metallicFactor',1)==0
            assert .429<=pbr.get('roughnessFactor',1)<=.561
            assert 'Terracotta' not in mat['name']
            if mat['name'].startswith('VK_Timber_Shingle_'):wood.append(pbr)
        assert len(wood)==4
        assert any(math.dist(w['baseColorFactor'],base)<1e-6 for w in wood)
        a.update({'asset_id':m['id'],'source_geometry_asset_id':m['source_geometry_asset_id'],
            'size_authoring_m':[round(bounds[1][i]-bounds[0][i],6) for i in range(3)],'bounds_authoring_m':bounds,
            'triangle_winding_positions_uv0_exactly_preserved':True,
            'normal_orientation_preserved_within_unit_vector_tolerance':.00015,'max_normal_vector_delta':max_normal_delta,
            'identity_node_and_original_datum_preserved':True,'self_contained_pbr':True,
            'wood_base_matches_beam_reference':True,'original_source_hash_unchanged':True,'requires_native_import':True})
        rows.append(a)
    source=json.loads((OUT/'timber-roof-source-audit.json').read_text())
    assert source['mesh_count']==2 and len(source['objects'])==2
    assert source['source_blend_sha256']==sha(OUT/source['source_blend'])
    report={'status':'passed','scope':'Independent GLB binary checks plus winding/position/normal/UV comparison to original roof geometry',
        'assets':rows,'asset_count':2,'total_triangles':sum(a['triangles'] for a in rows),
        'source_blend_hash_and_two_mesh_audit_verified':True,'native_import_verified':False,
        'inventory_or_recipe_binding_verified':False,'original_roof_and_beam_sources_unchanged':True}
    (OUT/'timber-roof-validation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report))
if __name__=='__main__':validate()
