"""Blender MCP: three minimal new components plus one reused SocietyKit wall."""
from pathlib import Path
import hashlib
import importlib.util
import json
import struct
import sys
import bpy
sys.dont_write_bytecode=True
OUT=Path(__file__).resolve().parent;ART=OUT.parent
sp=importlib.util.spec_from_file_location('public_wall_geometry',ART/'ToolKit/tool_geometry.py')
G=importlib.util.module_from_spec(sp);sp.loader.exec_module(G)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def doc(p):
    raw=p.read_bytes();n,k=struct.unpack_from('<II',raw,12);assert k==0x4E4F534A
    return json.loads(raw[20:20+n])
def write(name,data):(OUT/name).write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')
for o in list(bpy.data.objects):bpy.data.objects.remove(o,do_unlink=True)
for c in list(bpy.data.collections):bpy.data.collections.remove(c)
scene=bpy.context.scene;scene.name='PublicWallKit | independent local construction components'
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
scene.render.threads_mode='FIXED';scene.render.threads=3
references=[]
for rel,role in [('SocietyKit/modules/castle_wall_stone_2m.glb','unchanged source wall geometry and stone PBR'),
 ('VillageKit/modules/beam_timber_2m.glb','chestnut beam PBR'),('VillageKit/modules/floor_timber_2m.glb','honey plank and cut-oak PBR'),
 ('VillageKit/modules/carry_stones.glb','stone carry visual'),('VillageKit/modules/carry_planks.glb','plank carry visual'),
 ('SocietyKit/modules/goods_beams_bundle.glb','beam carry visual')]:
    p=ART/rel;references.append({'asset_glb':'../'+rel,'role':role,'sha256':sha(p)})
source=ART/'SocietyKit/modules/castle_wall_stone_2m.glb'
bpy.ops.import_scene.gltf(filepath=str(source));wall=next(o for o in scene.objects if o.type=='MESH')
materials={m.name:m for m in wall.data.materials}
for rel in ('VillageKit/modules/beam_timber_2m.glb','VillageKit/modules/floor_timber_2m.glb'):
    for md in doc(ART/rel)['materials']:
        if md['name'] in materials:continue
        m=bpy.data.materials.new(md['name']);m.use_nodes=True;node=m.node_tree.nodes.get('Principled BSDF');pbr=md['pbrMetallicRoughness']
        node.inputs['Base Color'].default_value=pbr['baseColorFactor'];m.diffuse_color=pbr['baseColorFactor']
        node.inputs['Metallic'].default_value=pbr.get('metallicFactor',1);node.inputs['Roughness'].default_value=pbr.get('roughnessFactor',1)
        materials[md['name']]=m
