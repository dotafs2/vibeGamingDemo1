"""Render one saved kit view at a time, suitable for bounded Blender MCP calls."""
from pathlib import Path
import json
import sys
import time
import bpy
from mathutils import Vector

OUT=Path(__file__).resolve().parent
ARGS=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
view=ARGS[0] if ARGS else 'roof'
views={
    'modules':('VillageKit.blend','VillageKit_Modules.png'),
    'houses':('VillageKit_Examples.blend','VillageKit_HouseChoices.png'),
    'roof':('VillageKit_RoofStudy.blend','VillageKit_RoofMaterials.png'),
    'construction':('VillageKit_Examples.blend','VillageKit_Stairwell.png'),
}
source,target=views[view]
start=time.monotonic()
bpy.ops.wm.open_mainfile(filepath=str(OUT/source))
scene=bpy.context.scene
if view=='modules':
    scene.camera.location=(8,-44,60)
    scene.camera.rotation_euler=(Vector((0,0,.7))-scene.camera.location).to_track_quat('-Z','Y').to_euler()
    scene.camera.data.ortho_scale=46
    scene.render.resolution_x=2400
    scene.render.resolution_y=1500
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT/source))
if view=='construction':
    for collection in bpy.data.collections:
        collection.hide_render=True
    collection=bpy.data.collections['EXAMPLE | townhouse_terracotta']
    collection.hide_render=False
    for obj in collection.objects:
        module=obj.get('module_id','')
        obj.hide_render=not module.startswith(('foundation_','floor_','post_','beam_','stairs_'))
        obj.location.x-=5.8
        # Keep lower columns and right-side upper floor; top-storey frame would
        # obscure the opening in this technical cutaway.
        if module.startswith(('post_','beam_')) and obj.location.z>=2.4:
            obj.hide_render=True
    scene.camera.location=(-8,-10,9)
    scene.camera.rotation_euler=(Vector((0,0,1.2))-scene.camera.location).to_track_quat('-Z','Y').to_euler()
    scene.camera.data.ortho_scale=7.0
    scene.render.resolution_x=1600
    scene.render.resolution_y=1200
scene.cycles.samples=24
scene.render.filepath=str(OUT/'previews'/target)
bpy.ops.render.render(write_still=True)
report_path=OUT/'render-report.json'
report=json.loads(report_path.read_text()) if report_path.exists() else {}
report[view]={'source':source,'preview':'previews/'+target,'seconds':round(time.monotonic()-start,3),
              'engine':scene.render.engine,'samples':scene.cycles.samples,
              'resolution':[scene.render.resolution_x,scene.render.resolution_y]}
report_path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('VILLAGE_KIT_RENDER_COMPLETE',view,flush=True)
