"""Independent exported geometry, living-space and interaction-contract checks."""
from pathlib import Path
import hashlib
import itertools
import json
import math
import struct
import sys
from collections import deque
OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(OUT.parent.parent/'Plugins/ThreeHearths/Tools'))
from validate_village_kit import inspect_asset

def read_triangles(path):
    raw=path.read_bytes();length=struct.unpack_from('<I',raw,12)[0]
    d=json.loads(raw[20:20+length]);offset=20+length
    data=raw[offset+8:]
    assert len(d['nodes'])==1 and not any(k in d['nodes'][0] for k in ('matrix','rotation','translation','scale'))
    def accessor(index):
        a=d['accessors'][index];v=d['bufferViews'][a['bufferView']]
        n={'SCALAR':1,'VEC3':3}[a['type']];code={5123:'H',5125:'I',5126:'f'}[a['componentType']]
        fmt='<'+code*n;size=struct.calcsize(fmt);stride=v.get('byteStride',size)
        start=v.get('byteOffset',0)+a.get('byteOffset',0)
        return [struct.unpack_from(fmt,data,start+j*stride) for j in range(a['count'])]
    triangles=[]
    for primitive in d['meshes'][0]['primitives']:
        # Standard glTF Y-up -> original Blender +Z, -Y front.
        vertices=[(x,-z,y) for x,y,z in accessor(primitive['attributes']['POSITION'])]
        ids=[i[0] for i in accessor(primitive['indices'])]
        triangles.extend(tuple(vertices[i] for i in ids[j:j+3]) for j in range(0,len(ids),3))
    return triangles

def bounds(triangles):
    points=list(itertools.chain.from_iterable(triangles))
    return [[min(p[i] for p in points) for i in range(3)],[max(p[i] for p in points) for i in range(3)]]

def upper_surface(triangles,x,y):
    heights=[]
    for a,b,c in triangles:
        denominator=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
        if abs(denominator)<1e-12:continue
        u=((b[1]-c[1])*(x-c[0])+(c[0]-b[0])*(y-c[1]))/denominator
        v=((c[1]-a[1])*(x-c[0])+(a[0]-c[0])*(y-c[1]))/denominator
        w=1-u-v
        if min(u,v,w)>=-1e-7:heights.append(u*a[2]+v*b[2]+w*c[2])
    return max(heights) if heights else None

def transformed(p,placement):
    a=math.radians(placement['yaw_degrees']);c,s=math.cos(a),math.sin(a)
    t=placement['position_m']
    return [c*p[0]-s*p[1]+t[0],s*p[0]+c*p[1]+t[1],p[2]+t[2]]

def world_bounds(local,placement):
    corners=[transformed(p,placement) for p in itertools.product(*zip(*local))]
    return [[min(p[i] for p in corners) for i in range(3)],[max(p[i] for p in corners) for i in range(3)]]

