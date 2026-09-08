"""Combine all tracked 3D sources, native meshes and 12 native art recipes.

Blender --background --factory-startup --threads 4 --python <this script>
Requires export_native_fbx.py first. Export entries are versions/representations,
not a claim of this many unique original models. Geometry remains separate.
"""
from pathlib import Path
from collections import Counter
import hashlib
import json
import math
import time
import sys
import bpy
from mathutils import Vector, Matrix

PROJECT = Path(__file__).resolve().parents[2]
REPO = PROJECT.parent
WORK = PROJECT/'Saved/ThreeHearths/CombinedFbx'
OUT = Path(__file__).resolve().parent
NATIVE = json.loads((WORK/'native-export-report.json').read_text(encoding='utf-8'))
CATALOG = json.loads((WORK/'catalog.json').read_text(encoding='utf-8'))
INVENTORY = json.loads((PROJECT/'Docs/Art_Asset_Inventory.json').read_text(encoding='utf-8'))
LABELS = {r['path']:r.get('name_zh',r['id']) for r in INVENTORY['records']}
COMMIT = NATIVE['commit']
FBX = OUT/('ThreeHearths_All_Git_Assets_'+COMMIT[:7]+'.fbx')
BLEND = FBX.with_suffix('.blend')
MANIFEST = FBX.with_suffix('.json')
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1.0
entries = []
sections = {}
native_data = {}
native_bounds_errors = []
started = time.monotonic()

def collection(name):
    if name not in sections:
        c = bpy.data.collections.new(name)
        scene.collection.children.link(c)
        sections[name] = c
    return sections[name]

def bounds(objects):
    bpy.context.view_layer.update()
    points = [o.matrix_world@Vector(p) for o in objects if o.type=='MESH' for p in o.bound_box]
    assert points
    return (Vector([min(p[k] for p in points) for k in range(3)]),
            Vector([max(p[k] for p in points) for k in range(3)]))

def statistics(objects):
    meshes = [o for o in objects if o.type=='MESH']
    for o in meshes:
        o.data.calc_loop_triangles()
    lo, hi = bounds(meshes)
    return {'mesh_objects':len(meshes), 'vertices':sum(len(o.data.vertices) for o in meshes),
        'triangles':sum(len(o.data.loop_triangles) for o in meshes),
        'uv_mesh_objects':sum(bool(o.data.uv_layers) for o in meshes),
        'armatures':sum(o.type=='ARMATURE' for o in objects),
        'bones':sum(len(o.data.bones) for o in objects if o.type=='ARMATURE'),
        'bounds_min_m':list(lo),'bounds_max_m':list(hi),'dimensions_m':list(hi-lo)}

def add_entry(key, group, objects, source, name_zh='', **extra):
    objects = [o for o in objects if o.type in ('MESH','EMPTY','ARMATURE')]
    assert any(o.type=='MESH' for o in objects), key
    c = collection(group)
    root = bpy.data.objects.new(key, None)
    c.objects.link(root)
    root['asset_id'] = key
    root['source'] = source
    root['name_zh'] = name_zh or key
    for i, obj in enumerate(objects):
        original = obj.name
        obj.name = key[:36]+'__%04d__'%i+original[:18]
        obj['source_object'] = original
        obj.hide_render = False
        obj.hide_viewport = False
        for old in list(obj.users_collection): old.objects.unlink(obj)
        c.objects.link(obj)
    for obj in objects:
        if obj.parent not in objects:
            world = obj.matrix_world.copy()
            obj.parent = root
            obj.matrix_world = world
    result = dict(id=key,group=group,source=source,name_zh=name_zh or key,
        root_name=root.name,objects=[o.name for o in objects],**statistics(objects),**extra)
    entries.append(result)
    print('FBX_ENTRY '+json.dumps({'n':len(entries),'id':key,'meshes':result['mesh_objects']}),flush=True)
    return result

