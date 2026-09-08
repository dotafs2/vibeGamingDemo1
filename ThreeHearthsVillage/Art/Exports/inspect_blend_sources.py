from pathlib import Path
import json
import subprocess
import bpy
from mathutils import Vector

PROJECT = Path(__file__).resolve().parents[2]
REPO = PROJECT.parent
paths = subprocess.check_output(['git', 'ls-files', '*.blend'], cwd=REPO).decode().splitlines()
report=[]
for path in paths:
    bpy.ops.wm.open_mainfile(filepath=str(REPO/path))
    meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
    points=[o.matrix_world@Vector(v) for o in meshes for v in o.bound_box]
    report.append({'path':path,'unit_scale':bpy.context.scene.unit_settings.scale_length,
        'meshes':len(meshes),'objects':len(bpy.context.scene.objects),
        'vertices':sum(len(o.data.vertices) for o in meshes),
        'dimensions':[max(p[i] for p in points)-min(p[i] for p in points) for i in range(3)],
        'mesh_names':[o.name for o in meshes],
        'hidden_meshes':[o.name for o in meshes if o.hide_render]})
out=PROJECT/'Saved/ThreeHearths/CombinedFbx'
out.mkdir(parents=True,exist_ok=True)
(out/'blend-source-report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('BLEND_SOURCES '+json.dumps([{k:v for k,v in r.items() if k not in ('mesh_names','hidden_meshes')} for r in report]),flush=True)
