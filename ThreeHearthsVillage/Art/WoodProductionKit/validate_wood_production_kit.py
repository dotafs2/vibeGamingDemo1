"""Independent geometry, actual kerf/flat-face and unchanged-reference checks."""
from pathlib import Path
import hashlib
import importlib.util
import json
import math
import sys
sys.dont_write_bytecode=True
OUT=Path(__file__).resolve().parent
sp=importlib.util.spec_from_file_location('wood_independent_glb',OUT.parent/'ToolKit/validate_tool_kit.py')
V=importlib.util.module_from_spec(sp);sp.loader.exec_module(V)

def top_at(triangles,x,y):
    hits=[]
    for a,b,c in triangles:
        den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
        if abs(den)<1e-12:continue
        u=((b[1]-c[1])*(x-c[0])+(c[0]-b[0])*(y-c[1]))/den
        v=((c[1]-a[1])*(x-c[0])+(a[0]-c[0])*(y-c[1]))/den;w=1-u-v
        if min(u,v,w)>=-1e-7:hits.append(u*a[2]+v*b[2]+w*c[2])
    return max(hits) if hits else None

def validate():
    specs=json.loads((OUT/'module-specs.json').read_text(encoding='utf-8'))
    reuse=json.loads((OUT/'reuse-manifest.json').read_text(encoding='utf-8'));layout=json.loads((OUT/'production-layout.json').read_text(encoding='utf-8'))
    assert {m['id'] for m in specs['modules']}=={'wip_log_to_planks','wip_log_to_beam'}
    assert not specs['spawns_stock'] and not specs['intermediates_are_inventory']
    assets=[];geometry={};bounds_by_id={};reference_checks=[]
    for m in specs['modules']:
        path=OUT/m['asset_glb'];a=V.inspect_asset(path);doc,bounds,tris=V.decode(path)
        assert a['triangles']<=3000 and len(doc['nodes'])==1
        node=doc['nodes'][0];assert not any(k in node for k in ('matrix','translation','rotation','scale'))
        assert node['extras']['intermediate_id']==m['id'] and not node['extras']['spawns_stock']
        assert not node['extras']['runtime_integration_verified']
        assert math.dist(node['extras']['work_anchor_m'],m['work_anchor_m'])<1e-6
        size=[bounds[1][k]-bounds[0][k] for k in range(3)]
        assert math.dist(size,m['nominal_size_m'])<2e-5
        assert abs(bounds[0][2])<1e-5 and all(abs(bounds[0][k]+bounds[1][k])<1e-5 for k in (0,1))
        assert not doc.get('textures') and not doc.get('images')
        for material in doc['materials']:assert 'pbrMetallicRoughness' in material and material['name'].startswith('WP_')
        assert m['inventory_identity'] is None
        geometry[m['id']]=tris;bounds_by_id[m['id']]=bounds;a['measured_size_m']=size;assets.append(a)
    plank=geometry['wip_log_to_planks']
    # The cut is real exported geometry: tops at the kerfs are deep below their adjacent wood.
    for y in (-.043,.043):
        valley=top_at(plank,.14,y);land=top_at(plank,.14,y+.018)
        assert valley is not None and land is not None and valley<.07 and land>.18,(y,valley,land)
    beam=geometry['wip_log_to_beam']
    assert .17<top_at(beam,.14,0)<.19 and top_at(beam,-.20,0)>.22
    for ref in reuse['references']:
        path=OUT/ref['asset_glb'];a=V.inspect_asset(path);doc,bounds,tris=V.decode(path)
        assert a['sha256']==ref['sha256'] and a['bytes']==ref['bytes'] and a['triangles']==ref['triangles']
        geometry[ref['reference_id']]=tris;bounds_by_id[ref['reference_id']]=bounds
        reference_checks.append({'id':ref['reference_id'],'source_unchanged':True,'triangles':a['triangles'],'sha256':a['sha256']})
    # Workpieces rest on the unobstructed front plank of the existing bench.
    support_checks=[]
    for branch in layout['branches']:
        x,y,z=branch['intermediate_bench_local_m'];heights=[top_at(geometry['carpenter_bench'],x+dx,y) for dx in (-.29,0,.29)]
        assert all(h is not None and abs(h-z)<.005 for h in heights),(branch['id'],'workpiece support',heights,z)
        support_checks.append({'branch':branch['id'],'exported_bench_support_height_m':heights,'support_verified':True})
    report={'status':'passed','scope':'Independent GLB bytes, real saw kerfs/hewn faces, PBR/UV/normals, origins, metadata, unchanged reuse hashes and bench support',
      'new_asset_count':len(assets),'new_total_triangles':sum(a['triangles'] for a in assets),'new_max_asset_triangles':max(a['triangles'] for a in assets),
      'assets':assets,'reused_asset_count':len(reference_checks),'unchanged_references':reference_checks,
      'real_saw_kerfs_verified':True,'partial_hewn_faces_verified':True,'support_checks':support_checks,
      'runtime_inventory_or_work_animation_verified':False,'render_review_verified':False}
    if (OUT/'source-audit.json').exists():
        audit=json.loads((OUT/'source-audit.json').read_text(encoding='utf-8'))
        assert audit['new_original_intermediates_only'] and len(audit['meshes'])==2 and len(audit['objects'])==4
        assert hashlib.sha256((OUT/'WoodProductionKit.blend').read_bytes()).hexdigest()==audit['source_blend_sha256']
        report['source_originals_only_verified']=True
    if (OUT/'visual-review.json').exists():
        review=json.loads((OUT/'visual-review.json').read_text(encoding='utf-8'))
        for item in review['reviewed_images']:assert hashlib.sha256((OUT/item['file']).read_bytes()).hexdigest()==item['sha256']
        assert hashlib.sha256((OUT/'WoodProductionKit.blend').read_bytes()).hexdigest()==review['source_blend_sha256']
        report['render_review_verified']=review['originals_and_reuse_production_preview_reviewed']
    (OUT/'validation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    files=[p for p in OUT.rglob('*') if p.is_file() and p.suffix in ('.py','.json','.md','.blend','.glb','.png') and p.name!='artifact-manifest.json']
    manifest={'kit_id':'wood_production_kit_01','artifacts':[{'file':p.relative_to(OUT).as_posix(),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(files)]}
    (OUT/'artifact-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k not in ('assets','unchanged_references','support_checks')}))

if __name__=='__main__':validate()