def import_fbx(path):
    before=set(scene.objects)
    # UE exports reflected Y coordinates in Z-up cm. Keeping those axes maps
    # native UE (x,y,z) to Blender metres (x,-y,z) without a second rotation.
    bpy.ops.import_scene.fbx(filepath=str(path),use_manual_orientation=True,
        axis_forward='Y',axis_up='Z',use_anim=False,automatic_bone_orientation=False)
    return list(set(scene.objects)-before)

# Preserve every Git-tracked native mesh, including sample/reference content.
for index, record in enumerate(NATIVE['meshes']):
    objects = import_fbx(record['fbx'])
    if record['class']=='StaticMesh':
        mesh_objects=[o for o in objects if o.type=='MESH']
        # Bake import unit conversion into vertices before recipe instancing.
        for obj in mesh_objects:
            world=obj.matrix_world.copy()
            obj.data=obj.data.copy()
            obj.data.transform(world)
            obj.parent=None
            obj.matrix_world=Matrix.Identity(4)
            # FBX import may use object-linked slots. Cache their effective
            # materials with geometry so new recipe instances inherit them.
            effective=[slot.material for slot in obj.material_slots]
            for slot in obj.material_slots: slot.link='DATA'
            for slot,mat in zip(obj.material_slots,effective): slot.material=mat
        lo, hi=bounds(mesh_objects)
        origin=Vector(record['bounds_origin_cm'])
        extent=Vector(record['bounds_extent_cm'])
        expected_lo=Vector((origin.x-extent.x,-origin.y-extent.y,origin.z-extent.z))*.01
        expected_hi=Vector((origin.x+extent.x,-origin.y+extent.y,origin.z+extent.z))*.01
        error=max(max(abs(lo[k]-expected_lo[k]),abs(hi[k]-expected_hi[k])) for k in range(3))
        if error>.003:
            native_bounds_errors.append({'path':record['path'],'max_error_m':error,
                'actual':[list(lo),list(hi)],'expected':[list(expected_lo),list(expected_hi)]})
        native_data[record['path']]=[o.data for o in mesh_objects]
    if record['in_git']:
        group='03_UE_Generated' if record['path'].startswith('/Game/ThreeHearths/') else '04_UE_Sample'
        add_entry('UE_'+str(index+1).zfill(3)+'_'+record['path'].split('/')[-1],group,objects,
            record['path'],native_class=record['class'])
    else:
        for obj in objects: bpy.data.objects.remove(obj,do_unlink=True)

# glTF sources are independent named deliverables; native representations above
# intentionally remain available under their own group for complete coverage.
for path in NATIVE['tracked_model_sources']:
    if Path(path).suffix.lower() not in ('.glb','.gltf'): continue
    before=set(scene.objects)
    bpy.ops.import_scene.gltf(filepath=str(REPO/path))
    objects=list(set(scene.objects)-before)
    kit=Path(path).parts[2]
    key='GLB_'+kit+'_'+Path(path).stem
    add_entry(key,'01_Source_'+kit,objects,path,LABELS.get(path,''),
        source_sha256=hashlib.sha256((REPO/path).read_bytes()).hexdigest())

# Source .blend scenes can contain additional UV/roof study versions. Append
# their visible art; omit photography floors and hidden construction helpers.
for path in NATIVE['tracked_model_sources']:
    if Path(path).suffix.lower()!='.blend': continue
    with bpy.data.libraries.load(str(REPO/path),link=False) as (source,target):
        target.scenes=source.scenes
    meshes=[]
    skipped=[]
    for source_scene in target.scenes:
        visible=set()
        def visit(c, hidden=False):
            hidden=hidden or c.hide_render
            for o in c.objects:
                if not hidden and not o.hide_render: visible.add(o)
            for child in c.children: visit(child,hidden)
        visit(source_scene.collection)
        for obj in list(source_scene.objects):
            if obj.type!='MESH': continue
            if obj not in visible or obj.name.lower().startswith(('stage |','studio |')):
                skipped.append(obj.name)
                continue
            # Evaluate source modifiers in the source scene's dependency graph.
            with bpy.context.temp_override(scene=source_scene,view_layer=source_scene.view_layers[0]):
                graph=bpy.context.evaluated_depsgraph_get()
                evaluated=obj.evaluated_get(graph)
                mesh=bpy.data.meshes.new_from_object(evaluated,preserve_all_data_layers=True,depsgraph=graph)
            clone=bpy.data.objects.new(obj.name,mesh)
            scene.collection.objects.link(clone)
            clone.matrix_world=obj.matrix_world.copy()
            meshes.append(clone)
        bpy.data.scenes.remove(source_scene)
    add_entry('BLEND_'+Path(path).stem,'05_Blender_Source_Versions',meshes,path,LABELS.get(path,''),
        source_sha256=hashlib.sha256((REPO/path).read_bytes()).hexdigest(),
        omitted_photography_or_hidden_objects=skipped)

