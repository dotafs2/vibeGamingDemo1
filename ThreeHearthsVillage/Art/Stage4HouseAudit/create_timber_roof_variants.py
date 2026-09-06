"""Run through Blender MCP: reuse roof meshes/UVs with VillageKit timber PBR."""
from pathlib import Path
import hashlib
import json
import struct
import sys
import bpy
sys.dont_write_bytecode=True
OUT=Path(__file__).resolve().parent;KIT=OUT.parent/'VillageKit'
def glb_doc(path):
    raw=path.read_bytes();size,kind=struct.unpack_from('<II',raw,12)
    assert kind==0x4E4F534A
    return json.loads(raw[20:20+size])
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

for o in list(bpy.data.objects):bpy.data.objects.remove(o,do_unlink=True)
for c in list(bpy.data.collections):bpy.data.collections.remove(c)
for mesh in list(bpy.data.meshes):
    if mesh.users==0:bpy.data.meshes.remove(mesh)
for m in list(bpy.data.materials):
    if m.users==0:bpy.data.materials.remove(m)
scene=bpy.context.scene;scene.name='Timber roof variants | original local datums'
scene.unit_settings.system='METRIC';scene.unit_settings.scale_length=1
scene.render.threads_mode='FIXED';scene.render.threads=3
beam=KIT/'modules/beam_timber_2m.glb';beam_hash=sha(beam)
beam_pbr=glb_doc(beam)['materials'][0]['pbrMetallicRoughness']
wood=[]
for i,(gain,rough) in enumerate(((1,.48),(1.14,.51),(.88,.53),(1.28,.50)),1):
    m=bpy.data.materials.new('VK_Timber_Shingle_'+str(i));m.use_nodes=True
    node=m.node_tree.nodes.get('Principled BSDF')
    color=[min(1,c*gain) for c in beam_pbr['baseColorFactor'][:3]]+[1]
    node.inputs['Base Color'].default_value=color
    node.inputs['Metallic'].default_value=0
    node.inputs['Roughness'].default_value=rough
    m.diffuse_color=color;wood.append(m)

old_specs=json.loads((KIT/'module-specs.json').read_text(encoding='utf-8'))
old_modules={m['id']:m for m in old_specs['modules']};modules=[];source_rows=[]
for part in ('slope','ridge'):
    source_id='roof_'+part+'_terracotta_2m';mid='roof_'+part+'_timber_2m'
    source=KIT/'modules'/(source_id+'.glb');source_hash=sha(source)
    before=set(scene.objects);bpy.ops.import_scene.gltf(filepath=str(source))
    imported=[o for o in scene.objects if o not in before and o.type=='MESH'];assert len(imported)==1
    o=imported[0];bpy.ops.object.select_all(action='DESELECT');o.select_set(True);bpy.context.view_layer.objects.active=o
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
    o.name=mid;o.data.name=mid+'_mesh'
    for slot in o.material_slots:
        if slot.material.name.startswith('VK_Terracotta_'):
            idx=int(slot.material.name.split('_')[2].split('.')[0])-1;slot.material=wood[idx]
    o['asset_id']=mid;o['material_family']='timber';o['geometry_source_asset_id']=source_id
    o['source_geometry_reused']=True;o['native_import_verified']=False
    collection=bpy.data.collections.new(mid);scene.collection.children.link(collection)
    for c in list(o.users_collection):c.objects.unlink(o)
    collection.objects.link(o)
    bpy.ops.export_scene.gltf(filepath=str(KIT/'modules'/(mid+'.glb')),export_format='GLB',use_selection=True,
        export_apply=False,export_yup=True,export_texcoords=True,export_normals=True,export_materials='EXPORT',
        export_extras=True,export_animations=False,export_cameras=False,export_lights=False)
    assert sha(source)==source_hash
    m=dict(old_modules[source_id]);m.update({'id':mid,'material_family':'timber','source_geometry_asset_id':source_id,
        'source_geometry_sha256':source_hash,'asset_glb':'../VillageKit/modules/'+mid+'.glb',
        'origin_convention':old_specs['conventions'][m['category']],
        'surface_description':'Rounded overlapping timber shingles; same low-poly silhouette and UVs as the existing roof modules.',
        'requires_native_import':True,'inventory_or_recipe_binding_verified':False,
        'resource_inputs':{'planks':1},'runtime_binding':'aligned_with_stage4_runtime','runtime_contract_confirmed_by_parent_task':True})
    modules.append(m)
    o.data.calc_loop_triangles();source_rows.append({'asset_id':mid,'source_id':source_id,'source_sha256':source_hash,
        'mesh_objects':1,'triangles':len(o.data.loop_triangles),'uv_layers':list(o.data.uv_layers.keys()),
        'materials':[s.material.name for s in o.material_slots]})
assert sha(beam)==beam_hash
# Only the two deliverable meshes and plain Principled materials remain in source.
for m in list(bpy.data.materials):
    if m.users==0:bpy.data.materials.remove(m)
for mesh in list(bpy.data.meshes):
    if mesh.users==0:bpy.data.meshes.remove(mesh)
bpy.context.preferences.filepaths.save_version=0
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'TimberRoofVariants.blend'))
(OUT/'timber-roof-specs.json').write_text(json.dumps({'schema_version':1,'units':'metres','authoring_axes':'+Z up / -Y front',
    'material_reference_asset':'../VillageKit/modules/beam_timber_2m.glb','material_reference_sha256':beam_hash,
    'base_wood_pbr':beam_pbr,'modules':modules,'original_sources_modified':False,'native_assets_imported':False,
    'inventory_or_recipe_binding_verified':False},indent=2)+'\n',encoding='utf-8')
(OUT/'timber-roof-source-audit.json').write_text(json.dumps({'modeling_interface':'Blender MCP execute_blender_code',
    'blender_version':bpy.app.version_string,'source_blend':'TimberRoofVariants.blend','source_blend_sha256':sha(OUT/'TimberRoofVariants.blend'),
    'objects':[o.name for o in scene.objects],'mesh_count':len([o for o in scene.objects if o.type=='MESH']),
    'assets':source_rows,'source_geometries_intentionally_unchanged':True,'external_images':[]},indent=2)+'\n',encoding='utf-8')
print('TIMBER_ROOF_VARIANTS_COMPLETE',flush=True)
