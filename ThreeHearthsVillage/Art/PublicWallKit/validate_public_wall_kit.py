"""Independent exported GLB bytes, assembly support and resource-contract audit."""
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
sp=importlib.util.spec_from_file_location('public_wall_glb_reader',OUT.parent/'ToolKit/validate_tool_kit.py')
V=importlib.util.module_from_spec(sp);sp.loader.exec_module(V)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def textured_triangles(path):
    raw=path.read_bytes();offset=12;chunks={}
    while offset<len(raw):
        size,kind=struct.unpack_from('<II',raw,offset);chunks[kind]=raw[offset+8:offset+8+size];offset+=size+8
    d=json.loads(chunks[0x4E4F534A]);blob=chunks[0x004E4942]
    def acc(i):
        a=d['accessors'][i];v=d['bufferViews'][a['bufferView']];n={'SCALAR':1,'VEC2':2,'VEC3':3}[a['type']]
        fmt='<'+{5123:'H',5125:'I',5126:'f'}[a['componentType']]*n;start=v.get('byteOffset',0)+a.get('byteOffset',0)
        return [struct.unpack_from(fmt,blob,start+j*v.get('byteStride',struct.calcsize(fmt))) for j in range(a['count'])]
    result=collections.Counter()
    for p in d['meshes'][0]['primitives']:
        positions=acc(p['attributes']['POSITION']);uv=acc(p['attributes']['TEXCOORD_0']);ids=[i[0] for i in acc(p['indices'])]
        for j in range(0,len(ids),3):
            tri=[positions[i]+uv[i] for i in ids[j:j+3]]
            result[min(tuple(tri[i:]+tri[:i]) for i in range(3))]+=1
    return result
