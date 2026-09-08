"""Write the local runtime manifest only after actual source/native acceptance."""
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
ART=ROOT/'Art/MedievalLife/PeopleRigged'
source=json.loads((ART/'people_deformation_and_fbx_audit.json').read_text(encoding='utf-8'))
imported=json.loads((ART/'UE_People_Rigged_Import_Report.json').read_text(encoding='utf-8'))
native=json.loads((ART/'UE_People_Rigged_Cold_Audit.json').read_text(encoding='utf-8'))
assert source['status']=='passed_actual_skin_contacts_loops_knees_and_cold_fbx'
assert imported['status']==native['status']=='passed'
assert len(native['people'])==3 and sum(len(p['meshes']) for p in native['people'])==20
for row in imported['people']:
    n=next(p for p in native['people'] if p['id']==row['id'])
    assert row['source_sha256']==n['source_sha256']
    for clip,path in row['source_files'].items():
        digest=hashlib.sha256((ROOT/path).read_bytes()).hexdigest()
        s=next(s for s in source['fbx_files'] if s['file']==Path(path).name)
        assert digest==row['source_sha256'][clip]==s['sha256']
    lo=[min(m['bounds_ue_cm']['min_cm'][i] for m in n['meshes']) for i in range(3)]
    hi=[max(m['bounds_ue_cm']['max_cm'][i] for m in n['meshes']) for i in range(3)]
    size=[hi[i]-lo[i] for i in range(3)]
    expected=next(s['size_m'] for s in source['fbx_files'] if s['file']==row['id']+'_idle.fbx')
    assert max(abs(size[i]-expected[i]*100) for i in range(3))<.05,(row['id'],'native/source dimensions',size,expected)
    row['runtime_walk_speed_cmps']=90
    row['native_size_cm']=size
manifest={'schema_version':1,'people':imported['people'],'walk_speed_cmps':90,
          'source_to_unreal_matrix':imported['source_to_unreal_matrix'],
          'validation':'20 layers / 3 skeletons / 6 actions, cold native load and actual source deformation passed',
          'limitations':['Idle and Walk only; no combat, seated driving or terrain foot IK.']}
target=ROOT/'Content/ThreeHearths/Data/MedievalLifePeopleRigCatalog.json'
target.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print('PEOPLE_RUNTIME_CATALOG',len(manifest['people']),sum(len(p['meshes']) for p in manifest['people']))
