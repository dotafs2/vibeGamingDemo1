"""Read the existing Cropout skeletal mesh for attachment fit measurements."""
from pathlib import Path
import json
import bpy
from mathutils import Vector

OUT=Path(__file__).resolve().parent
REF=OUT.parent.parent/'Saved/ThreeHearths/ResidentKitReference'
for obj in list(bpy.data.objects):bpy.data.objects.remove(obj,do_unlink=True)
bpy.ops.import_scene.fbx(filepath=str(REF/'SKM_Villager.fbx'),automatic_bone_orientation=False)
armatures=[o for o in bpy.context.scene.objects if o.type=='ARMATURE']
meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
report={'source_asset':'/Game/Characters/Meshes/SKM_Villager','source_kind':'existing Cropout fitting reference',
    'source_mesh_not_distributed_in_new_kit':True,'ue_body_relative_yaw_degrees':-90,'bones':[],'meshes':[]}
for arm in armatures:
    for bone in arm.data.bones:
        head=arm.matrix_world@bone.head_local
        tail=arm.matrix_world@bone.tail_local
        report['bones'].append({'name':bone.name,'parent':bone.parent.name if bone.parent else None,
            'head_m':[round(v,6) for v in head],'tail_m':[round(v,6) for v in tail],
            'world_matrix_rows':[[round(v,8) for v in row] for row in arm.matrix_world@bone.matrix_local]})
for obj in meshes:
    points=[obj.matrix_world@v.co for v in obj.data.vertices]
    obj.data.calc_loop_triangles()
    (REF/'body-surface.json').write_text(json.dumps({'source':'Cropout fitting reference only',
        'vertices_m':[[float(v) for v in p] for p in points],
        'triangles':[list(t.vertices) for t in obj.data.loop_triangles]})+'\n',encoding='utf-8')
    groups=[]
    for group in obj.vertex_groups:
        selected=[obj.matrix_world@v.co for v in obj.data.vertices if any(g.group==group.index and g.weight>.5 for g in v.groups)]
        if selected:
            groups.append({'name':group.name,'count':len(selected),'min_m':[round(min(p[i] for p in selected),6) for i in range(3)],
                'max_m':[round(max(p[i] for p in selected),6) for i in range(3)]})
    report['meshes'].append({'name':obj.name,'vertices':len(points),
        'min_m':[round(min(p[i] for p in points),6) for i in range(3)],
        'max_m':[round(max(p[i] for p in points),6) for i in range(3)],'weighted_regions':groups})
(OUT/'attachment-reference.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
bpy.ops.wm.save_as_mainfile(filepath=str(REF/'Cropout_Reference.blend'))
print('REFERENCE_MEASURED',json.dumps(report),flush=True)
