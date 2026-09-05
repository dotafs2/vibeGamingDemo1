"""Repair the saved cottage into a versioned shared-material-UV deliverable.

Existing .blend files and open unsaved scenes are preserved. No geometry rebuild.
"""
import bpy
import hashlib
import importlib
import json
import sys
from pathlib import Path
from mathutils import Vector

OUT=Path(__file__).resolve().parent
sys.path.insert(0,str(OUT))
from shared_roof_uv import apply_shared_roof_uv, ROOF_NAMES, UV_NAME

STEM='HearthCottage_SharedUV'
house=bpy.data.collections['HOUSE | Hearth Cottage']
objects=[o for o in house.objects if o.type=='MESH']

def geometry_fingerprint():
    data=[]
    for obj in sorted(objects,key=lambda o:o.name):
        data.append((obj.name,tuple(tuple(row) for row in obj.matrix_world),
                     [tuple(v.co) for v in obj.data.vertices],
                     [(tuple(p.vertices),p.material_index) for p in obj.data.polygons],
                     [m.name for m in obj.data.materials]))
    return hashlib.sha256(json.dumps(data).encode()).hexdigest()

if bpy.context.object and bpy.context.object.mode!='OBJECT':
    bpy.ops.object.mode_set(mode='OBJECT')
bpy.context.view_layer.update()
source_path=Path(bpy.data.filepath)
source_hash=hashlib.sha256(source_path.read_bytes()).hexdigest()
before=geometry_fingerprint()
report=apply_shared_roof_uv(house)
assert before==geometry_fingerprint(),'Geometry or materials changed during UV repair'
report['geometry_and_materials_unchanged']=True
report['source_blend']=source_path.name
report['source_sha256']=source_hash
(OUT/(STEM+'_uv-report.json')).write_text(json.dumps(report,indent=2),encoding='utf-8')

# Export UV0 with intentional roof overlap and no diagnostic images/materials.
bpy.ops.object.select_all(action='DESELECT')
for obj in house.objects:
    obj.select_set(True)
bpy.context.view_layer.objects.active=objects[0]
bpy.ops.export_scene.gltf(filepath=str(OUT/(STEM+'.glb')),export_format='GLB',use_selection=True,
    export_yup=True,export_apply=True,export_texcoords=True,export_animations=False,
    export_cameras=False,export_lights=False,export_extras=True,export_materials='EXPORT')
model_report=json.loads((OUT/'model-report.json').read_text(encoding='utf-8'))
model_report['asset']=STEM
model_report['uv']=report
(OUT/(STEM+'_model-report.json')).write_text(json.dumps(model_report,indent=2),encoding='utf-8')

# Export only distinct roof polygons, so the SVG represents the shared footprint
# rather than painting the same UV island hundreds of times.
polygons=set()
for obj in objects:
    if obj.name not in ROOF_NAMES: continue
    uv=obj.data.uv_layers.active.data
    for face in obj.data.polygons:
        coords=[(round(uv[i].uv.x,6),round(uv[i].uv.y,6)) for i in face.loop_indices]
        variants=[]
        for order in (coords,list(reversed(coords))):
            variants.extend(tuple(order[i:]+order[:i]) for i in range(len(order)))
        polygons.add(min(variants))
svg=['<svg xmlns="http://www.w3.org/2000/svg" width="1536" height="1536" viewBox="0 0 1536 1536">',
     '<rect width="1536" height="1536" fill="#252a32"/>']
for poly in sorted(polygons):
    points=' '.join(f'{x*1536:.3f},{(1-y)*1536:.3f}' for x,y in poly)
    svg.append(f'<polygon points="{points}" fill="#dfab69" fill-opacity=".12" stroke="#efad66" stroke-width="1.3"/>')
svg.append('</svg>')
(OUT/(STEM+'_RoofUV.svg')).write_text('\n'.join(svg),encoding='utf-8')

# Opening this file exposes roof reuse directly, with the rest of the cottage visible.
bpy.ops.object.select_all(action='DESELECT')
for obj in objects:
    if obj.name in ROOF_NAMES: obj.select_set(True)
bpy.context.view_layer.objects.active=bpy.data.objects['Roof | main curved tiles']
for obj in bpy.data.collections['PRESENTATION | not exported'].objects:
    obj.hide_set(True)
grid=bpy.data.images.get('UV inspection grid | generated, not a model texture')
if not grid:
    grid=bpy.data.images.new('UV inspection grid | generated, not a model texture',width=1024,height=1024)
    grid.generated_type='UV_GRID'
    grid.pack()
for workspace in bpy.data.workspaces:
    for screen in workspace.screens:
        for area in screen.areas:
            if area.type=='VIEW_3D':
                space=area.spaces.active
                space.shading.type='SOLID'
                space.shading.color_type='MATERIAL'
                space.overlay.show_floor=False
                space.overlay.show_axis_x=False
                space.overlay.show_axis_y=False
                space.overlay.show_faces=False
                space.overlay.show_relationship_lines=False
                space.region_3d.view_rotation=bpy.context.scene.camera.rotation_euler.to_quaternion()
                space.region_3d.view_location=Vector((0,-.1,1.55))
                space.region_3d.view_distance=7.5
                space.region_3d.view_perspective='ORTHO'
            elif area.type=='IMAGE_EDITOR':
                area.ui_type='UV'
                area.spaces.active.image=grid
bpy.context.window.workspace=bpy.data.workspaces['UV Editing']
bpy.ops.object.mode_set(mode='EDIT')
bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.uv.select_all(action='SELECT')
notes=bpy.data.texts.new('READ ME | Shared roof UV')
notes.write('Corrected version: UV_Material (texture UV0).\n'
            '182 curved tiles reuse one prototype unwrap; 13 ridge caps reuse a second.\n'
            'Same-size pieces have identical UVs; cut/short variants use a subregion.\n'
            'Two prototype unwraps, 12 charts total. Overlap is intentional.\n'
            'This is NOT a lightmap UV channel. Geometry and solid-colour materials are unchanged.\n')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/(STEM+'.blend')))
assert hashlib.sha256(source_path.read_bytes()).hexdigest()==source_hash
print('SHARED_UV_COMPLETE '+json.dumps(report),flush=True)