stone=materials['SK_BlueGrey_Stone'];stone_light=materials['SK_Stone_Edge'];stone_dark=materials['SK_Stone_Shadow']
wood=materials['VK_Chestnut'];plank=materials['VK_Honey_Plank'];cut=materials['VK_Cut_Oak']
modules=[]
def finish(mid,parts,role,inputs,collision,origin='Centre bottom at Z=0; length along +X, front -Y.',reused=False):
    bpy.ops.object.select_all(action='DESELECT')
    for o in parts:o.select_set(True)
    bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();o=bpy.context.view_layer.objects.active
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    scene.cursor.location=(0,0,0);bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    o.name=mid;o.data.name=mid+'_mesh'
    if not reused:G.uv_project(o.data)
    c=bpy.data.collections.new(mid);scene.collection.children.link(c)
    for old in list(o.users_collection):old.objects.unlink(o)
    c.objects.link(o)
    o['asset_id']=mid;o['category']=role;o['npc_portable']=False;o['front_axis']='-Y';o['resource_inputs']=inputs
    o['execution_unit']='one_site_installation';o['runtime_binding_verified']=False;o['installation_anchor_m']=[0.,0.,0.]
    coords=[v.co for v in o.data.vertices];bounds=[[min(v[i] for v in coords) for i in range(3)],[max(v[i] for v in coords) for i in range(3)]]
    size=[bounds[1][i]-bounds[0][i] for i in range(3)];o['measured_size_m']=size;o.data.calc_loop_triangles()
    bpy.ops.export_scene.gltf(filepath=str(OUT/'modules'/(mid+'.glb')),export_format='GLB',use_selection=True,
       export_apply=False,export_yup=True,export_texcoords=True,export_normals=True,export_materials='EXPORT',export_extras=True,
       export_animations=False,export_cameras=False,export_lights=False)
    modules.append({'id':mid,'role':role,'asset_glb':'modules/'+mid+'.glb','measured_size_m':size,'bounds_authoring_m':bounds,
       'origin_convention':origin,'installation_anchor_m':[0,0,0],'npc_handheld':False,'execution_unit':'one_site_installation',
       'resource_inputs':inputs,'recipe_status':'proposed_public_stock_contract_not_applied_to_runtime',
       'runtime_binding':'pending','native_import_status':'not_imported','collision_design':collision,
       'triangles':len(o.data.loop_triangles),'material_names':[m.name for m in o.data.materials],
       'geometry_source':'../SocietyKit/modules/castle_wall_stone_2m.glb' if reused else 'original_minimal_component',
       'source_geometry_reused_unchanged':reused})
    return o
def box_shape(center,size):return {'type':'box','center_authoring_m':center,'size_m':size}
finish('public_wall_stone_2m',[wall],'stone_wall',{'stone':8},[box_shape([0,0,1.2],[2,.4,2.4])],
   'Preserved SocietyKit structural datum at Z=0; geometry Z=.003..2.397. Length +X, front -Y.',True)
work=bpy.data.collections.new('Temporary modeling collection');scene.collection.children.link(work)
G.configure(work,{})
for ix in range(4):
    for iy in range(2):G.box('Rounded foundation stone',(-.75+ix*.5,-.225+iy*.45,.14),(.494,.444,.28),[stone,stone_light,stone_dark][(ix+2*iy)%3],.018)
finish('public_wall_foundation_2m',list(G.PARTS),'foundation',{'stone':4},[box_shape([0,0,.14],[2,.9,.28])])
G.configure(work,{})
for y in (-.14,.14):G.box('Chestnut deck bearer',(0,y,.08),(2,.12,.16),wood,.013)
for i in range(10):G.box('Honey board deck',(-.9+i*.2,0,.19),(.194,.9,.06),plank if i%3 else cut,.009)
finish('public_wall_walkway_2m',list(G.PARTS),'timber_walkway',{'planks':3,'beams':2},[box_shape([0,0,.11],[2,.9,.22])])
G.configure(work,{})
for x in (-.92,.92):G.box('Stout parapet end post',(x,0,.37),(.16,.14,.74),wood,.016)
for z in (.12,.58):G.box('Continuous parapet rail',(0,0,z),(2,.12,.12),wood,.012)
for i in range(6):G.box('Pale between parapet rails',(-.75+i*.3,0,.35),(.24,.075,.42),plank if i%2 else cut,.01)
finish('public_wall_parapet_2m',list(G.PARTS),'timber_parapet',{'planks':2,'beams':1},[box_shape([0,0,.37],[2,.14,.74])])
bpy.data.collections.remove(work)
for c in list(bpy.data.collections):
    if not c.objects and not c.children:bpy.data.collections.remove(c)
for mesh in list(bpy.data.meshes):
    if mesh.users==0:bpy.data.meshes.remove(mesh)
for m in list(bpy.data.materials):
    if m.users==0:bpy.data.materials.remove(m)
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'PublicWallKit.blend'))
resources={
 'stone':{'engine_stock_property':'StoneStock','npc_carry_asset':'../VillageKit/modules/carry_stones.glb','single_delivery_unit':1},
 'planks':{'engine_stock_property':'PlankStock','npc_carry_asset':'../VillageKit/modules/carry_planks.glb','single_delivery_unit':1},
 'beams':{'engine_stock_property':'BeamStock','npc_carry_asset':'../SocietyKit/modules/goods_beams_bundle.glb','single_delivery_unit':1,
          'society_catalog_visual_resource_alias':'timber_beams'}}
