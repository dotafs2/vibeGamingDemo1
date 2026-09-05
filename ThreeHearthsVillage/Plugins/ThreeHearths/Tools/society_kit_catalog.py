"""Build a deterministic, explicitly offline execution catalog for SocietyKit."""
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
KIT=ROOT/'Art/SocietyKit'
INPUTS={
    'wall':{'stone':12,'plaster':2},'battlement_wall':{'stone':16,'plaster':2},
    'gate_frame':{'stone':24,'plaster':4},'gate_leaves':{'planks':10,'iron':2},
    'tower_layer':{'stone':48,'planks':6,'plaster':6},'tower_cap':{'stone':24,'planks':12},
    'buttress':{'stone':8,'plaster':1},'walkway':{'planks':6,'beams':2},
    'banner':{'fabric':2,'wood':1},'market_stall':{'planks':12,'beams':4,'fabric':6},
    'storage_crate':{'planks':4},'storage_barrel':{'planks':6,'iron':1},
    'social_notice_board':{'planks':6,'wood':2},'street_light':{'wood':3,'iron':1},
    'woodworking':{'planks':12,'beams':4,'iron':2},'stoneworking':{'stone':10,'beams':4},
    'tile_firing':{'stone':12,'tiles':20,'plaster':4},'metalworking':{'stone':16,'iron':6},
    'king_identity':{'gold':2},'carpenter_identity':{'fabric':2},'mason_identity':{'fabric':3},
    'blacksmith_identity':{'leather':3},'hammer':{'wood':1,'iron':1},'saw':{'wood':1,'iron':2},
    'chisel':{'wood':1,'iron':1},'trowel':{'wood':1,'iron':1},
}
GOODS={'goods_planks_bundle':('planks',6),'goods_beams_bundle':('beams',4),
       'goods_bricks_tiles_crate':('tiles',6),'goods_paint_pails':('plaster',4)}

def build():
    specs=json.loads((KIT/'module-specs.json').read_text(encoding='utf-8'))
    layouts=json.loads((KIT/'example-layouts.json').read_text(encoding='utf-8'))
    imported=json.loads((KIT/'UE_Import_Report.json').read_text(encoding='utf-8'))
    assert imported['status']=='passed'
    native={row['id']:row for row in imported['assets']}
    modules=[]
    for spec in specs['modules']:
        module=dict(spec);asset=KIT/'modules'/(spec['id']+'.glb');row=native[spec['id']]
        assert hashlib.sha256(asset.read_bytes()).hexdigest()==row['source_sha256']
        module.update(asset_glb=asset.relative_to(KIT).as_posix(),native_mesh=row['mesh'],
            runtime_binding='pending',collision='engine_template_required')
        category=spec['category']
        if category=='goods':
            resource,capacity=GOODS[spec['id']]
            module.update(npc_handheld=True,execution_unit='one_inventory_carry_batch',
                represented_resource=resource,pack_capacity=capacity,
                resource_inputs=None,creates_inventory=False)
        else:
            portable=category in ('wearable','tool')
            module.update(npc_handheld=portable,
                execution_unit='one_manufactured_item' if portable else 'one_site_installation',
                resource_inputs=INPUTS[spec['role']])
        modules.append(module)
    by_id={m['id']:m for m in modules}
    blueprints=[]
    for layout in layouts['examples']:
        costs={};placements=[];ids=set()
        for entry in layout['placements']:
            assert entry['instance_id'] not in ids;ids.add(entry['instance_id'])
            module=by_id[entry['module_id']]
            for resource,amount in (module['resource_inputs'] or {}).items():
                costs[resource]=costs.get(resource,0)+amount
            placements.append({**entry,'execution_unit':module['execution_unit'],
                'resource_inputs':module['resource_inputs'],'native_mesh':module['native_mesh']})
        blueprints.append({'id':layout['id'],'execution_unit':'ordered_site_plan','npc_handheld':False,
            'installation_material_total':dict(sorted(costs.items())),
            'goods_packs_are_inventory_displays_not_free_stock':True,'placements':placements})
    result={'schema_version':1,'kit_id':specs['kit_id'],'status':'offline_catalog_not_live_economy',
        'units':'metres','grid_m':2,'storey_height_m':2.4,
        'resource_balance':'proposed game balance; not applied to any saved world',
        'execution_levels':['inventory item or carry batch','single site installation','multi-installation building plan'],
        'modules':modules,'blueprints':blueprints}
    assert len(modules)==32 and len(blueprints)==2
    (KIT/'catalog.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'status':'passed','modules':len(modules),'blueprints':len(blueprints),
        'placements':[len(b['placements']) for b in blueprints],'live_inventory_mutations':0}))

if __name__=='__main__':build()
