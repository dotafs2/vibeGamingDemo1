"""Validate the exported GLBs and all supported offline NPC house choices."""
from collections import Counter
import hashlib
import itertools
import json
import math
from pathlib import Path
import struct

from village_kit_catalog import KIT, compile_design

def inspect_asset(path, *, require_smooth=False):
    raw=path.read_bytes()
    assert 20<=len(raw)<=16*1024*1024, (path.name,'size')
    assert struct.unpack_from('<4sII',raw)==(b'glTF',2,len(raw)),path.name
    offset=12
    chunks={}
    while offset<len(raw):
        length,kind=struct.unpack_from('<II',raw,offset)
        assert length%4==0 and offset+8+length<=len(raw)
        assert kind not in chunks
        chunks[kind]=raw[offset+8:offset+8+length]
        offset+=8+length
    document=json.loads(chunks[0x4E4F534A])
    data=chunks[0x004E4942]
    assert not document.get('cameras') and not document.get('animations')
    assert not document.get('skins') and len(document['meshes'])==1
    assert all('uri' not in item for item in document.get('buffers',[])+document.get('images',[]))
    assert len(document['buffers'])==1 and document['buffers'][0]['byteLength']<=len(data)
    assert 1<=len(document['materials'])<=32
    def accessor(index):
        a=document['accessors'][index]
        assert 'sparse' not in a
        view=document['bufferViews'][a['bufferView']]
        width={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
        code={5120:'b',5121:'B',5122:'h',5123:'H',5125:'I',5126:'f'}[a['componentType']]
        fmt='<'+code*width
        size=struct.calcsize(fmt)
        stride=view.get('byteStride',size)
        start=view.get('byteOffset',0)+a.get('byteOffset',0)
        assert stride>=size and start+(a['count']-1)*stride+size<=view.get('byteOffset',0)+view['byteLength']
        values=[struct.unpack_from(fmt,data,start+j*stride) for j in range(a['count'])]
        if a.get('normalized'):
            divisor={5120:127,5121:255,5122:32767,5123:65535}[a['componentType']]
            values=[tuple(max(-1,x/divisor) for x in v) for v in values]
        assert all(math.isfinite(x) for v in values for x in v)
        return values
    triangles=smooth=0
    for primitive in document['meshes'][0]['primitives']:
        assert primitive.get('mode',4)==4
        assert 0<=primitive['material']<len(document['materials'])
        attrs=primitive['attributes']
        assert {'POSITION','NORMAL','TEXCOORD_0'}<=set(attrs)
        vertices,normals,uv=[accessor(attrs[name]) for name in ('POSITION','NORMAL','TEXCOORD_0')]
        assert len(vertices)==len(normals)==len(uv)
        assert all(abs(sum(x*x for x in n)-1)<.002 for n in normals),(path.name,'unit normals')
        indices=[v[0] for v in accessor(primitive['indices'])]
        assert len(indices)%3==0 and all(0<=v<len(vertices) for v in indices)
        for start in range(0,len(indices),3):
            ids=indices[start:start+3]
            a,b,c=[vertices[i] for i in ids]
            ab=[b[i]-a[i] for i in range(3)]; ac=[c[i]-a[i] for i in range(3)]
            cross=[ab[1]*ac[2]-ab[2]*ac[1],ab[2]*ac[0]-ab[0]*ac[2],ab[0]*ac[1]-ab[1]*ac[0]]
            assert sum(x*x for x in cross)>1e-20,(path.name,'degenerate triangle')
            smooth+=max(math.dist(normals[ids[0]],normals[i]) for i in ids)>1e-5
            triangles+=1
    assert 0<triangles<=20000,(path.name,'triangle budget',triangles)
    if require_smooth:
        assert smooth/triangles>.15,(path.name,'roof still flat shaded')
        tile_materials=[m for m in document['materials'] if any(t in m.get('name','').lower() for t in ('tile','roof','terracotta','slate'))]
        assert tile_materials,(path.name,'tile material metadata')
        for material in tile_materials:
            pbr=material['pbrMetallicRoughness']
            assert 0.2<=pbr.get('roughnessFactor',1)<=0.5,(path.name,'roof roughness',material['name'])
            assert pbr.get('metallicFactor',1)==0,(path.name,'metallic ceramic')
    return {'asset':path.name,'bytes':len(raw),'triangles':triangles,
        'materials':len(document['materials']),'uv0':True,'unit_normals':True,
        'smooth_triangle_fraction':smooth/triangles,'sha256':hashlib.sha256(raw).hexdigest()}

def validate():
    catalog=json.loads((KIT/'catalog.json').read_text(encoding='utf-8'))
    layouts=json.loads((KIT/'example-layouts.json').read_text(encoding='utf-8-sig'))
    modules=catalog['modules']
    assert len({m['id'] for m in modules})==len(modules)>=24
    assets=[]
    for module in modules:
        assert module['npc_handheld']==(module['category']=='carry')
        assets.append(inspect_asset(KIT/module['asset_glb'],require_smooth=module['category'] in ('roof_slope','ridge','canopy')))
    checks=[]
    choices=catalog['design_choices']
    for blueprint,wall,roof in itertools.product(choices['blueprint'],choices['wall_material'],choices['roof_material']):
        design={'blueprint':blueprint,'wall_material':wall,'roof_material':roof}
        plan=compile_design(catalog,layouts,design,site_id='test_plot',resident_id='test_resident')
        assert plan==compile_design(catalog,layouts,design,site_id='test_plot',resident_id='test_resident')
        assert plan['plan_id']!=compile_design(catalog,layouts,design,site_id='another_plot',resident_id='test_resident')['plan_id']
        quantities=Counter(); seen=set()
        for job in plan['jobs']:
            assert set(job['depends_on'])<=seen
            assert job['operation_id'] not in seen
            seen.add(job['operation_id'])
            for delivery in job['deliveries']:
                assert delivery['operation_id'] not in seen
                seen.add(delivery['operation_id'])
                assert 0<delivery['quantity']<=catalog['resources'][delivery['resource']]['pack_capacity']
                quantities[delivery['resource']]+=delivery['quantity']
        assert dict(quantities)==plan['resource_totals']
        assert not plan['live_execution_ready']
        for placement in plan['placements']:
            assert len(placement['position_m'])==3 and all(math.isfinite(v) for v in placement['position_m'])
            assert math.isfinite(placement['yaw_degrees'])
        checks.append({'design':design,'installations':len(plan['jobs']),'balanced_delivery_quantities':True})
    invalid={'blueprint':choices['blueprint'][0],'wall_material':'unknown','roof_material':'terracotta'}
    try: compile_design(catalog,layouts,invalid,site_id='test',resident_id='test')
    except ValueError: pass
    else: raise AssertionError('Invalid NPC material choice accepted')
    report={'status':'passed','scope':'independent exported GLB and offline construction-contract checks',
        'asset_count':len(assets),'design_combinations_checked':len(checks),
        'assets':assets,'design_checks':checks,'invalid_choice_rejected':True,
        'npc_runtime_execution_verified':False}
    (KIT/'validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k not in ('assets','design_checks')}))

if __name__=='__main__': validate()
