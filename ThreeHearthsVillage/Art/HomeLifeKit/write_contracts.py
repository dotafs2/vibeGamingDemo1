"""Stable module and interaction contracts for HomeLifeKit; no stock is created."""
from pathlib import Path
import json
OUT=Path(__file__).resolve().parent

def point(pid,kind,position,yaw=0,**extra):
    return dict(id=pid,kind=kind,position_m=position,yaw_degrees=yaw,**extra)

def approach(pid,pos,yaw):
    return point(pid,'approach_ground',pos,yaw,clearance_radius_m=.45,height_m=1.56)

def module(mid,category,size,points,**extra):
    return dict(id=mid,category=category,nominal_size_m=size,origin='ground/support-surface centre',
                interaction_points=points,creates_inventory=False,**extra)

modules=[
 module('bed_single_1_1x2m','installed_bed',[1.1,2.0,.87],[
  approach('approach_right',[1.06,0,0],-90),
  point('sleep_0','lying_support',[0,0,.505],0,head_direction='+Y',support_length_m=1.82,support_width_m=1.0),
  point('pillow_0','head_support',[0,.63,.60])],occupants=1),
 module('bed_double_2x2m','installed_bed',[2.0,2.0,.87],[
  approach('approach_left',[-1.52,0,0],90),approach('approach_right',[1.52,0,0],-90),
  point('sleep_0','lying_support',[-.47,0,.505],head_direction='+Y',support_length_m=1.82,support_width_m=.91),
  point('sleep_1','lying_support',[.47,0,.505],head_direction='+Y',support_length_m=1.82,support_width_m=.91)],occupants=2),
 module('chair_oak_wide','installed_seat',[.98,.9,1.08],[
  approach('approach_side',[1.02,0,0],-90),point('seat_0','sitting_contact',[0,-.01,.45]),
  point('pelvis_0','pelvis_pose_candidate',[0,-.01,.67],body_bone='pelvis')],occupants=1,seat_width_m=.9,seat_depth_m=.72),
 module('bench_backed_1_8m','installed_seat',[1.88,.9,1.08],[
  approach('approach_left',[-1.46,0,0],90),approach('approach_right',[1.46,0,0],-90),
  point('seat_0','sitting_contact',[-.45,-.01,.45]),point('seat_1','sitting_contact',[.45,-.01,.45]),
  point('pelvis_0','pelvis_pose_candidate',[-.45,-.01,.67],body_bone='pelvis'),
  point('pelvis_1','pelvis_pose_candidate',[.45,-.01,.67],body_bone='pelvis')],occupants=2,seat_width_per_person_m=.9,seat_depth_m=.72),
 module('table_dining_1_2m','installed_table',[1.2,.8,.78],[
  approach('service_left',[-1.14,0,0],90),point('surface','place_surface',[0,0,.78],usable_size_m=[1.1,.7])]),
 module('table_communal_2_6m','installed_table',[2.6,1.1,.78],[
  approach('service_left',[-1.82,0,0],90),approach('service_right',[1.82,0,0],-90),
  point('surface','place_surface',[0,0,.78],usable_size_m=[2.5,1.0])]),
 module('food_tray_bread','inventory_display',[.46,.32,.18],[
  point('grip_left','carry_grip',[-.22,0,.025]),point('grip_right','carry_grip',[.22,0,.025])],
  visual_resource_ids=['bread','prepared_food'],authoritative_inventory_required=True,visual_quantity_is_not_stock=True),
 module('basket_berries','inventory_display',[.49,.49,.43],[
  point('grip_left','carry_grip',[-.233,0,.35]),point('grip_right','carry_grip',[.233,0,.35])],
  visual_resource_ids=['berries'],authoritative_inventory_required=True,visual_quantity_is_not_stock=True),
 module('basket_empty','carry_container',[.49,.49,.43],[
  point('grip_left','carry_grip',[-.233,0,.35]),point('grip_right','carry_grip',[.233,0,.35])],
  requires_owned_container=True,capacity_not_implemented=True),
 module('grain_chest','installed_storage',[1.0,.58,.68],[
  approach('access_front',[0,-.84,0],180),point('transfer','inventory_transfer',[0,-.31,.48])],
  authoritative_inventory_required=True,initial_stock=0),
 module('firewood_stack','inventory_display',[.85,.52,.53],[
  approach('access_front',[0,-.79,0],180),point('pickup','inventory_transfer',[0,-.27,.32])],
  visual_resource_ids=['wood'],authoritative_inventory_required=True,visual_quantity_is_not_stock=True),
 module('fence_low_2m','installed_boundary',[2.0,.16,.76],[
  approach('repair_front',[0,-.61,0],180)],snap_endpoints_m=[[-1,0,0],[1,0,0]])
]
spec={
 'schema_version':1,'kit_id':'home_life_kit_01','units':'metres','up_axis':'+Z','front_axis':'-Y',
 'source_resident_height_m':1.557142,'measured_hip_width_m':.803452,'measured_hip_depth_m':.673079,
 'installed_objects_require_construction_or_owned_placement':True,
 'rendered_food_and_materials_never_create_inventory':True,
 'interaction_points_are_contract_candidates':True,'animation_and_navigation_verified':False,
 'conventions':{
  'floor':'Installed furniture uses floor centre at Z=0. Carryable visuals use bottom support-plane centre at Z=0.',
  'rotation':'Positive yaw rotates about +Z; yaw=0 faces -Y. All point positions/yaws are local to the asset.',
  'sitting':'sitting_contact is the furniture surface; pelvis_pose_candidate adds .22m based on the measured pelvis-bottom offset. Neither is a tested animation root.',
  'sleeping':'lying_support is a mattress support plane with head toward +Y; use a future lying animation and capsule policy, not standing pose rotation.',
  'approach':'Ground points reserve a .45m radius / 1.56m height standing region. Runtime navigation, entry manoeuvres and reservations remain to be integrated.',
  'stock':'A food tray, full basket or woodpile only visualizes an existing inventory record. Spawning its mesh must never grant resources.'},
 'modules':modules}
