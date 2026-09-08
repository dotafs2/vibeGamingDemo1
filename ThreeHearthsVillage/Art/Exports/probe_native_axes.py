import bpy
from pathlib import Path
from mathutils import Vector
import json
ROOT=Path(__file__).resolve().parents[2]/'Saved/ThreeHearths/CombinedFbx'
files=list(ROOT.glob('*.fbx'))[:5]
for path in files:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(path),use_manual_orientation=True,axis_forward='Y',axis_up='Z')
    obs=[o for o in bpy.context.scene.objects if o.type=='MESH']
    pts=[o.matrix_world@Vector(v) for o in obs for v in o.bound_box]
    print('AXES '+json.dumps({'file':path.name,'min':[min(p[i] for p in pts) for i in range(3)],
        'max':[max(p[i] for p in pts) for i in range(3)],'matrices':[[list(row) for row in o.matrix_world] for o in obs]}),flush=True)
