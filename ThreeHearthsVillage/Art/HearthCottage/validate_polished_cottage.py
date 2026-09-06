"""Independent byte-stream comparison of old and polished cottage GLBs. No Blender."""
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

OUT=Path(__file__).resolve().parent
SOURCE='HearthCottage_SharedUV'
TARGET=SOURCE+'_Polished'

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def rounded(v):return tuple(round(x,5) for x in v)
def subtract(a,b):return tuple(x-y for x,y in zip(a,b))
def cross(a,b):return (a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0])
def length(v):return math.sqrt(sum(x*x for x in v))
def dot(a,b):return sum(x*y for x,y in zip(a,b))

def read(path):
    raw=path.read_bytes();assert raw[:4]==b'glTF'
    n=struct.unpack_from('<I',raw,12)[0];d=json.loads(raw[20:20+n]);blob=raw[28+n:]
    assert len(d['buffers'])==1 and 'uri' not in d['buffers'][0]
    assert not d.get('images') and not d.get('textures')
    def accessor(i):
        a=d['accessors'][i];v=d['bufferViews'][a['bufferView']];assert not a.get('sparse')
        fmt='<'+{5121:'B',5123:'H',5125:'I',5126:'f'}[a['componentType']]*{'SCALAR':1,'VEC2':2,'VEC3':3,'VEC4':4}[a['type']]
        stride=v.get('byteStride',struct.calcsize(fmt));offset=v.get('byteOffset',0)+a.get('byteOffset',0)
        return [struct.unpack_from(fmt,blob,offset+j*stride) for j in range(a['count'])]
    result={}
    for node in d['nodes']:
        if 'mesh' not in node:continue
        name=node['name'];mesh=d['meshes'][node['mesh']]
        geometry=Counter();uv_counter=Counter();shading=Counter();smooth=0;flat=0;triangles=0;points=[]
        for p in mesh['primitives']:
            assert p.get('mode',4)==4
            assert all(k in p['attributes'] for k in ('POSITION','NORMAL','TEXCOORD_0'))
            assert 'TEXCOORD_1' not in p['attributes']
            pos=accessor(p['attributes']['POSITION']);norm=accessor(p['attributes']['NORMAL']);uv=accessor(p['attributes']['TEXCOORD_0'])
            index=[v[0] for v in accessor(p['indices'])];material=d['materials'][p['material']]['name']
            for normal in norm:assert abs(length(normal)-1)<2e-5,(name,'non-unit normal',normal)
            for coordinate in uv:assert all(math.isfinite(v) for v in coordinate)
            for j in range(0,len(index),3):
                ids=index[j:j+3];assert len(ids)==3
                vertices=[pos[i] for i in ids];face=cross(subtract(vertices[1],vertices[0]),subtract(vertices[2],vertices[0]));area=length(face)
                assert area>1e-10,(name,'degenerate triangle',area)
                face=tuple(v/area for v in face)
                tris=tuple(sorted((rounded(pos[i]),rounded(uv[i])) for i in ids))
                geometry[(material,tris)]+=1
                shading[(material,tuple(sorted((rounded(pos[i]),rounded(uv[i]),rounded(norm[i])) for i in ids)))]+=1
                uv_counter[tuple(sorted(rounded(uv[i]) for i in ids))]+=1
                is_flat=all(dot(norm[i],face)>.99999 for i in ids)
                flat+=is_flat;smooth+=not is_flat;triangles+=1;points.extend(vertices)
        result[name]={'geometry':geometry,'uv':uv_counter,'shading':shading,
          'triangles':triangles,'smooth_triangles':smooth,'flat_triangles':flat,
          'bounds':{'min':[min(p[i] for p in points) for i in range(3)],'max':[max(p[i] for p in points) for i in range(3)]},
          'node_transform':{k:node[k] for k in ('matrix','translation','rotation','scale') if k in node}}
    return d,result