def validate():
    specs=json.loads((OUT/'module-specs.json').read_text(encoding='utf-8'))
    layouts=json.loads((OUT/'example-layouts.json').read_text(encoding='utf-8'))
    modules={m['id']:m for m in specs['modules']};assert len(modules)==12
    assets=[];geometry={};limits={};checks=[]
    for m in modules.values():
        path=OUT/'modules'/(m['id']+'.glb');asset=inspect_asset(path)
        assert asset['triangles']<=8000,(m['id'],'triangle budget')
        assert m['creates_inventory'] is False
        triangles=read_triangles(path);geometry[m['id']]=triangles
        local=bounds(triangles);limits[m['id']]=local
        assert -.003<=local[0][2]<=.037,(m['id'],'support-plane origin',local[0][2])
        assert all(local[1][i]-local[0][i]<=m['nominal_size_m'][i]+.035 for i in range(3)),(m['id'],'nominal envelope')
        for p in m['interaction_points']:
            assert len(p['position_m'])==3 and all(math.isfinite(x) for x in p['position_m'])
            if p['kind']=='approach_ground':assert p['position_m'][2]==0
            if p['kind'] in ('sitting_contact','lying_support','place_surface'):
                x,y,z=p['position_m']
                surfaces=[upper_surface(triangles,x+dx,y+dy) for dx,dy in ((-.035,0),(.035,0),(0,-.035),(0,.035))]
                assert any(h is not None and abs(h-z)<.017 for h in surfaces),(m['id'],p['id'],'support surface mismatch',surfaces,z)
        assets.append(asset)
    assert modules['chair_oak_wide']['seat_width_m']>specs['measured_hip_width_m']+.08
    assert modules['chair_oak_wide']['seat_depth_m']>specs['measured_hip_depth_m']
    for x,y in ((0,0),(.07,0),(-.07,0),(0,.07),(0,-.07)):
        top=upper_surface(geometry['basket_empty'],x,y)
        assert top is not None and .01<=top<=.06,('empty basket opening blocked',x,y,top)
    for example in layouts['examples']:
        placements=example['placements'];assert len({p['instance_id'] for p in placements})==len(placements)
        boxes={p['instance_id']:world_bounds(limits[p['module_id']],p) for p in placements}
        index={p['instance_id']:p for p in placements}
        lo,hi=example['interior_bounds_m']
        ground=[p for p in placements if not p.get('support_instance')]
        for p in placements:
            box=boxes[p['instance_id']]
            assert all(box[0][i]>=lo[i]-.02 and box[1][i]<=hi[i]+.02 for i in (0,1)),(example['id'],p['instance_id'],'outside layout')
            if p.get('support_instance'):
                owner=index[p['support_instance']];ownerbox=boxes[owner['instance_id']]
                assert abs(box[0][2]-ownerbox[1][2])<.04
                assert all(box[0][i]>=ownerbox[0][i] and box[1][i]<=ownerbox[1][i] for i in (0,1)),(p['instance_id'],'unsupported tabletop display')
        for a,b in itertools.combinations(ground,2):
            aa,bb=boxes[a['instance_id']],boxes[b['instance_id']]
            assert not all(min(aa[1][i],bb[1][i])-max(aa[0][i],bb[0][i])>.002 for i in (0,1)),(example['id'],'furniture footprint overlap',a['instance_id'],b['instance_id'])
        active=[]
        for p in placements:
            valid={q['id'] for q in modules[p['module_id']]['interaction_points']}
            assert set(p.get('disabled_interactions',[]))<=valid
            for q in modules[p['module_id']]['interaction_points']:
                if q['kind']!='approach_ground' or q['id'] in p.get('disabled_interactions',[]):continue
                at=transformed(q['position_m'],p);radius=q['clearance_radius_m']
                assert all(lo[i]+radius<=at[i]<=hi[i]-radius for i in (0,1)),(example['id'],p['instance_id'],q['id'],'standing circle leaves layout')
                for obstacle in ground:
                    b=boxes[obstacle['instance_id']]
                    dist=math.sqrt(sum(max(b[0][i]-at[i],0,at[i]-b[1][i])**2 for i in (0,1)))
                    assert dist>=radius-.005,(example['id'],p['instance_id'],q['id'],'approach blocked',obstacle['instance_id'],dist)
                active.append({'instance_id':p['instance_id'],'point_id':q['id'],'world_position_m':at})
        seats=sum(modules[p['module_id']].get('occupants',0) for p in placements if modules[p['module_id']]['category']=='installed_seat')
        if 'seat_capacity' in example:assert seats==example['seat_capacity']==10
        shared=[{'a':[a['instance_id'],a['point_id']],'b':[b['instance_id'],b['point_id']],
                 'required_policy':'mutually_exclusive_approach_reservations'}
                for a,b in itertools.combinations(active,2) if math.dist(a['world_position_m'],b['world_position_m'])<.90]
        # Conservative 2D walking-centre flood fill, using exported furniture
        # bounds expanded by the standing radius. This does not replace UE nav.
        step=.05;radius=.45
        xmin,ymin=lo[0]+radius,lo[1]+radius
        nx=int((hi[0]-radius-xmin)/step)+1;ny=int((hi[1]-radius-ymin)/step)+1
        blocked=[]
        for obstacle in ground:blocked.append(boxes[obstacle['instance_id']])
        free=set()
        for ix in range(nx):
            for iy in range(ny):
                x,y=xmin+ix*step,ymin+iy*step
                if all(sum(max(b[0][i]-v,0,v-b[1][i])**2 for i,v in enumerate((x,y)))>=radius**2 for b in blocked):free.add((ix,iy))
        def nearest(p):
            return min(free,key=lambda q:(xmin+q[0]*step-p[0])**2+(ymin+q[1]*step-p[1])**2)
        start=nearest(example['entry_m'])
        assert math.dist((xmin+start[0]*step,ymin+start[1]*step),example['entry_m'][:2])<.08
        seen={start};queue=deque([start])
        while queue:
            ix,iy=queue.popleft()
            for q in ((ix-1,iy),(ix+1,iy),(ix,iy-1),(ix,iy+1)):
                if q in free and q not in seen:seen.add(q);queue.append(q)
        for a in active:
            target=nearest(a['world_position_m'])
            assert math.dist((xmin+target[0]*step,ymin+target[1]*step),a['world_position_m'][:2])<.08
            assert target in seen,(example['id'],a['instance_id'],a['point_id'],'approach disconnected from entry')
        # Verify that exported example instances really use the same layouts.
        example_path=OUT/'examples'/(example['id']+'.glb')
        assert example_path.exists(),('render/export the example before final validation',example['id'])
        if example_path.exists():
            raw=example_path.read_bytes();length=struct.unpack_from('<I',raw,12)[0]
            doc=json.loads(raw[20:20+length]);nodes={n['name']:n for n in doc['nodes']}
            for p in placements:
                n=nodes[p['instance_id']]
                assert n['extras']['module_id']==p['module_id'] and not n['extras']['creates_inventory']
                x,y,z=p['position_m'];expected=[x,z,-y]
                assert math.dist(n.get('translation',[0,0,0]),expected)<1e-5
                angle=math.radians(p['yaw_degrees'])/2;expected_q=[0,math.sin(angle),0,math.cos(angle)]
                q=n.get('rotation',[0,0,0,1])
                assert min(math.dist(q,expected_q),math.dist(q,[-v for v in expected_q]))<1e-5
        checks.append({'example_id':example['id'],'no_furniture_footprint_overlap':True,
          'active_standing_points_clear':len(active),'seat_capacity':seats,'world_approach_points':active,
          'static_2d_access_from_entry_verified':True,'clearance_grid_step_m':step,
          'overlapping_approaches_require_reservation':shared,'exported_example_transforms_verified':example_path.exists()})
    visual_reviewed=False
    review_path=OUT/'visual-review.json'
    if review_path.exists():
        review=json.loads(review_path.read_text(encoding='utf-8'))
        visual_reviewed=review['overview_and_two_layouts_reviewed']
        for item in review['reviewed_images']:
            assert hashlib.sha256((OUT/item['file']).read_bytes()).hexdigest()==item['sha256']
        assert hashlib.sha256((OUT/'HomeLifeKit.blend').read_bytes()).hexdigest()==review['source_blend_sha256']
    report={'status':'passed','asset_count':len(assets),'assets':assets,
        'total_triangles':sum(a['triangles'] for a in assets),'max_asset_triangles':max(a['triangles'] for a in assets),
        'empty_basket_opening_verified':True,'seat_bed_table_support_surfaces_verified':True,
        'layout_checks':checks,'creates_inventory':False,'navigation_and_animation_verified':False,
        'visual_review_verified':visual_reviewed,
        'scope':'Independent exported GLB, reference dimensions, static support points and standing clearance circles'}
    (OUT/'validation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    files=[p for p in OUT.rglob('*') if p.is_file() and p.suffix in ('.py','.json','.glb','.blend','.png','.md') and p.name!='artifact-manifest.json' and 'mcp' not in p.parts]
    manifest={'kit_id':'home_life_kit_01','artifacts':[{'file':p.relative_to(OUT).as_posix(),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(files)],
        'navigation_and_animation_verified':False,'spawning_visuals_must_not_create_stock':True}
    (OUT/'artifact-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k not in ('assets','layout_checks')}))

if __name__=='__main__':validate()
