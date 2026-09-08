"""Round-trip structural, scale, UV and skeleton verification of delivered FBX."""
from pathlib import Path
import json
import math
import bpy
from mathutils import Vector

OUT=Path(__file__).resolve().parent
manifest_path=OUT/'ThreeHearths_All_Git_Assets_ccd6757.json'
manifest=json.loads(manifest_path.read_text(encoding='utf-8'))
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(OUT/manifest['fbx']),use_custom_props=True,use_anim=False,
    automatic_bone_orientation=False)
scene=bpy.context.scene
def stats(objects):
    meshes=[o for o in objects if o.type=='MESH']
    for o in meshes:o.data.calc_loop_triangles()
    points=[o.matrix_world@Vector(p) for o in meshes for p in o.bound_box]
    return {'mesh_objects':len(meshes),'triangles':sum(len(o.data.loop_triangles) for o in meshes),
        'uv_mesh_objects':sum(bool(o.data.uv_layers) for o in meshes),
        'armatures':sum(o.type=='ARMATURE' for o in objects),
        'bones':sum(len(o.data.bones) for o in objects if o.type=='ARMATURE'),
        'lo':[min(p[k] for p in points) for k in range(3)],
        'hi':[max(p[k] for p in points) for k in range(3)]}
results=[]
errors=[]
for entry in manifest['entries']:
    root=bpy.data.objects.get(entry['root_name'])
    if root is None:
        errors.append({'id':entry['id'],'error':'missing root'})
        continue
    actual=stats(root.children_recursive)
    mismatch={k:[entry[k],actual[k]] for k in ('mesh_objects','triangles','uv_mesh_objects','armatures','bones') if entry[k]!=actual[k]}
    tolerance=.003
    error=max(abs(actual[key][k]-entry[ref][k]) for key,ref in (
        ('lo','layout_bounds_min_m'),('hi','layout_bounds_max_m')) for k in range(3))
    if error>tolerance:mismatch['bounds_error_m']=error
    if root.get('asset_id')!=entry['id']:mismatch['asset_id']='lost custom property'
    if mismatch:errors.append({'id':entry['id'],'mismatch':mismatch})
    results.append({'id':entry['id'],'bounds_error_m':error,**actual})
actual=stats(list(scene.objects))
for key in ('mesh_objects','triangles','uv_mesh_objects','armatures','bones'):
    if actual[key]!=manifest['expected'][key]:errors.append({'total':key,'expected':manifest['expected'][key],'actual':actual[key]})
report={'fbx':manifest['fbx'],'git_commit':manifest['git_commit'],'entry_count':len(results),
    'passed':not errors,'actual':actual,'errors':errors,'entries':results,
    'materials':len(bpy.data.materials),'images':len(bpy.data.images),
    'missing_images':[im.filepath for im in bpy.data.images if im.source=='FILE' and not im.packed_file and not Path(bpy.path.abspath(im.filepath)).is_file()]}
if report['missing_images']:
    errors.append({'missing_images':report['missing_images']})
report['passed']=not errors
(OUT/'FBX_Validation.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
print('FBX_VALIDATION '+json.dumps({k:v for k,v in report.items() if k!='entries'}),flush=True)
assert not errors,errors[:5]

# Render actual round-tripped additions so placement, rotations and material
# overrides can be inspected, rather than reusing an earlier source preview.
THUMBS=OUT/'Preview_New_Assets'
THUMBS.mkdir(exist_ok=True)
all_objects=list(scene.objects)
for obj in all_objects:obj.hide_render=True
scene.render.engine='BLENDER_WORKBENCH'
scene.render.resolution_x=960
scene.render.resolution_y=720
scene.render.resolution_percentage=100
scene.render.image_settings.file_format='PNG'
scene.render.image_settings.color_mode='RGBA'
scene.render.film_transparent=True
scene.display.render_aa='16'
shade=scene.display.shading
shade.light='STUDIO'
shade.studio_light='paint.sl'
shade.color_type='MATERIAL'
shade.show_shadows=True
shade.show_cavity=True
shade.cavity_type='BOTH'
shade.background_type='WORLD'
scene.view_settings.view_transform='Standard'
for entry in [e for e in manifest['entries'] if e['id'].startswith('NEW_')]:
    root=bpy.data.objects[entry['root_name']]
    root.hide_render=False
    objects=list(root.children_recursive)
    for obj in objects:obj.hide_render=False
    lo=Vector(entry['layout_bounds_min_m']);hi=Vector(entry['layout_bounds_max_m'])
    center=(lo+hi)*.5;span=max(hi-lo)
    camera_data=bpy.data.cameras.new('Verification camera')
    camera=bpy.data.objects.new('Verification camera',camera_data)
    scene.collection.objects.link(camera)
    direction=Vector((6,-9,7)).normalized()
    camera.location=center+direction*span*4
    camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
    camera_data.type='ORTHO'
    camera_data.clip_start=.001
    camera_data.clip_end=max(1000,span*20)
    camera_data.sensor_fit='HORIZONTAL'
    rotation=camera.rotation_euler.to_matrix().transposed()
    pts=[rotation@(Vector((x,y,z))-center) for x in (lo.x,hi.x) for y in (lo.y,hi.y) for z in (lo.z,hi.z)]
    width=max(p.x for p in pts)-min(p.x for p in pts)
    height=max(p.y for p in pts)-min(p.y for p in pts)
    camera_data.ortho_scale=max(width,height*960/720)*1.12
    scene.camera=camera
    scene.render.filepath=str(THUMBS/(entry['id']+'.png'))
    bpy.ops.render.render(write_still=True)
    for obj in objects:obj.hide_render=True
    bpy.data.objects.remove(camera,do_unlink=True)
    print('FBX_PREVIEW '+entry['id'],flush=True)
