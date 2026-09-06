"""Read-only independent audit of existing modular stage-4 sample-house assets."""
from pathlib import Path
import collections
import hashlib
import importlib.util
import json
import math
import sys
sys.dont_write_bytecode=True
OUT=Path(__file__).resolve().parent;PROJECT=OUT.parent.parent;KIT=OUT.parent/'VillageKit'
sp=importlib.util.spec_from_file_location('stage4_glb_reader',OUT.parent/'ToolKit/validate_tool_kit.py')
V=importlib.util.module_from_spec(sp);sp.loader.exec_module(V)
STAGES={
 'foundation_stone_2m':1,'floor_timber_2m':1,
 'post_timber_2_4m':2,'beam_timber_2m':2,
 'wall_plaster_2m':3,'wall_door_plaster_2m':3,'wall_window_plaster_2m':3,'gable_plaster_4m':3,
 'roof_slope_terracotta_2m':4,'roof_ridge_terracotta_2m':4}
STAGE_NAMES={1:'foundation_and_floor',2:'frame',3:'wall_panels_and_gables',4:'roof'}

def box(center,size):return {'type':'box','center_authoring_m':center,'size_m':size}

def forward_ray_hits(triangles,x,z):
    hits=[]
    for a,b,c in triangles:
        den=(b[2]-c[2])*(a[0]-c[0])+(c[0]-b[0])*(a[2]-c[2])
        if abs(den)<1e-12:continue
        u=((b[2]-c[2])*(x-c[0])+(c[0]-b[0])*(z-c[2]))/den
        v=((c[2]-a[2])*(x-c[0])+(a[0]-c[0])*(z-c[2]))/den;w=1-u-v
        if min(u,v,w)>=-1e-7:hits.append(u*a[1]+v*b[1]+w*c[1])
    return hits

COLLISION={
 'foundation_stone_2m':{'suggestion':[box([0,0,-.12],[2,2,.24])],'note':'Top surface is the structural datum at Z=0; foundation extends downward.'},
 'floor_timber_2m':{'suggestion':[box([0,0,.08],[2,2,.16])],'note':'One simple floor box bridges cosmetic plank gaps; finished floor is Z=.16.'},
 'post_timber_2_4m':{'suggestion':[box([0,0,1.2],[.18,.18,2.4])],'note':'One box per post; do not combine all posts into a house-sized convex hull.'},
 'beam_timber_2m':{'suggestion':[box([0,0,.10],[1.82,.18,.20])],'note':'Place the underside at storey datum +2.2; rotate around Z for Y-axis members.'},
 'wall_plaster_2m':{'suggestion':[box([0,0,1.20],[1.82,.16,2.08])],'note':'Panel fills the space between separate posts and beams.'},
 'wall_door_plaster_2m':{'suggestion':[box([-.69,0,1.2],[.44,.22,2.08]),box([.69,0,1.2],[.44,.22,2.08]),box([0,0,2.15],[.94,.22,.18])],
   'clear_door_opening_m':{'x':[-.47,.47],'z':[.16,2.06]},'note':'Keep the doorway empty. A single panel-sized box would incorrectly seal the entrance.'},
 'wall_window_plaster_2m':{'suggestion':[box([0,0,1.2],[1.82,.22,2.08])],'note':'Closed painted-glass window may block as one wall box; use separate edge/glass shapes if traversal/opening is later required.'},
 'gable_plaster_4m':{'suggestion':[{'type':'convex_triangular_prism','vertices_authoring_m':[[x,y,z] for y in (-.08,.08) for x,z in ((-2,0),(2,0),(0,1.2))]}],
   'note':'Triangular wall closure, not a whole-roof or whole-house bounding box.'},
 'roof_slope_terracotta_2m':{'suggestion':[{'type':'oriented_box','center_authoring_m':[1.0671,0,.5584],
   'size_m':[math.hypot(2.18,1.2),2,.085],'rotation_euler_degrees':[0,math.degrees(math.atan2(1.2,2.18)),0]}],
   'note':'One simple shape for the underlying roof bed. Do not generate collision separately for all decorative tiles.'},
 'roof_ridge_terracotta_2m':{'suggestion':[],'note':'Decorative ridge caps can inherit roof support and remain nonblocking.'},
 'frame_timber_2m':{'suggestion':[],'note':'Standalone one-bay prototype only. For a multi-bay house, reuse separate posts/beams once per grid vertex/edge; never use one convex hull sealing its interior.'}}

