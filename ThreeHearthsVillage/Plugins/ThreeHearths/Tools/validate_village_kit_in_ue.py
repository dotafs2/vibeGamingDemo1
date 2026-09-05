"""Read-only native asset and placement checks in the VillageKit showcase editor.

Open L_VillageKit_Showcase, then execute this file through the editor Python API.
This does not test collision, navigation, construction, or packaged runtime use.
"""
import hashlib
import json
from pathlib import Path
import unreal as ue

ROOT=Path(__file__).resolve().parents[3]
KIT=ROOT/'Art/VillageKit'

def xyz(vector): return [vector.x,vector.y,vector.z]

def main():
    world=ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    assert world and world.get_name()=='L_VillageKit_Showcase', 'Open the showcase outside PIE'
    imported=json.loads((KIT/'UE_Import_Report.json').read_text(encoding='utf-8'))
    source=json.loads((KIT/'model-report.json').read_text(encoding='utf-8'))
    catalog=json.loads((KIT/'catalog.json').read_text(encoding='utf-8'))
    roof_ids={m['id'] for m in catalog['modules'] if m['category'] in ('roof_slope','ridge','canopy')}
    dimensions={m['id']:m['dimensions_m'] for m in source['modules']}
    showcase=json.loads((KIT/'UE_Showcase_Report.json').read_text(encoding='utf-8'))
    actors=ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()
    assert not any(isinstance(actor,ue.HearthVillage) for actor in actors)
    by_label={actor.get_actor_label():actor for actor in actors}
    assert len([a for a in actors if a.get_actor_label().startswith('VillageKit | ') and a.get_actor_label()!='VillageKit | Review floor'])==41
    rows=[]
    for row in imported['assets']:
        assert hashlib.sha256((ROOT/row['source']).read_bytes()).hexdigest()==row['source_sha256'],row['id']
        mesh=ue.load_asset(row['mesh'])
        assert isinstance(mesh,ue.StaticMesh) and mesh.get_num_lods()>0,row['id']
        extent=xyz(mesh.get_bounds().box_extent)
        if row['id'] in dimensions:
            assert all(abs(actual*2-expected*100)<0.02 for actual,expected in zip(extent,dimensions[row['id']])),(row['id'],'metres-to-centimetres')
        tiles=[]
        slots=mesh.get_editor_property('static_materials')
        assert len(slots)==row['material_slots']
        for slot in slots:
            material=slot.get_editor_property('material_interface')
            assert material is not None,row['id']
            if material.get_name().startswith(('VK_Terracotta_','VK_SlateBlue_')):
                rough=ue.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(material,'RoughnessFactor')
                metal=ue.MaterialEditingLibrary.get_material_instance_scalar_parameter_value(material,'MetallicFactor')
                assert 0.339<=rough<=0.401 and metal==0,(row['id'],'ceramic material')
                tiles.append({'material':material.get_name(),'roughness':rough,'metallic':metal})
        if row['id'] in roof_ids or row['id'].startswith('example__'):
            assert len(tiles)==4,(row['id'],'missing tile variants')
        placement=next(p for p in showcase['placements'] if p['id']==row['id'])
        actor=by_label['VillageKit | '+row['id']]
        rotation=actor.get_actor_rotation()
        assert abs(rotation.pitch)<0.001 and abs(rotation.roll)<0.001 and abs(rotation.yaw-placement['yaw_degrees'])<0.001,(row['id'],'rotation')
        assert all(abs(a-b)<0.01 for a,b in zip(xyz(actor.get_actor_location()),placement['location_cm']))
        _,world_extent=actor.get_actor_bounds(False)
        assert abs(world_extent.z-extent[2])<0.02,(row['id'],'vertical extent')
        rows.append({'id':row['id'],'source_hash_current':True,'material_slots':len(slots),
            'dimensions_cm':[v*2 for v in extent], 'upright_placement':True,'tile_materials':tiles})
    assert len(rows)==len(showcase['placements'])==41
    report={'status':'passed','engine':ue.SystemLibrary.get_engine_version(),
        'scope':'native renderable assets, source hashes, module scale, roof parameters and upright showcase placement',
        'native_meshes':len(rows),'module_scale_checks':len(dimensions),
        'npc_runtime_execution_verified':False,'collision_navigation_verified':False,'assets':rows}
    (KIT/'UE_Validation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k!='assets'}))

if __name__=='__main__': main()