placements=[]
for bay,x in enumerate((-2.,0.,2.),1):
    base_id=f'public_wall_b{bay:02d}_foundation';wall_id=f'public_wall_b{bay:02d}_stone';walk_id=f'public_wall_b{bay:02d}_walkway'
    for cid,mid,stage,at,deps in [(base_id,'public_wall_foundation_2m',1,[x,0,0],[]),
        (wall_id,'public_wall_stone_2m',2,[x,0,.28],[base_id]),
        (walk_id,'public_wall_walkway_2m',3,[x,0,2.677],[wall_id]),
        (f'public_wall_b{bay:02d}_parapet_front','public_wall_parapet_2m',4,[x,-.38,2.897],[walk_id]),
        (f'public_wall_b{bay:02d}_parapet_back','public_wall_parapet_2m',4,[x,.38,2.897],[walk_id])]:
        placements.append({'component_id':cid,'component_asset_id':mid,'installation_stage':stage,'position_authoring_m':at,
            'yaw_degrees':0,'scale':[1,1,1],'requires_installed_component_ids':deps})
totals={k:sum(next(m['resource_inputs'] for m in modules if m['id']==p['component_asset_id']).get(k,0) for p in placements) for k in resources}
write('catalog.json',{'schema_version':1,'kit_id':'public_wall_kit_01','status':'offline_art_and_proposed_construction_contract',
 'units':'metres','authoring_axes':'+Z up / -Y front','longitudinal_axis':'+X','grid_m':2,'resources':resources,'modules':modules,
 'npc_transport_rule':'NPCs carry the resource bundles listed above; finished wall modules are installed on site, never handheld.',
 'recipes_applied_to_runtime':False,'native_assets_imported':False})
write('assembly.json',{'schema_version':1,'assembly_id':'first_public_wall_6m','component_count':len(placements),'unique_component_assets':4,
 'nominal_length_m':6,'footprint_m':[6,.9],'highest_geometry_z_m':3.637,'placements':placements,
 'resource_inputs_total':totals,'only_adds_components':True,'whole_wall_glb_created':False,
 'walkway_access_or_navigation_verified':False,'note':'Three straight 2m bays. Access stairs, gates, corners and towers are deliberately outside this first minimal segment.'})
write('recipes.json',{'status':'proposed_not_live','resources':resources,
 'recipes':[{'id':'install_'+m['id'],'resource_inputs':m['resource_inputs'],'output_component_asset_id':m['id'],'output_count':1,
   'requires_site_installation':True,'inventory_deduction_verified':False} for m in modules],
 'example_6m_segment_inputs':totals,'visual_block_count_does_not_define_inventory_quantity':True})
for ref in references:assert sha(OUT/ref['asset_glb'])==ref['sha256']
write('reuse-manifest.json',{'existing_assets_modified':False,'references':references,'reused_wall_geometry_unchanged':True})
write('source-audit.json',{'modeling_interface':'Blender MCP execute_blender_code','blender_version':bpy.app.version_string,
 'source_blend':'PublicWallKit.blend','source_blend_sha256':sha(OUT/'PublicWallKit.blend'),
 'objects':[o.name for o in scene.objects],'mesh_count':len([o for o in scene.objects if o.type=='MESH']),
 'source_contains_four_independent_modules':True,'source_contains_assembled_whole_wall':False,'source_contains_reference_character':False,
 'triangles_by_asset':{m['id']:m['triangles'] for m in modules}})
print('PUBLIC_WALL_KIT_CREATED',len(modules),sum(m['triangles'] for m in modules),totals,flush=True)
