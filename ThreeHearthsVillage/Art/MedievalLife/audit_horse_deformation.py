"""Evaluate actual deformed meshes, beyond the rig author's IK target checks."""
import json
from pathlib import Path
import bpy
from mathutils import Vector

OUT=Path(__file__).resolve().parent/'Rigged'
bpy.ops.wm.open_mainfile(filepath=str(OUT/'Medieval_Horses_Rigged.blend'))
scene=bpy.context.scene
phases={'HL':0,'FL':.25,'HR':.5,'FR':.75}
report={'status':'running','horses':[],'scope':'actual evaluated source meshes with bevel and skin; native engine import is a separate gate'}
for coat in ('bay','grey'):
    rig=bpy.data.objects['Horse_'+coat+'_Rig']
    finish=next(o for o in rig.children if o.type=='MESH' and o.get('layer')=='finish')
    walks=[]
    for clip,frames in [('Idle',61),('Walk',37)]:
        rig.animation_data.action=bpy.data.actions['Horse_'+coat+'_'+clip]
        samples=[];first_positions=None;last_positions=None
        for frame in range(1,frames+1):
            scene.frame_set(frame)
            graph=bpy.context.evaluated_depsgraph_get()
            evaluated=finish.evaluated_get(graph);mesh=evaluated.to_mesh()
            to_local=rig.matrix_world.inverted()@evaluated.matrix_world
            points={leg:[] for leg in phases}
            all_positions=[]
            for vertex in mesh.vertices:
                pos=to_local@vertex.co;all_positions.append(pos)
                weights=[g for g in vertex.groups if g.weight>.99]
                for weight in weights:
                    name=finish.vertex_groups[weight.group].name
                    if name.endswith('_foot'):
                        points[name[:2]].append(pos)
            if frame==1:first_positions=all_positions
            if frame==frames:last_positions=all_positions
            for leg,positions in points.items():
                assert positions,(coat,clip,leg,'no hoof vertices')
                phase=((frame-1)/(frames-1)+phases[leg])%1
                stance=clip=='Idle' or phase<.62
                centre=sum(positions,Vector())/len(positions)
                samples.append({'frame':frame,'leg':leg,'phase':phase,'stance':stance,
                                'min_z_m':min(p.z for p in positions),'centre_y_m':centre.y})
            evaluated.to_mesh_clear()
        stance_error=max(abs(s['min_z_m']-.015) for s in samples if s['stance'])
        closure=max((a-b).length for a,b in zip(first_positions,last_positions))
        minimum=min(s['min_z_m'] for s in samples)
        slides=[]
        for leg in phases:
            sequence=[s for s in samples if s['leg']==leg]
            for a,b in zip(sequence,sequence[1:]):
                if a['stance'] and b['stance'] and b['phase']>a['phase']:
                    expected=.6/30 if clip=='Walk' else 0
                    slides.append(abs(b['centre_y_m']-a['centre_y_m']-expected))
        slip=max(slides,default=0)
        assert minimum>=-.001,(coat,clip,'ground penetration',minimum)
        assert stance_error<.002,(coat,clip,'stance mesh floor',stance_error)
        assert closure<.0002,(coat,clip,'loop closure',closure)
        assert slip<.001,(coat,clip,'actual planted hoof velocity mismatch',slip)
        walks.append({'clip':clip,'frames':frames,'minimum_hoof_z_m':minimum,
                      'max_stance_floor_error_m':stance_error,'max_loop_closure_m':closure,
                      'max_planted_foot_slip_m_per_frame':slip,'samples':samples})
    report['horses'].append({'coat':coat,'clips':walks})
report['status']='passed_deformed_hoof_contacts_and_loops'
(OUT/'deformation_audit.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('HORSE_DEFORMATION_AUDIT',report['status'],flush=True)
