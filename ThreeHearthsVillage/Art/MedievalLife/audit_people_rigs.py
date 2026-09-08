"""Validate saved deformed human meshes and six cold FBX deliveries in Blender."""
import hashlib
import json
from pathlib import Path
import bpy
from mathutils import Vector

OUT=Path(__file__).resolve().parent/'PeopleRigged'
REPORT=OUT/'people_deformation_and_fbx_audit.json'
EXPECTED={'root','pelvis','spine','neck','head'}
for side in ('l','r'):
    EXPECTED.update(part+'_'+side for part in ('thigh','shin','foot','upper_arm','forearm','hand'))
PEOPLE={'gatekeeper':7,'royal_guard':9,'carter':4}
report={'status':'running','scope':'Actual source mesh deformation and cold FBX import. Native engine/runtime are separate gates.',
        'source_clips':[],'fbx_files':[]}

def capture(rig,meshes):
    graph=bpy.context.evaluated_depsgraph_get();points=[];boots={'l':[],'r':[]}
    for ob in meshes:
        evaluated=ob.evaluated_get(graph);mesh=evaluated.to_mesh()
        matrix=rig.matrix_world.inverted()@evaluated.matrix_world
        for vertex in mesh.vertices:
            p=matrix@vertex.co;points.append(p)
            for group in vertex.groups:
                name=ob.vertex_groups[group.group].name
                if name in ('foot_l','foot_r') and group.weight>.99:boots[name[-1]].append(p)
        evaluated.to_mesh_clear()
    return points,boots

def audit_motion(rig,meshes,identity,clip,end):
    scene=bpy.context.scene;samples=[];first=last=None
    for frame in range(1,end+1):
        scene.frame_set(frame);points,boots=capture(rig,meshes)
        if frame==1:first=points
        if frame==end:last=points
        for side,positions in boots.items():
            assert positions,(identity,clip,'no boot geometry',side)
            phase=((frame-1)/(end-1)+(0 if side=='l' else .5))%1
            centre=sum(positions,Vector())/len(positions)
            hip=rig.pose.bones['thigh_'+side].head;knee=rig.pose.bones['shin_'+side].head;ankle=rig.pose.bones['foot_'+side].head
            axis=(ankle-hip).normalized();bend=knee-hip-axis*(knee-hip).dot(axis)
            # Forward is -Y. Audit the resulting actual skeleton, not author IK targets.
            assert bend.y<.0005,(identity,clip,frame,side,'backwards knee',bend.y)
            samples.append({'frame':frame,'side':side,'phase':phase,'stance':clip=='Idle' or phase<.62,
                            'min_z':min(p.z for p in positions),'y':centre.y})
    closure=max((a-b).length for a,b in zip(first,last))
    floor_error=max(abs(s['min_z']-.002) for s in samples if s['stance'])
    minimum=min(s['min_z'] for s in samples);slips=[]
    for side in ('l','r'):
        sequence=[s for s in samples if s['side']==side]
        for a,b in zip(sequence,sequence[1:]):
            if a['stance'] and b['stance'] and b['phase']>a['phase']:
                slips.append(abs(b['y']-a['y']-(.9/30 if clip=='Walk' else 0)))
    slip=max(slips,default=0)
    assert minimum>=-.001,(identity,clip,'boot penetration',minimum)
    assert floor_error<.002,(identity,clip,'planted boot floor',floor_error,[(s['frame'],s['side'],s['min_z']) for s in samples if s['stance'] and abs(s['min_z']-.002)>.002][:12])
    assert closure<.0003,(identity,clip,'all mesh loop closure',closure)
    assert slip<.001,(identity,clip,'planted boot sliding',slip)
    return {'id':identity,'clip':clip,'frames':end,'minimum_boot_z_m':minimum,'max_floor_error_m':floor_error,
            'max_all_mesh_loop_error_m':closure,'max_planted_boot_slip_m_per_frame':slip,'samples':samples}

try:
    bpy.ops.wm.open_mainfile(filepath=str(OUT/'Medieval_Service_Residents_Rigged.blend'))
    for identity,count in PEOPLE.items():
        rig=bpy.data.objects[identity+'_Rig'];meshes=[ob for ob in rig.children if ob.type=='MESH']
        assert len(meshes)==count,(identity,len(meshes),count)
        assert set(rig.data.bones.keys())==EXPECTED,identity
        for clip,end in (('Idle',61),('Walk',31)):
            rig.animation_data.action=bpy.data.actions[identity+'_'+clip]
            report['source_clips'].append(audit_motion(rig,meshes,identity,clip,end))
    for identity,count in PEOPLE.items():
        for clip,end in (('Idle',61),('Walk',31)):
            path=OUT/(identity+'_'+clip.lower()+'.fbx')
            bpy.ops.wm.read_factory_settings(use_empty=True)
            bpy.ops.import_scene.fbx(filepath=str(path),use_anim=True,anim_offset=0)
            rigs=[ob for ob in bpy.data.objects if ob.type=='ARMATURE'];meshes=[ob for ob in bpy.data.objects if ob.type=='MESH']
            assert len(rigs)==1 and len(meshes)==count,(path.name,len(rigs),len(meshes))
            rig=rigs[0];assert set(rig.data.bones.keys())==EXPECTED,path.name
            # Blender's FBX importer auto-connects matching parent tails, even
            # with force_connect_children=False (import_fbx.py child_connect).
            # This suppresses valid animated translations. All authored bones
            # are unconnected; restore that explicit source contract for QA.
            auto_connected=[b.name for b in rig.data.bones if b.use_connect]
            bpy.context.view_layer.objects.active=rig;rig.select_set(True)
            bpy.ops.object.mode_set(mode='EDIT')
            for bone in rig.data.edit_bones:bone.use_connect=False
            bpy.ops.object.mode_set(mode='OBJECT')
            assert rig.animation_data and rig.animation_data.action,path.name
            assert tuple(round(n) for n in rig.animation_data.action.frame_range)==(1,end),path.name
            assert bpy.context.scene.render.fps==30,path.name
            positions=[];mesh_rows=[]
            for ob in meshes:
                assert any(m.type=='ARMATURE' and m.object==rig for m in ob.modifiers),(path.name,ob.name)
                assert ob.data.uv_layers and all(s.material for s in ob.material_slots),(path.name,ob.name,'UV/material')
                for v in ob.data.vertices:
                    assert abs(sum(g.weight for g in v.groups)-1)<.001,(path.name,ob.name,'weights',v.index)
                    assert all(ob.vertex_groups[g.group].name in EXPECTED for g in v.groups),path.name
                    positions.append(ob.matrix_world@v.co)
                mesh_rows.append({'name':ob.name,'vertices':len(ob.data.vertices),'material_slots':len(ob.material_slots),'uv_channels':len(ob.data.uv_layers)})
            size=[max(p[i] for p in positions)-min(p[i] for p in positions) for i in range(3)]
            assert .7<size[0]<1.5 and .3<size[1]<1 and 1.8<size[2]<3,(path.name,'scale',size)
            motion=audit_motion(rig,meshes,identity,clip,end)
            report['fbx_files'].append({'file':path.name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
                                        'bones':len(EXPECTED),'size_m':size,'meshes':mesh_rows,'motion':motion,
                                        'blender_import_auto_connection_normalized':auto_connected})
    report['status']='passed_actual_skin_contacts_loops_knees_and_cold_fbx'
except Exception as exc:
    report['status']='failed';report['error']=str(exc)
    raise
finally:
    REPORT.write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('PEOPLE_RIG_AUDIT',report['status'],report.get('error',''),flush=True)
