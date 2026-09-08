"""Read-only cold UE audit of MedievalLife layers, materials, UVs and axes."""
import json
from pathlib import Path
import unreal as ue

ROOT=Path(__file__).resolve().parents[3]
ART=ROOT/'Art/MedievalLife'
index=json.loads((ROOT/'Content/ThreeHearths/Data/MedievalLifeCatalog.json').read_text(encoding='utf-8'))
source=json.loads((ART/'catalog.json').read_text(encoding='utf-8'))
subsystem=ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
if subsystem is None:
    # Commandlets do not instantiate this UI subsystem. Its read-only mesh
    # getters have no instance state and can be called on the class default.
    subsystem=ue.get_default_object(ue.StaticMeshEditorSubsystem)
report={'status':'running','audit':'cold native reload; no assets modified','layers':[],'modules':[]}
try:
    assert len(index['assets'])==sum(len(m['layers']) for m in source['modules'])
    all_bounds={}
    for row in index['assets']:
        mesh=ue.load_asset(row['mesh']);assert isinstance(mesh,ue.StaticMesh),row['id']
        assert not mesh.get_editor_property('nanite_settings').get_editor_property('enabled'),row['id']
        uvs=subsystem.get_num_uv_channels(mesh,0);verts=subsystem.get_number_verts(mesh,0)
        assert uvs>=1 and verts>0,row['id']
        slots=mesh.get_editor_property('static_materials');assert slots,row['id']
        for slot in slots:
            mat=slot.get_editor_property('material_interface');assert mat,row['id']
            if row['module_id']=='royal_banner':
                overrides=mat.get_editor_property('base_property_overrides')
                assert overrides.get_editor_property('override_two_sided')
                assert overrides.get_editor_property('two_sided')
                assert overrides.get_editor_property('override_blend_mode')
                assert overrides.get_editor_property('blend_mode')==ue.BlendMode.BLEND_MASKED
        b=mesh.get_bounds()
        lo=[b.origin.x-b.box_extent.x,b.origin.y-b.box_extent.y,b.origin.z-b.box_extent.z]
        hi=[b.origin.x+b.box_extent.x,b.origin.y+b.box_extent.y,b.origin.z+b.box_extent.z]
        all_bounds.setdefault(row['module_id'],[]).append((lo,hi))
        report['layers'].append({'id':row['id'],'mesh':row['mesh'],'uv_channels':uvs,
                                  'vertices':verts,'material_slots':len(slots),'nanite':False})
    for spec in source['modules']:
        bounds=all_bounds[spec['id']]
        actual_lo=[min(b[0][i] for b in bounds) for i in range(3)]
        actual_hi=[max(b[1][i] for b in bounds) for i in range(3)]
        s=spec['bounds_m'];expected_lo=[s['min'][0]*100,-s['max'][1]*100,s['min'][2]*100]
        expected_hi=[s['max'][0]*100,-s['min'][1]*100,s['max'][2]*100]
        error=max(abs(a-b) for a,b in zip(actual_lo+actual_hi,expected_lo+expected_hi))
        assert error<.5,(spec['id'],error,actual_lo,expected_lo)
        report['modules'].append({'id':spec['id'],'bounds_axis_error_cm':error})
    report['status']='passed'
    report['layer_count']=len(report['layers']);report['module_count']=len(report['modules'])
    report['material_slots']=sum(r['material_slots'] for r in report['layers'])
    report['limitations']=['No runtime NPC/transport behavior tested here.','No animation or rig verification.']
except Exception as exc:
    report.update(status='failed',error=str(exc));raise
finally:
    (ART/'UE_ColdAudit.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    ue.log('[MedievalLifeAudit] '+report['status'])
