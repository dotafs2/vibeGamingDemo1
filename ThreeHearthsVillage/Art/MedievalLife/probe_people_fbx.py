import bpy,json
from pathlib import Path
OUT=Path(__file__).resolve().parent/'PeopleRigged'
result={}
for mode in ('source','fbx'):
    if mode=='source':
        bpy.ops.wm.open_mainfile(filepath=str(OUT/'Medieval_Service_Residents_Rigged.blend'))
        rig=bpy.data.objects['gatekeeper_Rig'];rig.animation_data.action=bpy.data.actions['gatekeeper_Idle']
    else:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.fbx(filepath=str(OUT/'gatekeeper_idle.fbx'),anim_offset=0)
        rig=next(o for o in bpy.data.objects if o.type=='ARMATURE')
    rows=[]
    for frame in (1,16,31,46,61):
        bpy.context.scene.frame_set(frame)
        rows.append({'frame':frame,'rig_matrix':[list(r) for r in rig.matrix_world],
                     'bones':{k:{'head':list(rig.pose.bones[k].head),'connected':rig.data.bones[k].use_connect,'basis_translation':list(rig.pose.bones[k].matrix_basis.translation),'rest_head':list(rig.data.bones[k].head_local)} for k in ('root','pelvis','thigh_l','shin_l','foot_l')}})
    result[mode]=rows
(OUT/'fbx_probe.json').write_text(json.dumps(result,indent=2))
print(json.dumps(result))