(OUT/'module-specs.json').write_text(json.dumps(spec,indent=2)+'\n',encoding='utf-8')

def place(i,mid,p,yaw=0,**extra):
    return dict(instance_id=i,module_id=mid,position_m=p,yaw_degrees=yaw,**extra)

cabin=[place('bed','bed_single_1_1x2m',[-1.25,.72,0]),
 place('chest','grain_chest',[1.35,1.40,0]),
 place('table','table_dining_1_2m',[.95,-.65,0]),
 place('chair','chair_oak_wide',[.95,-1.52,0],180),
 place('breakfast','food_tray_bread',[.95,-.65,.78],support_instance='table'),
 place('empty_basket','basket_empty',[-1.47,-1.35,0])]
meal=[]
for side in (-1,1):
    tag='left' if side==-1 else 'right';x=side*2.1
    meal.append(place(tag+'_table','table_communal_2_6m',[x,0,0],disabled_interactions=['service_left' if side==-1 else 'service_right']))
    meal.append(place(tag+'_bench_front','bench_backed_1_8m',[x,-1.27,0],180))
    meal.append(place(tag+'_bench_back','bench_backed_1_8m',[x,1.27,0]))
    meal.append(place(tag+'_chair','chair_oak_wide',[side*3.97,0,0],90 if side==-1 else -90))
    for j in (-1,1):
        meal.append(place(tag+'_tray_'+str(j),'food_tray_bread',[x+j*.67,-.21,.78],support_instance=tag+'_table'))
    meal.append(place(tag+'_fruit','basket_berries',[x,.18,.78],support_instance=tag+'_table'))
meal.extend([place('fuel','firewood_stack',[-4.35,2.40,0]),
 place('fence_left','fence_low_2m',[-3,2.90,0]),place('fence_right','fence_low_2m',[3,2.90,0])])
layouts={'schema_version':1,'examples':[
 {'id':'cabin_living_4x4m','interior_bounds_m':[[-2,-2,0],[2,2,2.4]],'placements':cabin,
  'nominal_central_aisle_width_m':1.05,'entry_m':[-.15,-1.52,0],
  'notes':'One bed, one dining seat, compact storage. Front and right walls omitted for a cutaway review.'},
 {'id':'common_meal_10_seats','interior_bounds_m':[[-4.9,-2.8,0],[4.9,3.0,2.4]],'placements':meal,
  'seat_capacity':10,'entry_m':[0,-2.25,0],
  'notes':'Ten seats: four two-person benches plus two wide chairs. Food meshes are visual examples of existing stock only.'}
]}
(OUT/'example-layouts.json').write_text(json.dumps(layouts,indent=2)+'\n',encoding='utf-8')
print('HOME_LIFE_CONTRACTS',len(modules),len(layouts['examples']))