tints={}
def tint_material(color):
    key=tuple(round(float(c),6) for c in color)
    if key not in tints:
        mat=bpy.data.materials.new('RecipeTint_'+'_'.join(str(round(c*255)) for c in key[:3]))
        mat.diffuse_color=key
        mat.use_nodes=True
        bsdf=mat.node_tree.nodes.get('Principled BSDF')
        bsdf.inputs['Base Color'].default_value=key
        bsdf.inputs['Roughness'].default_value=.78
        tints[key]=mat
    return tints[key]

# Reconstruct the exact C++ recipe parts, including native bounds centering,
# original modules, material overrides, nonuniform scale and signed UE yaw.
for entry in CATALOG['entries']:
    objects=[]
    for i,part in enumerate(entry['parts']):
        path=part['mesh_path'].split('.')[0]
        source=NATIVE['meshes'][next(k for k,r in enumerate(NATIVE['meshes']) if r['path']==path)]
        yaw=Matrix.Rotation(-math.radians(part['yaw_degrees']),4,'Z')
        scale=Matrix.Diagonal(Vector((*part['scale'],1.0)))
        v=part['offset_cm']
        location=Vector((v[0],-v[1],v[2]))*.01
        if part['center_native_bounds']:
            v=source['bounds_origin_cm']
            center=Vector((v[0],-v[1],v[2]))*.01
            location-=(yaw@scale).to_3x3()@center
        transform=Matrix.Translation(location)@yaw@scale
        for j,data in enumerate(native_data[path]):
            obj=bpy.data.objects.new(entry['id']+'_%04d_%d'%(i,j),data)
            scene.collection.objects.link(obj)
            obj.matrix_world=transform
            obj['native_mesh']=path
            if part.get('color_linear') is not None:
                mat=tint_material(part['color_linear'])
                if not obj.data.materials: obj.data.materials.append(mat)
                for slot in obj.material_slots:
                    slot.link='OBJECT'
                    slot.material=mat
            objects.append(obj)
    add_entry('NEW_'+entry['id'],'02_New_Buildings_And_Plants',objects,
        'Hearth.ExportArtCatalog/'+entry['id'],entry['name_zh'],
        source_recipe_parts=len(entry['parts']),preview_only=entry.get('preview_only',False))

# Catalog layout: real scale, separate roots, bottom on ground, no rescaling.
# Each category occupies a band, packed into rows with 80 metres maximum width.
y=0.0
for group in sorted(sections):
    x=0.0
    row_height=0.0
    for entry in [e for e in entries if e['group']==group]:
        lo=Vector(entry['bounds_min_m']); hi=Vector(entry['bounds_max_m']); size=hi-lo
        gap=max(1.5,min(5.0,max(size.x,size.y)*.12))
        if x>0 and x+size.x>80:
            x=0.0; y+=row_height+4.0; row_height=0.0
        offset=Vector((x-lo.x,y-lo.y,-lo.z))
        bpy.data.objects[entry['root_name']].location=offset
        entry['layout_offset_m']=list(offset)
        entry['layout_bounds_min_m']=list(lo+offset)
        entry['layout_bounds_max_m']=list(hi+offset)
        x+=size.x+gap
        row_height=max(row_height,size.y)
    y+=row_height+10.0
