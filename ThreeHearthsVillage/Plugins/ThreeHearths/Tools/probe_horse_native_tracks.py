"""Read actual reference bone/track sets from the imported layered horse."""
import json
from pathlib import Path
import unreal as ue
root=Path(__file__).resolve().parents[3]
catalog=json.loads((root/'Content/ThreeHearths/Data/MedievalLifeHorseRigCatalog.json').read_text(encoding='utf-8'))
rows=[]
for horse in catalog['horses']:
    row={'id':horse['id'],'meshes':[],'animations':[]}
    for item in horse['meshes']:
        mesh=ue.load_asset(item['mesh'])
        component=ue.SkeletalMeshComponent()
        component.set_skeletal_mesh_asset(mesh)
        row['meshes'].append({'layer':item['layer'],'bones':[str(component.get_bone_name(i)) for i in range(component.get_num_bones())]})
    for clip,item in horse['animations'].items():
        anim=ue.load_asset(item['asset'])
        row['animations'].append({'clip':clip,'tracks':[str(n) for n in ue.AnimationLibrary.get_animation_track_names(anim)]})
    rows.append(row)
(root/'Art/MedievalLife/Rigged/UE_NativeTrackProbe.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
