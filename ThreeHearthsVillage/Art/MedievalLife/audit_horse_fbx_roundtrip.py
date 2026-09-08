"""Cold reimport all delivered FBX files and check real skins and action tracks."""
import json
from pathlib import Path
import bpy
from mathutils import Vector

OUT=Path(__file__).resolve().parent/'Rigged'
expected={'root','body','neck','head','tail_base','tail_tip'}
for leg in ('FL','FR','HL','HR'):
    for part in ('upper','lower','foot'):expected.add(leg+'_'+part)
report={'status':'running','files':[],'scope':'Blender cold FBX reimport, not Unreal runtime evidence'}
for coat in ('bay','grey'):
    for clip,end in [('idle',61),('walk',37)]:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        path=OUT/f'horse_{coat}_{clip}.fbx'
        bpy.ops.import_scene.fbx(filepath=str(path),use_anim=True,anim_offset=0)
        rigs=[o for o in bpy.data.objects if o.type=='ARMATURE']
        meshes=[o for o in bpy.data.objects if o.type=='MESH']
        assert len(rigs)==1 and len(meshes)==3,(path.name,len(rigs),len(meshes))
        rig=rigs[0];names={b.name for b in rig.data.bones}
        assert names==expected,(path.name,names^expected)
        assert rig.animation_data and rig.animation_data.action,path.name
        action=rig.animation_data.action
        assert tuple(round(v) for v in action.frame_range)==(1,end),(path.name,list(action.frame_range))
        assert bpy.context.scene.render.fps==30,(path.name,bpy.context.scene.render.fps)
        mesh_rows=[];positions=[]
        for ob in meshes:
            assert any(m.type=='ARMATURE' and m.object==rig for m in ob.modifiers),(path.name,ob.name)
            assert ob.data.uv_layers,(path.name,ob.name,'missing UV')
            assert all(s.material for s in ob.material_slots),(path.name,ob.name,'material')
            for v in ob.data.vertices:
                total=sum(g.weight for g in v.groups)
                assert abs(total-1)<.001,(path.name,ob.name,v.index,total)
                assert all(ob.vertex_groups[g.group].name in expected for g in v.groups),path.name
                positions.append(ob.matrix_world@v.co)
            mesh_rows.append({'mesh':ob.name,'vertices':len(ob.data.vertices),'materials':len(ob.material_slots),'uv_channels':len(ob.data.uv_layers)})
        size=[max(p[axis] for p in positions)-min(p[axis] for p in positions) for axis in range(3)]
        assert .85<size[0]<1.1 and 3<size[1]<3.7 and 2.5<size[2]<2.9,(path.name,'scale/axis',size)
        # The action really changes evaluated vertices, not just an empty track.
        finish=next(o for o in meshes if 'finish' in o.name)
        captures=[]
        for frame in (1,1+(end-1)//4):
            bpy.context.scene.frame_set(frame)
            evaluated=finish.evaluated_get(bpy.context.evaluated_depsgraph_get());mesh=evaluated.to_mesh()
            captures.append([evaluated.matrix_world@v.co for v in mesh.vertices])
            evaluated.to_mesh_clear()
        motion=max((a-b).length for a,b in zip(*captures))
        assert motion>(.005 if clip=='idle' else .05),(path.name,'no deformed motion',motion)
        report['files'].append({'file':path.name,'bones':len(names),'fps':30,'frames':end,'size_m':size,'max_sample_motion_m':motion,'meshes':mesh_rows})
report['status']='passed_cold_fbx_skins_actions_and_scale'
(OUT/'fbx_roundtrip_audit.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('HORSE_FBX_AUDIT',report['status'],flush=True)