def audit():
    specs=json.loads((KIT/'module-specs.json').read_text(encoding='utf-8'));modules={m['id']:m for m in specs['modules']}
    imports=json.loads((KIT/'UE_Import_Report.json').read_text(encoding='utf-8'));imported={a['id']:a for a in imports['assets']}
    layout=next(e for e in json.loads((KIT/'example-layouts.json').read_text(encoding='utf-8'))['examples'] if e['id']=='cottage_terracotta')
    rows=[]
    for mid in [*STAGES,'frame_timber_2m']:
        path=KIT/'modules'/(mid+'.glb');a=V.inspect_asset(path,require_smooth=mid.startswith('roof_'))
        doc,bounds,tris=V.decode(path);node=doc['nodes'][0]
        assert len(doc['nodes'])==1 and len(doc['meshes'])==1
        assert not any(k in node for k in ('matrix','translation','rotation','scale'))
        size=[bounds[1][k]-bounds[0][k] for k in range(3)]
        category=modules[mid]['category']
        if category=='foundation':assert size[0]<=2.001 and size[1]<=2.001 and size[2]<.25 and bounds[1][2]<.001
        if category=='floor':assert size[0]<=2.001 and size[1]<=2.001 and size[2]<.17
        if category=='post':assert size[0]<.19 and size[1]<.19 and abs(size[2]-2.4)<.001
        if category=='beam':assert size[0]<1.83 and size[1]<.19 and size[2]<.21
        if category in ('wall','wall_door','wall_window'):assert size[0]<1.83 and size[1]<.4 and size[2]<2.2
        if category=='roof_slope':assert size[0]<2.4 and size[1]<2.01 and size[2]<1.5 and bounds[0][0]>-.1
        if category=='ridge':assert size[0]<.31 and size[1]<2.01 and size[2]<.3
        if mid=='wall_door_plaster_2m':
            assert all(not forward_ray_hits(tris,0,z) for z in (.6,1.4,1.95))
            for z in (.6,1.4,1.95):
                assert not any(all(abs(v-c)<=s/2 for v,c,s in zip((0,0,z),shape['center_authoring_m'],shape['size_m'])) for shape in COLLISION[mid]['suggestion'])
        if mid=='wall_plaster_2m':assert forward_ray_hits(tris,0,1.2)
        report=imported[mid];native_package=report['mesh'].split('.')[0]
        assert native_package.startswith('/Game/')
        native=PROJECT/'Content'/(native_package.removeprefix('/Game/')+'.uasset')
        assert native.is_file(),('missing native component',mid,str(native))
        assert report['source_sha256']==a['sha256'],('native import record is stale',mid)
        origin=specs['conventions'].get(category,specs['conventions'].get('wall') if category.startswith('wall') else '')
        rows.append({'component_asset_id':mid,'category':category,'selected_for_minimal_house':mid in STAGES,
          'installation_stage':STAGES.get(mid),'source_glb':'../VillageKit/modules/'+path.name,'source_sha256':a['sha256'],
          'generated_mesh_object_path':report['mesh'],'native_uasset_path':native.relative_to(PROJECT).as_posix(),
          'native_file_exists':True,'native_import_record_matches_source':True,
          'native_settings_inspected_in_running_ue':False,'import_record_collision_state':report.get('collision'),
          'measured_size_m':[round(x,6) for x in size],'bounds_authoring_m':bounds,
          'origin_convention':origin,'triangles':a['triangles'],'single_local_mesh_node':True,
          'materials':[m['name'] for m in doc['materials']],'uv0':a['uv0'],'unit_normals':a['unit_normals'],
          'collision_design':COLLISION[mid]})
    plan=[]
    def coord(value):
        cm=round(value*100);return ('m' if cm<0 else 'p')+str(abs(cm)).zfill(3)
    for p in layout['placements']:
        mid=p['module_id']
        if mid not in STAGES:continue
        x,y,z=p['position_m'];yaw=p['yaw_degrees']
        cid='s4_'+mid+'_x'+coord(x)+'_y'+coord(y)+'_z'+coord(z)+'_r'+str(yaw).zfill(3)
        plan.append({'component_id':cid,'component_asset_id':mid,'stage':STAGES[mid],
          'position_authoring_m':[x,y,z],'yaw_degrees':yaw,'scale':[1,1,1]})
    assert len(plan)==45 and len({p['component_id'] for p in plan})==45
    assert all(p['component_asset_id']!='frame_timber_2m' for p in plan)
    assert len({tuple(p['position_authoring_m']) for p in plan if p['component_asset_id']=='post_timber_2_4m'})==9
    assert len({(tuple(p['position_authoring_m']),p['yaw_degrees']) for p in plan if p['component_asset_id']=='beam_timber_2m'})==12
    counts=collections.Counter(p['stage'] for p in plan);assert dict(counts)=={1:8,2:21,3:10,4:6}
    catalog={'schema_version':1,'audit_id':'stage4_sample_house_modularity','new_geometry_required':False,'new_glb_files_created':0,
      'units':'metres','authoring_axes':'+Z up / -Y front','glb_to_authoring_coordinates':'(x,-z,y)',
      'asset_root_note':'Existing art sources are Art/VillageKit; native imports are Content/ThreeHearths/Generated/VillageKit.',
      'components':rows,'explicitly_excluded_whole_house_assets':['Art/HearthCottage/*.glb','Art/VillageKit/examples/*.glb'],
      'runtime_stage_system_or_collision_modified':False}
    assembly={'schema_version':1,'assembly_id':'s4_cottage_4x4_terracotta','source_layout':'../VillageKit/example-layouts.json#cottage_terracotta',
      'footprint_grid_m':[4,4],'floor_structural_datum_m':0,'finished_floor_z_m':.16,'frame_top_z_m':2.4,
      'component_count':len(plan),'unique_component_assets':len(STAGES),'placements':plan,
      'stages':[{'stage':s,'name':STAGE_NAMES[s],'adds_components':counts[s],
         'cumulative_components':sum(v for k,v in counts.items() if k<=s),
         'only_add_component_ids':[p['component_id'] for p in plan if p['stage']==s]} for s in range(1,5)],
      'retains_previous_component_instances':True,'stage_changes_replace_house_mesh':False,
      'uses_house_mesh_or_material_swap':False,'runtime_implementation_verified':False,
      'optional_details_excluded':['door_oak','shutter_sage','porch_steps_stone_2m','table_communal','bench_timber'],
      'central_post_note':'This reuses the existing canonical 4x4 frame layout, including its central supporting post; it is not a clear-span redesign.'}
    (OUT/'component-catalog.json').write_text(json.dumps(catalog,indent=2)+'\n',encoding='utf-8')
    (OUT/'stage4-assembly.json').write_text(json.dumps(assembly,indent=2)+'\n',encoding='utf-8')
    selected={r['component_asset_id']:r for r in rows if r['selected_for_minimal_house']}
    report={'status':'passed','new_geometry_required':False,'selected_component_asset_count':10,'prototype_frame_also_audited':True,
      'generated_native_files_present_and_import_hashes_match':len(rows),'single_local_mesh_glbs_verified':len(rows),
      'independent_structural_parts_verified':True,'whole_house_examples_excluded':True,
      'assembly_component_count':45,'per_stage_additions':dict(counts),
      'assembly_triangles':sum(selected[p['component_asset_id']]['triangles'] for p in plan),
      'unique_selected_asset_triangles':sum(r['triangles'] for r in selected.values()),
      'duplicate_posts_or_beams':False,'collision_suggestions_only':True,'ue_runtime_behavior_verified':False,'preview_review_verified':False}
    report['exported_door_opening_and_proposed_collision_clearance_verified']=True
    if (OUT/'visual-review.json').exists():
        review=json.loads((OUT/'visual-review.json').read_text(encoding='utf-8'))
        for img in review['images']:assert hashlib.sha256((OUT/img['file']).read_bytes()).hexdigest()==img['sha256']
        report['preview_review_verified']=review['four_cumulative_component_stages_reviewed']
    (OUT/'validation.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    files=[p for p in OUT.rglob('*') if p.is_file() and p.suffix in ('.py','.json','.md','.png') and p.name!='artifact-manifest.json']
    (OUT/'artifact-manifest.json').write_text(json.dumps({'artifacts':[{'file':p.relative_to(OUT).as_posix(),'bytes':p.stat().st_size,
      'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(files)]},indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report))

if __name__=='__main__':audit()