def main():
    old,original=read(OUT/(SOURCE+'.glb'));new,variant=read(OUT/(TARGET+'.glb'))
    assert original.keys()==variant.keys();roof=[]
    for name,a in original.items():
        b=variant[name];is_roof=name.startswith('Roof |')
        assert a['geometry']==b['geometry'],(name,'vertex, material assignment or UV topology changed')
        assert a['uv']==b['uv'] and a['bounds']==b['bounds'] and a['node_transform']==b['node_transform']
        if is_roof:
            assert a['smooth_triangles']==0,(name,'original not flat')
            assert b['smooth_triangles']/b['triangles']>.35,(name,'curved normal smoothing missing')
            assert b['flat_triangles']>0,(name,'ends no longer hard')
            assert max(b['uv'].values())>1,(name,'shared UV reuse missing')
            roof.append({'name':name,'triangles':b['triangles'],'original_smooth_triangles':0,
              'polished_smooth_triangles':b['smooth_triangles'],'polished_hard_face_triangles':b['flat_triangles'],
              'shared_uv_triangles':len(b['uv']),'maximum_repeated_uv_triangle':max(b['uv'].values()),
              'geometry_transform_and_shared_uv_identical':True})
        else:assert a['shading']==b['shading'],(name,'non-roof normals changed')
    old_m={m['name']:m for m in old['materials']};new_m={m['name']:m for m in new['materials']}
    assert old_m.keys()==new_m.keys();materials=[]
    for name,a in old_m.items():
        b=new_m[name]
        if name.startswith('Roof |'):
            pa=a['pbrMetallicRoughness'];pb=b['pbrMetallicRoughness']
            assert abs(pa['roughnessFactor']-.78)<1e-5
            assert .33999<=pb['roughnessFactor']<=.40001 and pb.get('metallicFactor',1)==0
            assert a.get('extensions')==b.get('extensions'),'IOR or other non-target controls changed'
            materials.append({'name':name,'before':pa,'after':pb})
        else:assert a==b,(name,'non-roof material changed')
    repair=json.loads((OUT/(TARGET+'_repair-report.json')).read_text())
    for path,digest in repair['original_files'].items():assert sha(Path(path))==digest,'Original overwritten'
    report={'status':'passed','source':SOURCE,'variant':TARGET,'mesh_count':len(variant),'material_count':len(new_m),
      'triangles':sum(a['triangles'] for a in variant.values()),'roof':roof,'roof_materials':materials,
      'geometry_topology_transforms_and_uv_identical':True,'non_roof_normals_and_materials_identical':True,
      'unit_normals':True,'degenerate_triangles':0,'external_textures':False,'original_source_files_preserved':True,
      'scope':'Independent GLB byte-stream comparison, not a UE import or runtime test'}
    if '--renders' in sys.argv:
        entries=[]
        for view,states in [('house',['original','polished']),('roof',['original','finish_only','polished'])]:
            cameras=[];lights=[];geometry=[]
            for state in states:
                p=OUT/(TARGET+'_'+view+'_'+state+'_render.json');meta=json.loads(p.read_text())
                image=OUT/meta['file'];assert sha(image)==meta['sha256']
                assert list(struct.unpack_from('>II',image.read_bytes(),16))==meta['resolution']
                assert meta['state']==state and meta['view']==view and meta['samples']==40 and meta['seed']==17
                cameras.append((meta['camera_matrix'],meta['ortho_scale'],meta['resolution']))
                lights.append(meta['lighting_fingerprint']);geometry.append(meta['geometry_and_uv_fingerprint'])
                entries.append({'view':view,'state':state,'file':image.name,'sha256':meta['sha256']})
            assert all(c==cameras[0] for c in cameras),'Comparison camera changed'
            assert len(set(lights))==1 and len(set(geometry))==1,'Comparison lighting/geometry changed'
        report['same_light_same_camera_comparisons_verified']=True
        report['render_outputs']=entries
    (OUT/(TARGET+'_validation.json')).write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k not in ('roof','roof_materials')}))

if __name__=='__main__':main()
