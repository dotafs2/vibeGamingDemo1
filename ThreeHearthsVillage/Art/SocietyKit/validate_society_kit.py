"""Independent exported-GLB checks, including rays through intended openings."""
from pathlib import Path
import hashlib
import json
import math
import struct
import sys

OUT=Path(__file__).resolve().parent
PROJECT=OUT.parent.parent
sys.path.insert(0,str(PROJECT/'Plugins/ThreeHearths/Tools'))
from validate_village_kit import inspect_asset


def triangles(path):
    raw=path.read_bytes();chunks={};offset=12
    while offset<len(raw):
        length,kind=struct.unpack_from('<II',raw,offset)
        chunks[kind]=raw[offset+8:offset+8+length];offset+=8+length
    doc=json.loads(chunks[0x4E4F534A]);data=chunks[0x004E4942]
    def acc(index):
        a=doc['accessors'][index];v=doc['bufferViews'][a['bufferView']]
        code={5123:'H',5125:'I',5126:'f'}[a['componentType']]
        width={'SCALAR':1,'VEC3':3}[a['type']]
        fmt='<'+code*width;stride=v.get('byteStride',struct.calcsize(fmt))
        start=v.get('byteOffset',0)+a.get('byteOffset',0)
        return [struct.unpack_from(fmt,data,start+j*stride) for j in range(a['count'])]
    result=[]
    for primitive in doc['meshes'][0]['primitives']:
        verts=acc(primitive['attributes']['POSITION'])
        ids=[v[0] for v in acc(primitive['indices'])]
        result.extend(tuple(verts[i] for i in ids[start:start+3]) for start in range(0,len(ids),3))
    return result


def sub(a,b):return tuple(x-y for x,y in zip(a,b))
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def gltf(v):return (v[0],v[2],-v[1])


def obstructed(mesh,a,b):
    origin,end=gltf(a),gltf(b);direction=sub(end,origin)
    for v0,v1,v2 in mesh:
        e1,e2=sub(v1,v0),sub(v2,v0);p=cross(direction,e2);det=dot(e1,p)
        if abs(det)<1e-10:continue
        inv=1/det;tvec=sub(origin,v0);u=dot(tvec,p)*inv
        if not 0<=u<=1:continue
        q=cross(tvec,e1);v=dot(direction,q)*inv
        if v<0 or u+v>1:continue
        t=dot(e2,q)*inv
        if 1e-5<t<1-1e-5:return True
    return False


def validate():
    specs=json.loads((OUT/'module-specs.json').read_text())
    assets=[];errors=[]
    for spec in specs['modules']:
        try:assets.append(inspect_asset(OUT/'modules'/(spec['id']+'.glb')))
        except AssertionError as exc:errors.append(str(exc))
    assert not errors,errors
    rays=[
        ('castle_gate_arch_4m',(-.70,-1,1.0),(-.70,1,1.0),'gate lower left'),
        ('castle_gate_arch_4m',(.70,-1,1.0),(.70,1,1.0),'gate lower right'),
        ('castle_gate_arch_4m',(0,-1,3.0),(0,1,3.0),'gate arch crown'),
        ('castle_tower_storey_4m',(0,-2.5,1.0),(0,-1.45,1.0),'tower doorway'),
        ('castle_tower_storey_4m',(0,1.45,1.6),(0,2.5,1.6),'tower rear arrow slit'),
        ('castle_tower_battlement_cap_4m',(-1,0,-.2),(-1,0,.4),'tower roof hatch'),
        ('workshop_tile_kiln',(0,-1,.70),(0,.4,.70),'kiln firebox'),
        ('workshop_blacksmith_forge',(0,-.6,1.1),(0,.55,1.1),'forge mouth'),
    ]
    opening_checks=[]
    cache={}
    for module,a,b,label in rays:
        if module not in cache:cache[module]=triangles(OUT/'modules'/(module+'.glb'))
        assert not obstructed(cache[module],a,b),(module,'blocked opening',label)
        opening_checks.append({'module_id':module,'opening':label,'segment_clear':True})
    # Positive controls ensure that ray tests would notice a solid wall.
    assert obstructed(cache['castle_gate_arch_4m'],(1.6,-1,1),(1.6,1,1))
    assert obstructed(cache['castle_tower_storey_4m'],(1,-2.5,1),(1,-1.45,1))
    for x in (-1.82,1.82):
        assert obstructed(cache['castle_tower_storey_4m'],(x,2.5,1),(x,1.45,1)),('unsealed tower rear corner',x)
    hood=triangles(OUT/'modules'/'profession_mason_hood.glb')
    assert obstructed(hood,(0,.04,.4),(0,.04,.05)), 'hood crown must cover the head'
    assert not obstructed(hood,(0,-.30,.10),(0,-.02,.10)), 'hood front face opening'
    layouts=json.loads((OUT/'example-layouts.json').read_text())
    for example in layouts['examples']:
        seen=set()
        for p in example['placements']:
            key=(p['module_id'],tuple(p['position_m']),p['yaw_degrees'])
            assert key not in seen,('duplicate placement',example['id'],key)
            seen.add(key)
    report={'status':'passed','scope':'exported geometry and visual assembly contract',
        'asset_count':len(assets),'assets':assets,'opening_checks':opening_checks,
        'positive_control_wall_hits':True,'tower_rear_corners_sealed':True,
        'hood_crown_closed_and_face_open':True,'duplicate_module_placements':False,
        'npc_runtime_execution_verified':False,'collision_navigation_verified':False}
    (OUT/'validation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k not in ('assets','opening_checks')}))

if __name__=='__main__':validate()
