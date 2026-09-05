"""Render one SocietyKit review view in a bounded Blender MCP invocation."""
from pathlib import Path
import json
import sys
import time
import bpy

OUT=Path(__file__).resolve().parent
ARGS=sys.argv[sys.argv.index('--')+1:] if '--' in sys.argv else []
view=ARGS[0] if ARGS else 'modules'
views={'modules':('SocietyKit.blend','SocietyKit_Modules.png'),
       'castle':('SocietyKit_kings_gate_courtyard.blend','SocietyKit_kings_gate_courtyard.png'),
       'market':('SocietyKit_guild_market_yard.blend','SocietyKit_guild_market_yard.png')}
source,target=views[view]
start=time.monotonic()
bpy.ops.wm.open_mainfile(filepath=str(OUT/source))
scene=bpy.context.scene
scene.cycles.samples=24
scene.render.filepath=str(OUT/'previews'/target)
bpy.ops.render.render(write_still=True)
path=OUT/'render-report.json'
report=json.loads(path.read_text()) if path.exists() else {}
report[view]={'source':source,'preview':'previews/'+target,'seconds':round(time.monotonic()-start,3),
    'engine':scene.render.engine,'samples':scene.cycles.samples,
    'resolution':[scene.render.resolution_x,scene.render.resolution_y]}
path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print('SOCIETY_RENDER_COMPLETE',view,flush=True)
