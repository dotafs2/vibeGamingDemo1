"""Import ResidentKit without recentering its bone-relative attachment roots."""
import hashlib
import json
import math
from pathlib import Path
import struct
import sys
import unreal as ue

ROOT=Path(__file__).resolve().parents[3]
sys.path.insert(0,str(Path(__file__).resolve().parent))
from import_village_kit import import_one

def source_details(path):
    raw=path.read_bytes();length=struct.unpack_from('<I',raw,12)[0]
    d=json.loads(raw[20:20+length]);data=raw[28+length:]
    def positions(index):
        a=d['accessors'][index];v=d['bufferViews'][a['bufferView']]
        assert a['type']=='VEC3' and a['componentType']==5126
        start=v.get('byteOffset',0)+a.get('byteOffset',0);stride=v.get('byteStride',12)
        return [struct.unpack_from('<3f',data,start+j*stride) for j in range(a['count'])]
    def matrix(n):
        if 'matrix' in n:return [[n['matrix'][c*4+r] for c in range(4)] for r in range(4)]
        x,y,z,w=n.get('rotation',[0,0,0,1]);s=n.get('scale',[1,1,1]);t=n.get('translation',[0,0,0])
        m=[[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w),t[0]],
           [2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w),t[1]],
           [2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y),t[2]],[0,0,0,1]]
        for r in range(3):
            for c in range(3):m[r][c]*=s[c]
        return m
    def multiply(a,b):return [[sum(a[r][k]*b[k][c] for k in range(4)) for c in range(4)] for r in range(4)]
    identity=[[int(r==c) for c in range(4)] for r in range(4)];points=[];triangle_counts=[]
    def visit(i,parent):
        n=d['nodes'][i];m=multiply(parent,matrix(n))
        if 'mesh' in n:
            for primitive in d['meshes'][n['mesh']]['primitives']:
                triangle_counts.append(d['accessors'][primitive['indices']]['count']//3)
                for p in positions(primitive['attributes']['POSITION']):
                    q=[sum(m[r][c]*p[c] for c in range(3))+m[r][3] for r in range(3)]
                    # UE5.8 GLTF::ConvertVec3 is (X,Z,Y); metres become centimetres.
                    points.append((q[0]*100,q[2]*100,q[1]*100))
        for child in n.get('children',[]):visit(child,m)
    for i in d['scenes'][d.get('scene',0)]['nodes']:visit(i,identity)
    lo=[min(p[i] for p in points) for i in range(3)];hi=[max(p[i] for p in points) for i in range(3)]
    return {'origin_cm':[(a+b)/2 for a,b in zip(lo,hi)],'extent_cm':[(b-a)/2 for a,b in zip(lo,hi)],
            'materials':d['materials'],'triangles':sum(triangle_counts)}

def verify_native(row,source):
    expected=source_details(source);sm=ue.load_asset(row['mesh']);assert isinstance(sm,ue.StaticMesh)
    b=sm.get_bounds();origin=[b.origin.x,b.origin.y,b.origin.z];extent=[b.box_extent.x,b.box_extent.y,b.box_extent.z]
    assert max(abs(a-b) for a,b in zip(origin,expected['origin_cm']))<.15,(row['id'],'root/origin shifted',origin,expected['origin_cm'])
    assert max(abs(a-b) for a,b in zip(extent,expected['extent_cm']))<.15,(row['id'],'dimension mismatch',extent,expected['extent_cm'])
    slots=sm.get_editor_property('static_materials')
    assert len(slots)==len(expected['materials']),(row['id'],'material slot count changed')
    source_pbr=[m.get('pbrMetallicRoughness',{}) for m in expected['materials']]
    details=[]
    for slot in slots:
        material=slot.get_editor_property('material_interface');assert material
        assert isinstance(material,ue.MaterialInstanceConstant),(row['id'],'unexpected material type',material.get_class().get_name())
        scalar_names=[str(n) for n in ue.MaterialEditingLibrary.get_scalar_parameter_names(material)]
        vector_names=[str(n) for n in ue.MaterialEditingLibrary.get_vector_parameter_names(material)]
        rough=next(n for n in scalar_names if n.lower()=='roughnessfactor')
        metal=next(n for n in scalar_names if n.lower()=='metallicfactor')
        base=next(n for n in vector_names if n.lower()=='basecolorfactor')
        r=ue.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(material,rough)
        m=ue.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(material,metal)
        c=ue.MaterialEditingLibrary.get_material_instance_vector_parameter_value(material,base)
        rgba=[c.r,c.g,c.b,c.a]
        assert any(abs(r-p.get('roughnessFactor',1))<1e-5 and abs(m-p.get('metallicFactor',1))<1e-5 and
                   max(abs(a-b) for a,b in zip(rgba,p.get('baseColorFactor',[1,1,1,1])))<1e-5 for p in source_pbr), (row['id'],'PBR changed',material.get_name(),r,m,rgba)
        details.append({'material':material.get_path_name(),'roughness':r,'metallic':m,'base_color_linear':rgba,'matches_source_pbr':True})
    uv=sm.get_num_tex_coords(0);assert uv>=1
    fallback=sm.get_num_triangles(0);nanite=sm.get_num_nanite_triangles()
    assert (nanite or fallback)==expected['triangles'],(row['id'],'full native geometry triangle count changed')
    row.update(bounds_origin_cm=origin,bounds_extent_cm=extent,expected_origin_cm=expected['origin_cm'],
        expected_extent_cm=expected['extent_cm'],origin_preserved=True,dimensions_verified=True,
        pbr_values_verified=True,uv_channels=uv,native_triangles=nanite or fallback,
        native_lod0_fallback_triangles=fallback,native_nanite_triangles=nanite,
        source_triangles=expected['triangles'],material_details=details)
    return row

def import_kit(kit_name,include_examples=False):
    kit=ROOT/'Art'/kit_name;dest='/Game/ThreeHearths/Generated/'+kit_name;report_path=kit/'UE_Import_Report.json'
    manifest=json.loads((kit/'artifact-manifest.json').read_text(encoding='utf-8'))
    # Only frozen GLBs are import inputs; acceptance reports may change while running.
    frozen={a['file']:a for a in manifest['artifacts'] if a['file'].endswith('.glb')}
    specs=json.loads((kit/'module-specs.json').read_text(encoding='utf-8'))
    sources=[(m['id'],kit/'modules'/(m['id']+'.glb')) for m in specs['modules']]
    if include_examples:
        layouts=json.loads((kit/'example-layouts.json').read_text(encoding='utf-8'))
        sources.extend(('example__'+e['id'],kit/'examples'/(e['id']+'.glb')) for e in layouts['examples'])
    for _,source in sources:
        f=frozen[source.relative_to(kit).as_posix()]
        assert hashlib.sha256(source.read_bytes()).hexdigest()==f['sha256'] and source.stat().st_size==f['bytes']
    previous={}
    if report_path.exists():
        prior=json.loads(report_path.read_text(encoding='utf-8'))
        previous={r['id']:r for r in prior.get('resume_assets',[])+prior.get('assets',[])}
    report={'status':'running','engine':ue.SystemLibrary.get_engine_version(),'kit':kit_name,
        'scope':'Native art import and preserved roots; no skeletal binding, animation or gameplay activation',
        'source_blender_to_ue_cm':'(x,y,z)m -> (100*x,-100*y,100*z)cm',
        'bake_meshes':True,'recenter_applied':False,'assets':[],'resume_assets':list(previous.values())}
    try:
        for mid,source in sources:
            row=import_one(source,mid,previous,destination_root=dest)
            report['assets'].append(row)
            report_path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
            verify_native(row,source)
            report_path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
            ue.log('['+kit_name+'Import] '+mid+' imported and measured')
        report['status']='passed';report.pop('resume_assets',None)
    except Exception as exc:
        report.update(status='failed',error=repr(exc));raise
    finally:report_path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')

if __name__=='__main__':import_kit('ResidentKit')
