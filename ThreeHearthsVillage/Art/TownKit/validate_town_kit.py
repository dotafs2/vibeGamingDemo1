from pathlib import Path
import json, struct, hashlib
OUT=Path(__file__).resolve().parent
def gltf(path):
    raw=path.read_bytes(); assert raw[:4]==b'glTF'; n,kind=struct.unpack_from('<II',raw,12); assert kind==0x4E4F534A
    return json.loads(raw[20:20+n])
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
catalog=json.loads((OUT/'catalog.json').read_text()); assert len(catalog['modules'])==6
assert len(catalog['runtime_supported_module_ids'])==4 and len(catalog['blocked_module_ids'])==2
rows=[]
for m in catalog['modules']:
    p=OUT/m['asset_glb']; d=gltf(p); assert len(d['nodes'])==1 and len(d['meshes'])==1
    e=d['nodes'][0]['extras']; assert e['asset_id']==m['id'] and e['npc_portable'] is False and e['resource_inputs']==m['resource_inputs']
    assert d['asset']['version']=='2.0'; assert m['grid_m']==2; assert all(v>0 for v in m['resource_inputs'].values())
    expected = 'blocked_on_tiles_runtime_resource' if 'tiles' in m['resource_inputs'] else 'runtime_supported_existing_stock_candidate'
    assert m['runtime_support']==expected
    rows.append({'id':m['id'],'bytes':p.stat().st_size,'sha256':sha(p),'triangles':m['triangles'],'bounds':m['bounds_authoring_m'],'materials':m['material_names'],'runtime_support':m['runtime_support']})
sockets=json.loads((OUT/'attachment-sockets.json').read_text()); assert len(sockets['sockets'])==6
assert all(x['scale_note'] in ('one bay','repeated orthogonal runs') for x in sockets['assembly_examples'])
report={'status':'passed','asset_count':len(rows),'assets':rows,'runtime_supported_module_ids':catalog['runtime_supported_module_ids'],'blocked_module_ids':catalog['blocked_module_ids'],'runtime_supported_count':len(catalog['runtime_supported_module_ids']),'blocked_count':len(catalog['blocked_module_ids']),'socket_count':len(sockets['sockets']),'source_geometry_reused':False,'existing_kits_modified':False,'native_import_or_ue_runtime_verified':False,'blender_source_or_render_verified':False,'note':'GLB geometry/metadata/attachment contract verified offline. Four pieces use current stone/planks/beams stock; two roof pieces retain authored tiles and are blocked until a tiles runtime resource exists. Blender source/render and UE import remain pending.'}
(OUT/'validation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(report,indent=2))
