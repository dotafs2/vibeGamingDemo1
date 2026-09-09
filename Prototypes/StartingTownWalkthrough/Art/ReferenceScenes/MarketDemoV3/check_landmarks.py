import sys
from pathlib import Path
import bpy
HERE=Path(__file__).resolve().parent
sys.path.insert(0,str(HERE))
import craft_landmarks as lm
import build_reference_scenes_hq as hq

bpy.ops.wm.read_factory_settings(use_empty=True)
sc=hq.scene('Market_V3_Landmark_Check'); col=sc.collection
hq.camera(sc,'Landmark_Check_Camera',(0,-18,2.0),(0,20,7.0),62)
lm.build_gate(col,(0,24,0),15,10,6.6,7.0,3.0)
lm.build_tower(col,(6.1,26,0),2.1,10.8,3.0,4.4)
lm.build_dome_cluster(col,[(-4,44,0),(-10,49,0),(1,46,0)],4.2,13.0,2.3)
for o in sc.objects: o.select_set(o.type=='MESH')
bpy.context.scene.render.engine='BLENDER_WORKBENCH'; bpy.context.scene.render.resolution_x=800; bpy.context.scene.render.resolution_y=450; bpy.context.scene.render.resolution_percentage=100
bpy.context.scene.render.filepath=str(HERE/'landmark_check.png'); bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=str(HERE/'landmark_check.blend'),compress=True)
print('landmark_check_complete')