def validate():
    catalog=json.loads((OUT/'catalog.json').read_text());specs={m['id']:m for m in catalog['modules']}
    assert set(specs)=={'public_wall_foundation_2m','public_wall_stone_2m','public_wall_walkway_2m','public_wall_parapet_2m'}
    reuse=json.loads((OUT/'reuse-manifest.json').read_text());palette={}
    for ref in reuse['references']:
        path=OUT/ref['asset_glb'];assert sha(path)==ref['sha256']
        d,_,_=V.decode(path)
        for m in d['materials']:palette[m['name']]=m['pbrMetallicRoughness']
    rows=[]
    for mid,m in specs.items():
        path=OUT/m['asset_glb'];a=V.inspect_asset(path);d,bounds,tris=V.decode(path)
        assert a['triangles']==m['triangles'] and a['triangles']<=3000
        assert len(d['nodes'])==1 and len(d['meshes'])==1
        assert not any(k in d['nodes'][0] for k in ('matrix','translation','rotation','scale'))
        assert d['nodes'][0]['extras']['asset_id']==mid
        assert not d['nodes'][0]['extras']['npc_portable'] and not m['npc_handheld']
        assert dict(d['nodes'][0]['extras']['resource_inputs'])==m['resource_inputs']
        assert set(m['resource_inputs'])<=set(catalog['resources'])
        assert all(isinstance(v,int) and v>0 for v in m['resource_inputs'].values())
        assert all(abs(bounds[0][i]+bounds[1][i])<2e-6 for i in (0,1))
        assert abs(bounds[0][2]-(.003 if m['role']=='stone_wall' else 0))<2e-6
        assert math.dist([bounds[1][i]-bounds[0][i] for i in range(3)],m['measured_size_m'])<2e-6
        assert not d.get('textures') and not d.get('images')
        for mat in d['materials']:
            pbr=mat['pbrMetallicRoughness'];reference=palette[mat['name'].split('.00')[0]]
            assert math.dist(pbr['baseColorFactor'],reference['baseColorFactor'])<1e-6
            assert abs(pbr['roughnessFactor']-reference['roughnessFactor'])<1e-6 and pbr.get('metallicFactor',1)==0
        for shape in m['collision_design']:
            assert shape['type']=='box' and all(x>0 for x in shape['size_m'])
        if m['source_geometry_reused_unchanged']:
            assert textured_triangles(path)==textured_triangles(OUT/m['geometry_source'])
        a.update({'asset_id':mid,'size_authoring_m':m['measured_size_m'],'bounds_authoring_m':bounds,
           'single_identity_mesh_node':True,'original_palette_pbr_verified':True,'self_contained_pbr':True,
           'resource_inputs':m['resource_inputs'],'reused_wall_positions_winding_uv_exact':m['source_geometry_reused_unchanged']})
        rows.append(a)
    plan=json.loads((OUT/'assembly.json').read_text());placements={p['component_id']:p for p in plan['placements']}
    assert len(placements)==15 and plan['component_count']==15 and plan['unique_component_assets']==4
    world_bounds={cid:[[m['bounds_authoring_m'][side][i]+p['position_authoring_m'][i] for i in range(3)] for side in (0,1)]
        for cid,p in placements.items() for m in [specs[p['component_asset_id']]]}
    support=[]
    for cid,p in placements.items():
        assert p['scale']==[1,1,1] and p['yaw_degrees']==0
        for dep in p['requires_installed_component_ids']:
            assert placements[dep]['installation_stage']<p['installation_stage']
            a,b=world_bounds[dep],world_bounds[cid];gap=b[0][2]-a[1][2]
            assert -.00001<=gap<=.00301,(cid,'support gap',gap)
            assert all(min(a[1][i],b[1][i])>max(a[0][i],b[0][i]) for i in (0,1))
            support.append({'component_id':cid,'supported_by':dep,'vertical_gap_m':gap})
    totals={k:sum(specs[p['component_asset_id']]['resource_inputs'].get(k,0) for p in placements.values()) for k in catalog['resources']}
    recipes=json.loads((OUT/'recipes.json').read_text())
    assert totals==plan['resource_inputs_total']==recipes['example_6m_segment_inputs']=={'stone':36,'planks':21,'beams':12}
    for recipe in recipes['recipes']:assert recipe['resource_inputs']==specs[recipe['output_component_asset_id']]['resource_inputs']
    source=json.loads((OUT/'source-audit.json').read_text());assert source['mesh_count']==4 and len(source['objects'])==4
    assert source['source_blend_sha256']==sha(OUT/source['source_blend'])
    report={'status':'passed','asset_count':4,'source_mesh_count':4,'total_unique_triangles':sum(a['triangles'] for a in rows),
       'max_asset_triangles':max(a['triangles'] for a in rows),'assets':rows,'assembly_component_count':15,
       'assembly_triangles':sum(specs[p['component_asset_id']]['triangles'] for p in placements.values()),
       'support_contacts':support,'resource_inputs_total':totals,'existing_sources_unchanged':True,
       'whole_wall_mesh_created':False,'native_import_or_ue_runtime_verified':False,'collision_and_navigation_suggestions_only':True,
       'previews_reviewed':False}
    if (OUT/'visual-review.json').exists():
        review=json.loads((OUT/'visual-review.json').read_text());assert review['source_blend_sha256']==sha(OUT/'PublicWallKit.blend')
        assert review['assembly_sha256']==sha(OUT/'assembly.json')
        for img in review['images']:assert img['sha256']==sha(OUT/img['file'])
        report['previews_reviewed']=review['components_and_assembled_segment_reviewed']
    (OUT/'validation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    artifacts=[p for p in OUT.rglob('*') if p.is_file() and p.suffix in ('.py','.json','.md','.blend','.glb','.png') and p.name!='artifact-manifest.json']
    (OUT/'artifact-manifest.json').write_text(json.dumps({'kit_id':'public_wall_kit_01','artifacts':[{'file':p.relative_to(OUT).as_posix(),
       'bytes':p.stat().st_size,'sha256':sha(p)} for p in sorted(artifacts)]},indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k not in ('assets','support_contacts')}))
if __name__=='__main__':validate()