bpy.context.view_layer.update()

# Remove orphaned source-scene data, retaining used geometry and materials.
bpy.data.orphans_purge(do_local_ids=True,do_linked_ids=True,do_recursive=True)
# Blender FBX cannot share geometry across conflicting material-slot layouts.
# Reuse geometry only when the effective per-object material list also agrees.
material_variants={}
for obj in scene.objects:
    if obj.type!='MESH': continue
    effective=[slot.material for slot in obj.material_slots]
    key=(obj.data.as_pointer(),tuple(mat.as_pointer() if mat else 0 for mat in effective))
    if key not in material_variants:
        data=obj.data.copy()
        for i,mat in enumerate(effective): data.materials[i]=mat
        material_variants[key]=data
    obj.data=material_variants[key]
    for slot in obj.material_slots: slot.link='DATA'
# Imported custom int vectors trigger a Blender 5.2 FBX encoder assertion.
# Preserve non-scalar metadata as JSON strings; identifiers remain strings.
blocks=list(scene.objects)+list(bpy.data.meshes)+list(bpy.data.materials)+list(bpy.data.armatures)
blocks += [bone for arm in bpy.data.armatures for bone in arm.bones]
for block in blocks:
    for key in list(block.keys()):
        value=block[key]
        if not isinstance(value,(str,int,float,bool)):
            try: value=list(value)
            except TypeError: value=str(value)
            block[key]=json.dumps(value,default=str)
sys.path.insert(0,str(OUT))
from prepare_fbx_images import prepare_images
texture_report=prepare_images(WORK/'Textures')
bpy.ops.object.select_all(action='SELECT')
export_objects=list(scene.objects)
expected=statistics(export_objects)
manifest={'schema':1,'git_commit':COMMIT,'git_branch':'codex/town-growth-20260907',
    'fbx':FBX.name,'blend':BLEND.name,'units':'metres','layout':'Separate named roots, real scale, grouped rows',
    'entry_count':len(entries),'group_counts':dict(Counter(e['group'] for e in entries)),
    'textures':texture_report,
    'tracked_model_source_count':len(NATIVE['tracked_model_sources']),
    'native_tracked_mesh_count':sum(r['in_git'] for r in NATIVE['meshes']),
    'expected':expected,'native_bounds_checks':{'checked':len(native_data),'exceptions':native_bounds_errors},
    'counting_note':'Includes GLB, UE and Blend versions of shared geometry; entry count is not unique original model count.',
    'limitations':['FBX contains 3D geometry, UVs, available standard materials, vertex colors and skeletons. It does not contain game code, simulation saves, audio, documentation PNGs, animation clips or Unreal shader graphs.',
        'Source .blend photography floors, cameras, lights and hidden helpers are excluded. Meshes remain available in source files.',
        'Royal castle is a full design preview, not the construction state of a saved game.'],
    'entries':entries}
MANIFEST.write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
bpy.ops.wm.save_as_mainfile(filepath=str(BLEND),compress=True)
bpy.ops.export_scene.fbx(filepath=str(FBX),use_selection=True,object_types={'MESH','EMPTY','ARMATURE'},
    global_scale=1.0,apply_unit_scale=True,apply_scale_options='FBX_SCALE_UNITS',
    axis_forward='-Z',axis_up='Y',use_mesh_modifiers=True,mesh_smooth_type='OFF',
    add_leaf_bones=False,bake_anim=False,path_mode='COPY',embed_textures=True,use_custom_props=True)
manifest['fbx_bytes']=FBX.stat().st_size
manifest['fbx_sha256']=hashlib.sha256(FBX.read_bytes()).hexdigest()
manifest['seconds']=round(time.monotonic()-started,2)
MANIFEST.write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
print('COMBINED_FBX_COMPLETE '+json.dumps({k:manifest[k] for k in ('fbx','entry_count','expected','fbx_bytes','seconds')}),flush=True)
