"""Read-only inventory of existing production assets; never duplicate or modify them."""
from pathlib import Path
import importlib.util
import json
import sys
sys.dont_write_bytecode=True
OUT=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('wood_independent_glb',OUT.parent/'ToolKit/validate_tool_kit.py')
V=importlib.util.module_from_spec(spec);spec.loader.exec_module(V)
REFERENCES=[
 ('input_logs','VillageKit','carry_logs','primary input-stock visual','wood'),
 ('output_planks','SocietyKit','goods_planks_bundle','primary output-stock visual','planks'),
 ('output_beams','SocietyKit','goods_beams_bundle','primary output-stock visual','timber_beams'),
 ('carpenter_bench','VillageKit','workbench_carpenter','existing facility visual',None),
 ('saw','ToolKit','tool_saw','existing work tool',None),
 ('axe','ToolKit','tool_axe','existing hewing tool',None),
 ('portable_planks','VillageKit','carry_planks','alternative smaller carry pack','planks'),
 ('installed_beam','VillageKit','beam_timber_2m','installed structural counterpart, not a stock pack','timber_beams')]
items=[]
for rid,kit,mid,role,resource in REFERENCES:
    path=OUT.parent/kit/'modules'/(mid+'.glb');result=V.inspect_asset(path);doc,bounds,_=V.decode(path)
    items.append({'reference_id':rid,'source_kit':kit,'asset_id':mid,'asset_glb':'../'+kit+'/modules/'+path.name,
      'role':role,'resource_id_suggestion':resource,'bytes':result['bytes'],'sha256':result['sha256'],'triangles':result['triangles'],
      'bounds_authoring_m':bounds,'measured_size_m':[round(bounds[1][i]-bounds[0][i],6) for i in range(3)],
      'origin_policy':'Preserve source origin; do not recenter existing assets.',
      'floor_placement_z_offset_m':-bounds[0][2],'portable_pbr_verified':True,'materials':[m['name'] for m in doc['materials']]})
report={'schema_version':1,'kit_id':'wood_production_kit_01','reuse_count':len(items),'assets_copied':False,
 'existing_assets_modified':False,'visual_geometry_defines_recipe_quantities':False,'references':items,
 'selection_note':'Reuse existing logs, plank and beam stock, bench and tools. Only two missing static intermediate states are new.',
 'runtime_integration_verified':False}
(OUT/'reuse-manifest.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
layout={'schema_version':1,'units':'metres','authoring_axes':'+Z up / -Y front',
 'runtime_integration_verified':False,'stock_quantities_not_defined_by_this_layout':True,
 'bench_work_surface_z_m':.86,'bench_work_zone_m':{'x':[-.66,.12],'y':[-.35,-.12]},
 'branches':[
  {'id':'logs_to_planks','input_reference':'input_logs','facility_reference':'carpenter_bench','tool_reference':'saw',
   'intermediate_id':'wip_log_to_planks','output_reference':'output_planks','bench_world_m':[0,.93,0],
   'intermediate_bench_local_m':[-.27,-.23,.86],'input_world_m':[-1.57,.93,0],'output_world_m':[1.59,.93,0]},
  {'id':'logs_to_beams','input_reference':'input_logs','facility_reference':'carpenter_bench','tool_reference':'axe',
   'intermediate_id':'wip_log_to_beam','output_reference':'output_beams','bench_world_m':[0,-.93,0],
   'intermediate_bench_local_m':[-.27,-.23,.86],'input_world_m':[-1.57,-.93,0],'output_world_m':[1.75,-.93,0]}],
 'workpiece_visibility_contract':'Show one workpiece only while a real production job owns/reserves its input; remove or swap it when the job completes or cancels. This file implements no job or inventory code.',
 'quantity_note':'Existing pack lengths differ; visuals represent resource lots. Recipe amounts, yields and conservation remain owned by the production simulation.'}
(OUT/'production-layout.json').write_text(json.dumps(layout,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'reused':len(items),'reference_triangles':sum(i['triangles'] for i in items),'source_assets_copied':False}))
