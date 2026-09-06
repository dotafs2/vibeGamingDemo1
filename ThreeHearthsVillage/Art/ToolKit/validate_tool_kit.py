"""Independent exported GLB byte-stream inspection and tool contract checks."""
from pathlib import Path
import hashlib
import json
import math
import struct
import sys
sys.dont_write_bytecode=True
OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(OUT.parent.parent/'Plugins/ThreeHearths/Tools'))
from validate_village_kit import inspect_asset

def decode(path):
    raw=path.read_bytes();offset=12;chunks={}
    while offset<len(raw):
        size,kind=struct.unpack_from('<II',raw,offset);chunks[kind]=raw[offset+8:offset+8+size];offset+=8+size
    doc=json.loads(chunks[0x4E4F534A]);blob=chunks[0x004E4942]
    def acc(i):
        a=doc['accessors'][i];v=doc['bufferViews'][a['bufferView']]
        n={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']];fmt='<'+{5123:'H',5125:'I',5126:'f'}[a['componentType']]*n
        start=v.get('byteOffset',0)+a.get('byteOffset',0);stride=v.get('byteStride',struct.calcsize(fmt))
        return [struct.unpack_from(fmt,blob,start+j*stride) for j in range(a['count'])]
    points=[];triangles=[]
    for p in doc['meshes'][0]['primitives']:
        verts=[(x,-z,y) for x,y,z in acc(p['attributes']['POSITION'])];points.extend(verts)
        ids=[i[0] for i in acc(p['indices'])];triangles.extend(tuple(verts[i] for i in ids[j:j+3]) for j in range(0,len(ids),3))
        uv=acc(p['attributes']['TEXCOORD_0'])
        assert any(max(u[k] for u in uv)-min(u[k] for u in uv)>1e-5 for k in (0,1))
    bounds=[[min(p[k] for p in points) for k in range(3)],[max(p[k] for p in points) for k in range(3)]]
    return doc,bounds,triangles

def validate():
    specs=json.loads((OUT/'module-specs.json').read_text(encoding='utf-8'));assets=[]
    expected={f'tool_{name}':name for name in ('hammer','mallet','axe','saw','pickaxe','shovel','hoe','trowel')}
    assert {m['id']:m['tool_id'] for m in specs['modules']}==expected
    assert not specs['tool_inventory_created'] and not specs['runtime_attachment_verified']
    for m in specs['modules']:
        path=OUT/m['asset_glb'];a=inspect_asset(path);doc,bounds,tris=decode(path)
        assert a['triangles']<=3000
        assert len(doc['nodes'])==1 and not any(k in doc['nodes'][0] for k in ('matrix','translation','rotation','scale'))
        size=[bounds[1][k]-bounds[0][k] for k in range(3)]
        assert math.dist(size,m['nominal_size_m'])<2e-5
        assert max(size)<=1.12 and max(size)>=.32
        assert abs(bounds[0][2])<1e-5 and all(abs(bounds[0][k]+bounds[1][k])<1e-5 for k in (0,1))
        extras=doc['nodes'][0]['extras'];assert extras['tool_id']==m['tool_id']
        assert not extras['runtime_attachment_verified'] and not extras['tool_inventory_created']
        assert math.dist(extras['grip_anchor_m'],m['grip_anchor']['position_m'])<1e-6
        assert math.dist(extras['working_tip_m'],m['working_tip_m'])<1e-6
        anchors=[m['grip_anchor']['position_m'],m['working_tip_m']]
        if 'secondary_grip_anchor_m' in m:anchors.append(m['secondary_grip_anchor_m'])
        for p in anchors:
            assert len(p)==3 and all(math.isfinite(v) for v in p)
            assert all(bounds[0][k]-.035<=p[k]<=bounds[1][k]+.035 for k in range(3)),(m['id'],'anchor outside tool envelope',p)
        # Intersect the independently decoded mesh with a local Y-directed ray
        # through each handle grip. This catches a grip placed in empty air or
        # in the saw handle opening; it is not a skeleton/animation fit check.
        grip_checks=[]
        for p in [m['grip_anchor']['position_m']]+([m['secondary_grip_anchor_m']] if 'secondary_grip_anchor_m' in m else []):
            x,y,z=p;hits=[]
            for aa,bb,cc in tris:
                den=(bb[2]-cc[2])*(aa[0]-cc[0])+(cc[0]-bb[0])*(aa[2]-cc[2])
                if abs(den)<1e-12:continue
                u=((bb[2]-cc[2])*(x-cc[0])+(cc[0]-bb[0])*(z-cc[2]))/den
                v=((cc[2]-aa[2])*(x-cc[0])+(aa[0]-cc[0])*(z-cc[2]))/den;w=1-u-v
                if min(u,v,w)>=-1e-6:hits.append(u*aa[1]+v*bb[1]+w*cc[1])
            assert len(hits)>=2 and min(hits)<=y<=max(hits),(m['id'],'grip is not within a handle section',p)
            thickness=max(hits)-min(hits);assert .015<=thickness<=.12,(m['id'],'unexpected grip thickness',thickness)
            grip_checks.append({'anchor_m':p,'section_thickness_m':thickness})
        assert not doc.get('textures') and not doc.get('images')
        for material in doc['materials']:
            pbr=material['pbrMetallicRoughness'];assert len(pbr['baseColorFactor'])==4
            assert all(0<=x<=1 for x in pbr['baseColorFactor'])
            assert 0<=pbr.get('roughnessFactor',1)<=1 and 0<=pbr.get('metallicFactor',1)<=1
        a.update({'id':m['id'],'tool_id':m['tool_id'],'size_authoring_m':size,'bottom_center_origin_verified':True,
          'grip_and_working_tip_metadata_verified':True,'grip_within_exported_handle_section':grip_checks,
          'self_contained_pbr':True,'nonconstant_uv':True})
        assets.append(a)
    report={'status':'passed','scope':'Independent GLB binary geometry, indices, nondegenerate triangles, UV, normals, PBR, identity transforms, bounds and tool metadata',
      'asset_count':len(assets),'unique_tool_count':len(expected),'total_triangles':sum(a['triangles'] for a in assets),
      'max_asset_triangles':max(a['triangles'] for a in assets),'assets':assets,'runtime_holding_or_work_animation_verified':False,
      'render_review_verified':False}
    if (OUT/'source-audit.json').exists():
        audit=json.loads((OUT/'source-audit.json').read_text(encoding='utf-8'))
        assert audit['original_tools_only'] and not audit['licensed_character_geometry_in_source']
        assert len(audit['meshes'])==8 and len(audit['objects'])==16 and not audit['armatures'] and not audit['actions']
        assert hashlib.sha256((OUT/'ToolKit.blend').read_bytes()).hexdigest()==audit['source_blend_sha256']
        report['source_audit_original_tools_only']=True
    if (OUT/'visual-review.json').exists():
        review=json.loads((OUT/'visual-review.json').read_text(encoding='utf-8'))
        assert hashlib.sha256((OUT/'ToolKit.blend').read_bytes()).hexdigest()==review['source_blend_sha256']
        for item in review['reviewed_images']:assert hashlib.sha256((OUT/item['file']).read_bytes()).hexdigest()==item['sha256']
        report['render_review_verified']=review['overview_and_grip_guide_reviewed']
    (OUT/'validation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    files=[p for p in OUT.rglob('*') if p.is_file() and p.suffix in ('.py','.json','.md','.blend','.glb','.png') and p.name!='artifact-manifest.json']
    (OUT/'artifact-manifest.json').write_text(json.dumps({'kit_id':'tool_kit_01','artifacts':[{'file':p.relative_to(OUT).as_posix(),'bytes':p.stat().st_size,
       'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(files)]},indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k!='assets'}))

if __name__=='__main__':validate()
