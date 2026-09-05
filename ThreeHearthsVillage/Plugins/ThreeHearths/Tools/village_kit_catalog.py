"""Build the art catalog and compile an NPC's design into planned construction jobs.

This is an offline integration contract, not an executor for the current game.
Material quantities are draft balance values; new resources are explicitly gated.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re

PROJECT = Path(__file__).resolve().parents[3]
KIT = PROJECT / 'Art/VillageKit'

RESOURCES = {
    'wood': {'label': '木材', 'carry_asset': 'carry_logs', 'pack_capacity': 6, 'current_inventory': True},
    'stone': {'label': '石材', 'carry_asset': 'carry_stones', 'pack_capacity': 6, 'current_inventory': True},
    'planks': {'label': '木板', 'carry_asset': 'carry_planks', 'pack_capacity': 6, 'current_inventory': False},
    'tiles': {'label': '屋瓦', 'carry_asset': 'carry_tiles', 'pack_capacity': 6, 'current_inventory': False},
    'plaster': {'label': '灰泥', 'carry_asset': 'carry_plaster', 'pack_capacity': 4, 'current_inventory': False},
}

def inputs_for(module):
    kind, family = module['category'], module.get('material_family')
    if kind == 'carry': return {}
    if kind == 'foundation': return {'stone': 6}
    if kind in ('floor', 'floor_opening'): return {'wood': 2, 'planks': 4}
    if kind == 'frame': return {'wood': 12}
    if kind in ('post', 'beam'): return {'wood': 2}
    if kind in ('wall', 'wall_door', 'wall_window', 'gable'):
        amount = 4 if kind in ('wall', 'gable') else 3
        return {'wood': 1, {'plaster':'plaster', 'timber':'planks', 'stone':'stone'}[family]: amount}
    if kind in ('roof_slope', 'ridge', 'canopy'):
        return {'wood': 2, 'tiles': 6 if kind == 'roof_slope' else 3}
    if kind == 'steps': return {'stone': 4}
    if kind == 'stairs': return {'wood': 6, 'planks': 8}
    if kind in ('fixture', 'railing'): return {'planks': 2}
    if kind == 'porch': return {'wood': 2}
    if kind == 'facility': return {'wood': 4, 'planks': 4}
    raise ValueError('No construction recipe for category: ' + kind)

def make_catalog(kit=KIT):
    specs = json.loads((kit/'module-specs.json').read_text(encoding='utf-8-sig'))
    modules=[]
    for spec in specs['modules']:
        portable=spec['category']=='carry'
        modules.append(dict(spec, asset_glb='modules/'+spec['id']+'.glb',
            npc_handheld=portable,
            execution_unit='material_delivery' if portable else 'one_site_installation',
            resource_inputs=inputs_for(spec),
            runtime_binding='pending', collision='engine_template_required',
            navigation='not_inferred_from_visual_mesh'))
    return {'schema_version':1, 'kit_id':'three_hearths_village_kit_01',
        'units':specs['units'], 'front_axis':specs['front_axis'], 'up_axis':specs['up_axis'],
        'grid_m':specs['grid_m'], 'floor_to_floor_m':specs['floor_to_floor_m'],
        'status':'art_assets_and_offline_construction_contract',
        'resource_balance':'draft_not_applied_to_live_inventory',
        'resources':RESOURCES, 'conventions':specs['conventions'],
        'design_choices':{'blueprint':[e['id'] for e in specs['examples']],
            'wall_material':['plaster','timber','stone'], 'roof_material':['terracotta','slateblue']},
        'blueprints':specs['examples'], 'modules':modules}

def compile_design(catalog, layouts, design, *, site_id, resident_id):
    allowed=catalog['design_choices']
    if set(design) != set(allowed):
        raise ValueError('Design requires only blueprint, wall_material and roof_material')
    if any(not isinstance(value,str) or not re.fullmatch(r'[A-Za-z0-9_.-]{1,128}',value) for value in (site_id,resident_id)):
        raise ValueError('Persistent site/resident IDs must be bounded opaque identifiers')
    for key, options in allowed.items():
        if design[key] not in options: raise ValueError('Unsupported design choice: '+key)
    modules={m['id']:m for m in catalog['modules']}
    examples=layouts['examples'] if isinstance(layouts,dict) else layouts
    layout=next(e for e in examples if e['id']==design['blueprint'])
    payload=json.dumps({'design':design,'site':site_id,'resident':resident_id},sort_keys=True)
    plan_id='house_'+hashlib.sha256(payload.encode()).hexdigest()[:20]
    placements=[]
    jobs=[]
    total=Counter()
    used_ids=set()
    order={'foundation':0,'floor':10,'floor_opening':10,'steps':10,
        'post':20,'porch':20,'frame':20,'beam':30,'stairs':40,
        'wall':50,'wall_door':50,'wall_window':50,'gable':60,
        'roof_slope':70,'canopy':70,'ridge':75,'fixture':80,'railing':80,'facility':90}
    assembly=sorted(layout['placements'],key=lambda p:(p['level'],order[modules[p['module_id']]['category']]))
    for index, placement in enumerate(assembly):
        module=modules[placement['module_id']]
        category=module['category']
        family=design['wall_material'] if category in ('wall','wall_door','wall_window','gable') else (
            design['roof_material'] if category in ('roof_slope','ridge','canopy') else None)
        if family:
            candidates=[m for m in modules.values() if m['category']==category and m.get('material_family')==family]
            if len(candidates)!=1: raise ValueError('Missing/ambiguous material variant: '+category+'/'+family)
            module=candidates[0]
        if module['npc_handheld']: raise ValueError('Carry packs are not permanent house geometry')
        instance=placement['instance_id']
        if instance in used_ids: raise ValueError('Duplicate installation ID: '+instance)
        used_ids.add(instance)
        placements.append(dict(placement,module_id=module['id']))
        job_id=plan_id+'/'+instance
        deliveries=[]
        for resource, count in module['resource_inputs'].items():
            total[resource]+=count
            capacity=catalog['resources'][resource]['pack_capacity']
            for batch, offset in enumerate(range(0,count,capacity)):
                deliveries.append({'operation_id':job_id+'/deliver/'+resource+'/'+str(batch),
                    'resource':resource,'quantity':min(capacity,count-offset),
                    'carry_asset':catalog['resources'][resource]['carry_asset'],
                    'state':'planned', 'inventory_effect':'debit_once_on_withdraw_credit_site_once_on_arrival'})
        # Structural order puts stairs before upper floors and roof coverings
        # after gables. Serial dependencies are conservative until an executor
        # can reserve and validate safe parallel work.
        jobs.append({'operation_id':job_id,'module_id':module['id'],'state':'planned',
            'site_id':site_id,'resident_id':resident_id,
            'depends_on':[] if index==0 else [jobs[-1]['operation_id']],
            'deliveries':deliveries,'completion':'install_after_all_deliveries_and_work',
            'placement':placements[-1]})
    required=[r for r in total if not catalog['resources'][r]['current_inventory']]
    return {'schema_version':1,'plan_id':plan_id,'design':design,
        'site_id':site_id,'resident_id':resident_id,'status':'planned_only',
        'live_execution_ready':False,
        'runtime_requirements':['persistent_site_and_resident_ids','legal_plot_and_route_checks',
            'task_reservations_and_save_resume','collision_and_door_templates','NPC_executor_binding'],
        'new_inventory_resources_required':sorted(required),
        'resource_totals':dict(sorted(total.items())), 'placements':placements,'jobs':jobs}

def write_catalog_and_examples(kit=KIT):
    catalog=make_catalog(kit)
    (kit/'catalog.json').write_text(json.dumps(catalog,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    layouts=json.loads((kit/'example-layouts.json').read_text(encoding='utf-8-sig'))
    plans=[]
    for index, blueprint in enumerate(catalog['blueprints']):
        design={'blueprint':blueprint['id'],'wall_material':blueprint['wall'],'roof_material':blueprint['roof']}
        plans.append(compile_design(catalog,layouts,design,site_id=f'example_plot_{index+1}',resident_id=f'example_resident_{index+1}'))
    (kit/'construction-plans.json').write_text(json.dumps({'schema_version':1,'examples':plans},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'modules':len(catalog['modules']),'example_plans':len(plans),
        'installation_counts':[len(p['jobs']) for p in plans]}))

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kit',type=Path,default=KIT)
    write_catalog_and_examples(parser.parse_args().kit)
