"""Independent accessory GLB, reference and ten-adult look manifest checks."""
from pathlib import Path
import hashlib
import json
import math
import sys

OUT=Path(__file__).resolve().parent
PROJECT=OUT.parent.parent
sys.path.insert(0,str(PROJECT/'Plugins/ThreeHearths/Tools'))
from validate_village_kit import inspect_asset

def validate():
    specs=json.loads((OUT/'module-specs.json').read_text(encoding='utf-8'))
    looks=json.loads((OUT/'looks.json').read_text(encoding='utf-8'))['looks']
    reference=json.loads((OUT/'attachment-reference.json').read_text(encoding='utf-8'))
    model=json.loads((OUT/'model-report.json').read_text(encoding='utf-8'))
    bones={b['name'] for b in reference['bones']}
    modules={m['id']:m for m in specs['modules']}
    assets=[]
    assert len(modules)==16
    for m in modules.values():
        assert m['bone'] in bones
        asset=inspect_asset(OUT/'modules'/(m['id']+'.glb'))
        assert asset['triangles']<=5000
        assets.append(asset)
    assert model['original_accessory_source_only'] and model['armature_datablock_count']==0
    assert model['scalp_fitting']['misses']==0
    assert len(looks)==10 and len({x['id'] for x in looks})==10
    references=[]
    for look in looks:
        assert look['adult_age']>=18
        assert len(look['attachments'])==len(set(look['attachments']))
        assert all(x in modules for x in look['attachments'])
        for slot in ('head_cover','back'):
            assert sum(modules[x]['slot']==slot for x in look['attachments'])<=1
        for ref in look['references']:
            assert ref['bone'] in bones
            source=OUT.parent/ref['kit']/'modules'/(ref['id']+'.glb')
            assert source.exists() and not (OUT/'modules'/source.name).exists()
            if not ref.get('display_only'):
                assert 0<ref['scale']<=3 and all(math.isfinite(x) for x in ref['position_m'])
            references.append({'file':source.relative_to(OUT.parent).as_posix(),
                'sha256':hashlib.sha256(source.read_bytes()).hexdigest()})
    review_path=OUT/'visual-review.json'
    reviewed=False
    if review_path.exists():
        review=json.loads(review_path.read_text(encoding='utf-8'))
        reviewed=review['reference_pose_front_and_back_reviewed']
        for item in review['reviewed_images']:
            assert hashlib.sha256((OUT/item['file']).read_bytes()).hexdigest()==item['sha256']
        assert hashlib.sha256((OUT/'ResidentKit.blend').read_bytes()).hexdigest()==review['source_blend_sha256']
    report={'status':'passed','scope':'exported static accessories and measured fitting contracts',
        'asset_count':len(assets),'adult_look_count':len(looks),'minimum_adult_age':min(x['adult_age'] for x in looks),
        'assets':assets,'referenced_existing_assets':references,'all_bones_exist':True,
        'ue_attachment_conversion_verified':False,'animation_binding_verified':False,
        'reference_pose_visual_review_required':not reviewed,
        'total_triangles':sum(a['triangles'] for a in assets),'max_asset_triangles':max(a['triangles'] for a in assets)}
    (OUT/'validation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    source_files=[p for p in OUT.iterdir() if p.suffix in ('.py','.blend','.md','.json') and p.name not in ('artifact-manifest.json','validation.json')]
    source_files+=list((OUT/'previews').glob('*.png'))
    manifest={'kit_id':'resident_kit_01','artifacts':[{'file':'modules/'+x['asset'],'sha256':x['sha256'],'bytes':x['bytes']} for x in assets],
        'source_and_preview_artifacts':[{'file':p.relative_to(OUT).as_posix(),'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'bytes':p.stat().st_size} for p in sorted(source_files)],
        'referenced_existing_assets':references,'byte_identical_regeneration_guaranteed':False}
    (OUT/'artifact-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k not in ('assets','referenced_existing_assets')}))

if __name__=='__main__':validate()
