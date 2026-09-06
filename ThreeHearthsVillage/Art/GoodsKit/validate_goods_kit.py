"""Read independent GLB bytes; do not import Blender or trust model-report counts."""
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

def geometry(path):
    raw=path.read_bytes();offset=12;chunks={}
    while offset<len(raw):
        size,kind=struct.unpack_from('<II',raw,offset);chunks[kind]=raw[offset+8:offset+8+size];offset+=8+size
    doc=json.loads(chunks[0x4E4F534A]);blob=chunks[0x004E4942]
    assert len(doc['nodes'])==1 and not any(k in doc['nodes'][0] for k in ('matrix','translation','rotation','scale'))
    def acc(i):
        a=doc['accessors'][i];v=doc['bufferViews'][a['bufferView']]
        count={'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']];fmt='<'+{5123:'H',5125:'I',5126:'f'}[a['componentType']]*count
        stride=v.get('byteStride',struct.calcsize(fmt));start=v.get('byteOffset',0)+a.get('byteOffset',0)
        return [struct.unpack_from(fmt,blob,start+j*stride) for j in range(a['count'])]
    points=[]
    for p in doc['meshes'][0]['primitives']:
        points.extend((x,-z,y) for x,y,z in acc(p['attributes']['POSITION']))
        uv=acc(p['attributes']['TEXCOORD_0'])
        assert max(u[0] for u in uv)-min(u[0] for u in uv)>.0001 or max(u[1] for u in uv)-min(u[1] for u in uv)>.0001
    bounds=[[min(p[i] for p in points) for i in range(3)],[max(p[i] for p in points) for i in range(3)]]
    return doc,bounds

def validate():
    specs=json.loads((OUT/'module-specs.json').read_text(encoding='utf-8'));assets=[]
    expected={'goods_raw_clay_basket':'raw_clay','goods_bricks_crate':'bricks','goods_tiles_terracotta_crate':'tiles_terracotta',
      'goods_tiles_slateblue_crate':'tiles_slateblue','goods_lime_pail':'lime','goods_pigment_pots':'pigment',
      'goods_iron_ingots_bundle':'iron_ingots','goods_nails_box':'nails'}
    assert {m['id']:m['commodity_id'] for m in specs['modules']}==expected
    assert len(set(expected.values()))==8
    assert specs['creates_inventory'] is False and specs['runtime_integration_verified'] is False
    for m in specs['modules']:
        path=OUT/m['asset_glb'];a=inspect_asset(path);doc,bounds=geometry(path)
        assert a['triangles']<=3000,(m['id'],'triangle budget')
        size=[bounds[1][i]-bounds[0][i] for i in range(3)]
        assert .35<=size[0]<=.75 and .25<=size[2]<=.50,(m['id'],'carry envelope',size)
        assert abs(bounds[0][2])<1e-5 and all(abs(bounds[0][i]+bounds[1][i])<1e-5 for i in (0,1)),(m['id'],'bottom-centre origin')
        assert math.dist(size,m['nominal_size_m'])<2e-5
        node=doc['nodes'][0];extras=node['extras']
        assert extras['commodity_id']==m['commodity_id'] and extras['creates_inventory'] is False
        assert extras['runtime_integration_verified'] is False
        assert math.dist(extras['carry_anchor_m'],m['carry_anchor']['position_m'])<1e-5
        assert m['holding_animation_verified'] is False and m['recipe_bound'] is False
        for p in [m['carry_anchor']['position_m'],*m['grip_points'].values()]:
            assert len(p)==3 and all(math.isfinite(x) for x in p)
            assert all(bounds[0][i]-.04<=p[i]<=bounds[1][i]+.04 for i in range(3)),(m['id'],'anchor/hand grip outside carry envelope',p)
        # Conservative scalar spacing only, not skeleton/IK/animation fitting.
        # Existing ResidentKit torso front envelope is approximately Y=-.33 m.
        rear_y=specs['suggested_resident_target_m'][1]-m['carry_anchor']['position_m'][1]+bounds[1][1]
        torso_gap=-.33-rear_y
        assert torso_gap>=.025,(m['id'],'proposal places pack too near reference torso',torso_gap)
        for material in doc['materials']:
            pbr=material['pbrMetallicRoughness']
            assert len(pbr['baseColorFactor'])==4 and all(0<=x<=1 for x in pbr['baseColorFactor'])
            assert 0<=pbr.get('metallicFactor',1)<=1 and 0<=pbr.get('roughnessFactor',1)<=1
            if 'Tile' in material['name']:assert .2<=pbr['roughnessFactor']<=.5 and pbr.get('metallicFactor',1)==0
        assert not doc.get('textures') and not doc.get('images')
        a.update({'id':m['id'],'commodity_id':m['commodity_id'],'bounds_authoring_m':bounds,
          'bottom_center_origin':True,'single_commodity_extras_verified':True,'carry_contract_exported':True,
          'portable_pbr_factors':True,'nonconstant_uv0':True,'proposed_carry_rear_y_m':rear_y,
          'conservative_reference_torso_gap_m':torso_gap,'torso_gap_check_is_animation_fitting':False})
        assets.append(a)
    report={'status':'passed','scope':'Independent GLB binary accessors, indices, nondegenerate triangles, UV, normalized normals, PBR, bounds and metadata',
      'asset_count':len(assets),'total_triangles':sum(a['triangles'] for a in assets),
      'max_asset_triangles':max(a['triangles'] for a in assets),'assets':assets,'unique_commodity_count':8,
      'stock_recipe_or_hold_binding_verified':False,'render_review_verified':False}
    audit_path=OUT/'source-audit.json'
    if audit_path.exists():
        audit=json.loads(audit_path.read_text(encoding='utf-8'))
        assert audit['original_goods_only'] and not audit['licensed_character_geometry_in_source']
        assert len(audit['meshes'])==8 and len(audit['objects'])==16 and not audit['armatures']
        report['source_audit_original_goods_only']=True
    review=OUT/'visual-review.json'
    if review.exists():
        r=json.loads(review.read_text(encoding='utf-8'))
        for item in r['reviewed_images']:assert hashlib.sha256((OUT/item['file']).read_bytes()).hexdigest()==item['sha256']
        assert hashlib.sha256((OUT/'GoodsKit.blend').read_bytes()).hexdigest()==r['source_blend_sha256']
        report['render_review_verified']=r['overview_and_character_scale_reviewed']
    (OUT/'validation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    files=[p for p in OUT.rglob('*') if p.is_file() and p.suffix in ('.py','.json','.glb','.blend','.png','.md') and p.name!='artifact-manifest.json']
    manifest={'kit_id':'goods_kit_01','artifacts':[{'file':p.relative_to(OUT).as_posix(),'bytes':p.stat().st_size,
       'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(files)],
       'stock_recipe_or_hold_binding_verified':False,'licensed_reference_source_included':False}
    (OUT/'artifact-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k!='assets'}))

if __name__=='__main__':validate()
